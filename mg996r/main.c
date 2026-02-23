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
#define ANGLE_STEP      1.0f        // 단일 방향 최대 step (°/tick)
#define SMOOTH_STEP     1.0f        // smooth 이동 step
#define LOOP_DELAY_US   10000       // 10ms

// ─────────────────────────────────────────────
//  키 인덱스 정의
// ─────────────────────────────────────────────
typedef enum {
    KEY_W = 0,
    KEY_A,
    KEY_S,
    KEY_D,
    KEY_COUNT
} KeyIndex;

// ─────────────────────────────────────────────
//  전역
// ─────────────────────────────────────────────
static PanTiltUnit      g_pantilt;
static volatile int     g_running = 1;
static struct termios   g_orig_term;

// ─────────────────────────────────────────────
//  터미널 유틸
// ─────────────────────────────────────────────
static void enable_raw_mode(void)
{
    tcgetattr(STDIN_FILENO, &g_orig_term);
    struct termios raw = g_orig_term;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN]  = 0;   // 읽을 문자 없어도 즉시 반환
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
}

static void disable_raw_mode(void)
{
    tcsetattr(STDIN_FILENO, TCSANOW, &g_orig_term);
}

// ─────────────────────────────────────────────
//  시그널 핸들러
// ─────────────────────────────────────────────
static void handle_signal(int sig)
{
    (void)sig;
    g_running = 0;
}

// ─────────────────────────────────────────────
//  smooth step
// ─────────────────────────────────────────────
static float smooth_step(float current, float target)
{
    float diff = target - current;
    if (fabsf(diff) <= SMOOTH_STEP) return target;
    return current + (diff > 0 ? SMOOTH_STEP : -SMOOTH_STEP);
}

// ─────────────────────────────────────────────
//  각도 직접 입력 모드
// ─────────────────────────────────────────────
static void handle_angle_input(float *target_pan, float *target_tilt)
{
    // 블로킹 모드로 전환
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);

    // canonical + echo 복원
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

    // raw 모드 + 논블로킹 복귀
    enable_raw_mode();
}

// ─────────────────────────────────────────────
//  stdin에서 가능한 모든 문자를 읽어 key_state 갱신
//
//  키 누름/뗌 감지 원리:
//    - 터미널 raw + non-blocking 환경에서는 눌린 키만
//      버퍼에 쌓이고, 뗀 키는 더 이상 들어오지 않음
//    - 매 루프마다 버퍼를 비운 뒤, 이번 루프에서
//      읽힌 키만 true로 표시 (나머지는 false)
//    - 결과적으로 "지금 이 순간 눌려 있는 키" 집합을 근사
// ─────────────────────────────────────────────
static void poll_keys(int key_state[KEY_COUNT],
                      char *one_shot_out)   // 단일 커맨드 키 반환
{
    // 이전 상태 초기화
    memset(key_state, 0, KEY_COUNT * sizeof(int));
    if (one_shot_out) *one_shot_out = '\0';

    char buf[32];
    ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
    if (n <= 0) return;

    for (ssize_t i = 0; i < n; i++) {
        char c = buf[i];
        switch (c) {
            case 'w': case 'W': key_state[KEY_W] = 1; break;
            case 's': case 'S': key_state[KEY_S] = 1; break;
            case 'a': case 'A': key_state[KEY_A] = 1; break;
            case 'd': case 'D': key_state[KEY_D] = 1; break;
            // 단일 커맨드 키는 마지막 것만 저장
            default:
                if (one_shot_out) *one_shot_out = c;
                break;
        }
    }
}

