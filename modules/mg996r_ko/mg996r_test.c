/*
 * mg996r_test.c - mg996r.ko userspace 테스트
 *
 * 빌드: gcc -Wall -O2 -o mg996r_test mg996r_test.c
 * 실행: sudo ./mg996r_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "mg996r.h"

int main(void)
{
    int fd = open(MG996R_DEV_PATH, O_RDWR);
    if (fd < 0) {
        perror("open /dev/mg996r");
        fprintf(stderr, "sudo insmod mg996r_driver.ko 먼저 실행하세요\n");
        return EXIT_FAILURE;
    }

    printf("=== mg996r driver test ===\n");

    // 1. 중앙 복귀
    printf("[1] Center...\n");
    ioctl(fd, MG996R_DO_CENTER);
    sleep(1);

    // 2. Pan 개별 설정
    printf("[2] Pan → 120°\n");
    int angle = 120;
    ioctl(fd, MG996R_SET_PAN, &angle);
    sleep(1);

    // 3. Tilt 개별 설정
    printf("[3] Tilt → 60°\n");
    angle = 60;
    ioctl(fd, MG996R_SET_TILT, &angle);
    sleep(1);

    // 4. Pan/Tilt 동시 설정
    printf("[4] Both → Pan:90°  Tilt:90°\n");
    struct mg996r_angle both = {.pan = 90, .tilt = 90};
    ioctl(fd, MG996R_SET_BOTH, &both);
    sleep(1);

    // 5. 현재 각도 읽기
    int pan_now, tilt_now;
    ioctl(fd, MG996R_GET_PAN,  &pan_now);
    ioctl(fd, MG996R_GET_TILT, &tilt_now);
    printf("[5] Current → Pan:%d°  Tilt:%d°\n", pan_now, tilt_now);

    // 6. 중앙 복귀
    printf("[6] Center...\n");
    ioctl(fd, MG996R_DO_CENTER);

    close(fd);
    printf("Done\n");
    return EXIT_SUCCESS;
}