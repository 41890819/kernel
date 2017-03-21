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
#include <linux/gpio.h>
#include <linux/circ_buf.h>

//#define	DEBUG_CP2615
#ifdef 	DEBUG_CP2615
#define	cp_debug(fmt, arg...)		printk(KERN_CRIT fmt, ##arg)
#else
#define	cp_debug(fmt, arg...)		printk(KERN_DEBUG fmt, ##arg)
#endif

#define DRIVER_NAME		        "cp2615"
#define IRQ_NAME		        "cp2615_irq"

#define addr_16_bit                     0x8000

#define TOUCH_MODE_IDEL                 0
#define TOUCH_MODE_PRES                 1
#define TOUCH_ACTION_PRES               0x81
#define TOUCH_ACTION_UP                 0x02
#define TOUCH_ACTION_UPSLIP             0x04
#define TOUCH_ACTION_DOWNSLIP           0x08

#define MCU_FIFO_MAX                    16
#define TOUCH_BUFF_MAX_IDX              48
#define TOUCH_BUFF_END_FLAG             0x7F

#define DEBUG_REG_SPACE                 0xb3429280
#define TP_TCSM_BASE_ADDR               0xb3429000
#define TP_TCSM_MCU_WP	                (TP_TCSM_BASE_ADDR + 0x120)
#define TP_TCSM_CPU_RP	                (TP_TCSM_MCU_WP + 0x04)
#define TP_TCSM_I2C_DATA(n)	        (TP_TCSM_CPU_RP + 0x04 + (n) * 4 * 3)
#define REG32(addr)	                *((volatile unsigned int *)(addr))
#define TP_ABS(a,b)                     (a>b?a-b:b-a)

struct i2c_client                       *client_global;

struct cp2615_i2c {
	struct i2c_client               *client;
	struct input_dev	        *input;
	struct regulator                *power;
	struct delayed_work	        cwork;
	struct work_struct	        dwork;
	struct circ_buf                 fifo;
	int                             irq;
	int                             irq_gpio;
	int                             touch_mode;
	int                             last_x;
	int                             first_x;
	int                             firstsec;
	int                             firstmsec;
	int                             longflag;
	//char                          xbuf[TOUCH_BUFF_MAX_IDX];
	//int                           startpos;
	//int                           endpos;
	spinlock_t		        lock;
	struct workqueue_struct         *i2c_workqueue;
};

s32 cp2615_i2c_read_tsdata(struct i2c_client *client, u8 *data, u8 length)
{
	struct i2c_msg msg;
	msg.addr = client->addr;
	msg.flags = I2C_M_RD;
	msg.len = length;
	msg.buf = data;
	return i2c_transfer(client->adapter, &msg, 1);
}

s32 cp2615_i2c_write_tsdata(struct i2c_client *client, u8 *data, u8 length)
{
	struct i2c_msg msg;
	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = length;
	msg.buf = data;
	return i2c_transfer(client->adapter, &msg, 1);
}

void master_write_i2c_reg(struct i2c_client *client, u16 addr, u16 reg_data)
{
	unsigned char data[4] = {0, 0, 0, 0};
	unsigned int len = 0;

	if (addr & addr_16_bit) {
		data[len++] = ((addr >> 8) & 0x7f);
	}
	data[len++] = (addr & 0xff);

	data[len++] = (reg_data >> 8) & 0xff;
	data[len++] = (reg_data & 0xff);
	
	cp2615_i2c_write_tsdata(client, data, len);

	return ;
}

void master_read_i2c_reg(struct i2c_client *client, u16 addr, u8 *ret_data, u8 length)
{
	
	unsigned char data[4];
	unsigned int len = 0;

	if (addr & addr_16_bit) {
		data[len++] = ((addr >> 8) & 0x7f);
	}
	data[len++] = (addr & 0xff);

	cp2615_i2c_write_tsdata(client, data, len);
	
	cp2615_i2c_read_tsdata(client, ret_data, length);
	
	return ;
}

static void updater_touch(struct cp2615_i2c *touch, u8 x , int y, int z)
{
	struct input_dev *input = touch->input;

	input_report_rel(input, REL_X, x);
	input_report_rel(input, REL_Y, y);
	input_report_rel(input, REL_Z, z);
	input_sync(input);

	return ;
}

