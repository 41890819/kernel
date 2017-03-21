/*
 * Sp948t touchpad with I2C interface
 *
 * Copyright (C) 2009 Compulab, Ltd.
 * Mike Rapoport <mike@compulab.co.il>
 * Igor Grinberg <grinberg@compulab.co.il>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 0*/

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/pm.h>
#include <linux/tsc.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>

#define DRIVER_NAME		"sp948t_i2c"

#define debug(fmt, arg...)			printk(KERN_CRIT fmt, ##arg)
//#define debug(fmt, arg...)			printk(KERN_DEBUG fmt, ##arg)

/*
 * When in Polling mode and no data received for NO_DATA_THRES msecs
 * reduce the polling rate to NO_DATA_SLEEP_MSECS
 */
#define NO_DATA_THRES		(MSEC_PER_SEC)
#define NO_DATA_SLEEP_MSECS	(MSEC_PER_SEC / 4)

/* Control IRQ / Polling option */
static bool polling_req;

/* Control Polling Rate */
static int scan_rate = 80;

/* The main device structure */
struct sp948t_i2c {
	struct i2c_client	*client;
	struct input_dev	*input;
	struct delayed_work	dwork;
	struct regulator        *power;
	spinlock_t		lock;
	int			no_data_count;
	int			scan_rate_param;
	int			scan_ms;
        int                     irq;
};

#define  I2C_bLeft     0x1
#define  I2C_bRight    0x2
#define  I2C_bMiddle   0x4
#define  I2C_bFlag1    0x8
#define  I2C_bFlag2    0x80

struct sp948t_i2c_packet {
	unsigned char state; // Button output
	char dX;
	char dY;
	char dW;  // scroll output
	unsigned char state1; // According to test, always 0x00
	unsigned char state2; // According to test, always 0x38
	char checksum;
};

/*
 * Driver's initial design makes no race condition possible on i2c bus,
 * so there is no need in any locking.
 * Keep it in mind, while playing with the code.
 */

static bool sp948t_i2c_get_input(struct sp948t_i2c *touch)
{
	struct input_dev *input = touch->input;
	struct i2c_client *client = touch->client;
	struct sp948t_i2c_packet packet;
	char sum = 0;
	int ret = 0;

	if ( polling_req && (*(volatile unsigned int *)(0xB0010600)) & 0x400 ) {
		dev_err(&client->dev, "High level, no data\n");
		return false;
	}

	ret = i2c_master_recv(client, (char *) &packet, 7);
	if (ret < 0) {
		dev_err(&client->dev, "Recieve failed: %d\n", ret);
		return false;
	}

	if(!(packet.state && I2C_bFlag1)){
		ret = packet.state;
		// printk("packet.state is: %d\n", ret);
		// printk("Invalid data return false.\n");
		// return false;
      	}
	
	sum = packet.state + packet.dX + packet.dY + packet.dW + packet.state1 + packet.state2;
	if (packet.checksum != sum) {
		dev_err(&client->dev, "Check sum failed.\n");
		return false;
	}

	//printk("#################################################################\n");
	//printk("Packet : %4d %4d %4d %02x %02x %02x\n",
	//packet.dX, packet.dY, packet.dW, packet.state, packet.state1, packet.state2);

	/* /\* Report the button event *\/ */
	input_report_key(input, BTN_LEFT, packet.state & 0x1 );
	input_report_key(input, BTN_RIGHT, packet.state & 0x2 );
	input_report_key(input, BTN_MIDDLE, packet.dW);

	/* Report the deltas */
	input_report_rel(input, REL_X, packet.dX);
	input_report_rel(input, REL_Y, packet.dY);
	if (packet.state == 0x88)
		input_report_rel(input, REL_Z, 0x81);
	else
		input_report_rel(input, REL_Z, packet.state);
	input_sync(input);

	return !!ret;
}

static void sp948t_i2c_reschedule_work(struct sp948t_i2c *touch,
				       unsigned long delay)
{
	unsigned long flags;

	spin_lock_irqsave(&touch->lock, flags);

	/*
	 * If work is already scheduled then subsequent schedules will not
	 * change the scheduled time that's why we have to cancel it first.
	 */
	__cancel_delayed_work(&touch->dwork);
	schedule_delayed_work(&touch->dwork, delay);

	spin_unlock_irqrestore(&touch->lock, flags);
}

