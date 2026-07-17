#include <stdio.h>
#include <unistd.h>
#include <memory.h>

#include "adc_key.h"
#include "hi_adc.h"
#include "hi_io.h"
#include "hi_gpio.h"
#include "hi_stdlib.h"

#define ADC_TEST_LENGTH 64
#define VLT_MIN 100

#define KEY_EVENT_NONE 0
#define KEY_EVENT_S1   1
#define KEY_EVENT_S2   2
#define KEY_EVENT_S3   3

// 静态变量，用于记录按键的按下状态，防止长按时一直触发
static char key_flg = 0; 
static int current_key_status = KEY_EVENT_NONE;

void Key_Init(void) {
    // 按键使用的是 GPIO_5，将其复用为普通 GPIO 输入模式
    hi_io_set_func(HI_IO_NAME_GPIO_5, HI_IO_FUNC_GPIO_5_GPIO);
    hi_gpio_set_dir(HI_GPIO_IDX_5, HI_GPIO_DIR_IN);
    
    // 初始化 ADC 模块 (如果系统默认已开，这里调用也没副作用)
    // hi_adc_init(); // 根据具体 SDK 版本，有时不需要显式调用
}

// 内部函数：读取 ADC 数据并判断是哪个按键
static void Scan_ADC_Key(void) {
    hi_u32 ret, i;
    hi_u16 data;
    hi_u16 g_adc_buf[ADC_TEST_LENGTH] = { 0 };

    // 连续采样 64 次，防止干扰
    for (i = 0; i < ADC_TEST_LENGTH; i++) {
        // HI_ADC_CHANNEL_2 是按键所在的通道
        ret = hi_adc_read((hi_adc_channel_index)HI_ADC_CHANNEL_2, &data, HI_ADC_EQU_MODEL_1, HI_ADC_CUR_BAIS_DEFAULT, 0);
        if (ret != 0) { // 0 表示成功 (HI_ERR_SUCCESS)
            return;
        }
        g_adc_buf[i] = data;
    }

    // 算出最大电压和最小电压
    float vlt_max = 0;
    float vlt_min = VLT_MIN;
    for (i = 0; i < ADC_TEST_LENGTH; i++) {
        float voltage = (float)g_adc_buf[i] * 1.8 * 4 / 4096;
        vlt_max = (vlt_max > voltage) ? vlt_max : voltage;
        vlt_min = (vlt_min < voltage) ? vlt_min : voltage;
    }

    // 取平均值
    float vlt_val = (vlt_max + vlt_min) / 2;

    // 根据电压区间判断是哪个按键按下了
    if ((vlt_val > 0.4) && (vlt_val < 0.6)) {
        if (key_flg == 0) {
            key_flg = 1;
            current_key_status = KEY_EVENT_S1;
        }
    } else if ((vlt_val > 0.8) && (vlt_val < 1.1)) {
        if (key_flg == 0) {
            key_flg = 1;
            current_key_status = KEY_EVENT_S2;
        }
    } else if ((vlt_val > 0.01) && (vlt_val < 0.3)) {
        if (key_flg == 0) {
            key_flg = 1;
            current_key_status = KEY_EVENT_S3;
        }
    } else if (vlt_val > 3.0) {
        // 电压大于 3.0V 说明没按键按下 (上拉状态)
        key_flg = 0;
    }
}

// 暴露给外部获取按键事件的接口
int Get_Key_Event(void) {
    // 扫描当前状态
    Scan_ADC_Key();
    
    // 如果有按键按下，读取后立刻清零，相当于“消费”了这个按键事件
    int temp = current_key_status;
    current_key_status = KEY_EVENT_NONE;
    
    return temp;
}