/******************************************************************************
 * isl29035.c - Linux kernel module
 * Create By Sakura.MaXing
 * It conclude the Test the device Function and Support the data channel
 * to the user,you can change the configure of the this device through the ioctl
 * ,also recieve the Light Sensor data.
 ******************************************************************************/

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/irq.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/workqueue.h>
#include "isl29035.h"

struct isl29035_data {
	struct i2c_client       *client;
	struct delayed_work	dwork;
	struct delayed_work	resumework;
	struct input_dev	*input;
	spinlock_t		lock;
        int  irq;
	int  enabled;
	int  half_range;
};

static int lux_range[4] = {1 * 1024, 4 * 1024, 16 * 1024, 64 * 1024};
static struct isl29035_data *attr_dev = NULL;
static struct kobject * light_sensor_kobj;

static int isl_write_reg(struct i2c_client *client, unsigned char addr, unsigned char value)
{
	unsigned char buf[2] = {addr, value};
	struct i2c_msg msg;
	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = buf;

	return i2c_transfer(client->adapter, &msg, 1);
}

static unsigned char isl_read_reg(struct i2c_client *client, unsigned char addr)
{
	unsigned int ret = 0;
	unsigned char value = 0;

        struct i2c_msg msg[2];
	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &addr;
	
        msg[1].addr  = client->addr;
        msg[1].flags = I2C_M_RD;
        msg[1].len   = 1;
        msg[1].buf   = &value;

        ret = i2c_transfer(client->adapter, msg, 2);
	if (ret < 2)
		return 0;

	return value;
}

static void isl_set_threshold_window(struct isl29035_data *dev, int value)
{
	unsigned int int_lt_value;
	unsigned int int_ht_value;

	if (value < dev->half_range)
		int_lt_value = 0;
	else
		int_lt_value = value - dev->half_range;

	if (value + dev->half_range > lux_range[LUX_RANGE])
		int_ht_value = lux_range[LUX_RANGE];
	else
		int_ht_value = value + dev->half_range;

	isl_write_reg(dev->client, 0x04, int_lt_value & 0xFF);
	isl_write_reg(dev->client, 0x05, (int_lt_value >> 8) & 0xFF);
	isl_write_reg(dev->client, 0x06, int_ht_value & 0xFF);
	isl_write_reg(dev->client, 0x07, (int_ht_value >> 8) & 0xFF);
}

static void isl_resume_handler(struct work_struct *work)
{
	  struct isl29035_data *dev = container_of(work, struct isl29035_data, resumework.work);
	  int ret = 0;

	  printk("isl_resume_handler in\n");
          dev->enabled = 0;
	  if (isl_write_reg(dev->client, 0x00, (WORK_MODE << 5) | PERSIST) < 0)
	    goto resume_fail;
	  if (isl_write_reg(dev->client, 0x01, (ADC_RES << 2) | LUX_RANGE) < 0)
	    goto resume_fail;
	  unsigned int lux_value = ((isl_read_reg(dev->client, 0x3) & 0xFF)<<8) | isl_read_reg(dev->client, 0x2);
	  isl_set_threshold_window(dev, lux_value);

	  enable_irq(dev->irq);
	  dev->enabled = 1;

	  return;

 resume_fail:
	  printk("resume fail\n");
	  __cancel_delayed_work(&dev->resumework);
	  schedule_delayed_work(&dev->resumework, HZ * 10);	  
}

static void isl_work_handler(struct work_struct *work)
{
	unsigned int lux_value = 0;
	struct isl29035_data *dev = container_of(work, struct isl29035_data, dwork.work);

	lux_value = ((isl_read_reg(dev->client, 0x3) & 0xFF)<<8) | isl_read_reg(dev->client, 0x2); 
	printk("lux_value:%d\n", lux_value);
	input_event(dev->input, EV_ABS, ABS_X, lux_value);
	input_sync(dev->input);

	// Set new limit window
	isl_set_threshold_window(dev, lux_value);

	// Clear interrupt through command-I
	lux_value = isl_read_reg(dev->client, 0x0);
	isl_write_reg(dev->client, 0x0, lux_value & (~(1 << 2)));
}

