/*
 * Ft3x07 touchpad with I2C interface
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

#define DRIVER_NAME		"ft3x07"
/* When in IRQ mode read the device every THREAD_IRQ_SLEEP_SECS */
#define THREAD_IRQ_SLEEP_SECS	2
#define THREAD_IRQ_SLEEP_MSECS	(THREAD_IRQ_SLEEP_SECS * MSEC_PER_SEC)

#define FT5X06_ID		0x55
#define FT5X16_ID		0x0A
#define FT5X36_ID		0x14
#define FT6X06_ID		0x06
#define FT6X36_ID       	0x36

#define FT5316_ID		0x0A
#define FT5306I_ID		0x55

#define LEN_FLASH_ECC_MAX 					0xFFFE

#define FTS_MAX_POINTS                        10

#define FTS_WORKQUEUE_NAME	"fts_wq"

#define FTS_DEBUG_DIR_NAME	"fts_debug"

#define FTS_INFO_MAX_LEN		512
#define FTS_FW_NAME_MAX_LEN	50

#define FTS_REG_ID		0xA3
#define FTS_REG_FW_VER		0xA6
#define FTS_REG_FW_VENDOR_ID	0xA8
#define FTS_REG_POINT_RATE					0x88

#define FTS_FACTORYMODE_VALUE	0x40
#define FTS_WORKMODE_VALUE	0x00
#define FTS_PACKET_LENGTH      	128
#define FTS_UPGRADE_LOOP		30
#define FTS_RST_CMD_REG2		0xBC
#define FTS_READ_ID_REG		0x90
#define FTS_ERASE_APP_REG	0x61
#define FTS_FW_WRITE_CMD		0xBF
#define FTS_REG_ECC		0xCC
#define FTS_UPGRADE_AA		0xAA
#define FTS_UPGRADE_55		0x55

#define FTS_DBG_EN 1
#if FTS_DBG_EN
#define FTS_DBG(fmt, args...) 				printk("[FTS]" fmt, ## args)
#else
#define FTS_DBG(fmt, args...) 				do{}while(0)
#endif

/*
 * When in Polling mode and no data received for NO_DATA_THRES msecs
 * reduce the polling rate to NO_DATA_SLEEP_MSECS
 */
#define NO_DATA_THRES		(MSEC_PER_SEC)
#define NO_DATA_SLEEP_MSECS	(MSEC_PER_SEC / 4)

static bool polling_req = false;      /* Control IRQ / Polling option */
static int scan_rate = 80;            /* Control Polling Rate */

/* The main device structure */
struct ft3x07_i2c {
	struct i2c_client	*client;
	struct input_dev	*input;
	struct delayed_work	dwork;
	struct regulator        *power;
	spinlock_t		lock;
	int			no_data_count;
	int			scan_rate_param;
	int			scan_ms;
        int                     irq;
	unsigned int            last_x;
	unsigned int            last_y;
};

#define MAX_POINT  2
#define PACKAGE_BUFFER_LEN  (3 + 6 * MAX_POINT)

static unsigned char CTPM_FW[] = {
	#include "FT_Upgrade_App.i"
};

/*******************************************************************************
*  Name: fts_i2c_read
*  Brief:
*  Input:
*  Output: 
*  Return: 
*******************************************************************************/
int fts_i2c_read(struct i2c_client *client, char *writebuf, int writelen, char *readbuf, int readlen)
{
	int ret;

	//mutex_lock(&i2c_rw_access);

	if(readlen > 0)
	{
		if (writelen > 0) {
			struct i2c_msg msgs[] = {
				{
					 .addr = client->addr,
					 .flags = 0,
					 .len = writelen,
					 .buf = writebuf,
				 },
				{
					 .addr = client->addr,
					 .flags = I2C_M_RD,
					 .len = readlen,
					 .buf = readbuf,
				 },
			};
			ret = i2c_transfer(client->adapter, msgs, 2);
			if (ret < 0)
				dev_err(&client->dev, "%s: i2c read error.\n", __func__);
		} else {
			struct i2c_msg msgs[] = {
				{
					 .addr = client->addr,
					 .flags = I2C_M_RD,
					 .len = readlen,
					 .buf = readbuf,
				 },
			};
			ret = i2c_transfer(client->adapter, msgs, 1);
			if (ret < 0)
				dev_err(&client->dev, "%s:i2c read error.\n", __func__);
		}
	}

	//mutex_unlock(&i2c_rw_access);
	
	return ret;
}