static void circle_upload_34reg_data_work_handler(struct work_struct *work)
{	

	struct cp2615_i2c *touch = container_of(work, struct cp2615_i2c, cwork.work);

	u8 ret_data[2] = { 0, 0 };
	unsigned long flags;

	master_read_i2c_reg(client_global, 0x0034, ret_data, 2);
	if ((touch->touch_mode != TOUCH_MODE_IDEL) && (touch->touch_mode == TOUCH_MODE_PRES) && (ret_data[1] != 00)) {
		if (ret_data[1]){
			if (touch->last_x == 0){
				touch->last_x = ret_data[1];
			}
			int diff_x = ret_data[1] - touch->last_x;
			updater_touch(touch, diff_x, 0, TOUCH_ACTION_PRES);
			touch->last_x = ret_data[1];
		}	
	}

	spin_lock_irqsave(&touch->lock, flags);

	__cancel_delayed_work(&touch->cwork);
	if ((touch->touch_mode != TOUCH_MODE_IDEL) && (touch->touch_mode == TOUCH_MODE_PRES)) {
		schedule_delayed_work(&touch->cwork, 1);
	}
	spin_unlock_irqrestore(&touch->lock, flags);

	return ;
}

/*irq from mcu*/
static void cp2615_work_handler(struct work_struct *work)
{
  	struct cp2615_i2c *touch = container_of(work, struct cp2615_i2c, dwork);
	struct timespec ts;
	
	int z = 0;
	int intctl = 0;
	int circ_cnt_num = 0;
	unsigned long flags;
	char ret_data[32][2] = { {0}, {0} };

	touch->fifo.head = REG32(TP_TCSM_MCU_WP);
        circ_cnt_num = CIRC_CNT(touch->fifo.head, touch->fifo.tail, 16);

	do {
		ret_data[1][1] = REG32(TP_TCSM_I2C_DATA(touch->fifo.tail) + 0);
		ret_data[2][1] = REG32(TP_TCSM_I2C_DATA(touch->fifo.tail) + 4);
		ret_data[3][1] = REG32(TP_TCSM_I2C_DATA(touch->fifo.tail) + 8);
				
		cp_debug("cpu read 03:%d 31:%d 34:%d\n", ret_data[1][1], ret_data[2][1], ret_data[3][1]);

		touch->fifo.tail ++;
		
		if (touch->fifo.tail == 16) {
			touch->fifo.tail = 0;
		}

		if (ret_data[1][1] == 1) {
			z = TOUCH_ACTION_PRES;
			spin_lock_irqsave(&touch->lock, flags);

			ts = current_kernel_time();
			if ((touch->touch_mode == TOUCH_MODE_PRES) &&(ret_data[3][1] != 0)) {
				if (touch->last_x == 0){
					touch->first_x = ret_data[3][1];
					cp_debug("first_x %d\n", touch->first_x);
					touch->firstmsec = ts.tv_nsec / 1000000;
					touch->firstsec = ts.tv_sec;
					//touch->xbuf[touch->endpos++] = 0;//first diffx is zero
				} else {
					int diff_x = ret_data[3][1] - touch->last_x;
					//touch->xbuf[touch->endpos++] = diff_x;
					//if (touch->endpos >= TOUCH_BUFF_MAX_IDX)
					//touch->endpos = 0;

					int duration = (ts.tv_sec - touch->firstsec) * 1000 + ts.tv_nsec / 1000000 - touch->firstmsec;
					if (duration > 680){
						if (touch->longflag == 0){
							touch->longflag = 1;
							updater_touch(touch, 0, 0, TOUCH_ACTION_PRES); //first update
							updater_touch(touch, ret_data[3][1] - touch->first_x, 0, TOUCH_ACTION_PRES); //first update
						} else {
							updater_touch(touch, diff_x, 0, TOUCH_ACTION_PRES);
						}
					}
				}

				touch->last_x = ret_data[3][1];
				//printk("sec:%ld nsec:%ld\n", ts.tv_sec, ts.tv_nsec);
				//updater_touch(touch, diff_x, 0, TOUCH_ACTION_PRES);
			}
			spin_unlock_irqrestore(&touch->lock, flags);
		} else if ((ret_data[1][1] == 2) && (ret_data[2][1] == 0 )) {
			z = TOUCH_ACTION_UP;
			__cancel_delayed_work(&touch->cwork);
		} else if (/* (ret_data[1][1] == 2) && */(ret_data[2][1] == 1 )) {
			z = TOUCH_ACTION_UPSLIP;
			//printk("z = %d\n", z);
			__cancel_delayed_work(&touch->cwork);
		} else if (/* (ret_data[1][1] == 2) && */(ret_data[2][1] == 2 )) {
			z = TOUCH_ACTION_DOWNSLIP;
			__cancel_delayed_work(&touch->cwork);
		}

		if (touch->touch_mode == TOUCH_MODE_IDEL) {
			if (z != TOUCH_ACTION_PRES) {
				cp_debug("__end\n");
				return ;
			}
			touch->touch_mode = TOUCH_MODE_PRES;
			touch->last_x = 0;
			touch->first_x = 0;
			touch->longflag = 0;
			if (ret_data[3][1] != 0) {
				//cp_debug("pres----------x = %02d z = %02x\n", ret_data[3][1], ret_data[1][1]);
				touch->last_x = ret_data[3][1];
				touch->first_x = ret_data[3][1];
				cp_debug("first_x = %d\n", touch->first_x);
				ts = current_kernel_time();
				//printk("sec:%ld nsec:%ld\n", ts.tv_sec, ts.tv_nsec);
				touch->firstsec = ts.tv_sec;
				touch->firstmsec = ts.tv_nsec / 1000000;
				//touch->xbuf[touch->endpos++] = 0; //first diffx is zero  hpwang
				//updater_touch(touch, 0, 0, z);
			}
		} else if (touch->touch_mode == TOUCH_MODE_PRES) {
			if (z != TOUCH_ACTION_PRES) {

				touch->touch_mode = TOUCH_MODE_IDEL;
				ts = current_kernel_time();
				//printk("sec:%ld nsec:%ld\n", ts.tv_sec, ts.tv_nsec);
				int duration = (ts.tv_sec - touch->firstsec) * 1000 + ts.tv_nsec / 1000000 - touch->firstmsec;
				//printk("%ld %ld %ld %ld\n", ts.tv_sec, touch->firstsec, ts.tv_nsec, touch->firstmsec);
				if (ret_data[3][1]){
					touch->last_x = ret_data[3][1];
				}
				cp_debug("duration:%d x:%d first:%d end:%d\n", duration, TP_ABS(touch->last_x, touch->first_x), touch->first_x, touch->last_x);

				if (duration < 680 || duration > 46196083){//return guest event
					int curr_x = touch->last_x;
					if (TP_ABS(curr_x, touch->first_x) >= 5 && touch->firstsec != 0){
						if (curr_x > touch->first_x){
							cp_debug("right\n");
							//right
							//public static final int GESTURE_SLIDE_RIGHT = 6;
							updater_touch(touch, 6, 0, 0x11);

						}else{
							cp_debug("left\n");
							//left
							//public static final int GESTURE_SLIDE_LEFT = 5;
							updater_touch(touch, 5, 0, 0x11);

						}
					}else if (z != TOUCH_ACTION_UP){
						cp_debug("%s\n", z == TOUCH_ACTION_DOWNSLIP ? "down"  : "up");
						//up or down
						//public static final int GESTURE_SLIDE_UP = 3;
						//public static final int GESTURE_SLIDE_DOWN = 4;
						updater_touch(touch, z == TOUCH_ACTION_DOWNSLIP ? 4 : 3, 0, 0x11);
					}/* else if (TP_ABS(curr_x, touch->first_x) >= 0 && TP_ABS(curr_x, touch->first_x) <= 2){ */
					else if (TP_ABS(curr_x, touch->first_x) >= 0 && TP_ABS(curr_x, touch->first_x) <= 4){
						cp_debug("tap\n");
						//public static final int GESTURE_SINGLE_TAP = 0;
						//but we use 101
						updater_touch(touch, 101, 0, 0x11);
						//tap
					} else if (touch->first_x == 0 && ret_data[2][1] == 0){
						cp_debug("tap\n");
						updater_touch(touch, 101, 0, 0x11);
					}
					//touch->startpos = 0;
					//touch->endpos = 0;
				}else{  //return touch event
					cp_debug("return touch event\n");
					//touch->xbuf[touch->endpos++] = TOUCH_BUFF_END_FLAG;
					updater_touch(touch, 0, 0, z);
				}
			
				touch->first_x = 0;
				touch->last_x = 0;
				touch->firstsec = 0;
				cp_debug("idel----------x = %02d z = %02d\n", ret_data[3][1], ret_data[1][1]);
			}
		}
	} while(circ_cnt_num --);

	return ;
}

