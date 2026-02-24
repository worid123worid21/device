/*
 * mg996r_driver.c - MG996R Pan/Tilt 서보 커널 모듈
 *
 * PWM 제어: 커널에서 sysfs 파일 직접 제어
 *   /sys/class/pwm/pwmchip0/pwm0/ (Pan  - GPIO18)
 *   /sys/class/pwm/pwmchip0/pwm1/ (Tilt - GPIO19)
 *
 * 빌드: make
 * 로드: sudo insmod mg996r_driver.ko
 * 해제: sudo rmmod mg996r_driver
 * 확인: ls /dev/mg996r  /  dmesg | tail
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include "mg996r.h"

// ─────────────────────────────────────────────
//  sysfs PWM 경로
// ─────────────────────────────────────────────
#define SYSFS_PWM_BASE      "/sys/class/pwm/pwmchip0"
#define PWM_PERIOD_NS       20000000    // 20ms (50Hz)
#define PWM_DUTY_MIN_NS     500000      // 0.5ms →   0°
#define PWM_DUTY_MAX_NS     2500000     // 2.5ms → 180°

// ─────────────────────────────────────────────
//  드라이버 내부 상태
// ─────────────────────────────────────────────
struct mg996r_dev {
    int          pan_angle;
    int          tilt_angle;
    struct mutex lock;
    struct cdev  cdev;
    dev_t        devno;
    struct class *class;
    struct device *device;
};

static struct mg996r_dev *g_dev;

// ─────────────────────────────────────────────
//  커널에서 sysfs 파일 쓰기
// ─────────────────────────────────────────────
static int sysfs_write(const char *path, const char *val)
{
    struct file *f;
    loff_t      pos = 0;
    int         ret;

    f = filp_open(path, O_WRONLY, 0);
    if (IS_ERR(f)) {
        pr_err("mg996r: filp_open failed: %s (%ld)\n", path, PTR_ERR(f));
        return PTR_ERR(f);
    }

    ret = kernel_write(f, val, strlen(val), &pos);
    filp_close(f, NULL);

    if (ret < 0) {
        pr_err("mg996r: kernel_write failed: %s (%d)\n", path, ret);
        return ret;
    }
    return 0;
}

// ─────────────────────────────────────────────
//  내부 헬퍼
// ─────────────────────────────────────────────
static int angle_to_duty_ns(int angle)
{
    return PWM_DUTY_MIN_NS +
           (int)(((long)angle * (PWM_DUTY_MAX_NS - PWM_DUTY_MIN_NS)) / 180);
}

static int clamp_pan(int a)
{
    if (a < MG996R_PAN_MIN)  return MG996R_PAN_MIN;
    if (a > MG996R_PAN_MAX)  return MG996R_PAN_MAX;
    return a;
}

static int clamp_tilt(int a)
{
    if (a < MG996R_TILT_MIN) return MG996R_TILT_MIN;
    if (a > MG996R_TILT_MAX) return MG996R_TILT_MAX;
    return a;
}

// ─────────────────────────────────────────────
//  PWM 채널 초기화
//  export → period → duty → enable
// ─────────────────────────────────────────────
static int pwm_ch_init(int ch, int angle)
{
    char path[128], val[32];
    int  ret;

    // 1. export (이미 된 경우 무시)
    snprintf(path, sizeof(path), SYSFS_PWM_BASE "/export");
    snprintf(val,  sizeof(val),  "%d", ch);
    sysfs_write(path, val);     // EBUSY 무시
    msleep(100);

    // 2. period
    snprintf(path, sizeof(path), SYSFS_PWM_BASE "/pwm%d/period", ch);
    snprintf(val,  sizeof(val),  "%d", PWM_PERIOD_NS);
    ret = sysfs_write(path, val);
    if (ret) return ret;

    // 3. duty
    snprintf(path, sizeof(path), SYSFS_PWM_BASE "/pwm%d/duty_cycle", ch);
    snprintf(val,  sizeof(val),  "%d", angle_to_duty_ns(angle));
    ret = sysfs_write(path, val);
    if (ret) return ret;

    // 4. enable
    snprintf(path, sizeof(path), SYSFS_PWM_BASE "/pwm%d/enable", ch);
    ret = sysfs_write(path, "1");
    if (ret) return ret;

    pr_info("mg996r: pwm%d initialized (angle=%d°)\n", ch, angle);
    return 0;
}

// ─────────────────────────────────────────────
//  PWM 각도 적용
// ─────────────────────────────────────────────
static int pwm_set_angle(int ch, int angle)
{
    char path[128], val[32];

    snprintf(path, sizeof(path), SYSFS_PWM_BASE "/pwm%d/duty_cycle", ch);
    snprintf(val,  sizeof(val),  "%d", angle_to_duty_ns(angle));
    return sysfs_write(path, val);
}

// ─────────────────────────────────────────────
//  PWM 채널 해제
// ─────────────────────────────────────────────
static void pwm_ch_cleanup(int ch)
{
    char path[128], val[32];

    // disable
    snprintf(path, sizeof(path), SYSFS_PWM_BASE "/pwm%d/enable", ch);
    sysfs_write(path, "0");

    // unexport
    snprintf(path, sizeof(path), SYSFS_PWM_BASE "/unexport");
    snprintf(val,  sizeof(val),  "%d", ch);
    sysfs_write(path, val);

    pr_info("mg996r: pwm%d released\n", ch);
}

// ─────────────────────────────────────────────
//  file_operations
// ─────────────────────────────────────────────
static int mg996r_open(struct inode *inode, struct file *file)
{
    file->private_data = g_dev;
    return 0;
}

static int mg996r_release(struct inode *inode, struct file *file)
{
    return 0;
}

static long mg996r_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct mg996r_dev   *dev = file->private_data;
    struct mg996r_angle  both;
    int                  angle;
    int                  ret = 0;

    mutex_lock(&dev->lock);

    switch (cmd) {

        case MG996R_SET_PAN:
            if (copy_from_user(&angle, (int __user *)arg, sizeof(int))) {
                ret = -EFAULT; break;
            }
            angle = clamp_pan(angle);
            ret = pwm_set_angle(0, angle);
            if (!ret) dev->pan_angle = angle;
            break;

        case MG996R_SET_TILT:
            if (copy_from_user(&angle, (int __user *)arg, sizeof(int))) {
                ret = -EFAULT; break;
            }
            angle = clamp_tilt(angle);
            ret = pwm_set_angle(1, angle);
            if (!ret) dev->tilt_angle = angle;
            break;

        case MG996R_SET_BOTH:
            if (copy_from_user(&both, (struct mg996r_angle __user *)arg,
                               sizeof(struct mg996r_angle))) {
                ret = -EFAULT; break;
            }
            both.pan  = clamp_pan(both.pan);
            both.tilt = clamp_tilt(both.tilt);
            ret = pwm_set_angle(0, both.pan);
            if (!ret) ret = pwm_set_angle(1, both.tilt);
            if (!ret) {
                dev->pan_angle  = both.pan;
                dev->tilt_angle = both.tilt;
            }
            break;

        case MG996R_DO_CENTER:
            ret = pwm_set_angle(0, MG996R_CENTER);
            if (!ret) ret = pwm_set_angle(1, MG996R_CENTER);
            if (!ret) {
                dev->pan_angle  = MG996R_CENTER;
                dev->tilt_angle = MG996R_CENTER;
            }
            break;

        case MG996R_GET_PAN:
            if (copy_to_user((int __user *)arg, &dev->pan_angle, sizeof(int)))
                ret = -EFAULT;
            break;

        case MG996R_GET_TILT:
            if (copy_to_user((int __user *)arg, &dev->tilt_angle, sizeof(int)))
                ret = -EFAULT;
            break;

        default:
            ret = -ENOTTY;
            break;
    }

    mutex_unlock(&dev->lock);
    return ret;
}

static const struct file_operations mg996r_fops = {
    .owner          = THIS_MODULE,
    .open           = mg996r_open,
    .release        = mg996r_release,
    .unlocked_ioctl = mg996r_ioctl,
};

// ─────────────────────────────────────────────
//  모듈 초기화
// ─────────────────────────────────────────────
static int __init mg996r_init(void)
{
    int ret;

    g_dev = kzalloc(sizeof(struct mg996r_dev), GFP_KERNEL);
    if (!g_dev) return -ENOMEM;

    mutex_init(&g_dev->lock);
    g_dev->pan_angle  = MG996R_CENTER;
    g_dev->tilt_angle = MG996R_CENTER;

    // ── PWM 초기화 ────────────────────────────
    ret = pwm_ch_init(0, MG996R_CENTER);   // Pan  (GPIO18)
    if (ret) { pr_err("mg996r: pan init failed\n");  goto err_free; }

    ret = pwm_ch_init(1, MG996R_CENTER);   // Tilt (GPIO19)
    if (ret) { pr_err("mg996r: tilt init failed\n"); goto err_pan; }

    // ── character device 등록 ─────────────────
    ret = alloc_chrdev_region(&g_dev->devno, 0, 1, MG996R_DEV_NAME);
    if (ret) { pr_err("mg996r: alloc_chrdev_region failed\n"); goto err_tilt; }

    cdev_init(&g_dev->cdev, &mg996r_fops);
    g_dev->cdev.owner = THIS_MODULE;
    ret = cdev_add(&g_dev->cdev, g_dev->devno, 1);
    if (ret) { pr_err("mg996r: cdev_add failed\n"); goto err_chrdev; }

    // ── /dev/mg996r 자동 생성 ─────────────────
    g_dev->class = class_create(MG996R_DEV_NAME);
    if (IS_ERR(g_dev->class)) {
        ret = PTR_ERR(g_dev->class);
        goto err_cdev;
    }

    g_dev->device = device_create(g_dev->class, NULL,
                                   g_dev->devno, NULL, MG996R_DEV_NAME);
    if (IS_ERR(g_dev->device)) {
        ret = PTR_ERR(g_dev->device);
        goto err_class;
    }

    pr_info("mg996r: loaded → /dev/%s (pan=GPIO18/pwm0, tilt=GPIO19/pwm1)\n",
            MG996R_DEV_NAME);
    return 0;

err_class:
    class_destroy(g_dev->class);
err_cdev:
    cdev_del(&g_dev->cdev);
err_chrdev:
    unregister_chrdev_region(g_dev->devno, 1);
err_tilt:
    pwm_ch_cleanup(1);
err_pan:
    pwm_ch_cleanup(0);
err_free:
    kfree(g_dev);
    return ret;
}

// ─────────────────────────────────────────────
//  모듈 해제
// ─────────────────────────────────────────────
static void __exit mg996r_exit(void)
{
    // 중앙 복귀
    pwm_set_angle(0, MG996R_CENTER);
    pwm_set_angle(1, MG996R_CENTER);
    msleep(300);

    device_destroy(g_dev->class, g_dev->devno);
    class_destroy(g_dev->class);
    cdev_del(&g_dev->cdev);
    unregister_chrdev_region(g_dev->devno, 1);

    pwm_ch_cleanup(1);
    pwm_ch_cleanup(0);
    kfree(g_dev);

    pr_info("mg996r: unloaded\n");
}

module_init(mg996r_init);
module_exit(mg996r_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("GNAGHEE");
MODULE_DESCRIPTION("MG996R Pan/Tilt servo driver - sysfs PWM");
MODULE_VERSION("1.3");