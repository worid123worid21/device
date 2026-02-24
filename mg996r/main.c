#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <signal.h>
#include <math.h>
#include "servo_module.h"

// ─────────────────────────────────────────────
//  설정
// ─────────────────────────────────────────────
#define PWM_CHIP        0
#define PAN_CHANNEL     0
#define TILT_CHANNEL    1
#define ANGLE_STEP      1.0f
#define SMOOTH_STEP     1.0f
#define LOOP_DELAY_US   10000

// ─────────────────────────────────────────────
//  키 인덱스 (9방향)
// ─────────────────────────────────────────────
typedef enum {
    KEY_Q = 0,
    KEY_W,
    KEY_E,
    KEY_A,
    KEY_D,
    KEY_Z,
    KEY_X,
    KEY_C,
    KEY_COUNT
} KeyIndex;

// ─────────────────────────────────────────────
static PanTiltUnit      g_pantilt;
static volatile int     g_running = 1;
static struct termios   g_orig_term;

// ─────────────────────────────────────────────
static void enable_raw_mode(void)
{
    tcgetattr(STDIN_FILENO, &g_orig_term);
    struct termios raw = g_orig_term;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN]  = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
}

static void disable_raw_mode(void)
{
    tcsetattr(STDIN_FILENO, TCSANOW, &g_orig_term);
}

static void handle_signal(int sig)
{
    (void)sig;
    g_running = 0;
}

static float smooth_step(float current, float target)
{
    float diff = target - current;
    if (fabsf(diff) <= SMOOTH_STEP) return target;
    return current + (diff > 0 ? SMOOTH_STEP : -SMOOTH_STEP);
}

// ─────────────────────────────────────────────
static void handle_angle_input(float *target_pan, float *target_tilt)
{
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);

    struct termios cooked = g_orig_term;
    tcsetattr(STDIN_FILENO, TCSANOW, &cooked);

    char buf[64];
    float a;

    printf("\nEnter Pan  angle (70~170): ");
    fflush(stdout);
    if (fgets(buf, sizeof(buf), stdin)) {
        a = atof(buf);
        if (a < 70)  a = 70;
        if (a > 170) a = 170;
        *target_pan = a;
    }

    printf("Enter Tilt angle (0~180): ");
    fflush(stdout);
    if (fgets(buf, sizeof(buf), stdin)) {
        a = atof(buf);
        if (a < 0)   a = 0;
        if (a > 180) a = 180;
        *target_tilt = a;
    }

    printf("→ Moving Pan: %.0f°, Tilt: %.0f°\n", *target_pan, *target_tilt);

    enable_raw_mode();
}

// ─────────────────────────────────────────────
static void poll_keys(int key_state[KEY_COUNT],
                      char *one_shot_out)
{
    memset(key_state, 0, KEY_COUNT * sizeof(int));
    if (one_shot_out) *one_shot_out = '\0';

    char buf[32];
    ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
    if (n <= 0) return;

    for (ssize_t i = 0; i < n; i++) {
        char c = buf[i];
        switch (c) {
            case 'q': case 'Q': key_state[KEY_Q] = 1; break;
            case 'w': case 'W': key_state[KEY_W] = 1; break;
            case 'e': case 'E': key_state[KEY_E] = 1; break;
            case 'a': case 'A': key_state[KEY_A] = 1; break;
            case 'd': case 'D': key_state[KEY_D] = 1; break;
            case 'z': case 'Z': key_state[KEY_Z] = 1; break;
            case 'x': case 'X': key_state[KEY_X] = 1; break;
            case 'c': case 'C': key_state[KEY_C] = 1; break;
            default:
                if (one_shot_out) *one_shot_out = c;
                break;
        }
    }
}

