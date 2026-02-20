#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include "servo.h"

#define MOVE_TIME_SEC 5
#define HOLD_TIME_SEC 3

void smooth_move_two(int chip, int channel1, float *angle1,
                     int channel2, float *angle2, float target1, float target2)
{
    float diff1 = target1 - *angle1;
    float diff2 = target2 - *angle2;

    int steps1 = fabs(diff1);
    int steps2 = fabs(diff2);
    int steps = (steps1 > steps2) ? steps1 : steps2;

    if (steps == 0) return;

    float step_delay = (MOVE_TIME_SEC * 1000000.0) / steps; // us
    float dir1 = (diff1 > 0) ? 1 : -1;
    float dir2 = (diff2 > 0) ? 1 : -1;

    for (int i = 0; i < steps; i++)
    {
        if (i < steps1) *angle1 += dir1;
        if (i < steps2) *angle2 += dir2;

        servo_set_angle(chip, channel1, *angle1);
        servo_set_angle(chip, channel2, *angle2);
        usleep((int)step_delay);
    }

    *angle1 = target1;
    *angle2 = target2;
    servo_set_angle(chip, channel1, target1);
    servo_set_angle(chip, channel2, target2);
}

int main()
{
    int chip = 0;        // pwmchip0
    int servo1 = 0;      // PWM0 → BCM18 (Physical 12)
    int servo2 = 1;      // PWM1 → BCM19 (Physical 35)

    float angle1 = 45;   // 초기 각도
    float angle2 = 45;

    // ALT5 모드 강제 적용 (실시간)
    system("sudo raspi-gpio set 18 alt5"); // BCM18 → PWM0
    system("sudo raspi-gpio set 19 alt5"); // BCM19 → PWM1

    // PWM 초기화
    servo_init(chip, servo1);
    servo_init(chip, servo2);

    float positions[] = {45, 90, 135, 90, 45};
    int n = sizeof(positions)/sizeof(positions[0]);

    printf("Starting 2-Servo Parallel Smooth Motion\n");

    while (1)
    {
        for (int i = 0; i < n; i++)
        {
            printf("Moving to %.0f°\n", positions[i]);
            smooth_move_two(chip, servo1, &angle1, servo2, &angle2, positions[i], positions[i]);
            printf("Holding for %d seconds\n", HOLD_TIME_SEC);
            sleep(HOLD_TIME_SEC);
        }
    }

    // 종료 시 PWM 해제
    servo_cleanup(chip, servo1);
    servo_cleanup(chip, servo2);

    return 0;
}
