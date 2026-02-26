# mpu_6050 — MPU-6050 (GY-521) Linux Kernel Driver

Raspberry Pi에서 MPU-6050 센서를 **커널 모듈(.ko)** 로 구동하고,  
유저스페이스에서 **sysfs**를 통해 Pitch / Roll 각도와 가속도를 읽는 프로젝트입니다.

현재 환경 
BCM-2711
6.12.62+rpt-rpi-v8

---

## 파일 구조

```
mpu_6050_ko/
├── mpu6050.h            # 커널/유저 공유 헤더 (레지스터, 구조체, sysfs 경로)
├── mpu6050_driver.c     # 커널 모듈 구현 (I2C 드라이버, sysfs 속성)
├── main.c               # 유저스페이스 실행파일 소스
├── Makefile             # 커널 모듈 + 유저 바이너리 동시 빌드
└── README.md
```

---

## 환경

| 항목 | 내용 |
|------|------|
| OS | Raspberry Pi OS (64-bit) |
| 커널 | `6.12.62+rpt-rpi-v8` |
| 아키텍처 | AArch64 (cortex-a72) |
| 통신 | I2C-1 (`/dev/i2c-1`) |
| 센서 주소 | `0x68` |

---

## 빌드

```bash
# 커널 헤더 설치 (최초 1회)
sudo apt install raspberrypi-kernel-headers

# 빌드 (커널 모듈 + 유저 실행파일)
make
```

빌드 후 생성 파일:

```
mpu_6050.ko          # 커널 모듈
mpu_6050_main        # 유저 실행파일
```

---

## 사용법

```bash
# 1. 커널 모듈 로드
sudo insmod mpu_6050.ko

# 2. 실행 (자동으로 캘리브레이션 후 데이터 출력)
./mpu_6050_main

# 3. 커널 모듈 언로드
sudo rmmod mpu_6050
```

### Makefile 단축 명령

```bash
make load     # sudo insmod mpu_6050.ko
make unload   # sudo rmmod mpu_6050
make run      # ./mpu_6050_main
make log      # sudo dmesg | tail -30
make clean    # 빌드 결과물 전체 삭제
```

---

## 출력 예시

```
MPU-6050 Pitch / Roll Monitor  (Ctrl+C to exit)

[INFO] Gyro calibration done.

Pitch:   -1.23°  |  Roll:    0.87°
Accel (g)  X: -0.021  Y:  0.015  Z:  0.999
```

---

## sysfs 인터페이스

모듈 로드 후 `/sys/bus/i2c/devices/1-0068/` 에 아래 속성이 생성됩니다.

| 속성 | 방향 | 단위 | 설명 |
|------|------|------|------|
| `accel_x` | R | mg | 가속도 X축 |
| `accel_y` | R | mg | 가속도 Y축 |
| `accel_z` | R | mg | 가속도 Z축 |
| `gyro_x` | R | m°/s | 자이로 X축 |
| `gyro_y` | R | m°/s | 자이로 Y축 |
| `gyro_z` | R | m°/s | 자이로 Z축 |
| `calibrate` | W | - | `1` 쓰면 자이로 바이어스 재캘리브레이션 |

```bash
# 직접 읽기 예시
cat /sys/bus/i2c/devices/1-0068/accel_x
cat /sys/bus/i2c/devices/1-0068/gyro_x

# 캘리브레이션 재실행
echo 1 | sudo tee /sys/bus/i2c/devices/1-0068/calibrate
```

---

## 동작 원리

```
MPU-6050 센서
    │  I2C (400kHz)
    ▼
mpu6050_driver.ko  (커널 공간)
    │  sysfs (/sys/bus/i2c/devices/1-0068/)
    ▼
mpu_6050_main  (유저 공간)
    │
    ├── accel_{x,y,z}  읽기 → 가속도 (mg → g 변환)
    ├── gyro_{x,y,z}   읽기 → 각속도 (m°/s → °/s 변환)
    └── Complementary Filter → Pitch / Roll 계산
```

### 센서 퓨전 (Complementary Filter)

| 값 | 계산 방법 |
|----|----------|
| Pitch | 가속도 + 자이로 Y축 퓨전 (α = 0.96) |
| Roll | 가속도 + 자이로 X축 퓨전 (α = 0.96) |

**Pitch / Roll 독립 공식:**

```
accel_pitch = atan2(-ax, sqrt(ay² + az²))
accel_roll  = atan2( ay, sqrt(ax² + az²))  ← 핵심
```

roll 공식의 분모를 `sqrt(ax² + az²)` 로 사용하면 pitch 방향으로 기울여도  
분모 크기가 보존되어 roll 값이 간섭받지 않습니다.  
기존 `atan2(ay, az)` 공식은 pitch 변화 시 `az` 가 바뀌면서 roll도 같이 튀는 문제가 있습니다.

> **Z축(Yaw)은 사용하지 않습니다.**  
> Yaw는 가속도로 보정이 불가능하여 자이로 적분 오차(drift)가 누적됩니다.  
> 절대 방위가 필요하면 지자기 센서(HMC5883L 등)를 추가하세요.

---

## 트러블슈팅

**probe가 실행되지 않을 때**
```bash
sudo dmesg | grep mpu
# "MPU-6050 init OK" 가 없으면 I2C 연결 또는 주소 확인
```

**I2C 주소 확인**
```bash
sudo i2cdetect -y 1
# 0x68 위치에 숫자가 표시되어야 함
```

**커널 헤더 버전 불일치**
```bash
uname -r                              # 현재 커널 버전 확인
ls /lib/modules/$(uname -r)/build     # 헤더 경로 존재 여부 확인
```