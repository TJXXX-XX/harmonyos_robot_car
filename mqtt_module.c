#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "ohos_init.h"
#include "cmsis_os2.h"
#include "hi_wifi_api.h"
#include "lwip/ip_addr.h"
#include "lwip/netifapi.h"
#include "lwip/sockets.h"
#include "MQTTClient.h"
#include "mqtt_module.h"

int wifi_ok_flg = 0;
static struct netif *g_lwip_netif = NULL;
static MQTTClient mq_client;
unsigned char *onenet_mqtt_buf;
unsigned char *onenet_mqtt_readbuf;
int buf_size;
Network n;
MQTTPacket_connectData data = MQTTPacket_connectData_initializer;  

// 定义全局标志位，主任务会来读取它
volatile int mqtt_cmd_event = 0; 
// 【新增全局变量】
volatile int mqtt_remote_dir = 0; 

// --- WiFi 相关回调与初始化 ---
void hi_sta_reset_addr(struct netif *pst_lwip_netif) {
    ip4_addr_t st_gw, st_ipaddr, st_netmask;
    if (pst_lwip_netif == NULL) return;
    IP4_ADDR(&st_gw, 0, 0, 0, 0);
    IP4_ADDR(&st_ipaddr, 0, 0, 0, 0);
    IP4_ADDR(&st_netmask, 0, 0, 0, 0);
    netifapi_netif_set_addr(pst_lwip_netif, &st_ipaddr, &st_netmask, &st_gw);
}

void wifi_wpa_event_cb(const hi_wifi_event *hisi_event) {
    if (hisi_event == NULL) return;
    switch (hisi_event->event) {
        case HI_WIFI_EVT_CONNECTED:
            netifapi_dhcp_start(g_lwip_netif);
            wifi_ok_flg = 1;
            break;
        case HI_WIFI_EVT_DISCONNECTED:
            netifapi_dhcp_stop(g_lwip_netif);
            hi_sta_reset_addr(g_lwip_netif);
            wifi_ok_flg = 0;
            break;
        default:
            break;
    }
}

int hi_wifi_start_sta(void) {
    hi_wifi_assoc_request assoc_req = {0};
    char ifname[WIFI_IFNAME_MAX_SIZE + 1] = {0};
    int len = sizeof(ifname);
    
    hi_wifi_sta_start(ifname, &len);
    hi_wifi_register_event_callback(wifi_wpa_event_cb);
    g_lwip_netif = netifapi_netif_find(ifname);
    
    if (g_lwip_netif == NULL) return -1;
    
    memcpy_s(assoc_req.ssid, HI_WIFI_MAX_SSID_LEN + 1, "TJXsnhh666", strlen("TJXsnhh666"));
    assoc_req.auth = HI_WIFI_SECURITY_WPA2PSK;
    memcpy(assoc_req.key, "12345678", strlen("12345678"));
    return hi_wifi_sta_connect(&assoc_req);
}

void mqtt_callback(MessageData *msg_data) {
    char *msg = (char *)msg_data->message->payload;
    size_t len = msg_data->message->payloadlen;

    printf("[MQTT] Rx Cmd: %.*s\r\n", len, msg);

    // 1. 系统模式/调速指令
    if (strncmp(msg, "S1", len) == 0) mqtt_cmd_event = 1;
    else if (strncmp(msg, "S2", len) == 0) mqtt_cmd_event = 2;
    else if (strncmp(msg, "S3", len) == 0) mqtt_cmd_event = 3;
    
    // 2. 遥控方向指令
    else if (strncmp(msg, "F", len) == 0)  mqtt_remote_dir = 1; // 前
    else if (strncmp(msg, "B", len) == 0)  mqtt_remote_dir = 2; // 后
    else if (strncmp(msg, "L", len) == 0)  mqtt_remote_dir = 3; // 左
    else if (strncmp(msg, "R", len) == 0)  mqtt_remote_dir = 4; // 右
    else if (strncmp(msg, "FL", len) == 0) mqtt_remote_dir = 5; // 左前
    else if (strncmp(msg, "FR", len) == 0) mqtt_remote_dir = 6; // 右前
    else if (strncmp(msg, "BL", len) == 0) mqtt_remote_dir = 7; // 左后
    else if (strncmp(msg, "BR", len) == 0) mqtt_remote_dir = 8; // 右后
    else if (strncmp(msg, "STOP", len) == 0) mqtt_remote_dir = 0; // 停止
}

