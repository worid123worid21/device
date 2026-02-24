#ifndef MG996R_H
#define MG996R_H

#include <linux/ioctl.h>

// ─────────────────────────────────────────────
//  디바이스 정보
// ─────────────────────────────────────────────
#define MG996R_DEV_NAME     "mg996r"
#define MG996R_DEV_PATH     "/dev/mg996r"

// ─────────────────────────────────────────────
//  각도 범위
// ─────────────────────────────────────────────
#define MG996R_PAN_MIN      70
#define MG996R_PAN_MAX      170
#define MG996R_TILT_MIN     0
#define MG996R_TILT_MAX     180
#define MG996R_CENTER       90

// ─────────────────────────────────────────────
//  ioctl 데이터 구조체
// ─────────────────────────────────────────────
struct mg996r_angle {
    int pan;    // Pan  각도 (70~170)
    int tilt;   // Tilt 각도 (0~180)
};

// ─────────────────────────────────────────────
//  ioctl 명령 정의
//  매직 넘버: 0xB0 (임의 선택, 충돌 방지)
// ─────────────────────────────────────────────
#define MG996R_MAGIC        0xB0

#define MG996R_SET_PAN      _IOW(MG996R_MAGIC, 0, int)               // pan 각도 설정
#define MG996R_SET_TILT     _IOW(MG996R_MAGIC, 1, int)               // tilt 각도 설정
#define MG996R_SET_BOTH     _IOW(MG996R_MAGIC, 2, struct mg996r_angle) // 동시 설정
#define MG996R_DO_CENTER    _IO (MG996R_MAGIC, 3)                    // 중앙 복귀
#define MG996R_GET_PAN      _IOR(MG996R_MAGIC, 4, int)               // pan 각도 읽기
#define MG996R_GET_TILT     _IOR(MG996R_MAGIC, 5, int)               // tilt 각도 읽기

#endif /* MG996R_H */