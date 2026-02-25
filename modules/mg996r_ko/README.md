# MG996R + L298N DC 모터 제어

## 1️⃣ 환경 및 하드웨어

- **Raspberry Pi**: 3B+, 4, 또는 5 (arm64, 6.12.62+rpt-rpi-v8 커널 기준)
- **서보 모터**: MG996R 2개 (팬/틸트용)
  | 서보 | GPIO | 역할 |
  | --- | --- | --- |
  | Servo1 | GPIO18 (핀12) | Pan |
  | Servo2 | GPIO19 (핀35) | Tilt |
- ⚠ **모터 전원**: Raspberry Pi 5V 대신 외부 전원 사용 권장  
- ⚠ **GND**: Pi와 공통 연결 필수  

---

## 2️⃣ Raspberry Pi 설정

### 2.1 커널 버전 확인
```bash
uname -r

커널 모듈 빌드 시 vermagic과 일치해야 함

2.2 PWM, I2C, UART 활성화

/boot/firmware/config.txt 또는 /boot/config.txt 편집:

# pwm 필요 시 
dtoverlay=pwm-2chan
# I2C 필요 시
dtparam=i2c_arm=on
# UART 필요 시
dtparam=uart=on

재부팅 후 확인:

ls /sys/class/pwm/
# pwmchip0 출력 확인

PWM이 나타나야 서보 모듈 제어 가능

3️⃣ 커널 모듈 적재 및 실행
cd ~/DEV(모듈 위치로 이동)

# 모듈 삽입
sudo insmod mg996r_driver.ko

# 모듈 확인
lsmod | grep mg996r_driver

# 디바이스 파일 권한 확인
ls -l /dev/mg996r

# 테스트 실행 (root 권한)
sudo ./mg996r_main

# 일반 사용자 접근 허용
sudo chmod 666 /dev/mg996r
4️⃣ 모터 제어 구조

MG996R 서보: HW PWM 채널 사용

커널 모듈: /dev/mg996r 디바이스를 통해 ioctl 또는 유저 공간 명령으로 각도 설정

GPIO 핀:

기능	GPIO	PWM 채널
Pan	GPIO18	pwm0
Tilt	GPIO19	pwm1
5️⃣ 전체 제어 순서 예시

커널 모듈 확인:

uname -r

PWM 활성화 확인:

ls /sys/class/pwm/

모듈 적재 및 실행:

sudo insmod mg996r_driver.ko
sudo ./mg996r_main
6️⃣ 유저 단 컨트롤

키보드 9방향 제어: QWE / AD / ZXC

기능 키:

키	기능
S	90° 복귀
O	현재 위치 저장
R	저장 위치 복귀
P	Pan/Tilt 각도 수동 입력 (Enter 후 이동)
T	종료 및 중앙 복귀
7️⃣ 빌드

커널 모듈:

make
sudo insmod mg996r_driver.ko

유저단 C 프로그램:

gcc -o mg996r_main mg996r_main.c

모듈 해제:

sudo rmmod mg996r_driver.ko

디바이스 확인:

ls /dev/mg996r
dmesg | tail
8️⃣ 파일 구조
mg996r_ko/
├─ mg996r_driver.c  # 커널 모듈
├─ mg996r.h         # ioctl 정의 및 각도 범위
├─ mg996r_main.c    # 유저단 컨트롤러
├─ Makefile         # 커널 모듈 빌드
└─ README.md        # 설명 문서