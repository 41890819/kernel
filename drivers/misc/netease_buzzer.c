/*
 * linux/drivers/misc/jz_efuse_v12.c - Ingenic efuse driver
 *
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 * Author: liu bo <bliu@ingenic.com>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/gpio_keys.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/pwm.h>

#include "netease_buzzer.h"



static int buzzer_open(struct inode * inode, struct file * filp)  
{  
	struct pwm_device *pwm;

	pwm = pwm_request(1, "buzzer");

	if (pwm == ERR_PTR(-ENODEV)) {
		printk("get pwm device failed\n");
		return -1;
	}

	filp->private_data = pwm;

	return 0;
}  

static int buzzer_release(struct inode *inode, struct file *file)  
{
	struct pwm_device *pwm = (struct pwm_device *)file->private_data;

	pwm_free(pwm);

	return 0;
}

static void tone(struct pwm_device *pwm, uint16_t pitch, uint8_t loud, uint16_t duration)
{
	uint32_t period_ns;
	uint32_t duty_ns;

	if (pitch == 0 || duration == 0)
		return;

	if (loud == 0)
		msleep(duration);
	else {
		period_ns = 1000000000 / pitch;
		duty_ns = period_ns * loud / 100;
		pwm_config(pwm, duty_ns, period_ns);

		pwm_enable(pwm);
		msleep(duration);
		pwm_disable(pwm);
	}
}


static void play_music(struct pwm_device *pwm, motif_t *motif, int size)
{
	int i;

	for (i = 0; i < size; i++, motif++)
		tone(pwm, motif->pitch, motif->loud, motif->duration); 
}



static long buzzer_ioctl(struct file *file, 
              unsigned int cmd, unsigned long arg)
{
	struct pwm_device *pwm = (struct pwm_device *)file->private_data;
	motif_t motif;

	switch (cmd) {
	case BUZZER_IOCTL_STARTUP:
		play_music(pwm, startup, sizeof(startup)/sizeof(startup[0]));
		break;

	case BUZZER_IOCTL_SHUTDOWN:
		play_music(pwm, shutdown, sizeof(shutdown)/sizeof(shutdown[0]));
		break;

	case BUZZER_IOCTL_CONFIG:
		play_music(pwm, config, sizeof(config)/sizeof(config[0]));
		break;

	case BUZZER_IOCTL_PLAY_PITCH:
		copy_from_user(&motif, (void __user *)arg, sizeof(motif_t));
		tone(pwm, motif.pitch, motif.loud, motif.duration);
		break;
	default:
		break;
	}

	return 0;
}


static const struct file_operations buzzer_fops = {
	.owner = THIS_MODULE,
	.open = buzzer_open,
	.release = buzzer_release,
	.unlocked_ioctl = buzzer_ioctl,
};

static struct miscdevice buzzer_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "buzzer",
	.fops = &buzzer_fops,
};



static int __init buzzer_init(void)
{
	return misc_register(&buzzer_dev);
}

static void __exit buzzer_exit(void)
{
	misc_deregister(&buzzer_dev);
}


module_init(buzzer_init);
module_exit(buzzer_exit);


MODULE_DESCRIPTION("smart pen buzzer driver");
MODULE_AUTHOR("liusishuang <hzliusishuang@crop.netease.com>");
MODULE_LICENSE("GPL v2");