int fts_read_reg(struct i2c_client *client, u8 addr, u8 *val)
{
	return fts_i2c_read(client, &addr, 1, val, 1);
}

/*******************************************************************************
*  Name: fts_i2c_write
*  Brief:
*  Input:
*  Output: 
*  Return: 
*******************************************************************************/
int fts_i2c_write(struct i2c_client *client, char *writebuf, int writelen)
{
	int ret;

	struct i2c_msg msgs[] = {
		{
			 .addr = client->addr,
			 .flags = 0,
			 .len = writelen,
			 .buf = writebuf,
		 },
	};
	//mutex_lock(&i2c_rw_access);

	if(writelen > 0)
	{
		ret = i2c_transfer(client->adapter, msgs, 1);
		if (ret < 0)
			dev_err(&client->dev, "%s: i2c write error.\n", __func__);
	}

	//mutex_unlock(&i2c_rw_access);
	
	return ret;
}

/*******************************************************************************
*  Name: fts_write_reg
*  Brief:
*  Input:
*  Output: 
*  Return: 
*******************************************************************************/
int fts_write_reg(struct i2c_client *client, u8 addr, const u8 val)
{
	u8 buf[2] = {0};

	buf[0] = addr;
	buf[1] = val;

	return fts_i2c_write(client, buf, sizeof(buf));
}

