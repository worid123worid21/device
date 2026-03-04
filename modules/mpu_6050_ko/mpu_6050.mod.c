#include <linux/module.h>
#include <linux/export-internal.h>
#include <linux/compiler.h>

MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};



static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x36a78de3, "devm_kmalloc" },
	{ 0x44b05684, "i2c_transfer_buffer_flags" },
	{ 0xe783e261, "sysfs_emit" },
	{ 0xf0fdf6cb, "__stack_chk_fail" },
	{ 0x14d51ef4, "_dev_info" },
	{ 0x7c1dcb20, "i2c_register_driver" },
	{ 0x88d4c348, "_dev_err" },
	{ 0xed85cddf, "sysfs_create_group" },
	{ 0x374b3c83, "sysfs_remove_group" },
	{ 0xd8ff692a, "i2c_del_driver" },
	{ 0xf9a482f9, "msleep" },
	{ 0x474e54d2, "module_layout" },
};

MODULE_INFO(depends, "");

MODULE_ALIAS("i2c:mpu_6050");
MODULE_ALIAS("of:N*T*Cinvensense,mpu6050");
MODULE_ALIAS("of:N*T*Cinvensense,mpu6050C*");

MODULE_INFO(srcversion, "0086B9DF7036425784B743E");
