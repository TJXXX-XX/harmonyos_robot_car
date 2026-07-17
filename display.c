#include <stdio.h>
#include <unistd.h>
#include "display.h"
#include "hi_io.h"
#include "iot_gpio.h"
#include "iot_i2c.h"
#include "ssd1306.h"
#include "mqtt_module.h"

// 引入模式的宏定义，确保能够识别
#define MODE_STOP  0
#define MODE_TRACK 1
#define MODE_REMOTE 2  // 【新增】遥控模式
#define MODE_EXTREME 3 // 【新增】极限循迹模式

#define OLED_I2C_BAUDRATE (400 * 1000) // 400kHz

void OLED_Init(void) {
    hi_io_set_func(HI_IO_NAME_GPIO_13, HI_IO_FUNC_GPIO_13_I2C0_SDA);
    hi_io_set_func(HI_IO_NAME_GPIO_14, HI_IO_FUNC_GPIO_14_I2C0_SCL);
    IoTI2cInit(0, OLED_I2C_BAUDRATE);
    usleep(20 * 1000);
    ssd1306_Init();
    ssd1306_Fill(Black);
    ssd1306_UpdateScreen();
}



// 核心显示函数：完美布局版
void OLED_Show_Status(int mode, const char* action, int speed_straight, int speed_turn) {

    Send_Status_To_App(mode, action, speed_straight, speed_turn);

    char buffer[32];

    // 1. 清空屏幕
    ssd1306_Fill(Black);

    // ==========================================
    // 全局外边框
    // ==========================================
    ssd1306_DrawRectangle(0, 0, 127, 63, White);

    // ==========================================
    // 状态区：第一行 模式 (原Y=2 -> 下移后Y=6)
    // ==========================================
    ssd1306_SetCursor(4, 6);
    // 你可以将原来的那句 snprintf 替换成这段：
    const char* mode_str = "STOPPED";
    if (mode == MODE_TRACK) mode_str = "TRACKING";
    else if (mode == MODE_REMOTE) mode_str = "REMOTE";
    else if (mode == MODE_EXTREME) mode_str = "EXTREME";
    snprintf(buffer, sizeof(buffer), "MODE: %s", mode_str);
    // 取消了反白背景，这里颜色改回 White
    ssd1306_DrawString(buffer, Font_7x10, White);

    // ==========================================
    // 状态区：第二行 动作 (原Y=17 -> 下移后Y=21)
    // ==========================================
    ssd1306_SetCursor(4, 21);
    snprintf(buffer, sizeof(buffer), "Act : %s", action);
    ssd1306_DrawString(buffer, Font_7x10, White);

    // ==========================================
    // 区域分割线 (横向平移留出1像素不破坏外框，画在Y=33)
    // ==========================================
    ssd1306_DrawLine(1, 33, 126, 33, White);

    // ==========================================
    // 参数区：直行速度 (原Y=31 -> 下移4 + 隔开2 = Y=37)
    // ==========================================
    ssd1306_SetCursor(4, 37);
    snprintf(buffer, sizeof(buffer), "Spd: %d", speed_straight);
    ssd1306_DrawString(buffer, Font_7x10, White);

    // 进度条往右平移2：X起点变为 62，框宽留出60的内部填充空间，所以右边界为 123
    ssd1306_DrawRectangle(62, 37, 123, 45, White);

    int fill_w1 = speed_straight * 60 / 100;
    if (fill_w1 > 60) fill_w1 = 60; // 上限保护
    if (fill_w1 < 0) fill_w1 = 0;   // 下限保护

    if (fill_w1 > 0) {
        for (int i = 0; i < fill_w1; i++) {
            // X坐标从63开始，刚好在外框内部
            ssd1306_DrawLine(63 + i, 38, 63 + i, 44, White);
        }
    }

    // ==========================================
    // 参数区：转弯速度 (原Y=45 -> 下移4 + 隔开2 = Y=51)
    // ==========================================
    ssd1306_SetCursor(4, 51);
    snprintf(buffer, sizeof(buffer), "Trn: %d", speed_turn);
    ssd1306_DrawString(buffer, Font_7x10, White);

    // 进度条往右平移2
    ssd1306_DrawRectangle(62, 51, 123, 59, White);

    int fill_w2 = speed_turn * 60 / 100;
    if (fill_w2 > 60) fill_w2 = 60;
    if (fill_w2 < 0) fill_w2 = 0;

    if (fill_w2 > 0) {
        for (int i = 0; i < fill_w2; i++) {
            ssd1306_DrawLine(63 + i, 52, 63 + i, 58, White);
        }
    }

    // 最后，将显存数据推送到物理屏幕
    ssd1306_UpdateScreen();
}