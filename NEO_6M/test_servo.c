#include <stdio.h>
#include <unistd.h>
#include "servo.h"

int main()
{
    int chip = 0;
    int servo = 0;  // pwm0 → GPIO12

    // PWM 초기화
    servo_init(chip, servo);

    float positions[] = {0, 45, 90, 135, 180, 90, 0};
    int n = sizeof(positions)/sizeof(positions[0]);

    while(1)
    {
        for (int i = 0; i < n; i++)
        {
            printf("Moving to %.0f°\n", positions[i]);
            servo_set_angle(chip, servo, positions[i]);
            sleep(1);
        }
    }

    servo_cleanup(chip, servo);
    return 0;
}

