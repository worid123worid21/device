// SPDX-License-Identifier: GPL-2.0
/*
 * mpu6050_driver.c  -  MPU-6050 (GY-521) I2C 커널 드라이버
 *
 * 기능:
 *   - 가속도계 (±2g)       → sysfs: accel_x / accel_y / accel_z  (단위: mg)
 *   - 자이로스코프 (±250°/s) → sysfs: gyro_x  / gyro_y  / gyro_z   (단위: m°/s)
 *   - 자이로 바이어스 캘리브레이션 → sysfs: calibrate (write "1")
 *
 * 대상: Linux 6.12.62+rpt-rpi-v8  (Raspberry Pi AArch64)
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/device.h>

#include "mpu6050.h"

/* =====================================================
 * 내부 I2C 유틸리티
 * ===================================================== */

static int _write_reg(struct i2c_client *client, u8 reg, u8 val)
{
    u8 buf[2] = { reg, val };
    int ret = i2c_master_send(client, buf, 2);
    if (ret < 0)
        dev_err(&client->dev, "write reg 0x%02X failed: %d\n", reg, ret);
    return (ret < 0) ? ret : 0;
}

static int _read_reg(struct i2c_client *client, u8 reg, u8 *buf, int len)
{
    int ret;
    ret = i2c_master_send(client, &reg, 1);
    if (ret < 0) {
        dev_err(&client->dev, "send reg 0x%02X failed: %d\n", reg, ret);
        return ret;
    }
    ret = i2c_master_recv(client, buf, len);
    if (ret < 0)
        dev_err(&client->dev, "recv reg 0x%02X failed: %d\n", reg, ret);
    return (ret < 0) ? ret : 0;
}

static s16 _read_word(struct i2c_client *client, u8 reg)
{
    u8 buf[2] = {0};
    _read_reg(client, reg, buf, 2);
    return (s16)((buf[0] << 8) | buf[1]);
}

/* =====================================================
 * 공개 API 구현
 * ===================================================== */

int mpu6050_init(mpu6050_dev_t *dev, struct i2c_client *client)
{
    u8  who_am_i = 0;
    int ret;

    dev->client  = client;
    dev->gx_bias = 0;
    dev->gy_bias = 0;
    dev->gz_bias = 0;

    ret = _read_reg(client, MPU6050_REG_WHO_AM_I, &who_am_i, 1);
    if (ret < 0)
        return ret;

    if (who_am_i != MPU6050_WHO_AM_I_VAL) {
        dev_err(&client->dev, "WHO_AM_I mismatch: got 0x%02X, expected 0x%02X\n",
                who_am_i, MPU6050_WHO_AM_I_VAL);
        return -ENODEV;
    }

    ret = _write_reg(client, MPU6050_REG_PWR_MGMT_1, 0x00); /* Wake up */
    if (ret < 0)
        return ret;

    msleep(100); /* 안정화 대기 */

    dev_info(&client->dev, "MPU-6050 init OK (WHO_AM_I=0x%02X)\n", who_am_i);
    return 0;
}

int mpu6050_calibrate(mpu6050_dev_t *dev)
{
    int  i;
    s64  gx_sum = 0, gy_sum = 0, gz_sum = 0;

    dev_info(&dev->client->dev,
             "Calibrating gyro (%d samples)... keep sensor still\n",
             MPU6050_CALIB_SAMPLES);

    for (i = 0; i < MPU6050_CALIB_SAMPLES; i++) {
        gx_sum += _read_word(dev->client, MPU6050_REG_GYRO_XOUT_H);
        gy_sum += _read_word(dev->client, MPU6050_REG_GYRO_XOUT_H + 2);
        gz_sum += _read_word(dev->client, MPU6050_REG_GYRO_XOUT_H + 4);
        msleep(10);
    }

    dev->gx_bias = (s32)(gx_sum / MPU6050_CALIB_SAMPLES);
    dev->gy_bias = (s32)(gy_sum / MPU6050_CALIB_SAMPLES);
    dev->gz_bias = (s32)(gz_sum / MPU6050_CALIB_SAMPLES);

    dev_info(&dev->client->dev,
             "Gyro bias  X:%d  Y:%d  Z:%d\n",
             dev->gx_bias, dev->gy_bias, dev->gz_bias);
    return 0;
}

int mpu6050_read_accel(mpu6050_dev_t *dev, mpu6050_accel_t *accel)
{
    s16 rx = _read_word(dev->client, MPU6050_REG_ACCEL_XOUT_H);
    s16 ry = _read_word(dev->client, MPU6050_REG_ACCEL_XOUT_H + 2);
    s16 rz = _read_word(dev->client, MPU6050_REG_ACCEL_XOUT_H + 4);

    accel->x = (s32)rx * 1000 / MPU6050_ACCEL_SCALE;
    accel->y = (s32)ry * 1000 / MPU6050_ACCEL_SCALE;
    accel->z = (s32)rz * 1000 / MPU6050_ACCEL_SCALE;

    dev->accel = *accel;
    return 0;
}

