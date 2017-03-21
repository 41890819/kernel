/* #include <config.h> */
/* #include <serial.h> */
/* #include <debug.h> */
/* #include <common.h> */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/videodev2.h>
#include <media/v4l2-device.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-mediabus.h>
#include <mach/ovisp-v4l2.h>
#include "ovisp/ovisp-isp.h"
#include "dw9714a.h"

struct dw9714a_info {
	struct v4l2_subdev sd;
	/* struct dw9714a_format_struct *fmt; */
	/* struct dw9714a_win_setting *win; */
};

static const struct v4l2_subdev_ops dw9714a_ops = {
	/* .core = &dw9714a_core_ops, */
	/* .video = &dw9714a_video_ops, */
};

int dw9714a_read(struct v4l2_subdev *sd, unsigned short *value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	printk("read addr client->addr = %x\n",client->addr);
	printk("read addr client->adapter->name = %s\n",client->adapter->name);
	unsigned char buf[2] = {0,0};
	struct i2c_msg msg = {
			.addr	= client->addr,
			.flags	= I2C_M_RD | I2C_CMD_DATA_16BIT | I2C_CMD_NO_ADDR,
			.len	= 2,
			.buf	= buf,
	};
	int ret;

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;
	*value = ((u16)buf[0])<<8 + buf[1];
	return ret;
}

int dw9714a_write(struct v4l2_subdev *sd, unsigned short value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	printk("write addr client->addr = %x\n",client->addr);
	unsigned char buf[2] = {value >> 8 , value & 0xff};
	struct i2c_msg msg = {
		.addr	= client->addr,
		.flags	= I2C_CMD_DATA_16BIT | I2C_CMD_NO_ADDR,
		.len	= 2,
		.buf	= buf,
	};
	int ret;

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}

#if 1
static int dw9714a_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct dw9714a_info *info;
	int ret;
	int count = 1;
	unsigned short value;
	unsigned short input;
	/* value = 0x3333; */
	info = kzalloc(sizeof(struct dw9714a_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;
	v4l2_i2c_subdev_init(sd, client, &dw9714a_ops);

	input = 0xeca3;
	ret = dw9714a_write(sd, input);

	input = 0xa10d;
	ret = dw9714a_write(sd, input);

	input = 0xf2e8;
	ret = dw9714a_write(sd, input);

	input = 0xdc51;
	ret = dw9714a_write(sd, input);

	while(0){

	    input = 0x1111;
	    printk("reg input = %x\n",input);
	    ret = dw9714a_write(sd, input);
	    mdelay(2000);
	    ret = dw9714a_read(sd, &value);
	    if(ret < 0){
		printk("faluare ret = %d\n",ret);
		
	    }
	    else{
		printk("reg value = %x\n",value);
	    }

	    input = 0x2222;
	    printk("reg input = %x\n",input);
	    ret = dw9714a_write(sd, input);
	    mdelay(2000);
	    ret = dw9714a_read(sd, &value);
	    if(ret < 0){
		printk("faluare ret = %d\n",ret);
		
	    }
	    else{
		printk("reg input = %x\n",value);
	    }


	    input = 0x1111;
	    printk("reg input = %x\n",input);
	    ret = dw9714a_write(sd, input);
	    mdelay(2000);
	    ret = dw9714a_read(sd, &value);
	    if(ret < 0){
		printk("faluare ret = %d\n",ret);
		
	    }
	    else{
		printk("reg input = %x\n",value);
	    }

	}
	return 0;
}

#else
static int dw9714a_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
    gpio_request(GPIO_I2C4_SDA,NULL);
    gpio_request(GPIO_I2C4_SCL,NULL);

	unsigned short value;
	unsigned short input;

	printk(" take out , now write 0x eca3 to dw9714a in 2 seconds\n");
	input = 0xeca3;
	mdelay(2);
	dw9714a_IICWrite(input);

	printk(" take out , now write 0x a10d to dw9714a in 2 seconds\n");
	input = 0xa10d;
	mdelay(2);
	dw9714a_IICWrite(input);

	printk(" take out , now write 0x f2e8 to dw9714a in 2 seconds\n");
	input = 0xf2e8;
	mdelay(2);
	dw9714a_IICWrite(input);

	printk(" take out , now write 0x dc51 to dw9714a in 2 seconds\n");
	input = 0xdc51;
	mdelay(2);
	dw9714a_IICWrite(input);
    while (1){
	printk(" take out , now write 0x 0001 to dw9714a in 2 seconds\n");
	input = 0x0001;
	mdelay(2000);
	dw9714a_IICWrite(input);
	printk(" take out , now read to dw9714a in 2 seconds\n");
	mdelay(2000);
	dw9714a_IICRead(&value);
	printk("dw9714a value = %x \n",value);

	printk(" take out , now write 0x 1111 to dw9714a in 2 seconds\n");
	input = 0x1111;
	mdelay(2000);
	dw9714a_IICWrite(input);
	printk(" take out , now read to dw9714a in 2 seconds\n");
	mdelay(2000);
	dw9714a_IICRead(&value);
	printk("dw9714a value = %x \n",value);

	printk(" take out , now write 0x d221 to dw9714a in 2 seconds\n");
	input = 0x3221;
	mdelay(2000);
	dw9714a_IICWrite(input);
	printk(" take out , now read to dw9714a in 2 seconds\n");
	mdelay(2000);
	dw9714a_IICRead(&value);
	printk("dw9714a value = %x \n",value);

	printk(" take out , now write 0x 2031 to dw9714a in 2 seconds\n");
	input = 0x2031;
	mdelay(2000);
	dw9714a_IICWrite(input);
	printk(" take out , now read to dw9714a in 2 seconds\n");
	mdelay(2000);
	dw9714a_IICRead(&value);
	printk("dw9714a value = %x \n",value);

    }

	return 0;
}
#endif
static int dw9714a_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id dw9714a_id[] = {
	{ "dw9714a", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, dw9714a_id);

static struct i2c_driver dw9714a_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "dw9714a",
	},
	.probe		= dw9714a_probe,
	.remove		= dw9714a_remove,
	.id_table	= dw9714a_id,
};

static __init int init_dw9714a(void)
{
	printk("init_dw9714a #########\n");
	return i2c_add_driver(&dw9714a_driver);
}

static __exit void exit_dw9714a(void)
{
	i2c_del_driver(&dw9714a_driver);
}

module_init(init_dw9714a);
module_exit(exit_dw9714a);

MODULE_DESCRIPTION("DW9714A lens module driver");
MODULE_LICENSE("GPL");
