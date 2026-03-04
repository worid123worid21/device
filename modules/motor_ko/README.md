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

## 적재 후 사용법 (단계별)

### Step 1. 모듈 적재

```bash
sudo insmod l298n_driver.ko
```

### Step 2. 적재 확인

```bash
# 모듈 목록 확인
lsmod | grep l298n

# 디바이스 노드 생성 확인
ls -l /dev/l298n

# 커널 로그 확인 (핀 매핑 출력됨)
dmesg | tail -5
```

정상 적재 시 `dmesg` 출력 예:
```
l298n: 드라이버 로드 완료 (major=240)
l298n: 핀 매핑 E1=BCM16(Phy36) M1=BCM20(Phy38) / E2=BCM17(Phy11) M2=BCM27(Phy13)
```

### Step 3. 권한 설정 (sudo 없이 실행하려면)

```bash
sudo chmod 666 /dev/l298n
```

### Step 4. 앱 실행

```bash
sudo ./motor_ctrl
```

실행 화면:
```
=== L298N 방향키 제어 ===
  ↑  전진    ↓  후진
  ←  좌회전  →  우회전
  q  종료
```

방향키를 누르면 즉시 모터가 동작하고, `q`를 누르면 정지 후 종료됩니다.

### Step 5. 종료 및 모듈 제거

```bash
# 앱에서 q 눌러 종료 후
sudo rmmod l298n_driver

# 제거 확인
lsmod | grep l298n     # 아무것도 안 나오면 정상
dmesg | tail -3        # "l298n: 드라이버 언로드" 확인
```

---

### 부팅 시 자동 적재 (선택)

```bash
# 1. 모듈을 커널 경로에 복사
sudo cp l298n_driver.ko /lib/modules/$(uname -r)/extra/

# 2. 모듈 의존성 갱신
sudo depmod -a

# 3. 부팅 시 자동 로드 등록
echo "l298n_driver" | sudo tee /etc/modules-load.d/l298n.conf

# 4. 재부팅 후 확인
sudo reboot
lsmod | grep l298n
```

---

### 문제 해결

| 증상 | 원인 | 해결 |
|------|------|------|
| `/dev/l298n` 없음 | 모듈 미적재 | `sudo insmod l298n_driver.ko` |
| `gpio_request 실패` | 핀 충돌 | `sudo rmmod` 후 핀 사용 확인 |
| `ioctl 실패` | 권한 없음 | `sudo` 또는 `chmod 666 /dev/l298n` |
| 모터 무반응 | 핀 매핑 불일치 | `l298n.h` GPIO 번호 재확인 |

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