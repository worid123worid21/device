#ifndef MOTOR_H
#define MOTOR_H

typedef enum {
    MOTOR_STOP     = 0,
    MOTOR_FORWARD  = 1,
    MOTOR_BACKWARD = 2,
    MOTOR_LEFT     = 3,
    MOTOR_RIGHT    = 4,
} MotorDir;

int  motor_init(void);       /* /dev/l298n 열기 */
void motor_cleanup(void);    /* fd 닫기 → 드라이버 release() → 자동 정지 */
int  motor_set(MotorDir dir);

#endif /* MOTOR_H */
