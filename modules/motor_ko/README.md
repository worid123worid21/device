# L298N DC Motor Kernel Driver

Raspberry Pi에서 L298N 모터 드라이버를 커널 모듈(.ko)로 제어하는 프로젝트입니다.  
방향키(↑↓←→)로 모터를 실시간 조작합니다.

---

## 파일 구조

```
motor_ko/
├── l298n_driver.c   # 커널 모듈 본체 (.ko 생성)
├── l298n.h          # 커널/유저 공유 헤더 (ioctl 명령, GPIO 핀 정의)
├── motor.c          # 유저단 라이브러리 (ioctl 호출)
├── motor.h          # 유저단 헤더
├── main.c           # 방향키 입력 처리 (유저 실행파일)
└── Makefile         # 커널 모듈 + 유저 앱 통합 빌드
```

---

## 하드웨어

### 모터 드라이버

| 항목 | 내용 |
|------|------|
| IC | L298N |
| 제어 방식 | GPIO ON/OFF (PWM 핀을 HIGH 고정) |
| 모터 채널 | A (좌), B (우) |

### GPIO 핀 매핑 (BCM 번호 기준)

| 드라이버 핀 | BCM | Physical | 역할 |
|-------------|-----|----------|------|
| E1 (PWMA)   | 16  | 36       | 모터A 활성 (HIGH=ON) |
| M1 (DIRA)   | 20  | 38       | 모터A 방향 (LOW=전진, HIGH=후진) |
| E2 (PWMB)   | 17  | 11       | 모터B 활성 (HIGH=ON) |
| M2 (DIRB)   | 27  | 13       | 모터B 방향 (LOW=전진, HIGH=후진) |

> `l298n.h` 상단 `#define`에서 핀 번호를 변경할 수 있습니다.

### 진리표

| 동작   | E1 | M1 | E2 | M2 |
|--------|:--:|:--:|:--:|:--:|
| 정지   | 0  | 0  | 0  | 0  |
| 전진   | 1  | 0  | 1  | 0  |
| 후진   | 1  | 1  | 1  | 1  |
| 좌회전 | 0  | 0  | 1  | 0  |
| 우회전 | 1  | 0  | 0  | 0  |

---

## 빌드 환경

| 항목 | 내용 |
|------|------|
| OS | Raspberry Pi OS (64-bit) |
| 커널 | 6.12.62+rpt-rpi-v8 |
| 아키텍처 | arm64 |

> **주의:** 커널 6.6 이상에서는 `class_create()`에서 `THIS_MODULE` 인자가 제거되었습니다.  
> `class_create("l298n")` 형태로 사용해야 합니다.

---

## 빌드 및 실행

### 1. 빌드

```bash
make
```

커널 모듈(`l298n_driver.ko`)과 유저 앱(`motor_ctrl`)이 동시에 빌드됩니다.

### 2. 커널 모듈 적재

```bash
sudo insmod l298n_driver.ko
# 또는
make load
```

적재 후 `/dev/l298n` 디바이스 노드가 자동 생성됩니다.

```bash
ls -l /dev/l298n   # 확인
dmesg | tail       # 커널 로그 확인
```

### 3. 유저 앱 실행

```bash
sudo ./motor_ctrl
```

| 키 | 동작 |
|----|------|
| ↑  | 전진 |
| ↓  | 후진 |
| ←  | 좌회전 |
| →  | 우회전 |
| q  | 종료 |

### 4. 모듈 제거

```bash
sudo rmmod l298n_driver
# 또는
make unload
```

---

## 동작 구조

```
유저 공간                        커널 공간
┌─────────────────┐             ┌──────────────────────┐
│  main.c         │             │  l298n_driver.ko     │
│  방향키 파싱    │             │                      │
│                 │             │  file_operations     │
│  motor.c        │──ioctl()──▶│  .unlocked_ioctl     │
│  /dev/l298n     │             │                      │
│  open/ioctl     │             │  gpio_set_value()    │
└─────────────────┘             │  E1/M1/E2/M2 제어   │
                                └──────────────────────┘
```

- 유저단은 `ioctl()`로 명령만 전달
- GPIO 제어는 커널 모듈이 전담
- 앱 종료 시 `release()` 핸들러에서 모터 자동 정지

---

## 정리

```bash
make clean
```
