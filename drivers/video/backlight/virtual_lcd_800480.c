/*
 * Copyright (C) 2015, Ingenic Semiconductor Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * You should have received a copy of the  GNU General Public License along
 * with this program; if not, write  to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/lcd.h>
#include <linux/fb.h>

struct virtual_data {
	struct lcd_device *lcd;
};

static int virtual_set_power(struct lcd_device *lcd, int power)
{
	return 0;
}

static int virtual_get_power(struct lcd_device *lcd)
{
	return 1;
}

static int virtual_set_mode(struct lcd_device *lcd, struct fb_videomode *mode)
{
	return 0;
}

static struct lcd_ops virtual_ops = {
	.set_power = virtual_set_power,
	.get_power = virtual_get_power,
	.set_mode = virtual_set_mode,
};

static int virtual_probe(struct platform_device *pdev)
{
	/* check the parameters from lcd_driver */
	int ret = 0;
	struct virtual_data *dev;
	dev = kzalloc(sizeof(struct virtual_data), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dev_set_drvdata(&pdev->dev, dev);

	dev->lcd = lcd_device_register("virtual-lcd", &pdev->dev, dev, &virtual_ops);

	if (IS_ERR(dev->lcd)) {
		ret = PTR_ERR(dev->lcd);
		dev->lcd = NULL;
		dev_info(&pdev->dev, "virtual lcd device register error: %d\n", ret);
	} else {
		dev_info(&pdev->dev, "virtual lcd device register success\n");
	}

	return 0;
}

static int virtual_remove(struct platform_device *pdev)
{

	struct virtual_data *dev = dev_get_drvdata(&pdev->dev);

	lcd_device_unregister(dev->lcd);

	dev_set_drvdata(&pdev->dev, NULL);
	kfree(dev);

	return 0;
}

static struct platform_driver virtual_driver = {
	.driver		= {
		.name	= "virtual-lcd-800480",
		.owner	= THIS_MODULE,
	},
	.probe		= virtual_probe,
	.remove		= virtual_remove,
};

static int __init virtual_init(void)
{
	return platform_driver_register(&virtual_driver);;
}
module_init(virtual_init);

static void __exit virtual_exit(void)
{
	platform_driver_unregister(&virtual_driver);
}
module_exit(virtual_exit);

MODULE_DESCRIPTION("virtual lcd driver");
MODULE_AUTHOR("Kznan <Derric.kznan@ingenic.com>");
MODULE_LICENSE("GPL");
