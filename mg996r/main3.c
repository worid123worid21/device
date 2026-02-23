#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <termios.h>
#include <math.h>
#include <fcntl.h>
#include "servo.h"

#define ANGLE_STEP 1
#define LOOP_DELAY 10000   // 10ms (0.01초)

struct termios orig_term;

void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_term);
    struct termios raw = orig_term;
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);

    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK); // 논블로킹 입력
}

void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_term);
}

// 1도씩 부드럽게 이동
float smooth_step(float current, float target) {
    if (current < target) return current + 1;
    if (current > target) return current - 1;
    return current;
}

int main()
{
    int chip = 0, servo1 = 0, servo2 = 1;
    float angle1 = 90, angle2 = 90;       // 현재 각도
    float target1 = 90, target2 = 90;     // 목표 각도
    float saved1 = 90, saved2 = 90;       // 저장 위치
    const float init1 = 90, init2 = 90;   // 초기 위치

    servo_init(chip, servo1);
    servo_init(chip, servo2);

    enable_raw_mode();
    printf("Hold WASD to move servos\n");
    printf("O -> Save, R -> Go to saved, E -> Go to init, Q -> Quit\n");

    while (1) {
        char c = getchar();

        if (c == 'q') break;

        // 현재 위치 저장
        if (c == 'o') {
            saved1 = angle1;
            saved2 = angle2;
            printf("\nSaved position: %.0f°, %.0f°\n", saved1, saved2);
        }
        // 저장 위치로 이동
        else if (c == 'r') {
            target1 = saved1;
            target2 = saved2;
        }
        // 초기 위치로 이동
        else if (c == 'e') {
            target1 = init1;
            target2 = init2;
        }
        // W/A/S/D 조작
        else if (c == 'w') target1 += ANGLE_STEP;
        else if (c == 's') target1 -= ANGLE_STEP;
        else if (c == 'a') target2 += ANGLE_STEP;
        else if (c == 'd') target2 -= ANGLE_STEP;

        // 범위 제한
        if (target1 < 70) target1 = 70;
        if (target1 > 170) target1 = 170;
        if (target2 < 0) target2 = 0;
        if (target2 > 180) target2 = 180;

        // 부드러운 이동
        angle1 = smooth_step(angle1, target1);
        angle2 = smooth_step(angle2, target2);

        // 서보 적용
        servo_set_angle(chip, servo1, angle1);
        servo_set_angle(chip, servo2, angle2);

        // 각도 표시
        printf("\rServo1: %.0f°, Servo2: %.0f°    ", angle1, angle2);
        fflush(stdout);

        usleep(LOOP_DELAY);
    }

    disable_raw_mode();
    servo_cleanup(chip, servo1);
    servo_cleanup(chip, servo2);
    printf("\nExiting...\n");
    return 0;
}