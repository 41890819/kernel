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

#define AUTO_BLINK_MODE    0
#define CHN_OUTPUT_MODE    1

static struct i2c_client * sgm_client = NULL;

static int sgm31324_write_reg(struct i2c_client *client, unsigned char reg, unsigned char val)
{
	int ret = 0;
	unsigned char data[2];
	struct i2c_adapter *adap = client->adapter;
	struct i2c_msg msg;

	data[0] = reg;
	data[1] = val;

	msg.addr = client->addr;
	msg.flags = client->flags & I2C_M_TEN;
	msg.len = 2;
	msg.buf = data;

	ret = i2c_transfer(adap, &msg, 1);

	if (ret < 0) {
		printk("SGM31324 : i2c_master_send error : %d\n", ret);
	}

	return 0;
}

static unsigned char sgm31324_read_reg(struct i2c_client *client, unsigned char reg)
{
	int ret = 0;
	unsigned char data;

	struct i2c_adapter *adap = client->adapter;
	struct i2c_msg msg[2];

	msg[0].addr = client->addr;
	msg[0].flags = (client->flags & I2C_M_TEN) | I2C_M_REV_DIR_ADDR;
	msg[0].len = 1;
	msg[0].buf = &reg;

	msg[1].addr = client->addr;
	msg[1].flags = (client->flags & I2C_M_TEN) | I2C_M_REV_DIR_ADDR;
	msg[1].flags |= I2C_M_RD | I2C_M_NOSTART;
	msg[1].len = 1;
	msg[1].buf = &data;

	ret = i2c_transfer(adap, msg, 2);
	if (ret != 2) {
		printk("SGM31324 : i2c_master_recv error : %d\n", ret);
		data = 0;
	} else {
		//printk("SGM31324 : read reg %d value 0x%x\n", reg, data);
	}

	return data;
}

/* 
 * write REG9[0] : 1 - Enable, 0 - Disable
 * While REG9[0] == 1, Pull up EN pin, device will go into auto blink mode after 4 s
 */
static void set_autobink_mode(struct i2c_client *client, int enable)
{
	if (enable == 0) {
		sgm31324_write_reg(client, 0x9, 0x6);
	} else {
		sgm31324_write_reg(client, 0x9, 0x7);
	}
}

static void init_sgm31324_chip(struct i2c_client *client, int mode)
{
	sgm31324_write_reg(client, 0x0, 0x7); // Reset chip

	if (mode == AUTO_BLINK_MODE) {
		set_autobink_mode(client, 1);
	} else if (mode == CHN_OUTPUT_MODE) {
		set_autobink_mode(client, 0);

		sgm31324_write_reg(client, 0x1, 0x0E); // Flash period : 2.18 s
		sgm31324_write_reg(client, 0x2, 0x80); // Timer1 ON time : 50%
		sgm31324_write_reg(client, 0x3, 0x80); // Timer2 ON time : 50%

		// Set Rais and Fall ramp time duration
		sgm31324_write_reg(client, 0x5, 0x0); // 1.5 ms

		// Set D1 - D3 current (0 - 191 - 255) - (0.125 - 24 -- 24 mA)
		sgm31324_write_reg(client, 0x6, 0x40);
		sgm31324_write_reg(client, 0x7, 0x40);
		sgm31324_write_reg(client, 0x8, 0x40);

		// Config channel status
		sgm31324_write_reg(client, 0x4, 0x15);
	}
}

// Enable / Disable device output
void enable_sgm31324(int enable)
{
	if (sgm_client == NULL) {
		// SGM31324 Not Ready
	} else if (enable == 0) {
		sgm31324_write_reg(sgm_client, 0x4, 0x15);
	} else {
		sgm31324_write_reg(sgm_client, 0x4, 0x55);
	}
}
EXPORT_SYMBOL(enable_sgm31324);

void change_sgm31324_current(unsigned int cur_level, unsigned int tar_level)
{
	unsigned char data;
	int diff = cur_level - tar_level;
	//printk("cur_level:%d, tar_level:%d, diff:%d\n",cur_level, tar_level, diff);
	if (sgm_client == NULL) {
		printk("change sgm31324 current is err\n");
	}

	data = sgm31324_read_reg(sgm_client, 0x6);

	if((diff > 5) || (diff < -5)){
		sgm31324_write_reg(sgm_client, 0x6, (data - diff));
		sgm31324_write_reg(sgm_client, 0x7, (data - diff));
		sgm31324_write_reg(sgm_client, 0x8, (data - diff));
	}
}
EXPORT_SYMBOL(change_sgm31324_current);

static int sgm31324_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	init_sgm31324_chip(client, CHN_OUTPUT_MODE);

	sgm_client = client;

	return 0;
}

static int sgm31324_remove(struct i2c_client *client)
{
	sgm_client = NULL;

	return 0;
}

static const struct i2c_device_id sgm31324_i2c_id[] = {
    {"SGM31324", 0},
    {}
};

MODULE_DEVICE_TABLE(i2c, sgm31324_i2c_id);

static struct i2c_driver sgm31324_driver = {
	.driver		= {               
		.name	= "sgm31324",
		.owner	= THIS_MODULE,
	},
	.id_table       = sgm31324_i2c_id,
	.probe	        = sgm31324_probe,
	.remove	        = sgm31324_remove,
};

static int __init sgm31324_init(void)
{
	return i2c_add_driver(&sgm31324_driver);
}

static void __exit sgm31324_exit(void)
{
	i2c_del_driver(&sgm31324_driver);
}

module_init(sgm31324_init);
module_exit(sgm31324_exit);

MODULE_AUTHOR("Derrick.kznan");
MODULE_LICENSE("GPL");
