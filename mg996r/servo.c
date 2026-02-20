#include <wiringPi.h>
#include <softPwm.h>
#include <stdio.h>
#include <unistd.h>

#define SERVO1 23 // GPIO13
#define SERVO2 24 // GPIO19

// softPWM 값 변환 (0°~180° → 5~25)
int angle_to_softpwm(int angle) {
    int val = 5 + (angle * 20 / 180);
    if(val < 0) val = 0;
    if(val > 25) val = 25;
    return val;
}

// 서보 각도 설정
void set_servo(int servo, int angle) {
    softPwmWrite(servo, angle_to_softpwm(angle));
}

// 부드럽게 이동
void move_servo_smooth(int servo, int start_angle, int end_angle, int duration_sec) {
    int steps = 50; // 이동 단계 수
    float delay_time = duration_sec * 1000.0 / steps; // ms
    float delta = (float)(end_angle - start_angle) / steps;

    for(int i=0; i<=steps; i++) {
        int angle = start_angle + (int)(delta * i);
        set_servo(servo, angle);
        delay((int)delay_time);
    }
}

int main(void) {
    if(wiringPiSetup() == -1) return 1;

    // 소프트 PWM 초기화
    softPwmCreate(SERVO1, 0, 200);
    softPwmCreate(SERVO2, 0, 200);

    int positions[] = {45, 90, 135, 90, 45};
    int n = sizeof(positions)/sizeof(positions[0]);

    printf("Starting Smooth Servo Test\n");

    for(int i=0; i<n; i++) {
        int target = positions[i];
        printf("Moving to %d°\n", target);
        // 두 서보 동시에 부드럽게 이동
        move_servo_smooth(SERVO1, positions[(i==0)?0:i-1], target, 5); // 5초 이동
        move_servo_smooth(SERVO2, positions[(i==0)?0:i-1], target, 5);
        printf("Holding %d° for 3 seconds\n", target);
        sleep(3); // 각 구간 3초 머무르기
    }

    printf("Smooth Servo Test Completed\n");
    return 0;
}

