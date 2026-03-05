#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#define CE_GPIO 8

int gpio_export(int gpio) {
    int fd = open("/sys/class/gpio/export", O_WRONLY);
    if(fd < 0) return -1;
    char buf[4];
    int len = snprintf(buf, sizeof(buf), "%d", gpio);
    write(fd, buf, len);
    close(fd);
    return 0;
}

int gpio_direction_out(int gpio) {
    char path[50];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", gpio);
    int fd = open(path, O_WRONLY);
    if(fd < 0) return -1;
    write(fd, "out", 3);
    close(fd);
    return 0;
}

int gpio_set_value(int gpio, int value) {
    char path[50];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", gpio);
    int fd = open(path, O_WRONLY);
    if(fd < 0) return -1;
    if(value) write(fd, "1", 1);
    else write(fd, "0", 1);
    close(fd);
    return 0;
}

int main() {
    gpio_export(CE_GPIO);
    gpio_direction_out(CE_GPIO);

    printf("CE HIGH\n");
    gpio_set_value(CE_GPIO, 1);
    usleep(100000);
    printf("CE LOW\n");
    gpio_set_value(CE_GPIO, 0);

    return 0;
}