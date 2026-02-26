# =====================================================
# MPU-6050 Makefile
# - 커널 모듈 : mpu_6050.ko
# - 유저 실행  : mpu_6050_main
# Target       : Linux 6.12.62+rpt-rpi-v8 (Raspberry Pi AArch64)
# =====================================================

# 커널 헤더 경로
KERNEL_DIR  ?= /lib/modules/$(shell uname -r)/build

# 유저스페이스 빌드 설정
CC          = gcc
USER_TARGET = mpu_6050_main
USER_SRC    = main.c
USER_math   = -lm
USER_CFLAGS = -Wall -Wextra -O2 \
              -march=armv8-a \
              -mcpu=cortex-a72 \
              -std=c11 \
              -D_POSIX_C_SOURCE=200809L

# 커널 모듈 빌드 설정
obj-m          := mpu_6050.o
mpu_6050-objs  := mpu6050_driver.o

# =====================================================
# 전체 빌드 (커널 모듈 + 유저 바이너리)
# =====================================================

all: module user

module:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

user: $(USER_SRC)
	$(CC) $(USER_CFLAGS) -o $(USER_TARGET) $(USER_SRC) $(USER_math)
	@echo "Build OK → $(USER_TARGET)"

# =====================================================
# 청소
# =====================================================

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean
	rm -f $(USER_TARGET)
	@echo "Clean done"

# =====================================================
# 모듈 로드 / 언로드 / 실행
# =====================================================

load:
	sudo insmod mpu_6050.ko
	@echo "mpu_6050.ko loaded"

unload:
	sudo rmmod mpu_6050
	@echo "mpu_6050.ko unloaded"

run:
	./$(USER_TARGET)

log:
	sudo dmesg | tail -30

.PHONY: all module user clean load unload run log