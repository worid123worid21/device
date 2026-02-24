#ifndef SERVO_MODULE_H
#define SERVO_MODULE_H

#include <pthread.h>
#include <stdint.h>

// ─────────────────────────────────────────────
//  에러 코드 정의
// ─────────────────────────────────────────────
typedef enum {
    SERVO_OK            =  0,
    SERVO_ERR_INIT      = -1,   // 초기화 실패
    SERVO_ERR_ANGLE     = -2,   // 각도 범위 초과
    SERVO_ERR_IO        = -3,   // 파일 I/O 실패
    SERVO_ERR_NOT_INIT  = -4,   // 초기화되지 않은 채널
} ServoError;

// ─────────────────────────────────────────────
//  서보 채널 구조체 (내부 상태 캡슐화)
// ─────────────────────────────────────────────
typedef struct {
    int         pwm_chip;
    int         pwm_channel;
    float       current_angle;
    float       min_angle;
    float       max_angle;
    int         initialized;
    pthread_mutex_t lock;           // 멀티스레드 보호
} ServoChannel;

// ─────────────────────────────────────────────
//  Pan/Tilt 통합 구조체
// ─────────────────────────────────────────────
typedef struct {
    ServoChannel pan;               // 좌우 (수평)
    ServoChannel tilt;              // 상하 (수직)
} PanTiltUnit;

// ─────────────────────────────────────────────
//  ServoChannel API
// ─────────────────────────────────────────────

/**
 * @brief 서보 채널 초기화
 * @param ch        ServoChannel 포인터
 * @param pwm_chip  PWM 칩 번호 (보통 0)
 * @param pwm_ch    PWM 채널 번호
 * @param min_angle 최소 허용 각도 (0.0 ~ 180.0)
 * @param max_angle 최대 허용 각도 (0.0 ~ 180.0)
 * @return SERVO_OK or ServoError
 */
ServoError servo_channel_init(ServoChannel *ch,
                               int pwm_chip, int pwm_ch,
                               float min_angle, float max_angle);

/**
 * @brief 각도 즉시 설정 (스레드 안전)
 * @param ch    ServoChannel 포인터
 * @param angle 목표 각도
 * @return SERVO_OK or ServoError
 */
ServoError servo_channel_set_angle(ServoChannel *ch, float angle);

/**
 * @brief 현재 각도 읽기 (스레드 안전)
 * @param ch  ServoChannel 포인터
 * @param out 현재 각도 저장 포인터
 * @return SERVO_OK or ServoError
 */
ServoError servo_channel_get_angle(ServoChannel *ch, float *out);

/**
 * @brief 채널 해제 및 리소스 정리
 * @param ch ServoChannel 포인터
 */
void servo_channel_cleanup(ServoChannel *ch);

// ─────────────────────────────────────────────
//  PanTilt 통합 API
// ─────────────────────────────────────────────

/**
 * @brief Pan/Tilt 유닛 초기화
 *
 * 기본 각도 범위:
 *   pan  : 70° ~ 170° (수평 리밋)
 *   tilt : 0°  ~ 180° (수직 리밋)
 *
 * @param pt            PanTiltUnit 포인터
 * @param chip          PWM 칩 번호
 * @param pan_channel   Pan 서보 PWM 채널
 * @param tilt_channel  Tilt 서보 PWM 채널
 * @return SERVO_OK or ServoError
 */
ServoError pantilt_init(PanTiltUnit *pt,
                         int chip,
                         int pan_channel,
                         int tilt_channel);

/**
 * @brief Pan/Tilt 동시 이동 (스레드 안전)
 * @param pt        PanTiltUnit 포인터
 * @param pan_angle  Pan 목표 각도
 * @param tilt_angle Tilt 목표 각도
 * @return SERVO_OK or ServoError
 */
ServoError pantilt_set(PanTiltUnit *pt, float pan_angle, float tilt_angle);

/**
 * @brief 중앙(90°)으로 복귀
 * @param pt PanTiltUnit 포인터
 * @return SERVO_OK or ServoError
 */
ServoError pantilt_center(PanTiltUnit *pt);

/**
 * @brief Pan/Tilt 유닛 해제
 * @param pt PanTiltUnit 포인터
 */
void pantilt_cleanup(PanTiltUnit *pt);

// ─────────────────────────────────────────────
//  유틸리티
// ─────────────────────────────────────────────

/**
 * @brief 에러 코드를 문자열로 변환
 */
const char *servo_strerror(ServoError err);

#endif /* SERVO_MODULE_H */