static void cp2615_i2c_set_input_params(struct cp2615_i2c *touch)
{
	struct input_dev *input = touch->input;

	input->name = "touchpanel";
	input->id.bustype = BUS_I2C;
	input->id.version = 0x0001;
	input->dev.parent = &touch->client->dev;
	input_set_drvdata(input, touch);

	/* Register the device as mouse */
	__set_bit(EV_REL, input->evbit);
	__set_bit(REL_X, input->relbit);
	__set_bit(REL_Y, input->relbit);
	__set_bit(REL_Z, input->relbit);
	__set_bit(EV_KEY, input->evbit);
	__set_bit(BTN_LEFT, input->keybit);
	__set_bit(BTN_RIGHT, input->keybit);

       return ;
}

static void cp2615_irq_workqueue(struct cp2615_i2c *touch, unsigned long delay)
{
	/* unsigned long flags; */

	queue_work(touch->i2c_workqueue, &touch->dwork);
	/* __cancel_delayed_work(&touch->dwork); */
	/* schedule_delayed_work(&touch->dwork, delay); */

	return ;
}
static irqreturn_t cp2615_i2c_irq(int irq, void *dev_id)
{
	struct cp2615_i2c *touch = dev_id;

	cp2615_irq_workqueue(touch, 0);
 
        return IRQ_HANDLED;
}

