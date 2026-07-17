#ifndef ADC_KEY_H
#define ADC_KEY_H

// 定义按键事件枚举
#define KEY_EVENT_NONE 0
#define KEY_EVENT_S1   1
#define KEY_EVENT_S2   2
#define KEY_EVENT_S3   3

// 初始化按键 (配置 ADC)
void Key_Init(void);

// 获取当前的按键状态 (主程序每次循环调用一次)
// 返回值：KEY_EVENT_NONE(没按), KEY_EVENT_S1, KEY_EVENT_S2, KEY_EVENT_S3
int Get_Key_Event(void);

#endif // ADC_KEY_H