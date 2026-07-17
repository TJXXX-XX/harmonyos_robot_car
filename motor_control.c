#include <unistd.h>

#include "hi_io.h"
#include "hi_pwm.h"

// ===== 引脚与PWM通道映射 =====
#define GPIO0 0   // 左轮 A (PWM3)
#define GPIO1 1   // 左轮 B (PWM4)
#define GPIO9 9   // 右轮 A (PWM0)
#define GPIO10 10 // 右轮 B (PWM1)

#define PWM_LEFT_A  HI_PWM_PORT_PWM3
#define PWM_LEFT_B  HI_PWM_PORT_PWM4
#define PWM_RIGHT_A HI_PWM_PORT_PWM0
#define PWM_RIGHT_B HI_PWM_PORT_PWM1

// 核心函数：控制单个硬件通道的占空比 (speed: 0 ~ 100)
// 将周期设为 40000 -> 频率 = 160MHz / 40000 = 4kHz (完美适配电机)
#define PWM_PERIOD 40000

void set_motor_speed(hi_pwm_port port, int speed) {
    if (speed <= 0) {
        // 关键修复：绝不能用 hi_pwm_stop，那会让引脚悬空！
        // 给一个极小的占空比 1，强制把它拉成纯低电平
        hi_pwm_start(port, 1, PWM_PERIOD);
    } else {
        if (speed > 100) speed = 100;
        // 速度转化为占空比 (最大 40000)
        hi_pwm_start(port, (hi_u16)(speed * 400), PWM_PERIOD);
    }
}

// ===== 硬件 PWM 初始化与控制 =====
void pwm_motor_init(void) {
    // 初始化左轮引脚为PWM功能
    hi_io_set_func(GPIO0, HI_IO_FUNC_GPIO_0_PWM3_OUT);
    hi_io_set_func(GPIO1, HI_IO_FUNC_GPIO_1_PWM4_OUT);
    // 初始化右轮引脚为PWM功能
    hi_io_set_func(GPIO9, HI_IO_FUNC_GPIO_9_PWM0_OUT);
    hi_io_set_func(GPIO10, HI_IO_FUNC_GPIO_10_PWM1_OUT);

    hi_pwm_init(PWM_LEFT_A);  hi_pwm_init(PWM_LEFT_B);
    hi_pwm_init(PWM_RIGHT_A); hi_pwm_init(PWM_RIGHT_B);

    // 设置时钟频率为 160M
    hi_pwm_set_clock(PWM_CLK_160M);
}

// ===== 丝滑调速运动控制模块 =====

// 直行：左右轮全速前进
void car_forward(int speed) {
    set_motor_speed(PWM_LEFT_A,  speed); set_motor_speed(PWM_LEFT_B,  0);
    set_motor_speed(PWM_RIGHT_A, speed); set_motor_speed(PWM_RIGHT_B, 0);
}

// 后退
void car_backward(int speed) {
    set_motor_speed(PWM_LEFT_A,  0); set_motor_speed(PWM_LEFT_B,  speed);
    set_motor_speed(PWM_RIGHT_A, 0); set_motor_speed(PWM_RIGHT_B, speed);
}

// // 左转
// // (注：直流电机给 20% 一般带不动，改为了 40% 克服死区)
// void car_left(int speed, int bkspeed) {
//     set_motor_speed(PWM_LEFT_A,  0);  set_motor_speed(PWM_LEFT_B,  bkspeed); 
//     set_motor_speed(PWM_RIGHT_A, speed); set_motor_speed(PWM_RIGHT_B, 0);  
// }

// // 右转
// void car_right(int speed, int bkspeed) {
//     set_motor_speed(PWM_LEFT_A,  speed); set_motor_speed(PWM_LEFT_B,  0);  
//     set_motor_speed(PWM_RIGHT_A, 0);  set_motor_speed(PWM_RIGHT_B, bkspeed); 
// }

// 左转 (增加 tick 参数，控制内侧左轮 1/5 时间倒退)
void car_left(int speed, int bkspeed, int tick, int edge) {
    // 外侧右轮正常前进
    set_motor_speed(PWM_RIGHT_A, speed); 
    set_motor_speed(PWM_RIGHT_B, 0);  

    // 内侧左轮 1/5 时间倒退，4/5 时间静止
    if (tick <= edge) {
        set_motor_speed(PWM_LEFT_A,  0);       
        set_motor_speed(PWM_LEFT_B,  bkspeed); // 1/5 时间强制倒退
    } else {
        set_motor_speed(PWM_LEFT_A,  0);       
        set_motor_speed(PWM_LEFT_B,  0);       // 4/5 时间保持静止
    }
}