static irqreturn_t isl_handle_irq(int irq, void *dev_id)
{
	struct isl29035_data *dev = (struct isl29035_data *)dev_id; 
	unsigned long flags;
	
	spin_lock_irqsave(&dev->lock, flags);

	__cancel_delayed_work(&dev->dwork);
	schedule_delayed_work(&dev->dwork, 0);

	spin_unlock_irqrestore(&dev->lock, flags);

        return IRQ_HANDLED;
}

static void enable_isl29035(struct isl29035_data *dev)
{
	unsigned int lux_value = 0;

	if (dev->enabled)
		return;
	printk("enable_isl29035 in\n");
	isl_write_reg(dev->client, 0x00, (WORK_MODE << 5) | PERSIST);
	isl_write_reg(dev->client, 0x01, (ADC_RES << 2) | LUX_RANGE);
	lux_value = ((isl_read_reg(dev->client, 0x3) & 0xFF)<<8) | isl_read_reg(dev->client, 0x2); 
	isl_set_threshold_window(dev, lux_value);

	enable_irq(dev->irq);

	dev->enabled = 1;
}

static void disable_isl29035(struct isl29035_data *dev)
{
	if (!dev->enabled)
		return;
	printk("disable_isl29035 in\n");
	disable_irq(dev->irq);

	isl_write_reg(dev->client, 0x00, (POWER_DOWM << 5) | PERSIST);

	dev->enabled = 0;
}

static int isl29035_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	struct isl29035_data *dev = NULL;

	dev = kzalloc(sizeof(struct isl29035_data), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dev->client = client;
	i2c_set_clientdata(client, dev);

	INIT_DELAYED_WORK(&dev->dwork, isl_work_handler);
	INIT_DELAYED_WORK(&dev->resumework, isl_resume_handler);
     
	// Alloc I2C interrupt
	if (client->irq < 1) {
		dev_err(&client->dev, "No irq suport\n");
		return -1;
	} else {
		if (gpio_request(client->irq, "light_sensor") < 0){
			dev_err(&client->dev, "Request GPIO %d error!\n", client->irq);
			return -1;
		}

		if (gpio_direction_input(client->irq) < 0) {
			dev_err(&client->dev, "Config GPIO %d error!\n", client->irq);
			return -1;
		}

		dev->irq = gpio_to_irq(client->irq);
		if (dev->irq < 0) {
			dev_err(&client->dev, "GPIO to irq error!\n");
			return -1;
		}

		ret = request_irq(dev->irq, isl_handle_irq, IRQF_TRIGGER_FALLING | IRQF_DISABLED,
				  "isl29035_irq", dev);
		if(ret){
			printk(" The irq alloc failed! Please reloade!!\n");         
			goto err_irq_free;
		}

		// Masking isl29035 irq until to enable isl29035
		disable_irq(dev->irq);
	}

	// Alloc Input Device
	dev->input = input_allocate_device();
	if (!dev->input) {
		ret = -ENOMEM;
		goto err_irq_free;
	}

	dev->input->name = "light_sensor";

	__set_bit(EV_ABS, dev->input->evbit);
	input_set_abs_params(dev->input, ABS_X, 0, lux_range[LUX_RANGE], 0, 0);

	ret = input_register_device(dev->input);
	if (ret) {
		dev_err(&client->dev, "Input device register failed: %d\n", ret);
		goto err_input_free;
	}

	i2c_set_clientdata(client, dev);

	// Initial ISL29035
	printk("The isl chip ID is %#x\n", isl_read_reg(client, 0xF));
	dev->half_range = lux_range[LUX_RANGE] / (TOTAL_LEVEL * 2);
	dev->enabled = 0;
	attr_dev = dev;

        return 0;

 err_input_free:
	input_free_device(dev->input);

 err_irq_free:
	free_irq(dev->irq, dev);
	gpio_free(client->irq);
	kfree(dev);

	dev_err(&client->dev, "isl29035 probe error\n");

	return ret;
}

static int isl29035_remove(struct i2c_client *client)
{
	struct isl29035_data *dev = i2c_get_clientdata(client);

	free_irq(dev->irq, dev);

	input_unregister_device(dev->input);

	return 0;
}

