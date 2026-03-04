# ================================================================
# L298N DC Motor Driver 빌드 시스템
#   make           → 커널 모듈 + 유저 앱 빌드
#   make load      → .ko 적재 (insmod)
#   make unload    → 모듈 제거 (rmmod)
#   make clean     → 전체 정리
# ================================================================

KDIR    ?= /lib/modules/$(shell uname -r)/build
obj-m   := l298n_driver.o

CC      = gcc
CFLAGS  = -Wall -Wextra -O2
TARGET  = motor_ctrl
USER_SRCS = main.c motor.c
USER_OBJS = $(USER_SRCS:.c=.o)

all: module app

module:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

app: $(USER_OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

load: module
	sudo insmod l298n_driver.ko
	@echo "[OK] l298n_driver.ko 적재 완료"
	@ls -l /dev/l298n

unload:
	sudo rmmod l298n_driver
	@echo "[OK] l298n_driver 제거 완료"

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f $(USER_OBJS) $(TARGET)

.PHONY: all module app load unload clean
