//오차 개선1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <math.h>

#define GPS_SERIAL "/dev/serial0"
#define BAUDRATE B9600
#define QUEUE_SIZE 20  // 큐 개수

// NMEA DDMM.MMMM → Decimal Degrees 변환
double nmea_to_decimal(const char *nmea, char direction) {
    if (!nmea || strlen(nmea) < 4) return 0.0;
    double val = atof(nmea);
    int deg = (int)(val / 100);
    double min = val - deg * 100;
    double dec = deg + min / 60.0;
    if (direction == 'S' || direction == 'W') dec = -dec;
    return dec;
}

// ΔN, ΔE, 거리 계산
void calc_offset(double lat1, double lon1, double lat2, double lon2, double *north_m, double *east_m) {
    *north_m = (lat1 - lat2) * 111320.0; // 위도 1도 ≈ 111.32 km
    *east_m = (lon1 - lon2) * 111320.0 * cos(lat1 * M_PI / 180.0); // 경도
}

int main() {
    int fd = open(GPS_SERIAL, O_RDONLY | O_NOCTTY);
    if (fd == -1) { perror("Unable to open serial port"); return 1; }

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

    double lat_queue[QUEUE_SIZE] = {0}, lon_queue[QUEUE_SIZE] = {0};
    int queue_idx = 0, queue_count = 0;

    // 이전 평균값 저장 (초기값 0)
    double prev_lat_avg = 0.0, prev_lon_avg = 0.0;

    printf("Waiting for GPS fix...\n");

    while (1) {
        int n = read(fd, buf, sizeof(buf));
        if (n > 0) {
            for (int i = 0; i < n; i++) {
                char c = buf[i];
                if (c == '\n') {
                    line[line_idx] = '\0';
                    line_idx = 0;

                    if (strncmp(line, "$GPGGA", 6) == 0) {
                        char *token = strtok(line, ",");
                        int field = 0;
                        char lat[16] = {0}, lon[16] = {0};
                        char ns = 'N', ew = 'E';
                        int fix_quality = 0;

                        while (token) {
                            field++;
                            switch (field) {
                                case 3: strcpy(lat, token); break;
                                case 4: ns = token[0]; break;
                                case 5: strcpy(lon, token); break;
                                case 6: ew = token[0]; break;
                                case 7: fix_quality = atoi(token); break;
                            }
                            token = strtok(NULL, ",");
                        }

                        if (fix_quality > 0) {
                            double latitude = nmea_to_decimal(lat, ns);
                            double longitude = nmea_to_decimal(lon, ew);

                            // 원형 큐에 저장
                            lat_queue[queue_idx] = latitude;
                            lon_queue[queue_idx] = longitude;
                            queue_idx = (queue_idx + 1) % QUEUE_SIZE;
                            if (queue_count < QUEUE_SIZE) queue_count++;

                            // 큐에 있는 값 개수만큼 평균 계산
                            double lat_sum = 0, lon_sum = 0;
                            for (int j = 0; j < queue_count; j++) {
                                lat_sum += lat_queue[j];
                                lon_sum += lon_queue[j];
                            }
                            double lat_avg = lat_sum / queue_count;
                            double lon_avg = lon_sum / queue_count;

                            // Δ 계산 (평균값 - 이전 평균값)
                            double delta_lat = lat_avg - prev_lat_avg;
                            double delta_lon = lon_avg - prev_lon_avg;

                            // 보정: 평균값에서 Δ를 더해줌 (이동 시 반영)
                            double lat_corrected = lat_avg + delta_lat;
                            double lon_corrected = lon_avg + delta_lon;

                            // ΔN, ΔE, 거리 계산 (이전 평균 기준)
                            double north_m, east_m;
                            calc_offset(lat_corrected, lon_corrected, lat_avg, lon_avg, &north_m, &east_m);
                            double distance = sqrt(north_m*north_m + east_m*east_m);

                            printf("Raw Avg: %.6f, %.6f | Corrected Avg: %.6f, %.6f | ΔN: %.1fm, ΔE: %.1fm, Distance: %.1fm | Queue: %d\n",
                                   lat_avg, lon_avg, lat_corrected, lon_corrected, north_m, east_m, distance, queue_count);

                            printf("Map Link: https://www.google.com/maps/dir/?api=1&origin=%.6f,%.6f&destination=%.6f,%.6f\n\n",
                                   lat_avg, lon_avg, lat_corrected, lon_corrected);

                            // 이전 평균값 갱신
                            prev_lat_avg = lat_avg;
                            prev_lon_avg = lon_avg;
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