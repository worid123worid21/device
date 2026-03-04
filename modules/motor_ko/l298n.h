#ifndef L298N_H
#define L298N_H

#include <linux/ioctl.h>

/* ------------------------------------------------------------------ */
/* GPIO 핀 번호 (BCM 기준)                                               */
/* 아두이노 예제 핀 매핑 → RPi BCM 핀으로 변환                           */
/*   M1(DIRA) → BCM 4    E1(PWMA) → BCM 5                              */
/*   M2(DIRB) → BCM 7    E2(PWMB) → BCM 6                              */
/* ------------------------------------------------------------------ */
#define GPIO_E1  16   /* PWMA : HIGH=활성, LOW=정지  (Physical 36) */
#define GPIO_M1  20   /* DIRA : LOW=전진, HIGH=후진  (Physical 38) */
#define GPIO_E2  17   /* PWMB : HIGH=활성, LOW=정지  (Physical 11) */
#define GPIO_M2  27   /* DIRB : LOW=전진, HIGH=후진  (Physical 13) */

/* ------------------------------------------------------------------ */
/* ioctl 명령어                                                          */
/* ------------------------------------------------------------------ */
#define L298N_IOC_MAGIC  'L'

#define L298N_STOP      _IO(L298N_IOC_MAGIC, 0)
#define L298N_FORWARD   _IO(L298N_IOC_MAGIC, 1)
#define L298N_BACKWARD  _IO(L298N_IOC_MAGIC, 2)
#define L298N_LEFT      _IO(L298N_IOC_MAGIC, 3)
#define L298N_RIGHT     _IO(L298N_IOC_MAGIC, 4)

/* 디바이스 노드 */
#define L298N_DEV_PATH  "/dev/l298n"

#endif /* L298N_H */
