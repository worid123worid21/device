# ─────────────────────────────────────────────
# MG996R 커널 모듈 + 유저 프로그램 빌드 Makefile
# ─────────────────────────────────────────────

# 커널 모듈
obj-m += mg996r_driver.o

# 유저 프로그램
USER_PROG = mg996r_main
USER_SRCS = main.c

KDIR := /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

# ─────────────────────────────────────────────
all: kernel user

# ── 커널 모듈 빌드 ───────────────────────────
kernel:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

# ── 유저 프로그램 빌드 ───────────────────────
user:
	gcc -o $(USER_PROG) $(USER_SRCS) -lm

# ── 전체 클린 ───────────────────────────────
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f $(USER_PROG)

.PHONY: all kernel user clean