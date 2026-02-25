//다른 환경 설정
MG996R + L298N DC 모터 제어 README
1️⃣ 환경 및 하드웨어

Raspberry Pi: 3B+, 4, 또는 5 (arm64, 6.12.62+rpt-rpi-v8 커널 기준)

연결 핀맵
MG996R 서보 모터 2개 (HW PWM)
서보	GPIO	핀	역할
Servo1	GPIO18	12	팬/틸트 모터
Servo2	GPIO19	35	팬/틸트 모터

⚠ 모터 전원은 Raspberry Pi 5V 대신 외부 전원 사용 권장
⚠ GND는 Pi와 공통 연결

2️⃣ Raspberry Pi 설정
2.1 커널 모듈 확인
uname -r 
현재 부팅된 커널 버전 확인
모듈 빌드 시 vermagic과 일치해야 함
2.2 PWM, I2C, UART 활성화
/boot/firmware/config.txt 또는 /boot/config.txt 확인

dtoverlay=pwm-2chan
# IC2 필요 시 dtparam=i2c_arm=on
# UART 필요 시 dtparam=uart=on

재부팅 후 확인:
ls /sys/class/pwm/
# pwmchip0 출력 확인
PWM이 나타나야 서보 모듈 제어 가능

3️⃣ 커널 모듈 적재 및 실행
cd ~/DEV

# 커널 모듈 삽입
sudo insmod mg996r_driver.ko

# 모듈 확인
lsmod | grep mg996r_driver

# 디바이스 파일 권한 확인
ls -l /dev/mg996r

# 테스트용 실행 (root 권한)
sudo ./mg996r_main
일반 사용자 접근 허용:
sudo chmod 666 /dev/mg996r


4️⃣ 모터 제어 구조
MG996R 서보 (팬/틸트)
HW PWM 채널 사용
커널 모듈 (mg996r_driver) 통해 제어
/dev/mg996r를 open 후 ioctl 또는 사용자 공간 명령으로 각 서보 각도 설정


5️⃣ 전체 제어 순서 예시
# 1. 커널 모듈 확인
uname -r

# 2. PWM, I2C 활성화 확인 및 /sys/class/pwm/ 확인
ls /sys/class/pwm/

# 3. 모듈 적재 및 실행
sudo insmod mg996r_driver.ko
sudo ./mg996r_main


// 아래 항목은 사용 설명(위 내용과 중복 있음 커널 설정이 같은 경우는 아래와 같이 사용)
---
MG996R Pan/Tilt Servo Driver & Controller
개요

이 프로젝트는 Raspberry Pi 등에서 MG996R 서보 모터를 PWM을 통해 제어하는 커널 모듈 + 유저단 C 컨트롤러입니다.

커널 모듈(mg996r_driver.ko) → /dev/mg996r 디바이스 제공, ioctl 인터페이스

유저단 프로그램(mg996r_main) → 키보드 입력 및 각도 제어

디바이스 정보
항목	값
장치 이름	mg996r
장치 경로	/dev/mg996r
Pan 각도 범위	70° ~ 170°
Tilt 각도 범위	0° ~ 180°
중앙 위치	90°
기능

실시간 9방향 키보드 제어 (QWE / AD / ZXC)

S: 90° 복귀
O: 현재 위치 저장
R: 저장 위치 복귀
P: Pan/Tilt 각도 수동 입력 → Enter 후 이동
T: 종료 및 중앙 복귀

커널 모듈 설치
# 모듈 빌드
make

# 모듈 로드
sudo insmod mg996r_driver.ko

# 모듈 해제
sudo rmmod mg996r_driver.ko

# 디바이스 확인
ls /dev/mg996r
dmesg | tail
유저단 실행
# 실행
sudo ./mg996r_main

실행 후, 아래와 같은 안내 메시지가 표시됩니다:

=== MG996R Pan/Tilt Controller ===
QWE / AD / ZXC : 이동
S: 90° 복귀  O: 저장  R: 저장위치  P: 각도입력  T: 종료

P 입력 시: Pan/Tilt 각도 입력 후 Enter → 모터 이동

빌드

커널 모듈: make

유저단 C 프로그램: gcc -o mg996r_main mg996r_main.c

파일 구조
mg996r_ko/
├─ mg996r_driver.c   # 커널 모듈
├─ mg996r.h          # ioctl 정의 및 각도 범위
├─ mg996r_main.c     # 유저단 컨트롤러
├─ Makefile          # 커널 모듈 빌드
└─ README.md         # 설명 문서
참고

PWM 제어는 /sys/class/pwm/pwmchip0/ 경로를 사용합니다.

Pan → GPIO18 / pwm0, Tilt → GPIO19 / pwm1

안전을 위해 테스트 시 모터 연결 상태 확인 필수