/************************************************************************
* Name: fts_3x07_ctpm_fw_upgrade
* Brief:  fw upgrade
* Input: i2c info, file buf, file len
* Output: no
* Return: fail <0
***********************************************************************/
int fts_3x07_ctpm_fw_upgrade(struct i2c_client *client, u8 *pbt_buf, u32 dw_lenth)
{
	u8 reg_val[2] = {0};
	u32 i = 0;
	u32 packet_number;
	u32 j;
	u32 temp;
	u32 lenght;
	u32 fw_length;
	u8 packet_buf[FTS_PACKET_LENGTH + 6];
	u8 auc_i2c_write_buf[10];
	u8 bt_ecc;

	if(pbt_buf[0] != 0x02)
	{
		FTS_DBG("[FTS] FW first byte is not 0x02. so it is invalid \n");
		return -1;
	}

	if(dw_lenth > 0x11f)
	{
		fw_length = ((u32)pbt_buf[0x100]<<8) + pbt_buf[0x101];
		if(dw_lenth < fw_length)
		{
			FTS_DBG("[FTS] Fw length is invalid \n");
			return -1;
		}
	}
	else
	{
		FTS_DBG("[FTS] Fw length is invalid \n");
		return -1;
	}
	
	for (i = 0; i < FTS_UPGRADE_LOOP; i++) 
	{
		/*********Step 1:Reset  CTPM *****/
		fts_write_reg(client, FTS_RST_CMD_REG2, FTS_UPGRADE_AA);
		//msleep(fts_updateinfo_curr.delay_aa);
		msleep(10);
		fts_write_reg(client, FTS_RST_CMD_REG2, FTS_UPGRADE_55);
		//msleep(fts_updateinfo_curr.delay_55);
		msleep(10);
		/*********Step 2:Enter upgrade mode *****/
		auc_i2c_write_buf[0] = FTS_UPGRADE_55;
		fts_i2c_write(client, auc_i2c_write_buf, 1);
		auc_i2c_write_buf[0] = FTS_UPGRADE_AA;
		fts_i2c_write(client, auc_i2c_write_buf, 1);
		//msleep(fts_updateinfo_curr.delay_readid);
		msleep(10);
		/*********Step 3:check READ-ID***********************/		
		auc_i2c_write_buf[0] = FTS_READ_ID_REG;
		auc_i2c_write_buf[1] = auc_i2c_write_buf[2] = auc_i2c_write_buf[3] =0x00;
		reg_val[0] = 0x00;
		reg_val[1] = 0x00;
		fts_i2c_read(client, auc_i2c_write_buf, 4, reg_val, 2);


		if (reg_val[0] == 0x79
			&& reg_val[1] == 0x18)
		{
			FTS_DBG("[FTS] Step 3: GET CTPM ID OK,ID1 = 0x%x,ID2 = 0x%x\n",
				reg_val[0], reg_val[1]);
			break;
		}
		else
		{
			dev_err(&client->dev, "[FTS] Step 3: GET CTPM ID FAIL,ID1 = 0x%x,ID2 = 0x%x\n",
				reg_val[0], reg_val[1]);
		}
	}
	if (i >= FTS_UPGRADE_LOOP)
		return -EIO;

	auc_i2c_write_buf[0] = FTS_READ_ID_REG;
	auc_i2c_write_buf[1] = 0x00;
	auc_i2c_write_buf[2] = 0x00;
	auc_i2c_write_buf[3] = 0x00;
	auc_i2c_write_buf[4] = 0x00;
	fts_i2c_write(client, auc_i2c_write_buf, 5);

	/*Step 4:erase app and panel paramenter area*/
	FTS_DBG("Step 4:erase app and panel paramenter area\n");
	auc_i2c_write_buf[0] = FTS_ERASE_APP_REG;
	fts_i2c_write(client, auc_i2c_write_buf, 1);
	//msleep(fts_updateinfo_curr.delay_erase_flash);
	msleep(500);

	for(i = 0;i < 200;i++)
	{
		auc_i2c_write_buf[0] = 0x6a;
		auc_i2c_write_buf[1] = 0x00;
		auc_i2c_write_buf[2] = 0x00;
		auc_i2c_write_buf[3] = 0x00;
		reg_val[0] = 0x00;
		reg_val[1] = 0x00;
		fts_i2c_read(client, auc_i2c_write_buf, 4, reg_val, 2);
		if(0xb0 == reg_val[0] && 0x02 == reg_val[1])
		{
			FTS_DBG("[FTS] erase app finished \n");
			break;
		}
		msleep(50);
	}

	/*********Step 5:write firmware(FW) to ctpm flash*********/
	bt_ecc = 0;
	FTS_DBG("Step 5:write firmware(FW) to ctpm flash\n");

	dw_lenth = fw_length;
	packet_number = (dw_lenth) / FTS_PACKET_LENGTH;
	packet_buf[0] = FTS_FW_WRITE_CMD;
	packet_buf[1] = 0x00;

	for (j = 0; j < packet_number; j++) 
	{
		temp = j * FTS_PACKET_LENGTH;
		packet_buf[2] = (u8) (temp >> 8);
		packet_buf[3] = (u8) temp;
		lenght = FTS_PACKET_LENGTH;
		packet_buf[4] = (u8) (lenght >> 8);
		packet_buf[5] = (u8) lenght;

		for (i = 0; i < FTS_PACKET_LENGTH; i++) 
		{
			packet_buf[6 + i] = pbt_buf[j * FTS_PACKET_LENGTH + i];
			bt_ecc ^= packet_buf[6 + i];
		}
		
		fts_i2c_write(client, packet_buf, FTS_PACKET_LENGTH + 6);
		
		for(i = 0;i < 30;i++)
		{
			auc_i2c_write_buf[0] = 0x6a;
			auc_i2c_write_buf[1] = 0x00;
			auc_i2c_write_buf[2] = 0x00;
			auc_i2c_write_buf[3] = 0x00;
			reg_val[0] = 0x00;
			reg_val[1] = 0x00;
			fts_i2c_read(client, auc_i2c_write_buf, 4, reg_val, 2);
			if(0xb0 == (reg_val[0] & 0xf0) && (0x03 + (j % 0x0ffd)) == (((reg_val[0] & 0x0f) << 8) |reg_val[1]))
			{
				FTS_DBG("[FTS] write a block data finished \n");
				break;
			}
			msleep(1);
		}
	}

	if ((dw_lenth) % FTS_PACKET_LENGTH > 0) 
	{
		temp = packet_number * FTS_PACKET_LENGTH;
		packet_buf[2] = (u8) (temp >> 8);
		packet_buf[3] = (u8) temp;
		temp = (dw_lenth) % FTS_PACKET_LENGTH;
		packet_buf[4] = (u8) (temp >> 8);
		packet_buf[5] = (u8) temp;

		for (i = 0; i < temp; i++) 
		{
			packet_buf[6 + i] = pbt_buf[packet_number * FTS_PACKET_LENGTH + i];
			bt_ecc ^= packet_buf[6 + i];
		}

		fts_i2c_write(client, packet_buf, temp + 6);

		for(i = 0;i < 30;i++)
		{
			auc_i2c_write_buf[0] = 0x6a;
			auc_i2c_write_buf[1] = 0x00;
			auc_i2c_write_buf[2] = 0x00;
			auc_i2c_write_buf[3] = 0x00;
			reg_val[0] = 0x00;
			reg_val[1] = 0x00;
			fts_i2c_read(client, auc_i2c_write_buf, 4, reg_val, 2);
			if(0xb0 == (reg_val[0] & 0xf0) && (0x03 + (j % 0x0ffd)) == (((reg_val[0] & 0x0f) << 8) |reg_val[1]))
			{
				FTS_DBG("[FTS] write a block data finished \n");
				break;
			}
			msleep(1);
		}
	}


	/*********Step 6: read out checksum***********************/
	FTS_DBG("Step 6: read out checksum\n");
	auc_i2c_write_buf[0] = FTS_REG_ECC;
	fts_i2c_read(client, auc_i2c_write_buf, 1, reg_val, 1);
	if (reg_val[0] != bt_ecc) 
	{
		dev_err(&client->dev, "[FTS]--ecc error! FW=%02x bt_ecc=%02x\n",
					reg_val[0],
					bt_ecc);
		return -EIO;
	}

	/*********Step 7: reset the new FW***********************/
	FTS_DBG("Step 7: reset the new FW\n");
	auc_i2c_write_buf[0] = 0x07;
	fts_i2c_write(client, auc_i2c_write_buf, 1);
	msleep(300);

	return 0;
}

