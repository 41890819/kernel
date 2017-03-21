/* linux/drivers/video/backlight/himax7069.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/ctype.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/lcd.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>
#include <mach/jz_dsim.h>

#define lcd_to_master(a)	(a->dsim_dev->master)
#define lcd_to_master_ops(a)	((lcd_to_master(a))->master_ops)

struct himax7069_data {
        unsigned int power;
        unsigned int id;
        struct device *dev;
	struct lcd_device* ld;
	struct regulator *lcd_vcc_reg;
	struct i2c_client* client;
	struct mipi_dsim_lcd_device *dsim_dev;
	struct lcd_platform_data *ddi_pd;
	struct mutex  	lock;
	bool  enabled;
	int gpio_vdda;
        int backlight_enable;

};

static void himax7069_regulator_enable(struct himax7069_data *lcd)
{
	struct lcd_platform_data *pd = NULL;

	pd = lcd->ddi_pd;
	mutex_lock(&lcd->lock);
	regulator_enable(lcd->lcd_vcc_reg);
	msleep(pd->power_on_delay);
	mutex_unlock(&lcd->lock);
}

static void himax7069_regulator_disable(struct himax7069_data *lcd)
{
	mutex_lock(&lcd->lock);
	regulator_disable(lcd->lcd_vcc_reg);
	mutex_unlock(&lcd->lock);
}

static void himax7069_sleep_in(struct himax7069_data *lcd)
{
	struct dsi_master_ops *ops = lcd_to_master_ops(lcd);
	struct dsi_cmd_packet data_to_send = { 0x05, 0x10, 0x00 };

	ops->cmd_write(lcd_to_master(lcd), data_to_send);
}

static void himax7069_sleep_out(struct himax7069_data *lcd)
{
	struct dsi_master_ops *ops = lcd_to_master_ops(lcd);
	struct dsi_cmd_packet data_to_send = { 0x05, 0x11, 0x01 };

	ops->cmd_write(lcd_to_master(lcd), data_to_send);
}

static void himax7069_display_on(struct himax7069_data *lcd)
{
	struct dsi_master_ops *ops = lcd_to_master_ops(lcd);
	struct dsi_cmd_packet data_to_send = { 0x05, 0x29, 0x01 };

	ops->cmd_write(lcd_to_master(lcd), data_to_send);
}

static void himax7069_display_off(struct himax7069_data *lcd)
{
	struct dsi_master_ops *ops = lcd_to_master_ops(lcd);
	struct dsi_cmd_packet data_to_send = { 0x05, 0x28, 0x00 };

	ops->cmd_write(lcd_to_master(lcd), data_to_send);
}

static int himax7069_set_power(struct lcd_device *ld, int power)
{
	int ret = 0;

	return ret;
}

static int himax7069_get_power(struct lcd_device *ld)
{
	struct himax7069_data *lcd = lcd_get_data(ld);

	return lcd->power;
}

static struct lcd_ops himax7069_lcos_ops = {
	.set_power = himax7069_set_power,
	.get_power = himax7069_get_power,
};


struct dsi_cmd_packet himax7069_cmd_list[] = {

	{0x39, 0x04, 0x00,  {0xFF, 0x70, 0x69, 0x00}}, 	//unlock panel

	{0x39, 0x19, 0x00,  {0xB5, 
		0x01, 0xFF, 0x01, 0xA3, 0x01, 0x6A, 0x01, 0x3B, 0x01, 0x0E, 0x01, 0x0C,
		0x00, 0x00, 0x00, 0x5C, 0x00, 0x95, 0x00, 0xC4, 0x00, 0xF1, 0x00, 0xFF }
	}, 	//Gamma for R

	{0x39, 0x19, 0x00,  {0xB6, 
		0x01, 0xFF, 0x01, 0xA3, 0x01, 0x6A, 0x01, 0x3B, 0x01, 0x0E, 0x01, 0x0C,
		0x00, 0x00, 0x00, 0x5C, 0x00, 0x95, 0x00, 0xC4, 0x00, 0xF1, 0x00, 0xFF }
	}, 	//Gamma for G

	{0x39, 0x19, 0x00,  {0xB7, 
		0x01, 0xFF, 0x01, 0xA3, 0x01, 0x6A, 0x01, 0x3B, 0x01, 0x0E, 0x01, 0x0C,
		0x00, 0x00, 0x00, 0x5C, 0x00, 0x95, 0x00, 0xC4, 0x00, 0xF1, 0x00, 0xFF }
	}, 	//Gamma for B
	{0x39, 0x06, 0x00, {0xB2, 0x01, 0x0B, 0x00, 0xFF, 0x31} }, 	//VCOM
	{0x39, 0x03, 0x00, {0xB4, 0xF9, 0x05}},		      		//Vring
	{0x05, 0xD2, 0x00},

};

static void himax7069_panel_condition_setting(struct himax7069_data *lcd)
{
	int i;
	struct dsi_master_ops *ops = lcd_to_master_ops(lcd);
	struct dsi_device *dsi = lcd_to_master(lcd);

	for (i = 0; i < ARRAY_SIZE(himax7069_cmd_list); i++) {
		ops->cmd_write(dsi, himax7069_cmd_list[i]);
	}

}

static void himax7069_set_sequence(struct mipi_dsim_lcd_device *dsim_dev)
{
	struct himax7069_data *lcd = dev_get_drvdata(&dsim_dev->dev);

	himax7069_panel_condition_setting(lcd);
	himax7069_sleep_out(lcd);
	msleep(300);	//datesheet requirement
	himax7069_display_on(lcd);
//	msleep(10);
	lcd->power = FB_BLANK_UNBLANK;
}

static int himax7069_probe(struct mipi_dsim_lcd_device *dsim_dev)
{
	int err = 0;
	struct himax7069_data *lcd = NULL;

	lcd = devm_kzalloc(&dsim_dev->dev, sizeof(struct himax7069_data), GFP_KERNEL);
	if (!lcd) {
		printk("failed to allocate himax7069 structure.\n");
		return -ENOMEM;
	}

	lcd->dsim_dev = dsim_dev;
	lcd->ddi_pd = (struct lcd_platform_data *)dsim_dev->platform_data;
	lcd->dev = &dsim_dev->dev;

	mutex_init(&lcd->lock);

	lcd->lcd_vcc_reg = regulator_get(NULL, "lcd");
	if (IS_ERR(lcd->lcd_vcc_reg)) {
		printk("failed to get regulator lcd\n");
		return PTR_ERR(lcd->lcd_vcc_reg);
	}

	lcd->ld = lcd_device_register("himax7069", lcd->dev, lcd, &himax7069_lcos_ops);
	if (IS_ERR(lcd->ld)) {
		printk("failed to register lcd ops.\n");
		return PTR_ERR(lcd->ld);
	}

	err = gpio_request(lcd->gpio_vdda, "mipi vdda gpio");
	if(err < 0){
		gpio_free(lcd->gpio_vdda);
		gpio_request(lcd->gpio_vdda,"mipi vdda gpio");
	}

#ifdef CONFIG_BACKLIGHT_PWM_EN
	if (lcd->backlight_enable >= 0){
		gpio_request(lcd->backlight_enable, "lcd_pwm_enable");
		gpio_direction_output(lcd->backlight_enable, 1);
	}
#endif
	
	if (err) {
                printk("can's request mipi vdda\n");
                return err;
        }

	dev_set_drvdata(&dsim_dev->dev, lcd);
	printk("himax7069 probe success!\n");

	return 0;
}

#ifdef CONFIG_PM
static void himax7069_power_on(struct mipi_dsim_lcd_device *dsim_dev, int power)
{
	struct himax7069_data *lcd = dev_get_drvdata(&dsim_dev->dev);

	/* lcd power on */
	if (power){
		himax7069_regulator_enable(lcd);
	}else{
		himax7069_regulator_disable(lcd);
	}

	if (lcd->ddi_pd->power_on)
		lcd->ddi_pd->power_on(lcd->ld, power);
	/* lcd reset */
	if (lcd->ddi_pd->reset)
		lcd->ddi_pd->reset(lcd->ld);
}