/* Control the Device polling rate / Work Handler sleep time */
static unsigned long sp948t_i2c_adjust_delay(struct sp948t_i2c *touch,
					     bool have_data)
{
	unsigned long delay, nodata_count_thres;

	delay = touch->scan_ms;
	if (have_data) {
		touch->no_data_count = 0;
	} else {
		nodata_count_thres = NO_DATA_THRES / touch->scan_ms;
		if (touch->no_data_count < nodata_count_thres)
			touch->no_data_count++;
		else
			delay = NO_DATA_SLEEP_MSECS;
	}

	return msecs_to_jiffies(delay);
}

/* Work Handler */
static void sp948t_i2c_work_handler(struct work_struct *work)
{
	bool have_data;
	unsigned long delay = 0;
	struct sp948t_i2c *touch = container_of(work, struct sp948t_i2c, dwork.work);

	have_data = sp948t_i2c_get_input(touch);

	/*
	 * While interrupt driven, there is no real need to poll the device.
	 * But touchpads are very sensitive, so there could be errors
	 * related to physical environment and the attention line isn't
	 * necessarily asserted. In such case we can lose the touchpad.
	 * We poll the device once in THREAD_IRQ_SLEEP_SECS and
	 * if error is detected, we try to reset and reconfigure the touchpad.
	 */
	if (polling_req) {
		delay = sp948t_i2c_adjust_delay(touch, have_data);
		sp948t_i2c_reschedule_work(touch, delay);
	}
}

static irqreturn_t sp948t_i2c_irq(int irq, void *dev_id)
{
	struct sp948t_i2c *touch = dev_id;

	if (!polling_req)
		sp948t_i2c_reschedule_work(touch, 0);

	return IRQ_HANDLED;
}

static int sp948t_i2c_open(struct input_dev *input)
{
	struct sp948t_i2c *touch = input_get_drvdata(input);

	if (polling_req)
		sp948t_i2c_reschedule_work(touch, msecs_to_jiffies(NO_DATA_SLEEP_MSECS));

	return 0;
}

static void sp948t_i2c_close(struct input_dev *input)
{
	struct sp948t_i2c *touch = input_get_drvdata(input);

	cancel_delayed_work_sync(&touch->dwork);
}

static void sp948t_i2c_set_input_params(struct sp948t_i2c *touch)
{
	struct input_dev *input = touch->input;

	input->name = "touchpanel";
	input->phys = touch->client->adapter->name;
	input->id.bustype = BUS_I2C;
	input->id.version = 0x0001;
	input->dev.parent = &touch->client->dev;
	input->open = sp948t_i2c_open;
	input->close = sp948t_i2c_close;
	input_set_drvdata(input, touch);

	/* Register the device as mouse */
	__set_bit(EV_REL, input->evbit);
	__set_bit(REL_X, input->relbit);
	__set_bit(REL_Y, input->relbit);
	__set_bit(REL_Z, input->relbit);

	/* /\* Register device's buttons and keys *\/ */
	__set_bit(EV_KEY, input->evbit);
	__set_bit(BTN_LEFT, input->keybit);
	__set_bit(BTN_RIGHT, input->keybit);
}

static inline void set_scan_rate(struct sp948t_i2c *touch, int scan_rate)
{
	touch->scan_ms = MSEC_PER_SEC / scan_rate;
	touch->scan_rate_param = scan_rate;
}

static struct sp948t_i2c *sp948t_i2c_touch_create(struct i2c_client *client)
{
	struct sp948t_i2c *touch;

	touch = kzalloc(sizeof(struct sp948t_i2c), GFP_KERNEL);
	if (!touch)
		return NULL;

	touch->irq = -1;
	touch->client = client;
	set_scan_rate(touch, 30);
	INIT_DELAYED_WORK(&touch->dwork, sp948t_i2c_work_handler);
	spin_lock_init(&touch->lock);
	touch->power = regulator_get(NULL, "touchpanel");
	if (IS_ERR(touch->power)) {
	        dev_err(&client->dev, "Get touchpanel device failed\n");
		touch->power = NULL;
	} else
		regulator_enable(touch->power);

	return touch;
}

