#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <ncurses.h>
#include "servo.h"

#define ANGLE_STEP 1
#define LOOP_DELAY 10000   // 10ms

// 부드러운 이동
float smooth_step(float current, float target) {
    if (current < target) return current + 1;
    if (current > target) return current - 1;
    return current;
}

int main() {
    int chip = 0, servo1 = 0, servo2 = 1;
    float angle1 = 90, angle2 = 90;
    float target1 = 90, target2 = 90;
    float saved1 = 90, saved2 = 90;
    const float init1 = 90, init2 = 90;

    servo_init(chip, servo1);
    servo_init(chip, servo2);

    // ncurses 초기화
    initscr();
    cbreak();
    noecho();
    nodelay(stdscr, TRUE);
    keypad(stdscr, TRUE);

    printw("Hold WASD to move servos continuously\n");
    printw("O -> Save, R -> Go to saved, E -> Go to init, Q -> Quit\n");

    int running = 1;
    int key_state[256] = {0};

    while (running) {
        int ch;
        // 눌린 키 상태 갱신
        while ((ch = getch()) != ERR) {
            if (ch == 'q' || ch == 'Q') { running = 0; break; }
            else if (ch == 'w' || ch == 'W') key_state['w'] = 1;
            else if (ch == 's' || ch == 'S') key_state['s'] = 1;
            else if (ch == 'a' || ch == 'A') key_state['a'] = 1;
            else if (ch == 'd' || ch == 'D') key_state['d'] = 1;
            else if (ch == 'o' || ch == 'O') {
                saved1 = angle1; saved2 = angle2;
                printw("\nSaved position: %.0f°, %.0f°\n", saved1, saved2);
            }
            else if (ch == 'r' || ch == 'R') {
                target1 = saved1; target2 = saved2;
            }
            else if (ch == 'e' || ch == 'E') {
                target1 = init1; target2 = init2;
            }
        }

        // 🔹 반대 키 무시
        if (key_state['w']) key_state['s'] = 0;
        if (key_state['s']) key_state['w'] = 0;
        if (key_state['a']) key_state['d'] = 0;
        if (key_state['d']) key_state['a'] = 0;

        // 🔹 키 상태에 따라 목표 각도 갱신
        if (key_state['w']) target1 += ANGLE_STEP;
        if (key_state['s']) target1 -= ANGLE_STEP;
        if (key_state['a']) target2 += ANGLE_STEP;
        if (key_state['d']) target2 -= ANGLE_STEP;

        // 범위 제한
        if (target1 < 0) target1 = 0;
        if (target1 > 180) target1 = 180;
        if (target2 < 0) target2 = 0;
        if (target2 > 180) target2 = 180;

        // 부드러운 이동
        angle1 = smooth_step(angle1, target1);
        angle2 = smooth_step(angle2, target2);

        // 서보 적용
        servo_set_angle(chip, servo1, angle1);
        servo_set_angle(chip, servo2, angle2);

        // 각도 표시
        move(5,0);
        printw("Servo1: %.0f°, Servo2: %.0f°    ", angle1, angle2);
        refresh();

        // 🔹 현재 루프에서 키 떼어짐 감지
        // 논블로킹 getch()는 누른 상태만 반영 → 떼면 이동 멈춤
        key_state['w'] = key_state['w'] && (getch() != ERR);
        key_state['s'] = key_state['s'] && (getch() != ERR);
        key_state['a'] = key_state['a'] && (getch() != ERR);
        key_state['d'] = key_state['d'] && (getch() != ERR);

        usleep(LOOP_DELAY);
    }

    endwin();
    servo_cleanup(chip, servo1);
    servo_cleanup(chip, servo2);
    printf("\nExiting...\n");
    return 0;
}