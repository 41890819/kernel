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

#define IIC_TEST_HIMAX7097_REG  0x2C
#define IIC_TEST_HIMAX7097_DATA 0xF5
struct himax7097_data {
	struct lcd_device* lcd;
	struct regulator* power;
	struct i2c_client* client;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
        int backlight_pwm_enable;
};

static int himax7097_write_reg(struct i2c_client *client, unsigned char reg, unsigned char val)
{
	int ret;
	unsigned char msg_buf[2];

	msg_buf[0] = reg;
	msg_buf[1] = val;
        
        struct i2c_msg msg;
	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = msg_buf;

        ret = i2c_transfer(client->adapter, &msg, 1);
        if(ret < 0)
          dev_err(&client->dev, "i2c_master_send error : %d\n", ret);
        
        return (ret == 1) ? 2 : ret; 
	
}

static unsigned char himax7097_read_reg(struct i2c_client *client, unsigned char reg)
{
     int ret = 0;          
     unsigned char value = 0;

     struct i2c_msg msg[2];
     msg[0].addr  = client->addr;
     msg[0].flags = 0;
     msg[0].len   = 1;
     msg[0].buf   = &reg;

     msg[1].addr  = client->addr;
     msg[1].flags = I2C_M_RD;
     msg[1].len   = 1;
     msg[1].buf   = &value;
 
     ret = i2c_transfer(client->adapter,msg,2);
     
     if(ret < 2){
         dev_err(&client->dev, "i2c_master_read error : %d\n", ret);
	 return -1;
     }
     
     printk("himax7097 value is %d ret = %d\n",value,ret);
     return value;

}


static void Initial_IC(struct i2c_client *client)
{
	himax7097_write_reg(client, 0x00, 0x04 );
#if (defined(CONFIG_VIDEO_HIMAX7097_SECOND_UPSIDE_DOWN) && defined(CONFIG_VIDEO_HIMAX7097_SECOND_HORIZONTAL_EXCHANGE))
	himax7097_write_reg(client, 0x01, 0x02 );
#elif defined(CONFIG_VIDEO_HIMAX7097_SECOND_HORIZONTAL_EXCHANGE)
	himax7097_write_reg(client, 0x01, 0x10 );
#elif defined(CONFIG_VIDEO_HIMAX7097_SECOND_UPSIDE_DOWN)
	himax7097_write_reg(client, 0x01, 0x13 );
#else
	himax7097_write_reg(client, 0x01, 0x11 );
#endif
	himax7097_write_reg(client, 0x02, 0x04 );
	himax7097_write_reg(client, 0x03, 0x01 );
	himax7097_write_reg(client, 0x04, 0x0f );

	/* himax7097_write_reg(client, 0x05, 0xf5 ); */
	/* himax7097_write_reg(client, 0x06, 0x55 ); */
	/* himax7097_write_reg(client, 0x07, 0x55 ); */
	/* himax7097_write_reg(client, 0x08, 0x55 ); */

	/* R */
	himax7097_write_reg(client, 0x09, 0xFF );
	himax7097_write_reg(client, 0x0A, 0x96 );
	himax7097_write_reg(client, 0x0B, 0x78 );
	himax7097_write_reg(client, 0x0C, 0x5A );
	himax7097_write_reg(client, 0x0D, 0x3C );
	himax7097_write_reg(client, 0x0E, 0x0A );
					      
	himax7097_write_reg(client, 0x0F, 0x00 );
	himax7097_write_reg(client, 0x10, 0x69 );
	himax7097_write_reg(client, 0x11, 0x87 );
	himax7097_write_reg(client, 0x12, 0xA5 );
	himax7097_write_reg(client, 0x13, 0xC3 );
	himax7097_write_reg(client, 0x14, 0xF5 );
	/* G */
	himax7097_write_reg(client, 0x15, 0xFF );
	himax7097_write_reg(client, 0x16, 0x96 );
	himax7097_write_reg(client, 0x17, 0x78 );
	himax7097_write_reg(client, 0x18, 0x5A );
	himax7097_write_reg(client, 0x19, 0x3C );
	himax7097_write_reg(client, 0x1A, 0x0A );
					      
	himax7097_write_reg(client, 0x1B, 0x00 );
	himax7097_write_reg(client, 0x1C, 0x69 );
	himax7097_write_reg(client, 0x1D, 0x87 );
	himax7097_write_reg(client, 0x1E, 0xA5 );
	himax7097_write_reg(client, 0x1F, 0xC3 );
	himax7097_write_reg(client, 0x20, 0xF5 );
	/* B */
	himax7097_write_reg(client, 0x21, 0xFF );
	himax7097_write_reg(client, 0x22, 0x96 );
	himax7097_write_reg(client, 0x23, 0x78 );
	himax7097_write_reg(client, 0x24, 0x5A );
	himax7097_write_reg(client, 0x25, 0x3C );
	himax7097_write_reg(client, 0x26, 0x0A );
					      
	himax7097_write_reg(client, 0x27, 0x00 );
	himax7097_write_reg(client, 0x28, 0x69 );
	himax7097_write_reg(client, 0x29, 0x87 );
	himax7097_write_reg(client, 0x2A, 0xA5 );
	himax7097_write_reg(client, 0x2B, 0xC3 );
	himax7097_write_reg(client, 0x2C, 0xF5 );
}