static int himax7069_suspend(struct mipi_dsim_lcd_device *dsim_dev)
{
	struct himax7069_data *lcd = dev_get_drvdata(&dsim_dev->dev);

#ifdef CONFIG_BACKLIGHT_PWM_EN
	if (lcd->backlight_enable >= 0){
		gpio_direction_output(lcd->backlight_enable, 0);
	}
#endif

	himax7069_display_off(lcd);
	himax7069_sleep_in(lcd);
	msleep(140);
	msleep(lcd->ddi_pd->power_off_delay);

	himax7069_power_on(dsim_dev, 0);
	himax7069_regulator_disable(lcd);

	return 0;
}

static int himax7069_resume(struct mipi_dsim_lcd_device *dsim_dev)
{
	struct himax7069_data *lcd = dev_get_drvdata(&dsim_dev->dev);

	himax7069_regulator_enable(lcd);
	himax7069_power_on(dsim_dev, 1);
	himax7069_display_on(lcd);

#ifdef CONFIG_BACKLIGHT_PWM_EN
	if (lcd->backlight_enable >= 0){
		gpio_direction_output(lcd->backlight_enable, 1);
	}
#endif

	return 0;
}

#else
#define himax7069_suspend		NULL
#define himax7069_resume		NULL
#endif

static struct mipi_dsim_lcd_driver himax7069_dsim_ddi_driver = {
	.name 			= "himax7069",
	.id 			= 0,
	.power_on               = himax7069_power_on,
	.set_sequence 	        = himax7069_set_sequence,
	.probe 			= himax7069_probe,
	.suspend 		= himax7069_suspend,
	.resume 		= himax7069_resume, 
};

static int himax7069_init(void)
{
	mipi_dsi_register_lcd_driver(&himax7069_dsim_ddi_driver);
	return 0;
}

static void himax7069_exit(void)
{
	return;
}


module_init(himax7069_init);
module_exit(himax7069_exit);

MODULE_DESCRIPTION("Himax7069 lcd driver");
MODULE_AUTHOR("Qmyang <yeoman.qmyang@ingenic.com>");
MODULE_LICENSE("GPL");
