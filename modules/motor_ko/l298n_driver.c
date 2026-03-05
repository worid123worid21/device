#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/device.h>

#include "l298n.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("GNAGHEE");
MODULE_DESCRIPTION("L298N DC Motor Driver - BCM2711 direct register");
MODULE_VERSION("4.2");

/* ------------------------------------------------------------------ */
/* BCM2711 GPIO 레지스터 (Pi 4)                                          */
/* ------------------------------------------------------------------ */
#define GPIO_BASE     0xFE200000UL
#define GPIO_MAP_SIZE 0xB4

#define GPFSEL(n)     ((n) / 10)
#define GPSET(n)      (7  + (n) / 32)
#define GPCLR(n)      (10 + (n) / 32)
#define FSEL_SHIFT(n) (((n) % 10) * 3)

static void __iomem *g_base;

static void pin_output(unsigned int bcm)
{
    u32 val;
    val  = ioread32(g_base + GPFSEL(bcm) * 4);
    val &= ~(0x7u << FSEL_SHIFT(bcm));
    val |=  (0x1u << FSEL_SHIFT(bcm));
    iowrite32(val, g_base + GPFSEL(bcm) * 4);
}

static void pin_high(unsigned int bcm)
{
    iowrite32(1u << (bcm % 32), g_base + GPSET(bcm) * 4);
}

static void pin_low(unsigned int bcm)
{
    iowrite32(1u << (bcm % 32), g_base + GPCLR(bcm) * 4);
}

static void pin_set(unsigned int bcm, int val)
{
    if (val) pin_high(bcm); else pin_low(bcm);
}

/* ------------------------------------------------------------------ */
/* 실제 동작 분석 (현재 ← → ↑ ↓ 결과 기준)                              */
/*                                                                      */
/* 현재 ←  → 기존 forward()  : DIRA=H DIRB=L  PWMA=0 PWMB=1 → 전진    */
/* 현재 →  → 기존 curveLeft(): DIRA=L DIRB=H  PWMA=1 PWMB=0 → 좌회전  */
/* 현재 ↑  → 기존 right()    : DIRA=H DIRB=L  PWMA=1 PWMB=0 → 우모터전*/
/* 현재 ↓  → 기존 backward() : DIRA=H DIRB=H  PWMA=0 PWMB=1 → 우모터후*/
/*                                                                      */
/* 목표 ↑=전진  ↓=후진  ←=좌회전  →=우회전                              */
/*                                                                      */
/* 결론:                                                                 */
/*   전진   = 기존 forward()    → DIRA=H DIRB=L PWMA=0 PWMB=1          */
/*   후진   = 기존 backward()   → DIRA=H DIRB=H PWMA=0 PWMB=1          */
/*   좌회전 = 기존 curveLeft()  → DIRA=L DIRB=H PWMA=1 PWMB=0          */
/*   우회전 = 기존 curveRight() → DIRA=H DIRB=L PWMA=1 PWMB=0          */
/*                                                                      */
/*   ↑→전진, ↓→후진, ←→좌회전, →→우회전 으로 ioctl 매핑만 교정        */
/* ------------------------------------------------------------------ */

static void motor_stop(void)
{
    pin_low(GPIO_E1);   /* PWMA=0 */
    pin_low(GPIO_E2);   /* PWMB=0 */
}

static void motor_forward(void)
{
    /* DIRA=H DIRB=L PWMA=0 PWMB=1 */
    pin_high(GPIO_M1);
    pin_low(GPIO_M2);
    pin_low(GPIO_E1);
    pin_high(GPIO_E2);
}

static void motor_backward(void)
{
    /* DIRA=H DIRB=H PWMA=0 PWMB=1 */
    pin_high(GPIO_M1);
    pin_high(GPIO_M2);
    pin_low(GPIO_E1);
    pin_high(GPIO_E2);
}

static void motor_left(void)
{
    /* DIRA=L DIRB=H PWMA=1 PWMB=0 */
    pin_low(GPIO_M1);
    pin_high(GPIO_M2);
    pin_high(GPIO_E1);
    pin_low(GPIO_E2);
}

static void motor_right(void)
{
    /* DIRA=H DIRB=L PWMA=1 PWMB=0 */
    pin_high(GPIO_M1);
    pin_low(GPIO_M2);
    pin_high(GPIO_E1);
    pin_low(GPIO_E2);
}

/* ------------------------------------------------------------------ */
/* chrdev                                                               */
/* ------------------------------------------------------------------ */
static dev_t          g_devno;
static struct cdev    g_cdev;
static struct class  *g_class;
static struct device *g_device;

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
    case L298N_FORWARD:  motor_forward();  pr_info("l298n: ↑ 전진\n");   break;
    case L298N_BACKWARD: motor_backward(); pr_info("l298n: ↓ 후진\n");   break;
    case L298N_LEFT:     motor_left();     pr_info("l298n: ← 좌회전\n"); break;
    case L298N_RIGHT:    motor_right();    pr_info("l298n: → 우회전\n"); break;
    case L298N_STOP:     motor_stop();     pr_info("l298n: ■ 정지\n");   break;
    default: return -EINVAL;
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
/* init / exit                                                          */
/* ------------------------------------------------------------------ */
static int __init l298n_init(void)
{
    int ret;

    g_base = ioremap(GPIO_BASE, GPIO_MAP_SIZE);
    if (!g_base) { pr_err("l298n: ioremap 실패\n"); return -ENOMEM; }

    pin_output(GPIO_E1); pin_low(GPIO_E1);
    pin_output(GPIO_M1); pin_low(GPIO_M1);
    pin_output(GPIO_E2); pin_low(GPIO_E2);
    pin_output(GPIO_M2); pin_low(GPIO_M2);

    ret = alloc_chrdev_region(&g_devno, 0, 1, "l298n");
    if (ret) { pr_err("l298n: chrdev alloc 실패\n"); goto err_unmap; }

    cdev_init(&g_cdev, &l298n_fops);
    g_cdev.owner = THIS_MODULE;
    ret = cdev_add(&g_cdev, g_devno, 1);
    if (ret) { pr_err("l298n: cdev_add 실패\n"); goto err_chrdev; }

    g_class = class_create("l298n");
    if (IS_ERR(g_class)) { ret = PTR_ERR(g_class); goto err_cdev; }

    g_device = device_create(g_class, NULL, g_devno, NULL, "l298n");
    if (IS_ERR(g_device)) { ret = PTR_ERR(g_device); goto err_class; }

    pr_info("l298n: 로드 완료 (major=%d)\n", MAJOR(g_devno));
    pr_info("l298n: PWMA=BCM%d DIRA=BCM%d / PWMB=BCM%d DIRB=BCM%d\n",
            GPIO_E1, GPIO_M1, GPIO_E2, GPIO_M2);
    return 0;

err_class:   class_destroy(g_class);
err_cdev:    cdev_del(&g_cdev);
err_chrdev:  unregister_chrdev_region(g_devno, 1);
err_unmap:   iounmap(g_base);
    return ret;
}

static void __exit l298n_exit(void)
{
    motor_stop();
    device_destroy(g_class, g_devno);
    class_destroy(g_class);
    cdev_del(&g_cdev);
    unregister_chrdev_region(g_devno, 1);
    iounmap(g_base);
    pr_info("l298n: 언로드\n");
}

module_init(l298n_init);
module_exit(l298n_exit);