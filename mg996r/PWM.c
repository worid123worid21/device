#include <wiringPi.h>

#define SERVO1 2   // GPIO13 → wiringPi 2
#define SERVO2 24  // GPIO19 → wiringPi 24

int main(void) {
    wiringPiSetup();

    // PWM 모드로 변경
    pinMode(SERVO1, PWM_OUTPUT);
    pinMode(SERVO2, PWM_OUTPUT);

    // 50Hz 서보용 PWM 설정
    pwmSetMode(PWM_MODE_MS);
    pwmSetRange(1024);
    pwmSetClock(192);

    return 0;
}

