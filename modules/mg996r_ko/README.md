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