// 定义一个发布状态的函数
void Send_Status_To_App(int mode, const char* action, int speed_straight, int speed_turn) {
    char status_msg[64];
    // 格式化字符串: "mode,action,spd_s,spd_t"
    // 例如: "2,MoveForward,45,70"
    snprintf(status_msg, sizeof(status_msg), "%d,%s,%d,%d", mode, action, speed_straight, speed_turn);

    MQTTMessage message;
    message.qos = QOS0;
    message.retained = 0;
    message.payload = (void *)status_msg;
    message.payloadlen = strlen(status_msg);

    // 发布到 "car/status" 主题
    if (MQTTPublish(&mq_client, "car/status", &message) < 0) {
        printf("[MQTT] Status Publish Failed!\r\n");
    }
}

int mqtt_connect(void) {
    int rc; // 用于接收函数返回值
    NetworkInit(&n);
    
    printf("[MQTT] Connecting to Server 192.168.230.35 ...\r\n");
    // 1. 尝试连接底层网络端口
    rc = NetworkConnect(&n, "192.168.230.35", 1883);
    if (rc != 0) {
        printf("[MQTT Error] NetworkConnect Failed! Code: %d\r\n", rc);
        printf(">>> 提示: 请检查电脑IP是否正确, 防火墙是否关闭, Mosquitto是否运行!\r\n");
        return -1; // 💡 致命错误，直接退出，防止死机
    }

    buf_size = 4096 + 1024;
    onenet_mqtt_buf = (unsigned char *) malloc(buf_size);
    onenet_mqtt_readbuf = (unsigned char *) malloc(buf_size);
    if (onenet_mqtt_buf == NULL || onenet_mqtt_readbuf == NULL) {
        printf("[MQTT Error] Memory Allocation Failed!\r\n");
        return -1;
    }

    MQTTClientInit(&mq_client, &n, 1000, onenet_mqtt_buf, buf_size, onenet_mqtt_readbuf, buf_size);

    data.keepAliveInterval = 30;
    data.cleansession = 1;
    data.clientID.cstring = "ohos_hi3861_car";

    // 💡 【关键补齐】：把之前的账号密码加上
    data.username.cstring = "TJXXX";
    data.password.cstring = "051211llq";
    
    printf("[MQTT] Sending MQTT Connect Packet...\r\n");
    // 2. 尝试发送 MQTT 连接协议
    rc = MQTTConnect(&mq_client, &data);
    if (rc != 0) {
        printf("[MQTT Error] MQTTConnect Failed! Code: %d\r\n", rc);
        return -1; // 💡 致命错误，直接退出
    }
    
    printf("[MQTT] Connected to Broker Successfully!\r\n");

    // 3. 订阅主题
    MQTTSubscribe(&mq_client, "ohossub", 0, mqtt_callback);

    // 💡 4. 【核心修复】：必须在成功连接后，再开启 Paho 的后台任务！
    MQTTStartTask(&mq_client);

    // 5. 心跳包循环
    while(1) {
        MQTTMessage message;
        message.qos = QOS0;
        message.retained = 0;
        message.payload = (void *)"Car Online";
        message.payloadlen = strlen("Car Online");
        
        if (MQTTPublish(&mq_client, "ID", &message) < 0) {
             printf("[MQTT Error] Publish Failed!\r\n");
        }
        usleep(1000000); 
    }
    return 0;
}

void wifi_sta_task(void *arg) {
    (void)arg;
    hi_wifi_start_sta();
    
    // 1. 等待物理层连接成功
    while(wifi_ok_flg == 0) { 
        usleep(100000); 
    }
    
    // ⚠️ 只要串口没打印下面这句话，就说明你的代码没编译进去！
    printf("\r\n[System] WiFi Connected! Waiting 2s for DHCP IP...\r\n");
    
    // 2. 等待 2 秒 (2000000 微秒)
    usleep(2000000); 
    
    printf("[System] Starting MQTT...\r\n");
    mqtt_connect();
}

static void wifi_sta_entry(void) {
    osThreadAttr_t attr;
    attr.name = "wifi_sta_demo";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 8192; // MQTT 需要较大的栈
    attr.priority = 26;
    osThreadNew((osThreadFunc_t)wifi_sta_task, NULL, &attr);
}

// 独立启动网络任务，不需要在主任务里调用
APP_FEATURE_INIT(wifi_sta_entry);