int mpu6050_read_gyro(mpu6050_dev_t *dev, mpu6050_gyro_t *gyro)
{
    s16 rx = _read_word(dev->client, MPU6050_REG_GYRO_XOUT_H)     - (s16)dev->gx_bias;
    s16 ry = _read_word(dev->client, MPU6050_REG_GYRO_XOUT_H + 2) - (s16)dev->gy_bias;
    s16 rz = _read_word(dev->client, MPU6050_REG_GYRO_XOUT_H + 4) - (s16)dev->gz_bias;

    gyro->x = (s32)rx * 1000 / MPU6050_GYRO_SCALE;
    gyro->y = (s32)ry * 1000 / MPU6050_GYRO_SCALE;
    gyro->z = (s32)rz * 1000 / MPU6050_GYRO_SCALE;

    dev->gyro = *gyro;
    return 0;
}

/* =====================================================
 * sysfs 속성
 * ===================================================== */

#define ACCEL_SHOW(AXIS, axis) \
static ssize_t accel_##axis##_show(struct device *d,                    \
                                    struct device_attribute *attr,       \
                                    char *buf)                           \
{                                                                        \
    mpu6050_dev_t *dev = i2c_get_clientdata(to_i2c_client(d));          \
    mpu6050_accel_t a;                                                   \
    mpu6050_read_accel(dev, &a);                                         \
    return sysfs_emit(buf, "%d\n", a.axis);                             \
}                                                                        \
static DEVICE_ATTR_RO(accel_##axis)

#define GYRO_SHOW(AXIS, axis) \
static ssize_t gyro_##axis##_show(struct device *d,                     \
                                   struct device_attribute *attr,        \
                                   char *buf)                            \
{                                                                        \
    mpu6050_dev_t *dev = i2c_get_clientdata(to_i2c_client(d));          \
    mpu6050_gyro_t g;                                                    \
    mpu6050_read_gyro(dev, &g);                                          \
    return sysfs_emit(buf, "%d\n", g.axis);                             \
}                                                                        \
static DEVICE_ATTR_RO(gyro_##axis)

ACCEL_SHOW(X, x);
ACCEL_SHOW(Y, y);
ACCEL_SHOW(Z, z);
GYRO_SHOW(X, x);
GYRO_SHOW(Y, y);
GYRO_SHOW(Z, z);

static ssize_t calibrate_store(struct device *d, struct device_attribute *attr,
                                const char *buf, size_t count)
{
    mpu6050_dev_t *dev = i2c_get_clientdata(to_i2c_client(d));
    mpu6050_calibrate(dev);
    return count;
}
static DEVICE_ATTR_WO(calibrate);

static struct attribute *mpu6050_attrs[] = {
    &dev_attr_accel_x.attr,
    &dev_attr_accel_y.attr,
    &dev_attr_accel_z.attr,
    &dev_attr_gyro_x.attr,
    &dev_attr_gyro_y.attr,
    &dev_attr_gyro_z.attr,
    &dev_attr_calibrate.attr,
    NULL,
};

static const struct attribute_group mpu6050_attr_group = {
    .attrs = mpu6050_attrs,
};

/* =====================================================
 * probe / remove
 * ===================================================== */

static int mpu6050_probe(struct i2c_client *client)
{
    mpu6050_dev_t *dev;
    int ret;

    dev = devm_kzalloc(&client->dev, sizeof(*dev), GFP_KERNEL);
    if (!dev)
        return -ENOMEM;

    i2c_set_clientdata(client, dev);

    ret = mpu6050_init(dev, client);
    if (ret < 0)
        return ret;

    ret = mpu6050_calibrate(dev);
    if (ret < 0)
        return ret;

    ret = sysfs_create_group(&client->dev.kobj, &mpu6050_attr_group);
    if (ret < 0) {
        dev_err(&client->dev, "sysfs_create_group failed: %d\n", ret);
        return ret;
    }

    dev_info(&client->dev, "MPU-6050 driver loaded\n");
    return 0;
}

static void mpu6050_remove(struct i2c_client *client)
{
    sysfs_remove_group(&client->dev.kobj, &mpu6050_attr_group);
    dev_info(&client->dev, "MPU-6050 driver unloaded\n");
}

/* =====================================================
 * ID 테이블 / Device Tree / 드라이버 등록
 * ===================================================== */

static const struct i2c_device_id mpu6050_id[] = {
    { "mpu_6050", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, mpu6050_id);

static const struct of_device_id mpu6050_of_match[] = {
    { .compatible = "invensense,mpu6050" },
    { }
};
MODULE_DEVICE_TABLE(of, mpu6050_of_match);

static struct i2c_driver mpu6050_driver = {
    .driver = {
        .name           = "mpu_6050",
        .of_match_table = mpu6050_of_match,
    },
    .probe    = mpu6050_probe,
    .remove   = mpu6050_remove,
    .id_table = mpu6050_id,
};

module_i2c_driver(mpu6050_driver);

MODULE_AUTHOR("GANGHEE");
MODULE_DESCRIPTION("MPU-6050 (GY-521) I2C Kernel Driver");
MODULE_LICENSE("GPL v2");
