#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/time.h>

#define GPS_SERIAL "/dev/serial0"
#define BAUDRATE B9600

int main() {
    int fd = open(GPS_SERIAL, O_RDONLY | O_NOCTTY);
    if (fd == -1) {
        perror("Unable to open serial port");
        return 1;
    }

    struct termios options;
    tcgetattr(fd, &options);
    cfsetispeed(&options, BAUDRATE);
    cfsetospeed(&options, BAUDRATE);
    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    tcsetattr(fd, TCSANOW, &options);

    char buf[512], line[128];
    int line_idx = 0;
    int count = 0;

    struct timeval start_time, current_time;
    gettimeofday(&start_time, NULL);

    while (1) {
        int n = read(fd, buf, sizeof(buf));
        if (n > 0) {
            for (int i = 0; i < n; i++) {
                char c = buf[i];
                if (c == '\n') {
                    line[line_idx] = '\0';
                    line_idx = 0;

                    if (strncmp(line, "$GPGGA", 6) == 0) {
                        count++; // GPS 위치 수신 카운트
                    }

                    memset(line, 0, sizeof(line));
                } else if (c != '\r') {
                    line[line_idx++] = c;
                    if (line_idx >= sizeof(line)-1) line_idx = sizeof(line)-2;
                }
            }
        }

        gettimeofday(&current_time, NULL);
        double elapsed = (current_time.tv_sec - start_time.tv_sec) + 
                         (current_time.tv_usec - start_time.tv_usec) / 1000000.0;

        if (elapsed >= 1.0) { // 매 1초마다 출력
            printf("GPS positions received per second: %d\n", count);
            count = 0;
            start_time = current_time;
        }
    }

    close(fd);
    return 0;
}

