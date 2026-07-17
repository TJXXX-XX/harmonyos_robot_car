#ifndef MQTT_MODULE_H
#define MQTT_MODULE_H

// 暴露给主任务的 MQTT 指令标志位
// 0: 无动作, 1: 模拟按下S1, 2: 模拟按下S2, 3: 模拟按下S3
extern volatile int mqtt_cmd_event;
extern volatile int mqtt_remote_dir; // 【新增】0:停止, 1:前, 2:后, 3:左, 4:右, 5:左前, 6:右前, 7:左后, 8:右后

// 定义一个发布状态的函数
void Send_Status_To_App(int mode, const char* action, int speed_straight, int speed_turn);

#endif