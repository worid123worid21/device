#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "motor.h"
#include "l298n.h"   /* ioctl 명령, L298N_DEV_PATH */

static int g_fd = -1;

int motor_init(void)
{
    g_fd = open(L298N_DEV_PATH, O_RDWR);
    if (g_fd < 0) {
        perror("motor_init: " L298N_DEV_PATH " open 실패");
        return -1;
    }
    printf("[motor] 드라이버 연결: %s\n", L298N_DEV_PATH);
    return 0;
}

void motor_cleanup(void)
{
    if (g_fd >= 0) {
        close(g_fd);   /* release() → motor_stop() 자동 호출 */
        g_fd = -1;
        printf("[motor] 드라이버 연결 해제\n");
    }
}

int motor_set(MotorDir dir)
{
    unsigned int cmd;
    const char  *label;

    switch (dir) {
    case MOTOR_FORWARD:  cmd = L298N_FORWARD;  label = "↑ 전진";   break;
    case MOTOR_BACKWARD: cmd = L298N_BACKWARD; label = "↓ 후진";   break;
    case MOTOR_LEFT:     cmd = L298N_LEFT;     label = "← 좌회전"; break;
    case MOTOR_RIGHT:    cmd = L298N_RIGHT;    label = "→ 우회전"; break;
    case MOTOR_STOP:
    default:             cmd = L298N_STOP;     label = "■ 정지";   break;
    }

    if (ioctl(g_fd, cmd) < 0) {
        perror("motor_set: ioctl 실패");
        return -1;
    }
    printf("[motor] %s\n", label);
    return 0;
}