// 右转 (增加 tick 参数，控制内侧右轮 1/5 时间倒退)
void car_right(int speed, int bkspeed, int tick, int edge) {
    // 外侧左轮正常前进
    set_motor_speed(PWM_LEFT_A,  speed); 
    set_motor_speed(PWM_LEFT_B,  0);  

    // 内侧右轮 1/5 时间倒退，4/5 时间静止
    if (tick <= edge) {
        set_motor_speed(PWM_RIGHT_A, 0);       
        set_motor_speed(PWM_RIGHT_B, bkspeed); // 1/5 时间强制倒退
    } else {
        set_motor_speed(PWM_RIGHT_A, 0);       
        set_motor_speed(PWM_RIGHT_B, 0);       // 4/5 时间保持静止
    }
}

// ================== 新增：差速转弯 (用于摇杆斜推) ==================

// 左前行驶 (右轮快，左轮慢或停止)
void car_forward_left(int speed) {
    set_motor_speed(PWM_LEFT_A,  speed * 0.75); set_motor_speed(PWM_LEFT_B,  0);
    set_motor_speed(PWM_RIGHT_A, speed);       set_motor_speed(PWM_RIGHT_B, 0);
}

// 右前行驶 (左轮快，右轮慢或停止)
void car_forward_right(int speed) {
    set_motor_speed(PWM_LEFT_A,  speed);       set_motor_speed(PWM_LEFT_B,  0);
    set_motor_speed(PWM_RIGHT_A, speed * 0.75); set_motor_speed(PWM_RIGHT_B, 0);
}

void car_forward_left_ext(int speed, int tick, int edge) {
    set_motor_speed(PWM_LEFT_A,  speed * 0.75); set_motor_speed(PWM_LEFT_B,  0);

    if (tick <= edge) {
        set_motor_speed(PWM_RIGHT_A, speed);       
        set_motor_speed(PWM_RIGHT_B, 0); // 1/5 时间强制倒退
    } else {
        set_motor_speed(PWM_RIGHT_A, 0);       
        set_motor_speed(PWM_RIGHT_B, 0);       // 4/5 时间保持静止
    }
}

void car_forward_right_ext(int speed, int tick, int edge) {
    set_motor_speed(PWM_RIGHT_A, speed * 0.75); set_motor_speed(PWM_RIGHT_B, 0);

    if (tick <= edge) {
        set_motor_speed(PWM_LEFT_A, speed);       
        set_motor_speed(PWM_LEFT_B, 0); // 1/5 时间强制倒退
    } else {
        set_motor_speed(PWM_LEFT_A, 0);       
        set_motor_speed(PWM_LEFT_B, 0);       // 4/5 时间保持静止
    }
}

// 极限左转（原地坦克掉头，不加tick，直接暴力反转）
void car_spin_left(int speed) {
    set_motor_speed(PWM_RIGHT_A, speed);  // 右轮全速前进
    set_motor_speed(PWM_RIGHT_B, 0);
    set_motor_speed(PWM_LEFT_A,  0);
    set_motor_speed(PWM_LEFT_B,  speed);  // 左轮全速后退
}

// 极限右转（原地坦克掉头，不加tick，直接暴力反转）
void car_spin_right(int speed) {
    set_motor_speed(PWM_LEFT_A,  speed);  // 左轮全速前进
    set_motor_speed(PWM_LEFT_B,  0);
    set_motor_speed(PWM_RIGHT_A, 0);
    set_motor_speed(PWM_RIGHT_B, speed);  // 右轮全速后退
}

// 左后行驶
void car_backward_left(int speed) {
    set_motor_speed(PWM_LEFT_A,  0); set_motor_speed(PWM_LEFT_B,  speed * 0.75);
    set_motor_speed(PWM_RIGHT_A, 0); set_motor_speed(PWM_RIGHT_B,  speed);
}

// 右后行驶
void car_backward_right(int speed) {
    set_motor_speed(PWM_LEFT_A,  0); set_motor_speed(PWM_LEFT_B,  speed);
    set_motor_speed(PWM_RIGHT_A, 0); set_motor_speed(PWM_RIGHT_B,  speed * 0.75);
}

// 强力刹车
void car_stop(void) {
    set_motor_speed(PWM_LEFT_A,  100); set_motor_speed(PWM_LEFT_B,  100);
    set_motor_speed(PWM_RIGHT_A, 100); set_motor_speed(PWM_RIGHT_B, 100);
}