// ─────────────────────────────────────────────
//  WASD 입력 → Pan/Tilt delta 계산 (대각선 정규화)
//
//  대각선 정규화 이유:
//    단순 합산 시 대각선 이동이 단일 방향보다 √2배 빠름
//    → 벡터 크기를 1로 정규화 후 ANGLE_STEP 을 곱해
//       어느 방향이든 동일한 이동 속도 보장
// ─────────────────────────────────────────────
static void calc_delta(const int key_state[KEY_COUNT],
                       float *dpan, float *dtilt)
{
    float dx = 0, dy = 0;   // dx: pan, dy: tilt

    if (key_state[KEY_A]) dx += 1.0f;   // A: pan+
    if (key_state[KEY_D]) dx -= 1.0f;   // D: pan-
    if (key_state[KEY_W]) dy += 1.0f;   // W: tilt+
    if (key_state[KEY_S]) dy -= 1.0f;   // S: tilt-

    float magnitude = sqrtf(dx * dx + dy * dy);
    if (magnitude > 0.0f) {
        dx = (dx / magnitude) * ANGLE_STEP;
        dy = (dy / magnitude) * ANGLE_STEP;
    }

    *dpan  = dx;
    *dtilt = dy;
}

// ─────────────────────────────────────────────
//  main
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

    float pan_cur  = 90.0f, tilt_cur  = 90.0f;
    float pan_tgt  = 90.0f, tilt_tgt  = 90.0f;
    float pan_sav  = 90.0f, tilt_sav  = 90.0f;
    const float pan_init = 90.0f, tilt_init = 90.0f;

    enable_raw_mode();
    printf("=== Pan/Tilt Controller (Multi-key) ===\n");
    printf("W/A/S/D : 이동  (WA/WD/SA/SD 동시 입력 → 대각선)\n");
    printf("O: 위치 저장  R: 저장 위치로  E: 중앙 복귀  P: 각도 입력  Q: 종료\n\n");

    int  key_state[KEY_COUNT];
    char one_shot;

    while (g_running) {

        // ── 입력 폴링 ──────────────────────────
        poll_keys(key_state, &one_shot);

        // ── 단일 커맨드 처리 ───────────────────
        switch (one_shot) {
            case 'q': case 'Q':
                g_running = 0;
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

            case 'e': case 'E':
                pan_tgt  = pan_init;
                tilt_tgt = tilt_init;
                break;

            case 'p': case 'P':
                handle_angle_input(&pan_tgt, &tilt_tgt);
                break;

            default: break;
        }

        // ── WASD → target 갱신 (대각선 포함) ──
        float dpan, dtilt;
        calc_delta(key_state, &dpan, &dtilt);
        pan_tgt  += dpan;
        tilt_tgt += dtilt;

        // ── 범위 클램핑 ────────────────────────
        if (pan_tgt  < 70)  pan_tgt  = 70.0f;
        if (pan_tgt  > 170) pan_tgt  = 170.0f;
        if (tilt_tgt < 0)   tilt_tgt = 0.0f;
        if (tilt_tgt > 180) tilt_tgt = 180.0f;

        // ── Smooth 이동 ────────────────────────
        pan_cur  = smooth_step(pan_cur,  pan_tgt);
        tilt_cur = smooth_step(tilt_cur, tilt_tgt);

        // ── 서보 적용 ──────────────────────────
        err = pantilt_set(&g_pantilt, pan_cur, tilt_cur);
        if (err != SERVO_OK)
            fprintf(stderr, "\n[warn] %s\n", servo_strerror(err));

        // ── 상태 출력 ──────────────────────────
        printf("\r Pan:%6.1f°  Tilt:%6.1f°  [%s%s%s%s]    ",
               pan_cur, tilt_cur,
               key_state[KEY_W] ? "W" : " ",
               key_state[KEY_A] ? "A" : " ",
               key_state[KEY_S] ? "S" : " ",
               key_state[KEY_D] ? "D" : " ");
        fflush(stdout);

        usleep(LOOP_DELAY_US);
    }

    // ── 정리 ───────────────────────────────────
    disable_raw_mode();
    pantilt_center(&g_pantilt);
    usleep(300000);
    pantilt_cleanup(&g_pantilt);
    printf("\nExiting...\n");
    return EXIT_SUCCESS;
}