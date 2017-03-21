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

struct SSD1306_oled_data {
	struct lcd_device* lcd;
	struct regulator* power;
	struct i2c_client* client;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
        int backlight_pwm_enable;
};

#define SCL1 gpio_direction_output(GPIO_PE(3), 1)
#define SCL0 gpio_direction_output(GPIO_PE(3), 0)

#define SDA1 gpio_direction_output(GPIO_PE(0), 1)
#define SDA0 gpio_direction_output(GPIO_PE(0), 0)

static void start(){
  SCL1;
  SDA1;
  SDA0;
  SCL0;
}

static void stop(){
  SCL0;
  SDA0;
  SDA1;
  SCL1;
}

static void write_w(unsigned char dat){
  unsigned char m, da;
  unsigned char j;
  da = dat;
  for (j = 0; j < 8; j++){
    m = da;
    SCL0;
    m=m&0x80;
    if (m == 0x80){
      SDA1;
    }else{
      SDA0;
    }

    da = da << 1;
    SCL1;
  }

  SCL0;
  SCL1;
}

static void write_i(unsigned char ins){
  start();
  write_w(0x78);
  write_w(0x00);
  write_w(ins);
  stop();
}

static void write_d(unsigned char dat)
{
	udelay(50);
	start();
	write_w(0x78);
	write_w(0x40);
	write_w(dat);
	stop();
}

static int SSD1306_oled_write_reg(struct i2c_client *client, unsigned char reg, unsigned char val)
{
	int ret = 0;

	write_i(reg);
	
	return ret;
}

static unsigned char SSD1306_oled_read_reg(struct i2c_client *client, unsigned char reg)
{
     int ret = 0;          
     /* unsigned char value = 0; */

     /* struct i2c_msg msg[2]; */
     /* msg[0].addr  = client->addr; */
     /* msg[0].flags = 0; */
     /* msg[0].len   = 2; */
     /* msg[0].buf   = &reg; */

     /* msg[1].addr  = client->addr; */
     /* msg[1].flags = I2C_M_RD; */
     /* msg[1].len   = 1; */
     /* msg[1].buf   = &value; */
 
     /* ret = i2c_transfer(client->adapter,msg,2); */
     
     /* if(ret < 2){ */
     /*     dev_err(&client->dev, "i2c_master_read error : %d\n", ret); */
     /* 	 return -1; */
     /* } */
     
     /* printk("SSD1306_oled value is %d ret = %d\n",value,ret); */
     return ret;

}


static void Initial_IC(struct i2c_client *client)
{
}

static int SSD1306_oled_set_power(struct lcd_device *lcd, int power)
{
	return 0;
}       

static int SSD1306_oled_get_power(struct lcd_device *lcd)
{
	return 1;
}

static int SSD1306_oled_set_mode(struct lcd_device *lcd, struct fb_videomode *mode)
{
	return 0;
}

static struct lcd_ops SSD1306_oled_ops = {
	.set_power = SSD1306_oled_set_power,
	.get_power = SSD1306_oled_get_power,
	.set_mode  = SSD1306_oled_set_mode,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void SSD1306_oled_early_suspend(struct early_suspend *h)
{
        struct SSD1306_oled_data *dev = container_of(h, struct SSD1306_oled_data, early_suspend);

#ifdef CONFIG_BACKLIGHT_PWM_EN
	if (dev->backlight_pwm_enable >= 0){
		gpio_direction_output(dev->backlight_pwm_enable, 0);
	}
#endif

	if (dev->power)
		regulator_disable(dev->power);

#ifdef CONFIG_BOARD_COLDWAVE
	msleep(500);//Avoid splash screen when fast wake-up
#endif
}

static void SSD1306_oled_early_resume(struct early_suspend *h)
{
        struct SSD1306_oled_data *dev = container_of(h, struct SSD1306_oled_data, early_suspend);

/* 	if (dev->power) { */
/* 		regulator_set_voltage(dev->power, 1800000, 1800000); */
/* 		regulator_enable(dev->power); */
/* 	} */

/* 	Initial_IC(dev->client); */
	
/* #ifdef CONFIG_BACKLIGHT_PWM_EN */
/* 	if (dev->backlight_pwm_enable >= 0){ */
/* 		gpio_direction_output(dev->backlight_pwm_enable, 1); */
/* 	} */
/* #endif */

}
#endif

static ssize_t SSD1306_oled_iic_check_store(struct device *dev, struct device_attribute *attr,  const char *buf, size_t count){
  int i;
  printk("SSD1306_oled_iic_check_store in %d\n", count);

  for (i = 0; i < count; i++){
    printk("%d  %c\n", i, buf[i]);
  }
}

static ssize_t SSD1306_oled_iic_check_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
        

