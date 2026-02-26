#ifndef MPU6050_H
#define MPU6050_H

/* =====================================================
 * sysfs 경로 (유저스페이스 main.c에서 사용)
 * ===================================================== */
#define MPU6050_SYSFS_BASE  "/sys/bus/i2c/devices/1-0068"
#define MPU6050_SYSFS_AX    MPU6050_SYSFS_BASE "/accel_x"
#define MPU6050_SYSFS_AY    MPU6050_SYSFS_BASE "/accel_y"
#define MPU6050_SYSFS_AZ    MPU6050_SYSFS_BASE "/accel_z"
#define MPU6050_SYSFS_GX    MPU6050_SYSFS_BASE "/gyro_x"
#define MPU6050_SYSFS_GY    MPU6050_SYSFS_BASE "/gyro_y"
#define MPU6050_SYSFS_GZ    MPU6050_SYSFS_BASE "/gyro_z"
#define MPU6050_SYSFS_CALIB MPU6050_SYSFS_BASE "/calibrate"

/* =====================================================
 * 커널 모듈 전용 정의 (__KERNEL__ 빌드 시에만 포함)
 * ===================================================== */
#ifdef __KERNEL__

#include <linux/types.h>

/* I2C 주소 및 레지스터 */
#define MPU6050_I2C_ADDR          0x68
#define MPU6050_REG_PWR_MGMT_1    0x6B
#define MPU6050_REG_ACCEL_XOUT_H  0x3B
#define MPU6050_REG_GYRO_XOUT_H   0x43
#define MPU6050_REG_WHO_AM_I      0x75
#define MPU6050_WHO_AM_I_VAL      0x68

/* 스케일 상수 */
#define MPU6050_ACCEL_SCALE  16384   /* ±2g,     LSB/g     */
#define MPU6050_GYRO_SCALE   131     /* ±250°/s, LSB/(°/s) */

/* 캘리브레이션 샘플 수 */
#define MPU6050_CALIB_SAMPLES  200

/** 가속도계 데이터 (단위: mg, 1g = 1000mg) */
typedef struct {
    s32 x, y, z;
} mpu6050_accel_t;

/** 자이로스코프 데이터 (단위: m°/s, 1°/s = 1000m°/s) */
typedef struct {
    s32 x, y, z;
} mpu6050_gyro_t;

/** 디바이스 핸들 */
typedef struct {
    struct i2c_client *client;
    s32 gx_bias, gy_bias, gz_bias;
    mpu6050_accel_t accel;
    mpu6050_gyro_t  gyro;
} mpu6050_dev_t;

/* 함수 선언 */
int mpu6050_init(mpu6050_dev_t *dev, struct i2c_client *client);
int mpu6050_calibrate(mpu6050_dev_t *dev);
int mpu6050_read_accel(mpu6050_dev_t *dev, mpu6050_accel_t *accel);
int mpu6050_read_gyro(mpu6050_dev_t *dev, mpu6050_gyro_t *gyro);

#endif /* __KERNEL__ */
#endif /* MPU6050_H */
