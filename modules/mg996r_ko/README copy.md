# MG996R Pan/Tilt Controller

라즈베리파이 4 (BCM2711) + Linux sysfs PWM 기반 MG996R 서보모터 Pan/Tilt 제어

---

## 파일 구조

```
.
├── servo_module.h   # Pan/Tilt 모듈 헤더 (API 정의)
├── servo_module.c   # Pan/Tilt 모듈 구현체
├── main.c           # 키보드 제어 메인 (evdev 기반)
└── Makefile
```

---

## 하드웨어 연결

`/boot/firmware/config.txt` 설정: `dtoverlay=pwm-2chan` (기본값)

| 서보 역할 | PWM | GPIO | 물리 핀 |
|----------|-----|------|---------|
| Pan (좌우) | pwmchip0/pwm0 | GPIO19 | **35번** |
| Tilt (상하) | pwmchip0/pwm1 | GPIO18 | **12번** |

MG996R 신호 규격: 50Hz / 0.5ms(0°) ~ 2.5ms(180°)

---

## 빌드 및 실행

```bash
make
sudo ./pantilt_ctrl
```

> `sudo` 필요: `/dev/input/eventX` 접근 권한
> 권한 영구 부여: `sudo usermod -aG input $USER` 후 재로그인

---

## 조작 키

### 방향키 레이아웃 (USB 키보드 직접 연결)

```
Q(좌상)  W(상)   E(우상)
A(좌)    ─────   D(우)
Z(좌하)  X(하)   C(우하)
```

| 키 | Pan | Tilt |
|----|-----|------|
| `Q` | 좌 | 상 |
| `W` | ─  | 상 |
| `E` | 우 | 상 |
| `A` | 좌 | ─  |
| `D` | 우 | ─  |
| `Z` | 좌 | 하 |
| `X` | ─  | 하 |
| `C` | 우 | 하 |

대각선(Q/E/Z/C) 이동 시 벡터 정규화(`step / √2`)로 모든 방향 동일한 속도 보장

### 커맨드키

| 키 | 동작 |
|----|------|
| `S` | 중앙(90°) 복귀 |
| `O` | 현재 위치 저장 |
| `R` | 저장 위치로 이동 (recall) |
| `P` | 각도 직접 입력 |
| `ESC` | 종료 (중앙 복귀 후 PWM 해제) |

---

## 각도 범위

| 축 | 범위 |
|----|------|
| Pan | 70° ~ 170° |
| Tilt | 0° ~ 180° |

---

## servo_module API

```c
#include "servo_module.h"

PanTiltUnit pt;
pantilt_init(&pt, 0, 0, 1);        // chip=0, pan=ch0(GPIO18), tilt=ch1(GPIO19)
pantilt_set(&pt, 120.0f, 60.0f);   // Pan/Tilt 동시 이동
pantilt_center(&pt);                // 중앙 복귀
pantilt_cleanup(&pt);               // 해제
```

멀티스레드 환경에서 `pantilt_set()`은 내부 mutex로 보호됩니다.

---

## 주요 설계

- **sysfs PWM**: `/sys/class/pwm/pwmchip%d/pwm%d/` 직접 제어
- **evdev**: USB 키보드 `/dev/input/eventX` 하드웨어 이벤트 직접 읽기 → 진짜 동시 입력 감지
- **대각선 정규화**: 이동 벡터를 단위 벡터로 정규화 후 `ANGLE_STEP` 적용
- **스레드 안전**: `ServoChannel`마다 `pthread_mutex_t` 내장
- **에러 처리**: 모든 API가 `ServoError` 반환
- **엣지 트리거**: S/O/R/P/ESC 커맨드키는 누른 순간 1회만 동작