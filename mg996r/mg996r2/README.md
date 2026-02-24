//라즈베리에 직접 키보드 연결
# MG996R Pan/Tilt Controller

라즈베리파이 4 (BCM2711) + Linux sysfs PWM 기반 MG996R 서보모터 Pan/Tilt 제어 모듈

---

## 파일 구조

```
.
├── servo_module.h   # Pan/Tilt 모듈 헤더 (API 정의)
├── servo_module.c   # Pan/Tilt 모듈 구현체
├── main.c           # 키보드 제어 메인
└── Makefile
```

---

## 하드웨어 연결

| 서보 | PWM 칩 | PWM 채널 | 역할 |
|------|--------|----------|------|
| MG996R #1 | pwmchip0 | pwm0 | Pan (좌우) |
| MG996R #2 | pwmchip0 | pwm1 | Tilt (상하) |

MG996R 신호 규격: 50Hz / 0.5ms(0°) ~ 2.5ms(180°)

---

## 빌드 및 실행

```bash
make
sudo ./pantilt_ctrl
```

> sysfs PWM 접근에 root 권한 필요

---

## 조작 키

| 키 | 동작 |
|----|------|
| `W` | Tilt 위 |
| `S` | Tilt 아래 |
| `A` | Pan 왼쪽 |
| `D` | Pan 오른쪽 |
| `WA` / `WD` / `SA` / `SD` | 대각선 이동 (속도 정규화 적용) |
| `O` | 현재 위치 저장 |
| `R` | 저장 위치로 이동 |
| `E` | 중앙(90°)으로 복귀 |
| `P` | 각도 직접 입력 |
| `Q` | 종료 (중앙 복귀 후 PWM 해제) |

대각선 이동 시 벡터 정규화(`1/√2`)를 적용하여 단일 방향과 동일한 속도로 이동합니다.

---

## 각도 범위

| 축 | 범위 |
|----|------|
| Pan | 70° ~ 170° |
| Tilt | 0° ~ 180° |

---

## 모듈 API 요약

다른 모듈(카메라 추적, GPS 등)에서 아래와 같이 사용합니다.

```c
#include "servo_module.h"

PanTiltUnit pt;

// 초기화 (chip=0, pan=ch0, tilt=ch1)
pantilt_init(&pt, 0, 0, 1);

// Pan/Tilt 동시 이동
pantilt_set(&pt, 120.0f, 60.0f);

// 중앙 복귀
pantilt_center(&pt);

// 해제
pantilt_cleanup(&pt);
```

멀티스레드 환경에서 `pantilt_set()`은 내부 mutex로 보호되어 안전하게 호출 가능합니다.

---

## 주요 설계

- **sysfs PWM**: `/sys/class/pwm/pwmchip%d/pwm%d/` 직접 제어
- **스레드 안전**: `ServoChannel`마다 `pthread_mutex_t` 내장
- **에러 처리**: 모든 API가 `ServoError` 반환 (`servo_strerror()`로 메시지 확인)
- **동시 입력**: `read()`로 stdin 버퍼를 매 루프마다 일괄 처리하여 키 조합 감지
- **대각선 정규화**: 이동 벡터 크기를 1로 정규화하여 방향과 무관한 일정 속도 보장
