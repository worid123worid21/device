#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

#define GPS_SERIAL "/dev/serial0"
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
                if (c == '\n') {
                    line[line_idx] = '\0';
                    line_idx = 0;

                    if (strncmp(line, "$GPGGA", 6) == 0 || strncmp(line, "$GPRMC", 6) == 0) {
                        char *token = strtok(line, ",");
                        int field = 0;
                        char lat[16] = {0}, lon[16] = {0};
                        char ns = 'N', ew = 'E';
                        int fix = 0;

                        while (token) {
                            field++;
                            if (strncmp(line, "$GPGGA", 6) == 0) {
                                switch (field) {
                                    case 2: strcpy(lat, token); break;  // 위도
                                    case 3: ns = token[0]; break;       // N/S
                                    case 4: strcpy(lon, token); break;  // 경도
                                    case 5: ew = token[0]; break;       // E/W
                                    case 6: fix = atoi(token); break;   // fix
                                }
                            } else if (strncmp(line, "$GPRMC", 6) == 0) {
                                if (field == 2) strcpy(lat, token);
                                if (field == 3) ns = token[0];
                                if (field == 4) strcpy(lon, token);
                                if (field == 5) ew = token[0];
                                if (field == 3) fix = (token[0] == 'A') ? 1 : 0;  // A=fix, V=no fix
                            }
                            token = strtok(NULL, ",");
                        }

                        if (fix) {
                            double latitude = nmea_to_decimal(lat, ns);
                            double longitude = nmea_to_decimal(lon, ew);
                            printf("Latitude: %.6f, Longitude: %.6f\n", latitude, longitude);
                        } else {
                            printf("Waiting for GPS fix...\n");
                        }
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