static int __devinit sp948t_i2c_probe(struct i2c_client *client,
				      const struct i2c_device_id *dev_id)
{
	int ret = 0;
	struct sp948t_i2c *touch;

	// printk("\n\nsp948t_i2c_probe start\n");
	
	touch = sp948t_i2c_touch_create(client);
	
	if (!touch)
		return -ENOMEM;

	if (client->irq < 1)
		polling_req = true;
	else {
		if (gpio_request(client->irq, "sp948t_irq") < 0){
			dev_err(&client->dev, "Request GPIO %d error!\n", client->irq);
			polling_req = true;
		}

		if (gpio_direction_input(client->irq) < 0) {
			dev_err(&client->dev, "Config GPIO %d error!\n", client->irq);
			polling_req = true;
		}

		touch->irq = gpio_to_irq(client->irq);
		if (touch->irq < 0) {
			dev_err(&client->dev, "GPIO to irq error!\n");
			polling_req = true;
		}
	}

	touch->input = input_allocate_device();
	if (!touch->input) {
		ret = -ENOMEM;
		goto err_mem_free;
	}

	sp948t_i2c_set_input_params(touch);

	if (!polling_req) {
		ret = request_irq(touch->irq, sp948t_i2c_irq,
				  IRQF_DISABLED | IRQF_TRIGGER_FALLING,
				  DRIVER_NAME, touch);
		if (ret) {
			dev_err(&client->dev, "IRQ request failed: %d, falling back to polling\n", ret);
			polling_req = true;
		}
	}

	if (polling_req)
		dev_err(&client->dev, "Using polling at rate: %d times/sec\n", scan_rate);

	/* Register the device in input subsystem */
	ret = input_register_device(touch->input);
	if (ret) {
		dev_err(&client->dev, "Input device register failed: %d\n", ret);
		goto err_input_free;
	}

	i2c_set_clientdata(client, touch);

	// printk("sp948t_i2c_probe done OK\n\n");

	return 0;

 err_input_free:
	input_free_device(touch->input);

 err_mem_free:
	if (touch->power) {
		regulator_disable(touch->power);
		regulator_put(touch->power);
	}

	kfree(touch);

	dev_err(&client->dev, "sp948t_i2c_probe done ERROR\n\n");

	return ret;
}

static int __devexit sp948t_i2c_remove(struct i2c_client *client)
{
	struct sp948t_i2c *touch = i2c_get_clientdata(client);

	if (!polling_req)
		free_irq(client->irq, touch);

	input_unregister_device(touch->input);

	if (touch->power) {
		regulator_disable(touch->power);
		regulator_put(touch->power);
	}

	kfree(touch);

	return 0;
}

#ifdef CONFIG_PM
static int sp948t_i2c_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sp948t_i2c *touch = i2c_get_clientdata(client);

	if (polling_req)
		cancel_delayed_work_sync(&touch->dwork);

	if (touch->power)
		regulator_disable(touch->power);

	return 0;
}

static int sp948t_i2c_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct sp948t_i2c *touch = i2c_get_clientdata(client);

	if (touch->power)
		regulator_enable(touch->power);

	if (polling_req)
		sp948t_i2c_reschedule_work(touch, msecs_to_jiffies(NO_DATA_SLEEP_MSECS));
	
	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(sp948t_i2c_pm, sp948t_i2c_suspend, sp948t_i2c_resume);

static const struct i2c_device_id sp948t_i2c_id_table[] = {
	{ "sp948t_i2c", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, sp948t_i2c_id_table);

static struct i2c_driver sp948t_i2c_driver = {
	.driver = {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
		.pm	= &sp948t_i2c_pm,
	},

	.probe		= sp948t_i2c_probe,
	.remove		= __devexit_p(sp948t_i2c_remove),

	.id_table	= sp948t_i2c_id_table,
};

static int __init sp948t_i2c_init(void)
{
	return i2c_add_driver(&sp948t_i2c_driver);
}

static void __exit sp948t_i2c_exit(void)
{
	i2c_del_driver(&sp948t_i2c_driver);
}

module_init(sp948t_i2c_init);
module_exit(sp948t_i2c_exit);

MODULE_DESCRIPTION("Sp948t I2C touchpad driver");
MODULE_AUTHOR("Kznan");
MODULE_LICENSE("GPL");

