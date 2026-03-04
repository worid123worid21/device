#ifndef MOTOR_H
#define MOTOR_H

/* MG996R GPIO Pin 번호 (BCM 기준) */
#define MOTOR_PIN_FORWARD   17
#define MOTOR_PIN_BACKWARD  27
#define MOTOR_PIN_LEFT      22
#define MOTOR_PIN_RIGHT     23

/* 방향 정의 */
typedef enum {
    MOTOR_STOP     = 0,
    MOTOR_FORWARD  = 1,
    MOTOR_BACKWARD = 2,
    MOTOR_LEFT     = 3,
    MOTOR_RIGHT    = 4,
} MotorDir;

/* 초기화 / 해제 */
int  motor_init(void);
void motor_cleanup(void);

/* 제어 */
void motor_set(MotorDir dir);
void motor_stop(void);

#endif /* MOTOR_H */