static struct cp2615_i2c *cp2615_i2c_touch_create(struct i2c_client *client)
{
	struct cp2615_i2c *touch = NULL;

	touch = kzalloc(sizeof(struct cp2615_i2c), GFP_KERNEL);
	if (!touch) {
	        kfree(touch);
	        return NULL;
	}

	touch->client = client;
	touch->irq = -1;
	touch->touch_mode = TOUCH_MODE_IDEL;
	touch->i2c_workqueue = create_singlethread_workqueue(dev_name(&client->dev));
	touch->last_x = 0;
	touch->first_x = 0;
	touch->firstsec = 0;
	touch->firstmsec = 0;
	//touch->startpos = 0;
	//touch->endpos = 0;
	INIT_WORK(&touch->dwork, cp2615_work_handler);
	//INIT_DELAYED_WORK(&touch->cwork, circle_upload_34reg_data_work_handler);
	spin_lock_init(&touch->lock);
      
	touch->power = regulator_get(NULL, "touchpanel");
	if (IS_ERR(touch->power)) {
	        dev_err(&client->dev, "Get touchpanel device failed\n");
		touch->power = NULL;
	} else
		regulator_enable(touch->power);

	return touch;
}

void cpu_read_tcsm()
{
	u8 probe_ret_data[3][2] = { {0}, {0}, {0} };
	
	probe_ret_data[0][0] = REG32(TP_TCSM_BASE_ADDR + 0);
	probe_ret_data[0][1] = REG32(TP_TCSM_BASE_ADDR + 4);
	printk("cpu read ---------- 0x001d = %02x %02x\n", probe_ret_data[0][0], probe_ret_data[0][1]);
	probe_ret_data[1][0] = REG32(TP_TCSM_BASE_ADDR + 8); 
	probe_ret_data[1][1] = REG32(TP_TCSM_BASE_ADDR + 12);
	printk("cpu read ---------- 0x001d = %02x %02x\n", probe_ret_data[1][0], probe_ret_data[1][1]);
	probe_ret_data[2][0] = REG32(TP_TCSM_BASE_ADDR + 16);
	probe_ret_data[2][1] = REG32(TP_TCSM_BASE_ADDR + 20);
	printk("cpu read ---------- 0x0000 = %02x %02x\n", probe_ret_data[2][0], probe_ret_data[2][1]);

	return ;
}

