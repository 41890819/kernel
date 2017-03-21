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
#include <linux/spi/spi.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/lcd.h>
#include <linux/fb.h>
#include <linux/earlysuspend.h>
#include <linux/regulator/consumer.h>
#include <soc/gpio.h>

#include <linux/ecx336af.h>
extern void Initial_IC(struct platform_ecx336af_data *pdata);
extern void ECX336AF_POWER_ON(struct platform_ecx336af_data *pdata);
extern void ECX336AF_POWER_OFF(struct platform_ecx336af_data *pdata);
extern void ECX336AF_BACKLIGHT(unsigned char brightness);

struct ecx336af_data {
	int lcd_power;
	struct lcd_device *lcd;
	struct platform_ecx336af_data *pdata;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
};

static char ecx336af_probin = 0;

void ecx336af_backlight(unsigned char brightness){
  if (ecx336af_probin){
    ECX336AF_BACKLIGHT(brightness);
  }
}
EXPORT_SYMBOL(ecx336af_backlight);

static void ecx336af_on(struct ecx336af_data *dev) {
	dev->lcd_power = 1;

	/*spi interface init*/
	if (dev->pdata->gpio_spi_cs != -1)
		gpio_direction_output(dev->pdata->gpio_spi_cs, 1);
	if (dev->pdata->gpio_spi_clk != -1)
		gpio_direction_output(dev->pdata->gpio_spi_clk, 1);
	if (dev->pdata->gpio_spi_sdo != -1)
		gpio_direction_output(dev->pdata->gpio_spi_sdi, 1);
	if (dev->pdata->gpio_spi_sdi != -1)
		gpio_direction_input(dev->pdata->gpio_spi_sdo);

	ECX336AF_POWER_ON(dev->pdata);
	Initial_IC(dev->pdata);
}

static void ecx336af_off(struct ecx336af_data *dev)
{
	dev->lcd_power = 0;
	if (dev->pdata->gpio_spi_cs != -1)
		gpio_direction_output(dev->pdata->gpio_spi_cs, 0);
	if (dev->pdata->gpio_reset != -1)
		gpio_direction_output(dev->pdata->gpio_reset, 0);
	mdelay(2);
	ECX336AF_POWER_OFF(dev->pdata);
	mdelay(10);
}

static int ecx336af_set_power(struct lcd_device *lcd, int power)
{
	struct ecx336af_data *dev = lcd_get_data(lcd);

	if (!power && !(dev->lcd_power)) {
                ecx336af_on(dev);
        } else if (power && (dev->lcd_power)) {
                ecx336af_off(dev);
        }
	return 0;
}

static int ecx336af_get_power(struct lcd_device *lcd)
{
	struct ecx336af_data *dev= lcd_get_data(lcd);

	return dev->lcd_power;
}

static int ecx336af_set_mode(struct lcd_device *lcd, struct fb_videomode *mode)
{
	return 0;
}

static struct lcd_ops ecx336af_ops = {
	.set_power = ecx336af_set_power,
	.get_power = ecx336af_get_power,
	.set_mode = ecx336af_set_mode,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void ecx336af_early_suspend(struct early_suspend *h)
{
        struct ecx336af_data *dev = container_of(h, struct ecx336af_data, early_suspend);
	//ecx336af_set_power(dev->lcd, 0);
}

static void ecx336af_early_resume(struct early_suspend *h)
{
        struct ecx336af_data *dev = container_of(h, struct ecx336af_data, early_suspend);
	//ecx336af_set_power(dev->lcd, 1);
}
#endif

static int ecx336af_probe(struct platform_device *pdev)
{
	/* check the parameters from lcd_driver */
	int ret = 0;
	struct ecx336af_data *dev;
	dev = kzalloc(sizeof(struct ecx336af_data), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	ecx336af_probin = 1;

	dev->pdata = pdev->dev.platform_data;

	dev_set_drvdata(&pdev->dev, dev);
	dev->pdata->power_1v8 = regulator_get(NULL, "lcd");
	if (IS_ERR(dev->pdata->power_1v8)) {
		dev_err(&pdev->dev, "failed to get regulator vlcd\n");
		return PTR_ERR(dev->pdata->power_1v8);
	}

	if (dev->pdata->gpio_reset != -1)
		gpio_request(dev->pdata->gpio_reset, "reset");

	/* lcd spi interface */
	if (dev->pdata->gpio_spi_cs != -1)
		gpio_request(dev->pdata->gpio_spi_cs, "spi_cs");
	if (dev->pdata->gpio_spi_clk != -1)
		gpio_request(dev->pdata->gpio_spi_clk, "spi_clk");
	if (dev->pdata->gpio_spi_sdo != -1)
		gpio_request(dev->pdata->gpio_spi_sdo, "spi_sdo");
	if (dev->pdata->gpio_spi_sdi != -1)
		gpio_request(dev->pdata->gpio_spi_sdi, "spi_sdi");

	ecx336af_on(dev);

	dev->lcd = lcd_device_register("ecx336af-lcd", &pdev->dev, dev, &ecx336af_ops);

	if (IS_ERR(dev->lcd)) {
		ret = PTR_ERR(dev->lcd);
		dev->lcd = NULL;
		dev_info(&pdev->dev, "lcd device register error: %d\n", ret);
	} else {
		dev_info(&pdev->dev, "lcd device register success\n");
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	dev->early_suspend.suspend = ecx336af_early_suspend;
	dev->early_suspend.resume = ecx336af_early_resume;
	dev->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB;
	register_early_suspend(&dev->early_suspend);
#endif

	return 0;
}

static int __devinit ecx336af_remove(struct platform_device *pdev)
{

	struct ecx336af_data *dev = dev_get_drvdata(&pdev->dev);

	lcd_device_unregister(dev->lcd);

	ecx336af_off(dev);

	if (dev->pdata->power_1v8)
		regulator_put(dev->pdata->power_1v8);

	if (dev->pdata->gpio_reset != -1)
		gpio_free(dev->pdata->gpio_reset);
	if (dev->pdata->gpio_spi_cs != -1)
		gpio_free(dev->pdata->gpio_spi_cs);
	if (dev->pdata->gpio_spi_clk != -1)
		gpio_free(dev->pdata->gpio_spi_clk);
	if (dev->pdata->gpio_spi_sdo != -1)
		gpio_free(dev->pdata->gpio_spi_sdo);
	if (dev->pdata->gpio_spi_sdi != -1)
		gpio_free(dev->pdata->gpio_spi_sdi);

	dev_set_drvdata(&pdev->dev, NULL);
	kfree(dev);
	ecx336af_probin = 0;

	return 0;
}

static struct platform_driver ecx336af_driver = {
	.driver		= {
		.name	= "ecx336af-lcd",
		.owner	= THIS_MODULE,
	},
	.probe		= ecx336af_probe,
	.remove		= ecx336af_remove,
};

static int __init ecx336af_init(void)
{
	// register the panel with lcd drivers
	return platform_driver_register(&ecx336af_driver);;
}
module_init(ecx336af_init);

static void __exit ecx336af_exit(void)
{
	platform_driver_unregister(&ecx336af_driver);
}
module_exit(ecx336af_exit);

MODULE_DESCRIPTION("ecx336af lcd driver");
MODULE_AUTHOR("Kznan <Derric.kznan@ingenic.com>");
MODULE_LICENSE("GPL");
