#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <stdlib.h>

#define CE_GPIO 8
#define SPI_DEVICE "/dev/spidev0.1"

// GPIO sysfs 제어
void gpio_export(int gpio) {
    int fd = open("/sys/class/gpio/export", O_WRONLY);
    char buf[4]; int len = snprintf(buf,sizeof(buf),"%d",gpio);
    write(fd, buf, len); close(fd);
}

void gpio_direction_out(int gpio) {
    char path[50];
    snprintf(path,sizeof(path),"/sys/class/gpio/gpio%d/direction",gpio);
    int fd = open(path,O_WRONLY);
    write(fd,"out",3); close(fd);
}

void gpio_set(int gpio,int value) {
    char path[50];
    snprintf(path,sizeof(path),"/sys/class/gpio/gpio%d/value",gpio);
    int fd = open(path,O_WRONLY);
    if(value) write(fd,"1",1);
    else write(fd,"0",1);
    close(fd);
}

// SPI 초기화
int spi_init(const char *dev) {
    int fd = open(dev,O_RDWR);
    if(fd < 0) { perror("SPI open"); return -1; }

    uint8_t mode = SPI_MODE_0;
    uint32_t speed = 1000000; // 1MHz
    ioctl(fd,SPI_IOC_WR_MODE,&mode);
    ioctl(fd,SPI_IOC_WR_MAX_SPEED_HZ,&speed);
    return fd;
}

// SPI 전송
int spi_transfer(int fd,uint8_t *tx,uint8_t *rx,int len) {
    struct spi_ioc_transfer tr = {0};
    tr.tx_buf = (unsigned long)tx;
    tr.rx_buf = (unsigned long)rx;
    tr.len = len;
    tr.speed_hz = 1000000;
    tr.bits_per_word = 8;
    return ioctl(fd,SPI_IOC_MESSAGE(1),&tr);
}

// nRF24 명령
#define W_REGISTER 0x20
#define R_REGISTER 0x00
#define CONFIG 0x00

int main() {
    // CE GPIO 초기화
    gpio_export(CE_GPIO);
    gpio_direction_out(CE_GPIO);
    gpio_set(CE_GPIO,0);

    // SPI 초기화
    int spi_fd = spi_init(SPI_DEVICE);
    if(spi_fd < 0) return -1;

    printf("SPI and CE ready\n");

    // CONFIG 레지스터 설정: PWR_UP=1, PRIM_TX=1
    uint8_t tx_buf[2] = {W_REGISTER | CONFIG, 0x0A}; // 예: 00001010
    uint8_t rx_buf[2] = {0};

    spi_transfer(spi_fd,tx_buf,rx_buf,2);
    printf("CONFIG register written: 0x%02X\n",tx_buf[1]);

    // CE HIGH → 송신 시작
    gpio_set(CE_GPIO,1);
    usleep(150);  // 최소 10us 이상
    gpio_set(CE_GPIO,0);
    printf("CE pulse done (TX)\n");

    close(spi_fd);
    return 0;
}