// ────────────────────────────────────────────
//  calc_delta  (9방향 정규화)
// ────────────────────────────────────────────
static void calc_delta(const int key_state[KEY_COUNT],
                       float *dpan, float *dtilt)
{
    float dx = 0.0f;   // ← 원래 pan이었음
    float dy = 0.0f;   // ← 원래 tilt였음

    // 상단
    if (key_state[KEY_Q]) { dx -= 1.0f; dy += 1.0f; }
    if (key_state[KEY_W]) { dy += 1.0f; }
    if (key_state[KEY_E]) { dx += 1.0f; dy += 1.0f; }

    // 중단
    if (key_state[KEY_A]) { dx -= 1.0f; }
    if (key_state[KEY_D]) { dx += 1.0f; }

    // 하단
    if (key_state[KEY_Z]) { dx -= 1.0f; dy -= 1.0f; }
    if (key_state[KEY_X]) { dy -= 1.0f; }
    if (key_state[KEY_C]) { dx += 1.0f; dy -= 1.0f; }

    float magnitude = sqrtf(dx * dx + dy * dy);

    if (magnitude > 0.0f) {
        dx = (dx / magnitude) * ANGLE_STEP;
        dy = (dy / magnitude) * ANGLE_STEP;
    }

    // 🔥 여기서 축을 서로 교환
    *dpan  = dy;   // 원래 dx → dy
    *dtilt = -dx;   // 원래 dy → dx
}

// ─────────────────────────────────────────────
int main(void)
{
    signal(SIGINT,  handle_signal);
    signal(SIGTERM, handle_signal);

    ServoError err = pantilt_init(&g_pantilt, PWM_CHIP, PAN_CHANNEL, TILT_CHANNEL);
    if (err != SERVO_OK) {
        fprintf(stderr, "pantilt_init failed: %s\n", servo_strerror(err));
        return EXIT_FAILURE;
    }

    float pan_cur = 90.0f, tilt_cur = 90.0f;
    float pan_tgt = 90.0f, tilt_tgt = 90.0f;
    float pan_sav = 90.0f, tilt_sav = 90.0f;

    enable_raw_mode();

    printf("=== Pan/Tilt Controller (9-Direction) ===\n");
    printf("QWE / AD / ZXC : 이동\n");
    printf("S: 90° 복귀  O: 저장  R: 저장위치  P: 각도입력  T: 종료\n\n");

    int key_state[KEY_COUNT];
    char one_shot;

    while (g_running) {

        poll_keys(key_state, &one_shot);

        switch (one_shot) {

            case 't': case 'T':
                g_running = 0;
                break;

            case 's': case 'S':
                pan_tgt  = 90.0f;
                tilt_tgt = 90.0f;
                break;

            case 'o': case 'O':
                pan_sav  = pan_cur;
                tilt_sav = tilt_cur;
                printf("\n[Saved] Pan: %.1f°  Tilt: %.1f°\n", pan_sav, tilt_sav);
                break;

            case 'r': case 'R':
                pan_tgt  = pan_sav;
                tilt_tgt = tilt_sav;
                break;

            case 'p': case 'P':
                handle_angle_input(&pan_tgt, &tilt_tgt);
                break;

            default: break;
        }

        float dpan, dtilt;
        calc_delta(key_state, &dpan, &dtilt);
        pan_tgt  += dpan;
        tilt_tgt += dtilt;

        if (pan_tgt  < 70)  pan_tgt  = 70;
        if (pan_tgt  > 170) pan_tgt  = 170;
        if (tilt_tgt < 0)   tilt_tgt = 0;
        if (tilt_tgt > 180) tilt_tgt = 180;

        pan_cur  = smooth_step(pan_cur,  pan_tgt);
        tilt_cur = smooth_step(tilt_cur, tilt_tgt);

        err = pantilt_set(&g_pantilt, pan_cur, tilt_cur);
        if (err != SERVO_OK)
            fprintf(stderr, "\n[warn] %s\n", servo_strerror(err));

        printf("\r Tilt:%6.1f°  Pan:%6.1f°    ", pan_cur, tilt_cur);
        fflush(stdout);

        usleep(LOOP_DELAY_US);
    }

    disable_raw_mode();
    pantilt_center(&g_pantilt);
    usleep(300000);
    pantilt_cleanup(&g_pantilt);

    printf("\nExiting...\n");
    return EXIT_SUCCESS;
}