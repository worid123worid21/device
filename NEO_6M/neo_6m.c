#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

#define GPS_SERIAL "/dev/serial0"  // UART0 (GPIO14/15, 물리핀 8/10)
#define BAUDRATE B9600

// NMEA 형식(DDMM.MMMM) → 도(decimal degrees) 변환
double nmea_to_decimal(const char *nmea, char direction) {
    if (!nmea || strlen(nmea) < 4) return 0.0;
    double val = atof(nmea);
    int deg = (int)(val / 100);
    double min = val - deg * 100;
    double dec = deg + min / 60.0;
    if (direction == 'S' || direction == 'W') dec = -dec;
    return dec;
}

int main() {
    // UART0 열기 (GPIO14=TX, GPIO15=RX / 물리핀 8/10)
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

    char buf[512];
    char line[128];
    int line_idx = 0;

    printf("Waiting for GPS fix...\n");

    while (1) {
        int n = read(fd, buf, sizeof(buf));
        if (n > 0) {
            for (int i = 0; i < n; i++) {
                char c = buf[i];
                if (c == '\n') { // 한 줄 끝
                    line[line_idx] = '\0';
                    line_idx = 0;

                    if (strncmp(line, "$GPGGA", 6) == 0) {
                        // GPGGA 파싱
                        char *token = strtok(line, ",");
                        int field = 0;
                        char lat[16] = {0}, lon[16] = {0};
                        char ns = 'N', ew = 'E';

                        while (token) {
                            field++;
                            switch (field) {
                                case 3: strcpy(lat, token); break; // 위도
                                case 4: ns = token[0]; break;      // N/S
                                case 5: strcpy(lon, token); break; // 경도
                                case 6: ew = token[0]; break;      // E/W
                            }
                            token = strtok(NULL, ",");
                        }

                        double latitude = nmea_to_decimal(lat, ns);
                        double longitude = nmea_to_decimal(lon, ew);
                        if (latitude != 0 && longitude != 0)
                            printf("Latitude: %.6f, Longitude: %.6f\n", latitude, longitude);
                    }

                    memset(line, 0, sizeof(line));
                } else if (c != '\r') {
                    line[line_idx++] = c;
                    if (line_idx >= sizeof(line)-1) line_idx = sizeof(line)-2;
                }
            }
        }
        usleep(100000); // 0.1초
    }

    close(fd);
    return 0;
}