/*
 * Copyright (c) 2013 Ingenic Semiconductor Co.,Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 *
 * Author :   Kznan <Derrick.kznan@ingenic.com>
 * Description :  lcd driver for ili9342c
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/spi/spi.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/lcd.h>
#include <linux/fb.h>
#include <linux/regulator/consumer.h>

struct ili9342c_data {
	int lcd_power;
	struct lcd_device *lcd;
	struct lcd_platform_data *ctrl;
};

static int ili9342c_set_power(struct lcd_device *lcd, int power)
{
	struct ili9342c_data *dev = lcd_get_data(lcd);

	if (!power && dev->lcd_power) {
		dev->ctrl->power_on(lcd, 1);
	} else if (power && !dev->lcd_power) {
		dev->ctrl->power_on(lcd, 0);
	}
	dev->lcd_power = power;

	return 0;
}

static int ili9342c_get_power(struct lcd_device *lcd)
{
	struct ili9342c_data *dev = lcd_get_data(lcd);

	return dev->lcd_power;
}

static int ili9342c_set_mode(struct lcd_device *lcd, struct fb_videomode *mode)
{
	return 0;
}

static struct lcd_ops ili9342c_ops = {
	.set_power = ili9342c_set_power,
	.get_power = ili9342c_get_power,
	.set_mode = ili9342c_set_mode,
};

static int __devinit ili9342c_probe(struct platform_device *pdev)
{
	int ret;
	struct ili9342c_data *dev = NULL;

	dev = kzalloc(sizeof(struct ili9342c_data), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dev->ctrl = pdev->dev.platform_data;
	if (dev->ctrl == NULL) {
		dev_info(&pdev->dev, "no platform data!");
		return -EINVAL;
	}

	dev_set_drvdata(&pdev->dev, dev);

	dev->lcd = lcd_device_register("ili9342c_slcd", &pdev->dev, dev, &ili9342c_ops);
	if (IS_ERR(dev->lcd)) {
		ret = PTR_ERR(dev->lcd);
		dev->lcd = NULL;
		dev_info(&pdev->dev, "lcd device register error: %d\n", ret);
	} else {
		dev_info(&pdev->dev, "lcd device(ILI9342C TFT320240) register success\n");
	}

	return 0;
}

static int __devinit ili9342c_remove(struct platform_device *pdev)
{
	struct ili9342c_data *dev = dev_get_drvdata(&pdev->dev);

	if (dev->lcd_power)
		dev->ctrl->power_on(dev->lcd, 0);

	lcd_device_unregister(dev->lcd);
	dev_set_drvdata(&pdev->dev, NULL);
	kfree(dev);

	return 0;
}

#ifdef CONFIG_PM
static int ili9342c_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int ili9342c_resume(struct platform_device *pdev)
{
	return 0;
}
#else
#define ili9342c_suspend	NULL
#define ili9342c_resume         NULL
#endif

static struct platform_driver ili9342c_driver = {
	.driver		= {
		.name	= "ili9342c_slcd",
		.owner	= THIS_MODULE,
	},
	.probe		= ili9342c_probe,
	.remove		= ili9342c_remove,
	.suspend	= ili9342c_suspend,
	.resume		= ili9342c_resume,
};

static int __init ili9342c_init(void)
{
	return platform_driver_register(&ili9342c_driver);
}

static void __exit ili9342c_exit(void)
{
	platform_driver_unregister(&ili9342c_driver);
}

module_init(ili9342c_init);
module_exit(ili9342c_exit);

MODULE_DESCRIPTION("ILI9342C slcd panel driver");
MODULE_LICENSE("GPL");
