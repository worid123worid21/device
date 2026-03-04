#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <signal.h>
#include "motor.h"

/* ------------------------------------------------------------------ */
/* 터미널 raw mode                                                       */
/* ------------------------------------------------------------------ */

static struct termios g_orig_termios;

static void term_raw_enable(void)
{
    struct termios raw;
    tcgetattr(STDIN_FILENO, &g_orig_termios);
    raw = g_orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO); /* canonical 모드 OFF, 에코 OFF */
    raw.c_cc[VMIN]  = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static void term_raw_disable(void)
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig_termios);
}

/* ------------------------------------------------------------------ */
/* 시그널 핸들러 (Ctrl+C 등)                                             */
/* ------------------------------------------------------------------ */

static volatile int g_running = 1;

static void sig_handler(int sig)
{
    (void)sig;
    g_running = 0;
}

/* ------------------------------------------------------------------ */
/* 방향키 읽기                                                           */
/* 방향키: ESC(0x1B) → '[' → 'A'/'B'/'C'/'D'                           */
/* ------------------------------------------------------------------ */

typedef enum {
    KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
    KEY_QUIT, KEY_UNKNOWN
} Key;

static Key read_key(void)
{
    unsigned char c;
    if (read(STDIN_FILENO, &c, 1) != 1) return KEY_UNKNOWN;

    if (c == 'q' || c == 'Q') return KEY_QUIT;

    if (c == 0x1B) {                       /* ESC 시퀀스 시작 */
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
    signal(SIGINT,  sig_handler);
    signal(SIGTERM, sig_handler);

    if (motor_init() < 0) {
        fprintf(stderr, "모터 초기화 실패\n");
        return EXIT_FAILURE;
    }

    term_raw_enable();

    printf("=== MG996R 방향키 제어 ===\n");
    printf("  ↑  전진   ↓  후진\n");
    printf("  ←  좌회전  →  우회전\n");
    printf("  q  종료\n\n");

    while (g_running) {
        Key k = read_key();
        switch (k) {
        case KEY_UP:    motor_set(MOTOR_FORWARD);  break;
        case KEY_DOWN:  motor_set(MOTOR_BACKWARD); break;
        case KEY_LEFT:  motor_set(MOTOR_LEFT);     break;
        case KEY_RIGHT: motor_set(MOTOR_RIGHT);    break;
        case KEY_QUIT:  g_running = 0;             break;
        default:        motor_stop();              break;
        }
    }

    motor_stop();
    motor_cleanup();
    term_raw_disable();

    printf("\n종료\n");
    return EXIT_SUCCESS;
}

