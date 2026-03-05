// File: nrf24_server.c
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <dirent.h>

#define CE_GPIO 8
#define SPI_DEVICE "/dev/spidev0.1"
#define MAX_PAYLOAD 32

// sysfs GPIO 제어
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
void gpio_set(int gpio,int val) {
    char path[50];
    snprintf(path,sizeof(path),"/sys/class/gpio/gpio%d/value",gpio);
    int fd = open(path,O_WRONLY);
    if(val) write(fd,"1",1); else write(fd,"0",1);
    close(fd);
}

// SPI 초기화
int spi_init(const char *dev) {
    int fd = open(dev,O_RDWR);
    if(fd < 0) { perror("SPI open"); return -1; }
    uint8_t mode = SPI_MODE_0; uint32_t speed = 1000000;
    ioctl(fd,SPI_IOC_WR_MODE,&mode);
    ioctl(fd,SPI_IOC_WR_MAX_SPEED_HZ,&speed);
    return fd;
}
int spi_transfer(int fd,uint8_t *tx,uint8_t *rx,int len) {
    struct spi_ioc_transfer tr={0};
    tr.tx_buf = (unsigned long)tx;
    tr.rx_buf = (unsigned long)rx;
    tr.len = len;
    tr.speed_hz = 1000000;
    tr.bits_per_word = 8;
    return ioctl(fd,SPI_IOC_MESSAGE(1),&tr);
}

// 프로토콜 CMD
#define CMD_LIST 0x01
#define CMD_GET  0x02
#define CMD_DATA 0x03
#define CMD_ACK  0x04

// 파일 목록 chunk 전송
void send_file_list(int spi_fd, const char *dir_path) {
    DIR *d = opendir(dir_path);
    struct dirent *entry;
    char buf[MAX_PAYLOAD];
    if(!d) return;
    buf[0] = CMD_DATA;
    while((entry=readdir(d))!=NULL) {
        if(entry->d_name[0]=='.') continue; // skip hidden
        size_t len = strlen(entry->d_name);
        if(len>MAX_PAYLOAD-1) len = MAX_PAYLOAD-1;
        buf[1] = len;
        memcpy(&buf[2], entry->d_name, len);
        uint8_t rx[MAX_PAYLOAD]={0};
        spi_transfer(spi_fd,(uint8_t*)buf,rx,len+2);
        // ACK 무시 최소 예제
    }
    closedir(d);
}

// 파일 chunk 전송
void send_file(int spi_fd, const char *filename) {
    FILE *f = fopen(filename,"rb");
    if(!f) return;
    uint8_t buf[MAX_PAYLOAD];
    buf[0] = CMD_DATA;
    size_t n;
    while((n=fread(buf+2,1,MAX_PAYLOAD-2,f))>0) {
        buf[1] = n;
        uint8_t rx[MAX_PAYLOAD]={0};
        spi_transfer(spi_fd,buf,rx,n+2);
        // ACK 무시 최소 예제
    }
    fclose(f);
}

int main() {
    gpio_export(CE_GPIO);
    gpio_direction_out(CE_GPIO);
    gpio_set(CE_GPIO,0);

    int spi_fd = spi_init(SPI_DEVICE);
    if(spi_fd<0) return -1;

    printf("Raspberry Pi nRF24 server ready\n");

    uint8_t rx_buf[MAX_PAYLOAD]={0};
    uint8_t tx_buf[MAX_PAYLOAD]={0};
    while(1) {
        // 최소 예제: RX polling
        // 실제는 nRF24 RX FIFO 확인 후 수신
        usleep(100000);
        // 가정: rx_buf[0] CMD
        // 예시: 하드코딩 LIST 요청
        send_file_list(spi_fd,"/home/pi");
        // 예시: GET 요청 하드코딩
        send_file(spi_fd,"/home/pi/test.txt");
        break; // 최소 테스트용 한 번 전송
    }

    close(spi_fd);
    return 0;
}
