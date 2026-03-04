#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/device.h>

#include "l298n.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("l298n");
MODULE_DESCRIPTION("L298N DC Motor GPIO Driver (E=HIGH fixed, M=DIR)");
MODULE_VERSION("1.0");

/* ------------------------------------------------------------------ */
/* chrdev 내부 상태                                                      */
/* ------------------------------------------------------------------ */
static dev_t           g_devno;
static struct cdev     g_cdev;
static struct class   *g_class;
static struct device  *g_device;

/* ------------------------------------------------------------------ */
/* L298N 진리표 (GPIO 모드 - E핀 HIGH 고정)                              */
/*                                                                      */
/*  동작       E1  M1   E2  M2                                          */
/*  정지        0   x    0   x                                          */
/*  전진        1   0    1   0   (양쪽 모터 정방향)                      */
/*  후진        1   1    1   1   (양쪽 모터 역방향)                      */
/*  좌회전      0   x    1   0   (좌 모터 정지, 우 모터 전진)            */
/*  우회전      1   0    0   x   (좌 모터 전진, 우 모터 정지)            */
/* ------------------------------------------------------------------ */

static void motor_apply(int e1, int m1, int e2, int m2)
{
    gpio_set_value(GPIO_E1, e1);
    gpio_set_value(GPIO_M1, m1);
    gpio_set_value(GPIO_E2, e2);
    gpio_set_value(GPIO_M2, m2);
}

static void motor_stop(void)     { motor_apply(0, 0, 0, 0); }
static void motor_forward(void)  { motor_apply(1, 0, 1, 0); }
static void motor_backward(void) { motor_apply(1, 1, 1, 1); }
static void motor_left(void)     { motor_apply(0, 0, 1, 0); }
static void motor_right(void)    { motor_apply(1, 0, 0, 0); }

/* ------------------------------------------------------------------ */
/* file_operations                                                       */
/* ------------------------------------------------------------------ */
static int l298n_open(struct inode *inode, struct file *file)
{
    pr_info("l298n: open\n");
    return 0;
}

static int l298n_release(struct inode *inode, struct file *file)
{
    motor_stop();
    pr_info("l298n: release → 정지\n");
    return 0;
}

static long l298n_ioctl(struct file *file, unsigned int cmd,
                         unsigned long arg)
{
    (void)arg;

    if (_IOC_TYPE(cmd) != L298N_IOC_MAGIC)
        return -ENOTTY;

    switch (cmd) {
    case L298N_FORWARD:
        motor_forward();
        pr_info("l298n: ↑ 전진  (E1=1,M1=0 / E2=1,M2=0)\n");
        break;
    case L298N_BACKWARD:
        motor_backward();
        pr_info("l298n: ↓ 후진  (E1=1,M1=1 / E2=1,M2=1)\n");
        break;
    case L298N_LEFT:
        motor_left();
        pr_info("l298n: ← 좌회전 (E1=0 / E2=1,M2=0)\n");
        break;
    case L298N_RIGHT:
        motor_right();
        pr_info("l298n: → 우회전 (E1=1,M1=0 / E2=0)\n");
        break;
    case L298N_STOP:
        motor_stop();
        pr_info("l298n: ■ 정지  (E1=0 / E2=0)\n");
        break;
    default:
        return -EINVAL;
    }
    return 0;
}

static const struct file_operations l298n_fops = {
    .owner          = THIS_MODULE,
    .open           = l298n_open,
    .release        = l298n_release,
    .unlocked_ioctl = l298n_ioctl,
};

/* ------------------------------------------------------------------ */
/* 모듈 init / exit                                                      */
/* ------------------------------------------------------------------ */
static const int PINS[] = { GPIO_E1, GPIO_M1, GPIO_E2, GPIO_M2 };
#define PIN_COUNT (int)(ARRAY_SIZE(PINS))

static int __init l298n_init(void)
{
    int ret, i;

    /* 1. GPIO 요청 및 출력 설정 */
    for (i = 0; i < PIN_COUNT; i++) {
        ret = gpio_request(PINS[i], "l298n");
        if (ret) {
            pr_err("l298n: gpio_request(%d) 실패: %d\n", PINS[i], ret);
            goto err_gpio;
        }
        gpio_direction_output(PINS[i], 0); /* 초기값 LOW */
    }

    /* 2. chrdev 등록 */
    ret = alloc_chrdev_region(&g_devno, 0, 1, "l298n");
    if (ret) { pr_err("l298n: chrdev alloc 실패\n"); goto err_gpio; }

    cdev_init(&g_cdev, &l298n_fops);
    g_cdev.owner = THIS_MODULE;
    ret = cdev_add(&g_cdev, g_devno, 1);
    if (ret) { pr_err("l298n: cdev_add 실패\n"); goto err_chrdev; }

    /* 3. /dev/l298n 자동 생성 */
    g_class = class_create("l298n");  /* 6.6+ : THIS_MODULE 인자 제거됨 */
    if (IS_ERR(g_class)) { ret = PTR_ERR(g_class); goto err_cdev; }

    g_device = device_create(g_class, NULL, g_devno, NULL, "l298n");
    if (IS_ERR(g_device)) { ret = PTR_ERR(g_device); goto err_class; }

    pr_info("l298n: 드라이버 로드 완료 (major=%d)\n", MAJOR(g_devno));
    pr_info("l298n: 핀 매핑 E1=BCM%d(Phy36) M1=BCM%d(Phy38) / E2=BCM%d(Phy11) M2=BCM%d(Phy13)\n",
            GPIO_E1, GPIO_M1, GPIO_E2, GPIO_M2);
    return 0;

err_class:   class_destroy(g_class);
err_cdev:    cdev_del(&g_cdev);
err_chrdev:  unregister_chrdev_region(g_devno, 1);
err_gpio:
    for (i--; i >= 0; i--)
        gpio_free(PINS[i]);
    return ret;
}

static void __exit l298n_exit(void)
{
    int i;
    motor_stop();
    device_destroy(g_class, g_devno);
    class_destroy(g_class);
    cdev_del(&g_cdev);
    unregister_chrdev_region(g_devno, 1);
    for (i = 0; i < PIN_COUNT; i++)
        gpio_free(PINS[i]);
    pr_info("l298n: 드라이버 언로드\n");
}

module_init(l298n_init);
module_exit(l298n_exit);
