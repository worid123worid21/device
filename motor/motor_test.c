#include <wiringPi.h>
#include <softPwm.h>
#include <ncurses.h>

#define PWMA 16  // M1 PWM (physical pin 36)
#define DIRA 20  // M1 방향 (physical pin 38)
#define PWMB 17  // M2 PWM (physical pin 11)
#define DIRB 27  // M2 방향 (physical pin 13)

#define SPEED 100
#define TURN_SPEED 60   // 곡선 회전용 속도

void stopM() {
    softPwmWrite(PWMA, 0);
    softPwmWrite(PWMB, 0);
}

/* ===== 기본 직진 / 후진 ===== */
//반시계
void curveLeft() { digitalWrite(DIRA, LOW); digitalWrite(DIRB, HIGH); softPwmWrite(PWMA, SPEED); softPwmWrite(PWMB, 0); } // out3, out4 출력

//시계
void curveRight() { digitalWrite(DIRA, HIGH); digitalWrite(DIRB, LOW); softPwmWrite(PWMA, SPEED); softPwmWrite(PWMB, 0); } // out3, out4 출력

// 후진
void backward() {
    digitalWrite(DIRA, HIGH); digitalWrite(DIRB, HIGH); softPwmWrite(PWMA, 0); softPwmWrite(PWMB, SPEED); // out1, out2 출력
}

// 전진
void forward() {
    digitalWrite(DIRA, HIGH); digitalWrite(DIRB, LOW); softPwmWrite(PWMA, 0); softPwmWrite(PWMB, SPEED); // out1, out2 출력
}

int main() {

    wiringPiSetupGpio();

    pinMode(DIRA, OUTPUT);
    pinMode(DIRB, OUTPUT);

    softPwmCreate(PWMA, 0, 100);
    softPwmCreate(PWMB, 0, 100);

    initscr();
    cbreak();
    noecho();
    nodelay(stdscr, TRUE);

    printw("W:전진 S:후진\n");
    printw("A:좌 제자리회전 D:우 제자리회전\n");
    printw("Q:종료\n");
    refresh();

    int ch;

    while(1) {
        ch = getch();

        if      (ch == 'q') break;
        else if (ch == 'w') forward();
        else if (ch == 's') backward();
        else if (ch == 'd') curveRight();
        else if (ch == 'a') curveLeft();
        else stopM();

        delay(10);
    }

    stopM();
    endwin();
    return 0;
}