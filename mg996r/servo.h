#ifndef SERVO_H
#define SERVO_H

// PWM 채널 초기화
int servo_init(int pwm_chip, int pwm_channel);

// 목표 각도로 서보 이동
int servo_set_angle(int pwm_chip, int pwm_channel, float angle);

// PWM 채널 해제
void servo_cleanup(int pwm_chip, int pwm_channel);

#endif
