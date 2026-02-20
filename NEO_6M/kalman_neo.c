// 2D Kalman Filter + Offset Correction GPS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <math.h>

#define GPS_SERIAL "/dev/serial0"
#define BAUDRATE B9600

// ====== 기준점 오프셋 (필요시 수정) ======
#define LAT_OFFSET  (-0.000220)
#define LON_OFFSET  (0.000000)

// ====== Kalman 파라미터 ======
#define Q 1e-7   // 프로세스 노이즈
#define R 1e-6   // 측정 노이즈

typedef struct {
    double x;  // 상태
    double p;  // 오차 공분산
} Kalman;

// Kalman 초기화
void kalman_init(Kalman *k, double init_value) {
    k->x = init_value;
    k->p = 1.0;
}

// Kalman 업데이트
double kalman_update(Kalman *k, double measurement) {
    // 예측
    k->p += Q;

    // 이득 계산
    double K = k->p / (k->p + R);

    // 업데이트
    k->x = k->x + K * (measurement - k->x);
    k->p = (1 - K) * k->p;

    return k->x;
}

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
void calc_offset(double lat1, double lon1,
                 double lat2, double lon2,
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

    Kalman k_lat, k_lon;
    int kalman_initialized = 0;

    printf("Waiting GPS (Kalman Mode)...\n");

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

                        double raw_lat = nmea_to_decimal(lat, ns);
                        double raw_lon = nmea_to_decimal(lon, ew);

                        if (!kalman_initialized) {
                            kalman_init(&k_lat, raw_lat);
                            kalman_init(&k_lon, raw_lon);
                            kalman_initialized = 1;
                        }

                        double filtered_lat = kalman_update(&k_lat, raw_lat);
                        double filtered_lon = kalman_update(&k_lon, raw_lon);

                        // 오프셋 보정
                        double corrected_lat = filtered_lat + LAT_OFFSET;
                        double corrected_lon = filtered_lon + LON_OFFSET;

                        // 오차 계산
                        double north_m, east_m;
                        calc_offset(corrected_lat, corrected_lon,
                                    raw_lat, raw_lon,
                                    &north_m, &east_m);

                        double dist = sqrt(north_m*north_m + east_m*east_m);

                        printf("Raw: %.6f, %.6f\n", raw_lat, raw_lon);
                        printf("Filtered: %.6f, %.6f\n", filtered_lat, filtered_lon);
                        printf("Corrected: %.6f, %.6f\n", corrected_lat, corrected_lon);
                        printf("Filter Offset: %.2fm\n", dist);
                        printf("Map: https://www.google.com/maps?q=%.6f,%.6f\n\n",
                               corrected_lat, corrected_lon);
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

