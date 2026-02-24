/*
 * main.c - Pan/Tilt 키보드 제어 (evdev 기반)
 *
 * 빌드: make
 * 실행: sudo ./pantilt_ctrl
 *
 * sudo 필요 이유: /dev/input/eventX 접근 권한
 *
 * 조작:
 *   W/A/S/D        : 단일 방향 이동
 *   WA/WD/SA/SD    : 대각선 이동 (√2 정규화)
 *   E              : 중앙(90°) 복귀
 *   O              : 현재 위치 저장
 *   R              : 저장 위치로 이동
 *   Q / ESC        : 종료
 *
 * 동작 원리:
 *   evdev로 /dev/input/eventX에서 키 누름/뗌 이벤트를 직접 읽어
 *   key_state[] 배열로 현재 눌린 키 조합을 추적
 *   → 터미널 문자 스트림 방식과 달리 진짜 동시 입력 감지 가능
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <math.h>
#include <dirent.h>
#include <linux/input.h>
#include "servo_module.h"

// ─────────────────────────────────────────────
//  설정
// ─────────────────────────────────────────────
#define PWM_CHIP        0
#define PAN_CHANNEL     0           // GPIO18 (물리 12번 핀)
#define TILT_CHANNEL    1           // GPIO19 (물리 35번 핀)
#define ANGLE_STEP      1.0f        // 1틱당 이동 각도
#define LOOP_DELAY_US   10000       // 10ms
#define INPUT_DIR       "/dev/input"

// ─────────────────────────────────────────────
//  전역
// ─────────────────────────────────────────────
static PanTiltUnit  g_pantilt;
static volatile int g_running = 1;

// 키 상태 배열 (1 = 현재 눌림)
static int key_w = 0, key_a = 0, key_s = 0, key_d = 0;

// 커맨드 키 엣지 트리거용
static int prev_e = 0, prev_o = 0, prev_r = 0;
static int prev_q = 0, prev_esc = 0;

// ─────────────────────────────────────────────
//  시그널 핸들러
// ─────────────────────────────────────────────
static void handle_signal(int sig)
{
    (void)sig;
    g_running = 0;
}

// ─────────────────────────────────────────────
//  키보드 evdev 장치 자동 탐색
//  /dev/input/event* 중 KEY_W를 지원하는 장치 반환
// ─────────────────────────────────────────────
static int find_keyboard_fd(void)
{
    DIR *dir = opendir(INPUT_DIR);
    if (!dir) {
        perror("opendir /dev/input");
        return -1;
    }

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (strncmp(ent->d_name, "event", 5) != 0) continue;

        char path[64];
        snprintf(path, sizeof(path), "%s/%s", INPUT_DIR, ent->d_name);

        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;

        // EV_KEY 지원 여부 확인
        uint8_t evbit[EV_MAX / 8 + 1];
        memset(evbit, 0, sizeof(evbit));
        if (ioctl(fd, EVIOCGBIT(0, sizeof(evbit)), evbit) < 0) {
            close(fd);
            continue;
        }
        if (!(evbit[EV_KEY / 8] & (1 << (EV_KEY % 8)))) {
            close(fd);
            continue;
        }

        // KEY_W 지원 여부로 실제 키보드 판별
        uint8_t keybit[KEY_MAX / 8 + 1];
        memset(keybit, 0, sizeof(keybit));
        if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit) < 0) {
            close(fd);
            continue;
        }
        if (keybit[KEY_W / 8] & (1 << (KEY_W % 8))) {
            char name[128] = "Unknown";
            ioctl(fd, EVIOCGNAME(sizeof(name)), name);
            printf("[input] Keyboard found: %s (%s)\n", name, path);
            closedir(dir);
            return fd;
        }

        close(fd);
    }

    closedir(dir);
    return -1;
}

// ─────────────────────────────────────────────
//  evdev 이벤트 처리 → 키 상태 갱신
//
//  ev.value: 0=release, 1=press, 2=repeat
//  → press/repeat 시 1, release 시 0으로 상태 갱신
// ─────────────────────────────────────────────
static void process_events(int kbd_fd)
{
    struct input_event ev;
    while (read(kbd_fd, &ev, sizeof(ev)) > 0) {
        if (ev.type != EV_KEY) continue;

        int pressed = (ev.value == 1 || ev.value == 2);

        switch (ev.code) {
            // 이동 키 (레벨 트리거)
            case KEY_W:   key_w = pressed; break;
            case KEY_A:   key_a = pressed; break;
            case KEY_S:   key_s = pressed; break;
            case KEY_D:   key_d = pressed; break;

            // 커맨드 키 (엣지 트리거용 상태 저장)
            case KEY_E:   prev_e   = pressed; break;
            case KEY_O:   prev_o   = pressed; break;
            case KEY_R:   prev_r   = pressed; break;
            case KEY_Q:   prev_q   = pressed; break;
            case KEY_ESC: prev_esc = pressed; break;

            default: break;
        }
    }
}

// ─────────────────────────────────────────────
//  WASD → Pan/Tilt delta 계산 (대각선 정규화)
//
//  대각선 시 벡터 크기가 √2가 되므로
//  1/√2 배로 정규화 → 모든 방향 동일한 이동 속도
// ─────────────────────────────────────────────
static void calc_delta(float *dpan, float *dtilt)
{
    float dx = 0.0f, dy = 0.0f;

    if (key_a) dx += 1.0f;   // A: pan+
    if (key_d) dx -= 1.0f;   // D: pan-
    if (key_w) dy += 1.0f;   // W: tilt+
    if (key_s) dy -= 1.0f;   // S: tilt-

    float mag = sqrtf(dx * dx + dy * dy);
    if (mag > 0.0f) {
        dx = (dx / mag) * ANGLE_STEP;
        dy = (dy / mag) * ANGLE_STEP;
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

    // ── 키보드 장치 탐색 ──────────────────────
    int kbd_fd = find_keyboard_fd();
    if (kbd_fd < 0) {
        fprintf(stderr, "[main] Keyboard not found.\n");
        fprintf(stderr, "       USB 키보드를 연결하고 sudo로 실행하세요.\n");
        return EXIT_FAILURE;
    }

    // ── Pan/Tilt 초기화 ───────────────────────
    ServoError err = pantilt_init(&g_pantilt, PWM_CHIP, PAN_CHANNEL, TILT_CHANNEL);
    if (err != SERVO_OK) {
        fprintf(stderr, "[main] pantilt_init failed: %s\n", servo_strerror(err));
        close(kbd_fd);
        return EXIT_FAILURE;
    }

    float pan_cur  = 90.0f, tilt_cur  = 90.0f;
    float pan_tgt  = 90.0f, tilt_tgt  = 90.0f;
    float pan_sav  = 90.0f, tilt_sav  = 90.0f;

    printf("\n=== Pan/Tilt Controller (evdev) ===\n");
    printf("W/A/S/D : 이동   WA/WD/SA/SD : 대각선\n");
    printf("E : 중앙복귀   O : 저장   R : recall   Q/ESC : 종료\n\n");

    // ── 메인 루프 ─────────────────────────────
    while (g_running) {

        // 1. 키 이벤트 읽기
        process_events(kbd_fd);

        // 2. 커맨드 키 처리 (눌린 순간 1회)
        if (prev_q || prev_esc) {
            g_running = 0;
            break;
        }
        if (prev_e) {
            pan_tgt = tilt_tgt = 90.0f;
            printf("\n[center]\n");
            prev_e = 0;
        }
        if (prev_o) {
            pan_sav  = pan_cur;
            tilt_sav = tilt_cur;
            printf("\n[saved] Pan=%.1f°  Tilt=%.1f°\n", pan_sav, tilt_sav);
            prev_o = 0;
        }
        if (prev_r) {
            pan_tgt  = pan_sav;
            tilt_tgt = tilt_sav;
            prev_r = 0;
        }

        // 3. WASD → target 갱신 (대각선 포함)
        float dpan, dtilt;
        calc_delta(&dpan, &dtilt);
        pan_tgt  += dpan;
        tilt_tgt += dtilt;

        // 4. 범위 클램핑
        if (pan_tgt  < 70.0f)  pan_tgt  = 70.0f;
        if (pan_tgt  > 170.0f) pan_tgt  = 170.0f;
        if (tilt_tgt < 0.0f)   tilt_tgt = 0.0f;
        if (tilt_tgt > 180.0f) tilt_tgt = 180.0f;

        // 5. 서보 적용
        err = pantilt_set(&g_pantilt, pan_tgt, tilt_tgt);
        if (err != SERVO_OK)
            fprintf(stderr, "[warn] %s\n", servo_strerror(err));

        pan_cur  = pan_tgt;
        tilt_cur = tilt_tgt;

        // 6. 상태 출력
        printf("\r Pan:%6.1f°  Tilt:%6.1f°  [%s%s%s%s]   ",
               pan_cur, tilt_cur,
               key_w ? "W" : " ", key_a ? "A" : " ",
               key_s ? "S" : " ", key_d ? "D" : " ");
        fflush(stdout);

        usleep(LOOP_DELAY_US);
    }

    // ── 정리 ──────────────────────────────────
    printf("\n[main] Cleaning up...\n");
    pantilt_center(&g_pantilt);
    usleep(300000);
    pantilt_cleanup(&g_pantilt);
    close(kbd_fd);
    printf("[main] Done\n");
    return EXIT_SUCCESS;
}

