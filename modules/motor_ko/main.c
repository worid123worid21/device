#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <signal.h>
#include "motor.h"

/* ------------------------------------------------------------------ */
/* 터미널 raw mode                                                       */
/* ------------------------------------------------------------------ */
static struct termios g_orig;

static void term_raw_on(void)
{
    struct termios raw;
    tcgetattr(STDIN_FILENO, &g_orig);
    raw = g_orig;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN]  = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static void term_raw_off(void)
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig);
}

/* ------------------------------------------------------------------ */
/* 시그널                                                                */
/* ------------------------------------------------------------------ */
static volatile int g_run = 1;
static void on_signal(int s) { (void)s; g_run = 0; }

/* ------------------------------------------------------------------ */
/* 방향키 파싱  ESC [ A/B/C/D                                            */
/* ------------------------------------------------------------------ */
typedef enum { KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
               KEY_QUIT, KEY_UNKNOWN } Key;

static Key read_key(void)
{
    unsigned char c;
    if (read(STDIN_FILENO, &c, 1) != 1) return KEY_UNKNOWN;
    if (c == 'q' || c == 'Q') return KEY_QUIT;
    if (c == 0x1B) {
        unsigned char seq[2];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return KEY_UNKNOWN;
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return KEY_UNKNOWN;
        if (seq[0] == '[') {
            switch (seq[1]) {
            case 'A': return KEY_UP;
            case 'B': return KEY_DOWN;
            case 'C': return KEY_RIGHT;
            case 'D': return KEY_LEFT;
            }
        }
    }
    return KEY_UNKNOWN;
}

/* ------------------------------------------------------------------ */
/* main                                                                  */
/* ------------------------------------------------------------------ */
int main(void)
{
    signal(SIGINT,  on_signal);
    signal(SIGTERM, on_signal);

    if (motor_init() < 0) {
        fprintf(stderr,
            "오류: 먼저 커널 모듈을 적재하세요.\n"
            "  sudo insmod l298n_driver.ko\n");
        return EXIT_FAILURE;
    }

    term_raw_on();

    puts("=== L298N 방향키 제어 ===");
    puts("  ↑  전진    ↓  후진");
    puts("  ←  좌회전  →  우회전");
    puts("  q  종료\n");

    while (g_run) {
        switch (read_key()) {
        case KEY_UP:    motor_set(MOTOR_FORWARD);  break;
        case KEY_DOWN:  motor_set(MOTOR_BACKWARD); break;
        case KEY_LEFT:  motor_set(MOTOR_LEFT);     break;
        case KEY_RIGHT: motor_set(MOTOR_RIGHT);    break;
        case KEY_QUIT:  g_run = 0;                 break;
        default:        motor_set(MOTOR_STOP);     break;
        }
    }

    motor_set(MOTOR_STOP);
    motor_cleanup();
    term_raw_off();
    puts("\n종료");
    return EXIT_SUCCESS;
}