/************************************************************************
* Name: fts_ctpm_fw_upgrade_with_i_file
* Brief:  upgrade with *.i file
* Input: i2c info
* Output: no
* Return: fail <0
***********************************************************************/
int fts_ctpm_fw_upgrade_with_i_file(struct i2c_client *client)
{
	u8 *pbt_buf = NULL;
	int i_ret=0;
	int fw_len = sizeof(CTPM_FW);

	pbt_buf = CTPM_FW;

	i_ret = fts_3x07_ctpm_fw_upgrade(client, pbt_buf, sizeof(CTPM_FW));
	if (i_ret != 0)
	  dev_err(&client->dev, "%s:upgrade failed. err.\n",__func__);

	return 0;
}

/************************************************************************
* Name: fts_ctpm_auto_upgrade
* Brief:  auto upgrade
* Input: i2c info
* Output: no
* Return: 0
***********************************************************************/
int fts_ctpm_auto_upgrade(struct i2c_client *client)
{
	u8 uc_host_fm_ver = FTS_REG_FW_VER;
	u8 uc_tp_fm_ver;
	int i_ret;

	i_ret = fts_ctpm_fw_upgrade_with_i_file(client);

	return 0;
}

static bool ft3x07_get_input(struct ft3x07_i2c *touch)
{
	int i = 0;
	int ret = 0;
	unsigned char data[PACKAGE_BUFFER_LEN] = {0};
	unsigned int point_num = 0;
	int x = 0, y = 0;
	int up_x = 0;
	// int id = 0, event = 0;
	// int weight = 0, area = 0;
    
	if ( polling_req &&
	     (*(volatile unsigned int *)(0xB0010000 + (touch->client->irq / 32) * 0x100))
	     & (0x1 << (touch->client->irq % 32)) ) {
		dev_err(&touch->client->dev, "High level, no data\n");
		return false;
	}

	// Read Data
	ret = i2c_master_recv(touch->client, data, PACKAGE_BUFFER_LEN);
	if (ret < 0) {
		dev_err(&touch->client->dev, "Recieve failed: %d\n", ret);
		return false;
	}

	// Process Data
	point_num = data[2] & 0x0F;

	if (point_num == 0) {
		/* printk(KERN_DEBUG "No point report Event : %d\n", (data[4] >> 6) & 0x3); */
		input_report_rel(touch->input, REL_X, 0);
		input_report_rel(touch->input, REL_Y, 0);
		input_report_rel(touch->input, REL_Z, 0x80);
		input_sync(touch->input);
		touch->last_x == -1;
		touch->last_y == -1;
	} else {
		// FT3X07 can detect two flinger touch point, but we just use first one
		// If multi point touch, then use ABS_MT_POSITION_X, ABS_MT_POSITION_Y, 
		// input_mt_sync to report

		// event = (data[3 + 6 * i + 0] >> 6) & 0x3;

		x = data[3 + 6 * i + 0];
		x <<= 8;
		x &= 0x00000F00;
		x |= data[3 + 6 * i + 1];
        
		// id = (data[3 + 6 * i + 2] >> 4) & 0xF;

		y = data[3 + 6 * i + 2];
		y <<= 8;
		y &= 0x00000F00;
		y |= data[3 + 6 * i + 3];

		// weight = data[3 + 6 * i + 4];
		// area = (data[3 + 6 * i + 5] >> 4) & 0xF;
			
		printk("Point %d : X %d  Y %d\n", i, x, y);
		if ( (touch->last_x >= 0) && (touch->last_y >= 0) ) {
			up_x = (x - touch->last_x) * 2;
			// Limit special points
			if (up_x > 75) {
				up_x = 75;
			} else if (up_x < -75) {
				up_x = -75;
			}

			input_report_rel(touch->input, REL_X, up_x);
			input_report_rel(touch->input, REL_Y, (touch->last_y - y));
			/*printk("up x : %d y : %d\n", up_x, (touch->last_y - y)); */
			input_report_rel(touch->input, REL_Z, 0x81);
			input_sync(touch->input);
		}

		touch->last_x = x;
		touch->last_y = y;
	}

	return true;
}

