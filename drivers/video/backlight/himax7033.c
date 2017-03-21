/*
 * Copyright (C) 2015, Ingenic Semiconductor Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/lcd.h>
#include <linux/fb.h>
#include <linux/earlysuspend.h>
#include <linux/regulator/consumer.h>

struct himax7033_data {
	struct lcd_device* lcd;
	struct regulator* power;
	struct i2c_client* client;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
        int backlight_pwm_enable;
};

static int himax7033_write_reg(struct i2c_client *client, unsigned char reg, unsigned char val)
{
	int ret;
	unsigned char msg[2];

	msg[0] = reg;
	msg[1] = val;

	ret = i2c_master_send(client, msg, 2);

	if (ret < 2) {
		dev_err(&client->dev, "i2c_master_send error : %d\n", ret);
		return 1;
	}

	return 0;
}

static void Initial_IC(struct i2c_client *client)
{
	himax7033_write_reg(client, 0x00, 0x96 );
	himax7033_write_reg(client, 0x01, 0x00 );
	himax7033_write_reg(client, 0x02, 0x00 );
	himax7033_write_reg(client, 0x03, 0x50 );
	himax7033_write_reg(client, 0x04, 0x84 );
	himax7033_write_reg(client, 0x05, 0xf5 );
	himax7033_write_reg(client, 0x06, 0x55 );
	himax7033_write_reg(client, 0x07, 0x55 );

	himax7033_write_reg(client, 0x08, 0x55 );
	himax7033_write_reg(client, 0x09, 0x55 );
	himax7033_write_reg(client, 0x0A, 0xf6 );
	himax7033_write_reg(client, 0x0B, 0x80 );
	himax7033_write_reg(client, 0x0C, 0x00 );
	himax7033_write_reg(client, 0x0D, 0x4c );
	himax7033_write_reg(client, 0x0E, 0x6b );
	himax7033_write_reg(client, 0x0F, 0x8a );

	himax7033_write_reg(client, 0x10, 0xb5 );
	himax7033_write_reg(client, 0x11, 0xea );
	himax7033_write_reg(client, 0x12, 0x15 );
	himax7033_write_reg(client, 0x13, 0x4a );
	himax7033_write_reg(client, 0x14, 0x75 );
	himax7033_write_reg(client, 0x15, 0x94 );
	himax7033_write_reg(client, 0x16, 0xb2 );
	himax7033_write_reg(client, 0x17, 0xff );

	himax7033_write_reg(client, 0x18, 0x00 );
	himax7033_write_reg(client, 0x19, 0x4c );
	himax7033_write_reg(client, 0x1A, 0x6b );
	himax7033_write_reg(client, 0x1B, 0x8a );
	himax7033_write_reg(client, 0x1C, 0xb5 );
	himax7033_write_reg(client, 0x1D, 0xea );
	himax7033_write_reg(client, 0x1E, 0x15 );
	himax7033_write_reg(client, 0x1F, 0x4a );

	himax7033_write_reg(client, 0x20, 0x75 );
	himax7033_write_reg(client, 0x21, 0x94 );
	himax7033_write_reg(client, 0x22, 0xb2 );
	himax7033_write_reg(client, 0x23, 0xff );
	himax7033_write_reg(client, 0x24, 0x00 );
	himax7033_write_reg(client, 0x25, 0x4c );
	himax7033_write_reg(client, 0x26, 0x6b );
	himax7033_write_reg(client, 0x27, 0x8a );

	himax7033_write_reg(client, 0x28, 0xb5 );
	himax7033_write_reg(client, 0x29, 0xea );
	himax7033_write_reg(client, 0x2A, 0x15 );
	himax7033_write_reg(client, 0x2B, 0x4a );
	himax7033_write_reg(client, 0x2C, 0x75 );
	himax7033_write_reg(client, 0x2D, 0x94 );
	himax7033_write_reg(client, 0x2E, 0xb2 );
	himax7033_write_reg(client, 0x2F, 0xff );

	himax7033_write_reg(client, 0x30, 0x00 );
	himax7033_write_reg(client, 0x31, 0xff );
	himax7033_write_reg(client, 0x32, 0x50 );
	himax7033_write_reg(client, 0x33, 0x78 );
	himax7033_write_reg(client, 0x34, 0xa0 );
	himax7033_write_reg(client, 0x35, 0xc8 );
	himax7033_write_reg(client, 0x36, 0x04 );
	himax7033_write_reg(client, 0x37, 0x22 );

	himax7033_write_reg(client, 0x38, 0x88 );
	himax7033_write_reg(client, 0x39, 0x00 );
	himax7033_write_reg(client, 0x3A, 0x00 );
	himax7033_write_reg(client, 0x3B, 0x00 );
	himax7033_write_reg(client, 0x3C, 0x00 );
	himax7033_write_reg(client, 0x3D, 0x00 );
	himax7033_write_reg(client, 0x3E, 0x00 );
	himax7033_write_reg(client, 0x3F, 0x00 );
}

static int himax7033_set_power(struct lcd_device *lcd, int power)
{
	return 0;
}

static int himax7033_get_power(struct lcd_device *lcd)
{
	return 1;
}

static int himax7033_set_mode(struct lcd_device *lcd, struct fb_videomode *mode)
{
	return 0;
}

static struct lcd_ops himax7033_ops = {
	.set_power = himax7033_set_power,
	.get_power = himax7033_get_power,
	.set_mode  = himax7033_set_mode,
};


#ifdef CONFIG_HAS_EARLYSUSPEND
static void himax7033_early_suspend(struct early_suspend *h)
{
        struct himax7033_data *dev = container_of(h, struct himax7033_data, early_suspend);

#ifdef CONFIG_BACKLIGHT_PWM_EN
	if (dev->backlight_pwm_enable >= 0){
		gpio_direction_output(dev->backlight_pwm_enable, 0);
	}
#endif

	if (dev->power)
		regulator_disable(dev->power);
	printk("Himax7033 early suspend\n");
}

static void himax7033_early_resume(struct early_suspend *h)
{
        struct himax7033_data *dev = container_of(h, struct himax7033_data, early_suspend);

	if (dev->power) {
		regulator_set_voltage(dev->power, 1800000, 1800000);
		regulator_enable(dev->power);
	}

	Initial_IC(dev->client);

#ifdef CONFIG_BACKLIGHT_PWM_EN
	if (dev->backlight_pwm_enable >= 0){
		gpio_direction_output(dev->backlight_pwm_enable, 1);
	}
#endif
	printk("Himax7033 early resume\n");
}
#endif

static int himax7033_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	struct himax7033_data *dev = NULL;

	dev = kzalloc(sizeof(struct himax7033_data), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dev->client = client;
	dev->power = regulator_get(&client->dev, "lcd");
	if (IS_ERR(dev->power)) {
		pr_err("Get lcd regulator failed\n");
		dev->power = NULL;
	}

#ifdef CONFIG_BACKLIGHT_PWM_EN
	dev->backlight_pwm_enable = client->irq;
	if (dev->backlight_pwm_enable >= 0){
		gpio_request(dev->backlight_pwm_enable, "lcd_pwm_enable");
		gpio_direction_output(dev->backlight_pwm_enable, 1);
	}
#endif

	i2c_set_clientdata(client, dev);

	Initial_IC(dev->client);

	dev->lcd = lcd_device_register("himax7033", &client->dev, dev, &himax7033_ops);
	if (IS_ERR(dev->lcd)) {
		ret = PTR_ERR(dev->lcd);
		dev->lcd = NULL;
		dev_info(&client->dev, "lcd device register error: %d\n", ret);
	} else {
		dev_info(&client->dev, "lcd device register success\n");
	}
	
#ifdef CONFIG_HAS_EARLYSUSPEND
	dev->early_suspend.suspend = himax7033_early_suspend;
	dev->early_suspend.resume = himax7033_early_resume;
	dev->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB;
	register_early_suspend(&dev->early_suspend);
#endif
	return ret;
}

static int __devinit himax7033_remove(struct i2c_client *client)
{
	struct himax7033_data *dev = dev_get_drvdata(&client->dev);

	if (dev->power) {
		regulator_disable(dev->power);
		regulator_put(dev->power);
	}

	unregister_early_suspend(&dev->early_suspend);

	dev_set_drvdata(&client->dev, NULL);
	kfree(dev);

	return 0;
}

static const struct i2c_device_id himax7033_i2c_id[] = {
    {"himax7033", 0},
    {}
};

MODULE_DEVICE_TABLE(i2c, himax7033_i2c_id);

static struct i2c_driver himax7033_driver = {
	.driver		= {               
		.name	= "himax7033",
		.owner	= THIS_MODULE,
	},
	.id_table       = himax7033_i2c_id,
	.probe	        = himax7033_probe,
	.remove	        = himax7033_remove,
};

static int __init himax7033_lcd_init(void)
{
	return i2c_add_driver(&himax7033_driver);
}

static void __exit himax7033_lcd_exit(void)
{
	i2c_del_driver(&himax7033_driver);
}

module_init(himax7033_lcd_init);
module_exit(himax7033_lcd_exit);

MODULE_DESCRIPTION("HIMAX7033 lcd panel driver");
MODULE_AUTHOR("Kznan <Derric.kznan@ingenic.com>");
MODULE_LICENSE("GPL");
