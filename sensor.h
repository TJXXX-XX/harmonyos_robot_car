#include "iot_gpio.h"

#ifndef SENSOR_H
#define SENSOR_H

// 传感器初始化
void sensor_init(void);

// 获取超声波测距距离 (单位: 厘米)
float GetDistance(void);

// 获取红外循迹状态 
// 传入指针，函数内部会把左右传感器的值赋给这两个变量
// 0 表示检测到黑线，1 表示在白色区域
void get_tcrt5000_value(IotGpioValue *left_val, IotGpioValue *right_val);

#endif // SENSOR_H