static int cp2615_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
        int retval;
	
        struct cp2615_i2c *touch = NULL;

	client_global = client;
	
	cpu_read_tcsm();
	
        touch = cp2615_i2c_touch_create(client);
	if (!touch) {
	        retval = -ENOMEM;
		cp_debug("[err]: Kzalloc error\n");
		goto check_retval_err;
	}

	touch->fifo.tail = 0;

	i2c_set_clientdata(client, touch);

	if (gpio_request(client->irq, IRQ_NAME) < 0) {
	        retval = -ENOMEM;
		cp_debug("[err]: Request GPIO error!\n");
		goto check_retval_err;
	}

	if (gpio_direction_input(client->irq) < 0) {
	        retval = -ENOMEM;
	        cp_debug("[err]: Config GPIO %d error!\n", client->irq);
	        goto check_retval_err;
	}

	touch->irq = gpio_to_irq(client->irq);
	if (touch->irq < 0) {
	        retval = -ENOMEM;
	        cp_debug("[err]: Gpio to irq error!\n");
	        goto check_retval_err;
	}

	touch->irq_gpio = client->irq;

	retval = request_irq(touch->irq, cp2615_i2c_irq,
			     IRQF_TRIGGER_FALLING /* | IRQF_TRIGGER_RISING */ | IRQF_DISABLED,
			     IRQ_NAME, touch);
	if (retval != 0) {
	        dev_err(&client->dev, "[err]: Requset irq error, ERRNO:%d\n", retval);
		goto request_irq_err;
	}

	touch->input = input_allocate_device();
	if (!touch->input) {
	        retval = -ENOMEM;
		cp_debug("[err]: allocate input device failed!\n");
		goto input_alloc_err;
	}
	
	cp2615_i2c_set_input_params(touch);
	
	retval = input_register_device(touch->input);
	if (retval) {
		cp_debug("[err]: Failed to register input device!\n");
		goto input_regs_err;
	}
	
	return 0;

input_regs_err:
	input_free_device(touch->input);

input_alloc_err:
	free_irq(touch->irq, touch);

request_irq_err:

check_retval_err:
	if (touch->power) {
		regulator_disable(touch->power);
		regulator_put(touch->power);
	}
	kfree(touch);

	return retval;
}

static int cp2615_remove(struct i2c_client *client)
{
	struct cp2615_i2c *touch = i2c_get_clientdata(client);

	input_unregister_device(touch->input);

	if (touch->power) {
		regulator_disable(touch->power);
		regulator_put(touch->power);
	}

	kfree(touch);

        return 0;
}

#ifdef CONFIG_PM
static int cp2615_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ft3x07_i2c *touch = i2c_get_clientdata(client);

	if (touch->power)
		regulator_disable(touch->power);

	return 0;
}

static int cp2615_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ft3x07_i2c *touch = i2c_get_clientdata(client);

	if (touch->power)
		regulator_enable(touch->power);

	return 0;
}
#else
#define cp2615_suspend       NULL
#define cp2615_resume        NULL
#endif

static SIMPLE_DEV_PM_OPS(cp2615_pm, cp2615_suspend, cp2615_resume);

static const struct i2c_device_id cp2615_id[] = {
	{ "cp2615", 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, cp2615_id);

static struct i2c_driver cp2615_driver = {
        .driver     = {
	        .owner  = THIS_MODULE,
                .name   = DRIVER_NAME,
		.pm	= &cp2615_pm,
        },
        .probe      = cp2615_probe,
        .remove     = cp2615_remove,
	.id_table   = cp2615_id,
};

static int __init cp2615_i2c_init(void)
{
        return i2c_add_driver(&cp2615_driver);
}

static void __exit cp2615_i2c_exit(void)
{
        i2c_del_driver(&cp2615_driver);
}

module_init(cp2615_i2c_init);
module_exit(cp2615_i2c_exit);

#ifdef CONFIG_HARDWARE_DETECT
static int hardwaredet_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	u8 ret_data[2] = { 0 };
	struct regulator *power = regulator_get(NULL, "touchpanel");
	if (IS_ERR(power)) {
	        printk("Get touchpanel device failed\n");
	} else
		regulator_enable(power);

	/* Check touchpanel cp2615 status :
	 * 1. I2C is normal
	 * 2. Read 0x00 register which indicate that firmware has been download into TP
	 */
	master_write_i2c_reg(client, 0x001d, 0x0702);
	master_write_i2c_reg(client, 0x001d, 0x0700);
	master_read_i2c_reg(client, 0x0000, ret_data, 2);
	if (ret_data[0] == 26 && ret_data[1] == 15)
		printk("Check touchpanel cp2615 OK\n");
	else
		printk("Check touchpanel cp2615 FAIL\n");

	return 0;
}

static struct i2c_driver cp2615_detect_driver = {
        .driver     = {
	        .owner  = THIS_MODULE,
                .name   = DRIVER_NAME,
        },
        .probe      = hardwaredet_probe,
	.id_table   = cp2615_id,
};

static int __init cp2615_detect_init(void)
{
        return i2c_add_driver(&cp2615_detect_driver);
}

hardwaredet_initcall(cp2615_detect_init);
#endif

MODULE_AUTHOR("cyan");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Cp2615 touchpad driver");
