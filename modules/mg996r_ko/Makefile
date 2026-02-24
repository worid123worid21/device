# 커널 모듈 빌드 Makefile
#
# 빌드: make
# 로드: sudo insmod mg996r_driver.ko
# 해제: sudo rmmod mg996r_driver
# 로그: dmesg | tail

obj-m += mg996r_driver.o

KDIR    := /lib/modules/$(shell uname -r)/build
PWD     := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

.PHONY: all clean