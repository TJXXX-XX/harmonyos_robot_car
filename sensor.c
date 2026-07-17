#include "sensor.h"

#include "hi_io.h"

// ===== 引脚定义 =====
#define GPIO_FUNC 0

// 超声波引脚
#define GPIO_8 8 // Echo (输入引脚，接收回波)
#define GPIO_7 7 // Trig (输出引脚，发送触发信号)

// 红外循迹引脚
#define GPIO_11 11 // 左侧红外
#define GPIO_12 12 // 右侧红外

// ===== 传感器引脚初始化模块 =====
void sensor_init(void) {
    // 1. 超声波引脚初始化
    hi_io_set_func(GPIO_8, GPIO_FUNC);
    IoTGpioSetDir(GPIO_8, IOT_GPIO_DIR_IN);  // Echo (接收)
    
    hi_io_set_func(GPIO_7, GPIO_FUNC);       // 补充上GPIO7的复用设置更稳妥
    IoTGpioSetDir(GPIO_7, IOT_GPIO_DIR_OUT); // Trig (发送)

    // 2. 红外传感器引脚初始化
    hi_io_set_func(GPIO_11, GPIO_FUNC);
    IoTGpioSetDir(GPIO_11, IOT_GPIO_DIR_IN);  // 左红外
    
    hi_io_set_func(GPIO_12, GPIO_FUNC);
    IoTGpioSetDir(GPIO_12, IOT_GPIO_DIR_IN);  // 右红外
}

// ===== 超声波测距模块 =====
float GetDistance(void) {
    static unsigned long start_time = 0, time = 0;
    float distance = 0.0;
    IotGpioValue value = IOT_GPIO_VALUE0;
    unsigned int flag = 0;

    IoTWatchDogDisable();

    // 直接发送触发信号
    IoTGpioSetOutputVal(GPIO_7, IOT_GPIO_VALUE1);
    hi_udelay(20);
    IoTGpioSetOutputVal(GPIO_7, IOT_GPIO_VALUE0);

    // 循环等待回响信号
    while (1) {
        IoTGpioGetInputVal(GPIO_8, &value);
        if (value == IOT_GPIO_VALUE1 && flag == 0) {
            start_time = hi_get_us();
            flag = 1;
        }
        if (value == IOT_GPIO_VALUE0 && flag == 1) {
            time = hi_get_us() - start_time;
            start_time = 0;
            break;
        }
    }
    distance = time * 0.034 / 2;
    return distance;
}

// ===== 红外传感器读取 =====
void get_tcrt5000_value(IotGpioValue *left_val, IotGpioValue *right_val) {
    // 直接读取，不再重复初始化
    IoTGpioGetInputVal(GPIO_11, left_val);
    IoTGpioGetInputVal(GPIO_12, right_val);
}