static int himax7097_set_power(struct lcd_device *lcd, int power)
{
	return 0;
}       

static int himax7097_get_power(struct lcd_device *lcd)
{
	return 1;
}

static int himax7097_set_mode(struct lcd_device *lcd, struct fb_videomode *mode)
{
	return 0;
}

static struct lcd_ops himax7097_ops = {
	.set_power = himax7097_set_power,
	.get_power = himax7097_get_power,
	.set_mode  = himax7097_set_mode,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void himax7097_early_suspend(struct early_suspend *h)
{
        struct himax7097_data *dev = container_of(h, struct himax7097_data, early_suspend);

	if (dev->power)
		regulator_disable(dev->power);

#ifdef CONFIG_BOARD_COLDWAVE
	msleep(500);//Avoid splash screen when fast wake-up
#endif
}

static void himax7097_early_resume(struct early_suspend *h)
{
        struct himax7097_data *dev = container_of(h, struct himax7097_data, early_suspend);

	if (dev->power) {
		regulator_set_voltage(dev->power, 1800000, 1800000);
		regulator_enable(dev->power);
	}

	Initial_IC(dev->client);
	
}
#endif

static ssize_t himax7097_iic_check_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
        

	if ((himax7097_write_reg(client, IIC_TEST_HIMAX7097_REG, IIC_TEST_HIMAX7097_DATA) == 2) &&
	    (himax7097_read_reg(client, IIC_TEST_HIMAX7097_REG) == IIC_TEST_HIMAX7097_DATA))

	      return sprintf(buf, "%d\n", 1);
	else
	      return sprintf(buf, "%d\n", 0);
}

static DEVICE_ATTR(himax7097_iic_check, S_IRUGO | S_IWUSR, himax7097_iic_check_show, NULL);

static struct attribute *himax7097_attr_check[] = {
	&dev_attr_himax7097_iic_check.attr,
	NULL
};

static struct attribute_group m_lcd_gr = {
	.name = "himax7097_device_check",
	.attrs = himax7097_attr_check
};

static int himax7097_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	struct himax7097_data *dev = NULL;
	printk("himax7097_probe_2 in\n");
	dev = kzalloc(sizeof(struct himax7097_data), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	printk("i2c addr:%d name:%s\n", client->addr, client->name);

	dev->client = client;
	dev->power = regulator_get(&client->dev, "lcd");
	if (IS_ERR(dev->power)) {
		pr_err("Get lcd regulator failed\n");
		dev->power = NULL;
	}
   
        ret = sysfs_create_group(&client->dev.kobj, &m_lcd_gr);
	if (ret) 
		dev_err(&client->dev, "himax7097: device create file failed\n");

	i2c_set_clientdata(client, dev);

	Initial_IC(dev->client);

	dev->lcd = lcd_device_register("himax7097_2", &client->dev, dev, &himax7097_ops);
	if (IS_ERR(dev->lcd)) {
		ret = PTR_ERR(dev->lcd);
		dev->lcd = NULL;
		dev_info(&client->dev, "lcd device register error: %d\n", ret);
	} else {
		dev_info(&client->dev, "lcd device register success\n");
	}
    
#ifdef CONFIG_HAS_EARLYSUSPEND
	dev->early_suspend.suspend = himax7097_early_suspend;
	dev->early_suspend.resume = himax7097_early_resume;
	dev->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB;
	register_early_suspend(&dev->early_suspend);
#endif

	return ret;
}

static int __devinit himax7097_remove(struct i2c_client *client)
{
        struct himax7097_data *dev = i2c_get_clientdata(client);

	if (dev->power) {
		regulator_disable(dev->power);
		regulator_put(dev->power);
	}

	dev_set_drvdata(&client->dev, NULL);
	kfree(dev);

	return 0;
}

static const struct i2c_device_id himax7097_i2c_id[] = {
    {"himax7097_2", 0},
    {}
};

MODULE_DEVICE_TABLE(i2c, himax7097_i2c_id);

static struct i2c_driver himax7097_second_driver = {
	.driver		= {               
		.name	= "himax7097_2",
		.owner	= THIS_MODULE,
	},
	.id_table       = himax7097_i2c_id,
	.probe	        = himax7097_probe,
	.remove	        = himax7097_remove,
};

static int __init himax7097_second_lcd_init(void)
{
	return i2c_add_driver(&himax7097_second_driver);
}

static void __exit himax7097_second_lcd_exit(void)
{
	i2c_del_driver(&himax7097_second_driver);
}

module_init(himax7097_second_lcd_init);
module_exit(himax7097_second_lcd_exit);

MODULE_DESCRIPTION("HIMAX7097 lcd panel driver");
MODULE_AUTHOR("Kznan <Derric.kznan@ingenic.com>");
MODULE_LICENSE("GPL");