#ifdef CONFIG_PM     
static int isl29035_suspend(struct i2c_client *client, pm_message_t mesg)
{
        struct isl29035_data *dev = i2c_get_clientdata(client);

	printk("isl29035_suspend in\n");

	if (dev->enabled){
	  __cancel_delayed_work(&dev->resumework);
	  disable_irq(dev->irq);

	  //isl_write_reg(dev->client, 0x00, (POWER_DOWM << 5) | PERSIST);
	}
	//disable_isl29035(dev);

        return 0; 
}

static int isl29035_resume(struct i2c_client *client)
{
	struct isl29035_data *dev = i2c_get_clientdata(client);
	printk("isl29035_resume in %d\n", dev->enabled);

	schedule_delayed_work(&dev->dwork, 0);
	// Initial ISL29035
	if (dev->enabled){
	  __cancel_delayed_work(&dev->resumework);
	  schedule_delayed_work(&dev->resumework, HZ * 5);
	}

        return 0;
}
#else
#define	isl29035_suspend 	NULL
#define isl29035_resume		NULL
#endif

static const struct i2c_device_id isl29035_id[] = {
	{ "isl29035", 0 },
	{ }
};

static struct i2c_driver isl29035_driver = {
	.driver = {
		.name	= "isl29035",
	},
	.probe			= isl29035_probe,
	.remove			= isl29035_remove,
	.id_table		= isl29035_id,
        .suspend		= isl29035_suspend,
	.resume			= isl29035_resume
};

static ssize_t enable_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	if (attr_dev)
		printk("Light sensor is %s\n", attr_dev->enabled ? "enabled" : "disabled");
	else
		printk("Light sensor not opened!\n");

	return 0;
}
static ssize_t enable_store(struct kobject *kobj, struct kobj_attribute *attr,
			    const char *buf, size_t count)
{
	unsigned int state = 0;
	if (attr_dev && (sscanf(buf, "%u", &state) == 1)) {
	  printk("enable_store %d\n", state);
		if (state)
			enable_isl29035(attr_dev);
		else
			disable_isl29035(attr_dev);
	} else
		printk("Light sensor not opened!\n");

	return count;
}
light_attr(enable);

static ssize_t delay_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return 0;
}
static ssize_t delay_store(struct kobject *kobj, struct kobj_attribute *attr,
			   const char *buf, size_t count)
{
	return count;
}
light_attr(delay);

static struct attribute * light_attr[] = {
	&enable_attr.attr,
	&delay_attr.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = light_attr,
};

static int __init isl29035_init(void)
{
	light_sensor_kobj = kobject_create_and_add("light_sensor", NULL);
	if (!light_sensor_kobj)
		return -ENOMEM;
	if (sysfs_create_group(light_sensor_kobj, &attr_group)) {
		printk("Create light sensor dev error!\n");
		kobject_del(light_sensor_kobj);
	}

	return i2c_add_driver(&isl29035_driver);
}

static void __exit isl29035_exit(void)
{
	sysfs_remove_group(light_sensor_kobj, &attr_group);
	kobject_del(light_sensor_kobj);

	i2c_del_driver(&isl29035_driver);
}

module_init(isl29035_init);
module_exit(isl29035_exit);

#ifdef CONFIG_HARDWARE_DETECT
/* Check ISL29035 status :
 * Read 0xF register, which indicate ISL devices ID
 */
static int hardwaredet_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	unsigned char value = isl_read_reg(client, 0xF);

	if ( (value & 0x38) == 0x28 )
		printk("Check ISL29035 OK\n");
	else
		printk("Check ISL29035 FAIL\n");

	return 0;
}

static struct i2c_driver isl29035_detect_driver = {
	.driver = {
		.name	= "isl29035",
	},
	.probe			= hardwaredet_probe,
	.id_table		= isl29035_id,
};

static int __init isl_detect_init(void)
{
        return i2c_add_driver(&isl29035_detect_driver);
}

hardwaredet_initcall(isl_detect_init);
#endif

MODULE_AUTHOR("Sakura MaXing");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("ISL29035 ambient light sensor driver");
MODULE_VERSION("V1.0");
