#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

void pwm_motor_init(void);

void car_forward(int speed);
void car_backward(int speed);
void car_left(int speed, int bkspeed, int tick, int edge);
void car_right(int speed, int bkspeed, int tick, int edge);
void car_stop(void);
void car_forward_left(int speed);
void car_forward_left_ext(int speed, int tick, int edge);
void car_forward_right(int speed);
void car_forward_right_ext(int speed, int tick, int edge);
void car_backward_left(int speed);
void car_backward_right(int speed);

#endif // MOTOR_CONTROL_H