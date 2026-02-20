// GPS 안정화 + 오프셋 보정 버전

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <math.h>

#define GPS_SERIAL "/dev/serial0"
#define BAUDRATE B9600
#define QUEUE_SIZE 20

// ===== 기준점 오프셋 (필요 시 사용) =====
#define LAT_OFFSET  (-0.000561)   // 필요 없으면 0.0
#define LON_OFFSET  (-0.000008)

typedef struct {
    double lat;
    double lon;
} Position;

// NMEA → Decimal
double nmea_to_decimal(const char *nmea, char direction) {
    if (!nmea || strlen(nmea) < 4) return 0.0;

    double val = atof(nmea);
    int deg = (int)(val / 100);
    double min = val - deg * 100;
    double dec = deg + min / 60.0;

    if (direction == 'S' || direction == 'W')
        dec = -dec;

    return dec;
}

// 거리 계산
void calc_offset(double lat1, double lon1, double lat2, double lon2,
                 double *north_m, double *east_m)
{
    *north_m = (lat1 - lat2) * 111320.0;
    *east_m  = (lon1 - lon2) * 111320.0 * cos(lat1 * M_PI / 180.0);
}

int main() {

    int fd = open(GPS_SERIAL, O_RDONLY | O_NOCTTY);
    if (fd == -1) {
        perror("Serial open error");
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

    Position queue[QUEUE_SIZE];
    int queue_idx = 0;
    int queue_count = 0;

    printf("Waiting GPS...\n");

    while (1) {

        int n = read(fd, buf, sizeof(buf));
        if (n <= 0) continue;

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
                    int fix = 0;

                    while (token) {
                        field++;
                        switch (field) {
                            case 3: strcpy(lat, token); break;
                            case 4: ns = token[0]; break;
                            case 5: strcpy(lon, token); break;
                            case 6: ew = token[0]; break;
                            case 7: fix = atoi(token); break;
                        }
                        token = strtok(NULL, ",");
                    }

                    if (fix > 0) {

                        Position p;
                        p.lat = nmea_to_decimal(lat, ns);
                        p.lon = nmea_to_decimal(lon, ew);

                        // 큐 저장
                        queue[queue_idx] = p;
                        queue_idx = (queue_idx + 1) % QUEUE_SIZE;
                        if (queue_count < QUEUE_SIZE)
                            queue_count++;

                        // 평균 계산
                        double lat_sum = 0.0, lon_sum = 0.0;
                        for (int j = 0; j < queue_count; j++) {
                            lat_sum += queue[j].lat;
                            lon_sum += queue[j].lon;
                        }

                        double lat_avg = lat_sum / queue_count;
                        double lon_avg = lon_sum / queue_count;

                        // ===== 오프셋 보정 =====
                        double lat_corrected = lat_avg + LAT_OFFSET;
                        double lon_corrected = lon_avg + LON_OFFSET;

                        // ===== 평균 대비 오차 =====
                        double north_m, east_m;
                        calc_offset(lat_corrected, lon_corrected,
                                    lat_avg, lon_avg,
                                    &north_m, &east_m);

                        double dist = sqrt(north_m*north_m + east_m*east_m);

                        printf("Avg: %.6f, %.6f | Corrected: %.6f, %.6f | Offset: %.2fm\n",
                               lat_avg, lon_avg,
                               lat_corrected, lon_corrected,
                               dist);

                        printf("Map: https://www.google.com/maps?q=%.6f,%.6f\n\n",
                               lat_corrected, lon_corrected);
                    }
                }

                memset(line, 0, sizeof(line));

            } else if (c != '\r') {

                if (line_idx < sizeof(line)-1)
                    line[line_idx++] = c;
            }
        }

        usleep(100000);
    }

    close(fd);
    return 0;
}

