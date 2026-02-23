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

    // 논블로킹 입력 설정
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
}

void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_term);
}

int main()
{
    int chip = 0;
    int servo1 = 0;
    int servo2 = 1;
    float angle1 = 90;
    float angle2 = 90;

    // Raspberry Pi 환경에서 raspi-gpio가 없다면 삭제
    // system("sudo raspi-gpio set 18 alt5");
    // system("sudo raspi-gpio set 19 alt5");

    servo_init(chip, servo1);
    servo_init(chip, servo2);

    enable_raw_mode();

    printf("Hold WASD keys to move servos\n");
    printf("Q -> Quit\n");

    while (1)
    {
        char c = getchar();

        if (c == 'q')
            break;

        if (c == 'w') angle1 += ANGLE_STEP;
        else if (c == 's') angle1 -= ANGLE_STEP;
        else if (c == 'a') angle2 += ANGLE_STEP;
        else if (c == 'd') angle2 -= ANGLE_STEP;

        // 범위 제한
        if (angle1 < 0) angle1 = 0;
        if (angle1 > 180) angle1 = 180;
        if (angle2 < 0) angle2 = 0;
        if (angle2 > 180) angle2 = 180;

        servo_set_angle(chip, servo1, angle1);
        servo_set_angle(chip, servo2, angle2);

        // 🔥 각도 표시
        printf("\rServo1: %.0f°, Servo2: %.0f°    ", angle1, angle2);
        fflush(stdout);

        usleep(LOOP_DELAY);   // 10ms마다 반복
    }

    disable_raw_mode();

    servo_cleanup(chip, servo1);
    servo_cleanup(chip, servo2);

    printf("\nExiting...\n");
    return 0;
}