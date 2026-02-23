#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "servo.h"

#define PERIOD_NS 20000000   // 20ms = 50Hz

// 파일에 문자열 쓰기
static void write_file(const char *path, const char *value)
{
    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        perror(path);
        return;
    }
    write(fd, value, strlen(value));
    close(fd);
}

// PWM 채널 초기화
int servo_init(int chip, int channel)
{
    char path[100];
    char buffer[10];

    // export
    sprintf(path, "/sys/class/pwm/pwmchip%d/export", chip);
    sprintf(buffer, "%d", channel);
    write_file(path, buffer);
    usleep(100000);

    // period 설정
    sprintf(path, "/sys/class/pwm/pwmchip%d/pwm%d/period", chip, channel);
    sprintf(buffer, "%d", PERIOD_NS);
    write_file(path, buffer);

    // 초기 듀티 1.5ms (중앙)
    sprintf(path, "/sys/class/pwm/pwmchip%d/pwm%d/duty_cycle", chip, channel);
    sprintf(buffer, "%d", 1500000);
    write_file(path, buffer);

    // enable
    sprintf(path, "/sys/class/pwm/pwmchip%d/pwm%d/enable", chip, channel);
    write_file(path, "1");

    return 0;
}

// 목표 각도로 서보 이동
int servo_set_angle(int chip, int channel, float angle)
{
    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;

    char path[100];
    char buffer[20];

    // MG996R: 0.5ms ~ 2.5ms (500000~2500000 ns)
    int duty = 500000 + (angle / 180.0) * 2000000;

    sprintf(path, "/sys/class/pwm/pwmchip%d/pwm%d/duty_cycle", chip, channel);
    sprintf(buffer, "%d", duty);
    write_file(path, buffer);

    return 0;
}

// PWM 채널 해제
void servo_cleanup(int chip, int channel)
{
    char path[100];
    char buffer[10];

    sprintf(path, "/sys/class/pwm/pwmchip%d/pwm%d/enable", chip, channel);
    write_file(path, "0");

    sprintf(path, "/sys/class/pwm/pwmchip%d/unexport", chip);
    sprintf(buffer, "%d", channel);
    write_file(path, buffer);
}
