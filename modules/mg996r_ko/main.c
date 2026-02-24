#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>
#include <math.h>
#include <sys/ioctl.h>
#include "mg996r.h"

typedef enum { KEY_Q=0, KEY_W, KEY_E, KEY_A, KEY_D, KEY_Z, KEY_X, KEY_C, KEY_COUNT } KeyIndex;

static volatile int g_running = 1;
static struct termios g_orig_term;
static int g_fd = -1;

// ────────────── Signal / Terminal ──────────────
static void handle_signal(int sig) { (void)sig; g_running = 0; }

static void enable_raw_mode(void)
{
    tcgetattr(STDIN_FILENO, &g_orig_term);
    struct termios raw = g_orig_term;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
}

static void disable_raw_mode(void)
{
    tcsetattr(STDIN_FILENO, TCSANOW, &g_orig_term);
}

// ────────────── Smooth / Delta ──────────────
#define ANGLE_STEP 1.0f
#define SMOOTH_STEP 1.0f
#define LOOP_DELAY_US 10000

static float smooth_step(float cur, float tgt)
{
    float diff = tgt - cur;
    if (fabsf(diff) <= SMOOTH_STEP) return tgt;
    return cur + (diff > 0 ? SMOOTH_STEP : -SMOOTH_STEP);
}

static void poll_keys(int key_state[KEY_COUNT], char *one_shot_out)
{
    memset(key_state, 0, sizeof(int)*KEY_COUNT);
    if(one_shot_out) *one_shot_out='\0';

    char buf[32];
    ssize_t n = read(STDIN_FILENO, buf, sizeof(buf));
    if(n<=0) return;

    for(ssize_t i=0;i<n;i++){
        char c = buf[i];
        switch(c){
            case 'q': case 'Q': key_state[KEY_Q]=1; break;
            case 'w': case 'W': key_state[KEY_W]=1; break;
            case 'e': case 'E': key_state[KEY_E]=1; break;
            case 'a': case 'A': key_state[KEY_A]=1; break;
            case 'd': case 'D': key_state[KEY_D]=1; break;
            case 'z': case 'Z': key_state[KEY_Z]=1; break;
            case 'x': case 'X': key_state[KEY_X]=1; break;
            case 'c': case 'C': key_state[KEY_C]=1; break;
            default:
                if(one_shot_out) *one_shot_out=c;
                break;
        }
    }
}

static void calc_delta(const int key_state[KEY_COUNT], float *dpan, float *dtilt)
{
    float dx=0, dy=0;
    if(key_state[KEY_Q]) { dx-=1; dy+=1; }
    if(key_state[KEY_W]) { dy+=1; }
    if(key_state[KEY_E]) { dx+=1; dy+=1; }
    if(key_state[KEY_A]) { dx-=1; }
    if(key_state[KEY_D]) { dx+=1; }
    if(key_state[KEY_Z]) { dx-=1; dy-=1; }
    if(key_state[KEY_X]) { dy-=1; }
    if(key_state[KEY_C]) { dx+=1; dy-=1; }

    float mag = sqrtf(dx*dx+dy*dy);
    if(mag>0){ dx=dx/mag*ANGLE_STEP; dy=dy/mag*ANGLE_STEP; }

    *dpan = dy;
    *dtilt = -dx;
}

// ────────────── Servo Control ──────────────
static void set_servo(float pan, float tilt)
{
    if(g_fd<0) return;
    struct mg996r_angle angles;
    angles.pan  = (int)pan;
    angles.tilt = (int)tilt;
    if(ioctl(g_fd, MG996R_SET_BOTH, &angles)<0) perror("ioctl MG996R_SET_BOTH");
}

static void center_servo(void)
{
    if(g_fd<0) return;
    if(ioctl(g_fd, MG996R_DO_CENTER)<0) perror("ioctl MG996R_DO_CENTER");
}

// ────────────── Main ──────────────
int main(void)
{
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    g_fd = open(MG996R_DEV_PATH, O_RDWR);
    if(g_fd<0){ perror("open /dev/mg996r"); return -1; }

    float pan_cur=90, tilt_cur=90;
    float pan_tgt=90, tilt_tgt=90;
    float pan_sav=90, tilt_sav=90;

    enable_raw_mode();

    printf("=== MG996R Pan/Tilt Controller ===\n");
    printf("QWE / AD / ZXC : 이동\n");
    printf("S: 90° 복귀  O: 저장  R: 저장위치  P: 각도입력  T: 종료\n\n");

    int key_state[KEY_COUNT];
    char one_shot;

    while(g_running){
        poll_keys(key_state,&one_shot);

        switch(one_shot){
            case 't': case 'T': g_running=0; break;
            case 's': case 'S': pan_tgt=90; tilt_tgt=90; break;
            case 'o': case 'O': pan_sav=pan_cur; tilt_sav=tilt_cur;
                                printf("\n[Saved] Pan: %.0f Tilt: %.0f\n", pan_sav, tilt_sav);
                                break;
            case 'r': case 'R': pan_tgt=pan_sav; tilt_tgt=tilt_sav; break;

            case 'p': case 'P':
            {
                // ▼ raw 모드 끄고 blocking으로 변경
                int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
                fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);
                disable_raw_mode();

                char buf[64]; float a;
                printf("\nEnter Pan angle (70~170): "); fflush(stdout);
                if(fgets(buf,sizeof(buf),stdin)){
                    a = atof(buf);
                    if(a<MG996R_PAN_MIN)a=MG996R_PAN_MIN;
                    if(a>MG996R_PAN_MAX)a=MG996R_PAN_MAX;
                    pan_tgt=a;
                }

                printf("Enter Tilt angle (0~180): "); fflush(stdout);
                if(fgets(buf,sizeof(buf),stdin)){
                    a = atof(buf);
                    if(a<MG996R_TILT_MIN)a=MG996R_TILT_MIN;
                    if(a>MG996R_TILT_MAX)a=MG996R_TILT_MAX;
                    tilt_tgt=a;
                }

                printf("→ Moving Pan: %.0f°, Tilt: %.0f°\n", pan_tgt, tilt_tgt);

                // ▼ raw 모드 + non-blocking 복원
                enable_raw_mode();
            }
            break;

            default: break;
        }

        float dpan,dtilt;
        calc_delta(key_state,&dpan,&dtilt);
        pan_tgt+=dpan; tilt_tgt+=dtilt;

        if(pan_tgt<MG996R_PAN_MIN) pan_tgt=MG996R_PAN_MIN;
        if(pan_tgt>MG996R_PAN_MAX) pan_tgt=MG996R_PAN_MAX;
        if(tilt_tgt<MG996R_TILT_MIN) tilt_tgt=MG996R_TILT_MIN;
        if(tilt_tgt>MG996R_TILT_MAX) tilt_tgt=MG996R_TILT_MAX;

        pan_cur  = smooth_step(pan_cur, pan_tgt);
        tilt_cur = smooth_step(tilt_cur, tilt_tgt);

        set_servo(pan_cur, tilt_cur);

        printf("\rTilt:%6.1f Pan:%6.1f    ", tilt_cur, pan_cur);
        fflush(stdout);

        usleep(LOOP_DELAY_US);
    }

    disable_raw_mode();
    center_servo();
    usleep(300000);
    close(g_fd);
    printf("\nExiting...\n");
    return 0;
}