	/* if ((SSD1306_oled_write_reg(client, IIC_TEST_HIMAX7097_REG, IIC_TEST_HIMAX7097_DATA) == 2) && */
	/*     (SSD1306_oled_read_reg(client, IIC_TEST_HIMAX7097_REG) == IIC_TEST_HIMAX7097_DATA)) */

	/*       return sprintf(buf, "%d\n", 1); */
	/* else */
	/*       return sprintf(buf, "%d\n", 0); */
}

static DEVICE_ATTR(SSD1306_oled_iic_check, S_IRUGO | S_IWUSR, SSD1306_oled_iic_check_show, SSD1306_oled_iic_check_store);

static struct attribute *SSD1306_oled_attr_check[] = {
	&dev_attr_SSD1306_oled_iic_check.attr,
	NULL
};

static struct attribute_group m_lcd_gr = {
	.name = "SSD1306_oled_device_check",
	.attrs = SSD1306_oled_attr_check
};

static int SSD1306_oled_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	struct SSD1306_oled_data *dev = NULL;
	printk("SSD1306_oled_probe in\n");
	dev = kzalloc(sizeof(struct SSD1306_oled_data), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	gpio_request(GPIO_PE(0), "lcd_sda");
	gpio_request(GPIO_PE(3), "lcd_sck");
	/* while(1){ */
	/*   gpio_direction_output(GPIO_PE(0), 0); */
	/*   gpio_direction_output(GPIO_PE(3), 0); */
	/*   mdelay(1000); */
	/*   printk("set 0\n"); */
	/*   gpio_direction_output(GPIO_PE(0), 1); */
	/*   gpio_direction_output(GPIO_PE(3), 1); */
	/*   mdelay(1000); */
	/*   printk("set 1\n"); */
	/* } */

	dev->client = client;
	dev->power = regulator_get(&client->dev, "lcd");
	if (IS_ERR(dev->power)) {
		pr_err("Get lcd regulator failed\n");
		dev->power = NULL;
	}
   
        ret = sysfs_create_group(&client->dev.kobj, &m_lcd_gr);
	if (ret) 
		dev_err(&client->dev, "SSD1306_oled: device create file failed\n");
		
	i2c_set_clientdata(client, dev);

	Initial_IC(dev->client);

	dev->lcd = lcd_device_register("SSD1306_oled", &client->dev, dev, &SSD1306_oled_ops);
	if (IS_ERR(dev->lcd)) {
		ret = PTR_ERR(dev->lcd);
		dev->lcd = NULL;
		dev_info(&client->dev, "lcd device register error: %d\n", ret);
	} else {
		dev_info(&client->dev, "lcd device register success\n");
	}
    
#ifdef CONFIG_HAS_EARLYSUSPEND
	dev->early_suspend.suspend = SSD1306_oled_early_suspend;
	dev->early_suspend.resume = SSD1306_oled_early_resume;
	dev->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB;
	register_early_suspend(&dev->early_suspend);
#endif

	return ret;
}

static int __devinit SSD1306_oled_remove(struct i2c_client *client)
{
        struct SSD1306_oled_data *dev = i2c_get_clientdata(client);

	if (dev->power) {
		regulator_disable(dev->power);
		regulator_put(dev->power);
	}

	dev_set_drvdata(&client->dev, NULL);
	kfree(dev);

	return 0;
}

static const struct i2c_device_id SSD1306_oled_i2c_id[] = {
    {"SSD1306_oled", 0},
    {}
};

MODULE_DEVICE_TABLE(i2c, SSD1306_oled_i2c_id);

static struct i2c_driver SSD1306_oled_driver = {
	.driver		= {               
		.name	= "SSD1306_oled",
		.owner	= THIS_MODULE,
	},
	.id_table       = SSD1306_oled_i2c_id,
	.probe	        = SSD1306_oled_probe,
	.remove	        = SSD1306_oled_remove,
};

static int __init SSD1306_oled_init(void)
{
  printk("SSD1306_oled_init\n");
	return i2c_add_driver(&SSD1306_oled_driver);
}

static void __exit SSD1306_oled_exit(void)
{
  printk("SSD1306_oled_exit\n");
	i2c_del_driver(&SSD1306_oled_driver);
}

module_init(SSD1306_oled_init);
module_exit(SSD1306_oled_exit);

MODULE_DESCRIPTION("SSD1306 oled panel driver");
MODULE_AUTHOR("hpwang <wayne.hpwang@ingenic.com>");
MODULE_LICENSE("GPL");
