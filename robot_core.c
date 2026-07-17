#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#include "ohos_init.h"
#include "cmsis_os2.h"
#include "iot_gpio.h"
#include "hi_io.h"
#include "hi_time.h"

#include "motor_control.h"
#include "sensor.h"
#include "adc_key.h"
#include "display.h"
#include "mqtt_module.h"

// ===== 定义运行模式 =====
#define MODE_STOP    0  
#define MODE_TRACK   1  // 💡【核心】现在的 TRACK 模式自带双白记忆与终点刹车
#define MODE_REMOTE  2  

// ===== 核心主任务 =====
void RobotTask(void* parame) {
    (void)parame;

    // 1. 初始化外设
    pwm_motor_init();
    sensor_init();
    Key_Init();
    OLED_Init();
    printf("All Peripherals Initialized!\r\n");

    // 2. 状态机与速度变量初始化
    int current_mode = MODE_STOP;
    int speed_straight = 80; // 初始直行速度
    int speed_turn = 61;     // 初始转弯速度 (Sweet Point)

    const char* current_action = "Stopped";

    // 历史状态记录 
    int last_mode = -1;
    int last_speed = -1;
    int last_speed_turn = -1;
    const char* last_action = "";

    // 高级循迹记忆与计数器
    int loop_counter = 0;
    int last_extreme_turn = 0;    // 记忆最后一次转向（0:直行, 1:左转, 2:右转）
    int lost_line_counter = 0;    // 双白防抖计数器
    int double_black_counter = 0; // 双黑终点防抖计数器
    int oled_limit_counter = 0; // OLED 刷新锁帧计数器

    IotGpioValue left_ir, right_ir;
    float distance = 0.0;

    printf("System Ready! Waiting for S3 or MQTT Command...\r\n");

    // 3. 超高频主循环
    while (1) {

        // --- 第一部分：按键事件扫描与处理 ---
        int current_key = Get_Key_Event(); // 获取实体按键

        if (mqtt_cmd_event != 0) {
            if (mqtt_cmd_event == 1) current_key = KEY_EVENT_S1;
            if (mqtt_cmd_event == 2) current_key = KEY_EVENT_S2;
            if (mqtt_cmd_event == 3) current_key = KEY_EVENT_S3;
            mqtt_cmd_event = 0;
        }

        switch (current_key) {
        case KEY_EVENT_S3:
            // 💡【简化】：三态轮流切换 -> STOP -> TRACK -> REMOTE -> STOP ...
            if (current_mode == MODE_STOP) {
                current_mode = MODE_TRACK;
            }
            else if (current_mode == MODE_TRACK) {
                current_mode = MODE_REMOTE;
                car_stop();
            }
            else {
                current_mode = MODE_STOP;
                car_stop();
                last_extreme_turn = 0; // 重置最后一次转向
            }
            break;

        case KEY_EVENT_S2: // 加速
            speed_straight += 5;
            speed_turn += 1;

            if (speed_straight > 100) speed_straight = 100;
            if (speed_turn > 65) speed_turn = 65;
            break;

        case KEY_EVENT_S1: // 减速
            speed_straight -= 5;
            speed_turn -= 1;

            if (speed_straight < 50) speed_straight = 50;
            if (speed_turn < 55) speed_turn = 55;
            break;
        default:
            break;
        }

        // 💡 动态终点判定阈值计算
        //int finish_threshold = (19 - (speed_straight / 5)) * 2;
        int finish_threshold = 7 - (speed_straight / 80); // 阶段一：直行防抖时间


        // --- 第二部分：运动逻辑与动作状态获取 ---
        if (current_mode == MODE_TRACK) {
            distance = GetDistance();
            get_tcrt5000_value(&left_ir, &right_ir);

            if (distance > 0 && distance < 15.0) {
                car_stop();
                current_action = "Obstacle!";
                double_black_counter = 0; // 遇到障碍物，双黑嫌疑解除
            }
            else {
                // 1. 左边压线（微调阶段）：重置双白和双黑计数器
                if (left_ir == IOT_GPIO_VALUE1 && right_ir == IOT_GPIO_VALUE0) {
                    car_left(speed_turn, speed_turn, loop_counter, 1);
                    current_action = "Soft L";
                    last_extreme_turn = 1;
                    lost_line_counter = 0;
                    double_black_counter = 0;
                }
                // 2. 右边压线（微调阶段）：重置双白和双黑计数器
                else if (left_ir == IOT_GPIO_VALUE0 && right_ir == IOT_GPIO_VALUE1) {
                    car_right(speed_turn, speed_turn, loop_counter, 1);
                    current_action = "Soft R";
                    last_extreme_turn = 2;
                    lost_line_counter = 0;
                    double_black_counter = 0;
                }
                // 3. 双黑（十字路口 或 终点线）
                else if (left_ir == IOT_GPIO_VALUE1 && right_ir == IOT_GPIO_VALUE1) {
                    double_black_counter++; // 开始积累双黑时间

                    if (double_black_counter < finish_threshold) {
                        car_forward(speed_straight);
                        current_action = "Cross FW";
                        lost_line_counter = 0;
                    }
                    else {
                        // 超过阈值，确认为终点！
                        car_stop();
                        current_action = "FINISH!";
                        current_mode = MODE_STOP; // 强制切回 STOP 模式锁死车轮
                        double_black_counter = 0; // 重置以备下次起步
                        last_extreme_turn = 0; // 重置最后一次转向
                    }
                }
                // 4. 双白（丢线阶段防抖与坦克掉头）
                else {
                    double_black_counter = 0; // 冲出黑线了，双黑嫌疑解除

                    lost_line_counter++;
                    //int current_threshold = (14 - (speed_straight / 10)) * 2;
                    int current_threshold = (12 - (speed_straight / 10)) * 2;
                    int soft_turn_cycles = 10; // 💡【参数可调】：阶段二的微调持续时间（10次约等于20ms）
                    int spin_threshold = current_threshold + soft_turn_cycles; // 阶段三：触发坦克掉头的时间线

                    if (lost_line_counter < current_threshold) {
                        car_forward(speed_straight);
                        current_action = "FW Delay";
                    }
                    else if (lost_line_counter < spin_threshold) {
                        //else {
                        if (last_extreme_turn == 1) {
                            car_left(speed_turn, speed_turn, loop_counter, 2);
                            current_action = "SPIN L!";
                        }
                        else if (last_extreme_turn == 2) {
                            car_right(speed_turn, speed_turn, loop_counter, 2);
                            current_action = "SPIN R!";
                        }
                        else {
                            car_forward(speed_straight);
                            current_action = "Lost FW";
                        }
                    }
                    else {
                        // 阶段三：超过微调时间还没找到线（实打实的急弯），开启暴力找线！
                        if (last_extreme_turn == 1) {
                            car_left(speed_turn + 15, speed_turn + 15, loop_counter, 3); // 参数 2：坦克掉头
                            current_action = "SPIN L!";
                        }
                        else if (last_extreme_turn == 2) {
                            car_right(speed_turn + 15, speed_turn + 15, loop_counter, 3); // 参数 2：坦克掉头
                            current_action = "SPIN R!";
                        }
                        else {
                            car_forward(speed_straight);
                            current_action = "Lost FW";
                        }
                    }
                }
            }
        }
        else if (current_mode == MODE_REMOTE) {
            // 遥控逻辑
            switch (mqtt_remote_dir) {
            case 1: car_forward(speed_straight); current_action = "R: FWD"; break;
            case 2: car_backward(speed_straight); current_action = "R: BWD"; break;
            case 3: car_left(speed_turn + 20, speed_turn + 20, loop_counter, 2); current_action = "R: LEFT"; break;
            case 4: car_right(speed_turn + 20, speed_turn + 20, loop_counter, 2); current_action = "R: RIGHT"; break;
            case 5: car_forward_left(speed_straight); current_action = "R: FWD-L"; break;
            case 6: car_forward_right(speed_straight); current_action = "R: FWD-R"; break;
            case 7: car_backward_left(speed_straight); current_action = "R: BWD-L"; break;
            case 8: car_backward_right(speed_straight); current_action = "R: BWD-R"; break;
            case 0:
            default: car_stop(); current_action = "R: Standby"; break;
            }
        }
        else {
            current_action = "Stopped";
        }

        // --- 第三部分：OLED 智能刷新逻辑 (带锁帧器) ---
        oled_limit_counter++;

        // 假设 osDelay(2) 约等于 2~10 毫秒，我们积攒 20 次循环再允许刷新一次屏幕
        // 相当于把 OLED 的刷新率限制在 10~20 FPS，人类看着流畅，单片机也不累
        if (oled_limit_counter >= 5) {

            if (current_mode != last_mode || speed_straight != last_speed ||
                speed_turn != last_speed_turn || current_action != last_action) {

                OLED_Show_Status(current_mode, current_action, speed_straight, speed_turn);

                last_mode = current_mode;
                last_speed = speed_straight;
                last_speed_turn = speed_turn;
                last_action = current_action;
            }

            // 重置屏幕刷新计数器
            oled_limit_counter = 0;
        }

        loop_counter++;
        if (loop_counter >= 5) { loop_counter = 0; }

        // 💡 维持超高频 2ms 延迟
        osDelay(2);
    }
}

// ===== 任务创建 =====
static void RobotDemo(void) {
    osThreadAttr_t attr;
    attr.name = "RobotTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 10240;
    attr.priority = osPriorityNormal;

    if (osThreadNew(RobotTask, NULL, &attr) == NULL) {
        printf("[RobotDemo] Failed to create RobotTask!\n");
    }
}

APP_FEATURE_INIT(RobotDemo);