static void ft3x07_reschedule_work(struct ft3x07_i2c *touch,
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
static unsigned long ft3x07_adjust_delay(struct ft3x07_i2c *touch, bool have_data)
{
	unsigned long delay, nodata_count_thres;

	if (polling_req) {
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
	} else {
		delay = msecs_to_jiffies(THREAD_IRQ_SLEEP_MSECS);
		return round_jiffies_relative(delay);
	}
}

/* Work Handler */
static void ft3x07_work_handler(struct work_struct *work)
{
	bool have_data;
	struct ft3x07_i2c *touch = container_of(work, struct ft3x07_i2c, dwork.work);
	//unsigned long delay;

	have_data = ft3x07_get_input(touch);

	/*
	 * While interrupt driven, there is no real need to poll the device.
	 * But touchpads are very sensitive, so there could be errors
	 * related to physical environment and the attention line isn't
	 * necessarily asserted. In such case we can lose the touchpad.
	 * We poll the device once in THREAD_IRQ_SLEEP_SECS and
	 * if error is detected, we try to reset and reconfigure the touchpad.
	 */
	if (polling_req) {
		unsigned long delay = ft3x07_adjust_delay(touch, have_data);
		ft3x07_reschedule_work(touch, delay);
	}
}

static irqreturn_t ft3x07_irq(int irq, void *dev_id)
{
	struct ft3x07_i2c *touch = dev_id;

	ft3x07_reschedule_work(touch, 0);

	return IRQ_HANDLED;
}

static int ft3x07_open(struct input_dev *input)
{
	struct ft3x07_i2c *touch = input_get_drvdata(input);

	if (polling_req)
		ft3x07_reschedule_work(touch, msecs_to_jiffies(NO_DATA_SLEEP_MSECS));

	return 0;
}

static void ft3x07_close(struct input_dev *input)
{
	struct ft3x07_i2c *touch = input_get_drvdata(input);

	if (polling_req)
		cancel_delayed_work_sync(&touch->dwork);
}

static void ft3x07_set_input_params(struct ft3x07_i2c *touch)
{
	struct input_dev *input = touch->input;

	input->name = "touchpanel";
	input->phys = touch->client->adapter->name;
	input->id.bustype = BUS_I2C;
	input->id.version = 0x0001;
	input->dev.parent = &touch->client->dev;
	input->open = ft3x07_open;
	input->close = ft3x07_close;
	input_set_drvdata(input, touch);

	/* Register the device as mouse */
	__set_bit(EV_REL, input->evbit);
	__set_bit(REL_X, input->relbit);
	__set_bit(REL_Y, input->relbit);
	__set_bit(REL_Z, input->relbit); // Press down or Lift up
	__set_bit(EV_KEY, input->evbit);
	__set_bit(BTN_LEFT, input->keybit);
	__set_bit(BTN_RIGHT, input->keybit);
}

static inline void set_scan_rate(struct ft3x07_i2c *touch, int scan_rate)
{
	touch->scan_ms = MSEC_PER_SEC / scan_rate;
	touch->scan_rate_param = scan_rate;
}

static struct ft3x07_i2c *ft3x07_touch_create(struct i2c_client *client)
{
	struct ft3x07_i2c *touch;

	touch = kzalloc(sizeof(struct ft3x07_i2c), GFP_KERNEL);
	if (!touch)
		return NULL;

	touch->client = client;
	set_scan_rate(touch, 30);
	INIT_DELAYED_WORK(&touch->dwork, ft3x07_work_handler);
	spin_lock_init(&touch->lock);
	touch->irq = -1;
	touch->last_x = -1;
	touch->last_y = -1;

	/* FT3X07 has two power
	 * 1. VCC   : chip work power
	 * 2. IOVCC : chip i2c supply power
	 *
	 * We just control the VCC power, IOVCC should be protected by hardware
	 */
	touch->power = regulator_get(NULL, "touchpanel");
	if (IS_ERR(touch->power)) {
	        dev_err(&client->dev, "Get touchpanel device failed\n");
		touch->power = NULL;
	} else
		regulator_enable(touch->power);

	return touch;
}

static int __devinit ft3x07_probe(struct i2c_client *client,
				      const struct i2c_device_id *dev_id)
{
	int ret = 0;
	struct ft3x07_i2c *touch;

	// printk("\n\nft3x07_probe start\n");
	
	touch = ft3x07_touch_create(client);
	
	if (!touch)
		return -ENOMEM;

	// Alloc I2C interrupt
	if (client->irq < 1) {
		polling_req = true;
		dev_err(&client->dev, "Using polling at rate: %d times/sec\n", scan_rate);
	} else {
		if (gpio_request(client->irq, "ft3x07_irq") < 0){
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
		polling_req = false;
	}

	if (!polling_req) {
		ret = request_irq(touch->irq, ft3x07_irq,
				  IRQF_DISABLED | IRQF_TRIGGER_FALLING,
				  DRIVER_NAME, touch);
		if (ret) {
			dev_err(&client->dev, "IRQ request failed: %d, falling back to polling\n", ret);
			polling_req = true;
		}
	}


	// Alloc Input Device
	touch->input = input_allocate_device();
	if (!touch->input) {
		ret = -ENOMEM;
		goto err_mem_free;
	}

	ft3x07_set_input_params(touch);

	ret = input_register_device(touch->input);
	if (ret) {
		dev_err(&client->dev, "Input device register failed: %d\n", ret);
		goto err_input_free;
	}

	i2c_set_clientdata(client, touch);

	/* char uc_tp_fm_ver; */
	/* fts_read_reg(client, FTS_REG_FW_VER, &uc_tp_fm_ver); */
	/* printk("ft3x07 ver:0x%02x\n", uc_tp_fm_ver); */
	/* fts_ctpm_auto_upgrade(client); */

	// printk("ft3x07_probe done OK\n\n");

	return 0;

 err_input_free:
	input_free_device(touch->input);
 err_mem_free:
	if (touch->power) {
		regulator_disable(touch->power);
		regulator_put(touch->power);
	}
	kfree(touch);

	dev_err(&client->dev, "ft3x07_probe done ERROR\n\n");

	return ret;
}

static int __devexit ft3x07_remove(struct i2c_client *client)
{
	struct ft3x07_i2c *touch = i2c_get_clientdata(client);

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
static int ft3x07_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ft3x07_i2c *touch = i2c_get_clientdata(client);

	if (polling_req)
		cancel_delayed_work_sync(&touch->dwork);

	enable_irq_wake(touch->irq);

	if (touch->power)
		regulator_disable(touch->power);

	return 0;
}

static int ft3x07_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ft3x07_i2c *touch = i2c_get_clientdata(client);

	if (touch->power)
		regulator_enable(touch->power);
	
	disable_irq_wake(touch->irq);

	if (polling_req)
		ft3x07_reschedule_work(touch, msecs_to_jiffies(NO_DATA_SLEEP_MSECS));
	
	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(ft3x07_pm, ft3x07_suspend, ft3x07_resume);

static const struct i2c_device_id ft3x07_i2c_id_table[] = {
	{ DRIVER_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, ft3x07_i2c_id_table);

static struct i2c_driver ft3x07_i2c_driver = {
	.driver = {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
		.pm	= &ft3x07_pm,
	},

	.probe		= ft3x07_probe,
	.remove		= __devexit_p(ft3x07_remove),

	.id_table	= ft3x07_i2c_id_table,
};

static int __init ft3x07_init(void)
{
	return i2c_add_driver(&ft3x07_i2c_driver);
}

static void __exit ft3x07_exit(void)
{
	i2c_del_driver(&ft3x07_i2c_driver);
}

module_init(ft3x07_init);
module_exit(ft3x07_exit);

MODULE_DESCRIPTION("FT3X07 I2C touch panel driver");
MODULE_AUTHOR("Kznan");
MODULE_LICENSE("GPL");
