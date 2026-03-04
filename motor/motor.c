#include <wiringPi.h>
#include <softPwm.h>
#include <ncurses.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

#define PWMA 16
#define DIRA 20
#define PWMB 17
#define DIRB 27

#define MAX_SPEED 70
#define REDUCED_SPEED 45  // 2초 이상 누르면 20 낮춤

// 현재 속도
int curSpeedA = 0;
int curSpeedB = 0;

void stopM() {
    curSpeedA = 0;
    curSpeedB = 0;
    softPwmWrite(PWMA, 0);
    softPwmWrite(PWMB, 0);
}

// 속도 설정
void setSpeed(int targetA, int targetB) {
    curSpeedA = targetA;
    curSpeedB = targetB;
    softPwmWrite(PWMA, curSpeedA);
    softPwmWrite(PWMB, curSpeedB);
}

void forward(int speed) { digitalWrite(DIRA,HIGH); digitalWrite(DIRB,LOW); setSpeed(0,speed); }
void backward(int speed) { digitalWrite(DIRA,HIGH); digitalWrite(DIRB,HIGH); setSpeed(0,speed); }
void curveLeft(int speed) { digitalWrite(DIRA,LOW); digitalWrite(DIRB,HIGH); setSpeed(speed,0); }
void curveRight(int speed) { digitalWrite(DIRA,HIGH); digitalWrite(DIRB,LOW); setSpeed(speed,0); }

int main() {
    wiringPiSetupGpio();
    pinMode(DIRA, OUTPUT);
    pinMode(DIRB, OUTPUT);
    softPwmCreate(PWMA, 0, 100);
    softPwmCreate(PWMB, 0, 100);

    initscr(); cbreak(); noecho(); nodelay(stdscr, TRUE); keypad(stdscr, TRUE);
    printw("WSAD: 이동, X: 정지, Q: 종료\n"); refresh();

    int ch;
    time_t press_start = 0;
    int last_ch = 0;

    while(1) {
        ch = getch();
        time_t now = time(NULL);

        if(ch != ERR) {
            if(ch != last_ch) { // 키 변경 시 시작 시간 갱신
                press_start = now;
                last_ch = ch;
            }

            // 누른 시간 계산
            int speed = MAX_SPEED;
            if(now - press_start >= 2) speed = REDUCED_SPEED;

            switch(ch){
                case 'q': case 'Q': stopM(); endwin(); printf("Program exited.\n"); return 0;
                case 'w': case 'W': forward(speed); break;
                case 's': case 'S': backward(speed); break;
                case 'a': case 'A': curveLeft(speed); break;
                case 'd': case 'D': curveRight(speed); break;
                case 'x': case 'X': stopM(); break;
                default: stopM(); break;
            }
        } else {
            stopM();
            last_ch = 0;
        }

        // 화면 표시
        move(2,0); clrtoeol();
        if (ch=='w'||ch=='W') printw("Forward");
        else if (ch=='s'||ch=='S') printw("Backward");
        else if (ch=='a'||ch=='A') printw("Curve Left");
        else if (ch=='d'||ch=='D') printw("Curve Right");
        else printw("Stop");
        refresh();

        usleep(50000); // 0.05초 루프
    }

    stopM();
    endwin();
    return 0;
}