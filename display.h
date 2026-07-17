#ifndef DISPLAY_H
#define DISPLAY_H

// 初始化 OLED 屏幕及 I2C 引脚
void OLED_Init(void);

// 刷新屏幕显示小车状态和速度
// mode: 当前模式 (MODE_STOP 或 MODE_TRACK)
// action: 当前动作字符串 (如 "FORWARD", "STOP")
// speed: 当前 PWM 速度值
// speed_turn: 当前转弯速度值
void OLED_Show_Status(int mode, const char* action, int speed_straight, int speed_turn);

#endif // DISPLAY_H