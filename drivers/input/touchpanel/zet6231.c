#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/swab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/idr.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/android_alarm.h>
#include <linux/earlysuspend.h>
#include <linux/time.h>

#include <linux/irq.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/pm.h>
#include <linux/tsc.h>

//#include "zet6231_fw.h"

#define DRIVER_NAME		"zet6231_i2c"

#define TS_RST_GPIO GPIO_PC(19)
#define CMD_WRITE_PASSWORD			(0x20)
#define CMD_PASSWORD_HIBYTE			(0xC5)
#define CMD_PASSWORD_LOBYTE			(0x9D)
#define CMD_WRITE_PROGRAM			(0x22)
#define CMD_PAGE_ERASE				(0x23)
#define CMD_MASS_ERASE				(0x24)
#define CMD_PAGE_READ_PROGRAM			(0x25)
#define CMD_READ_CODE_OPTION			(0x27)
#define CMD_RESET_MCU				(0x29)
#define CMD_WRITE_SFR				(0x2B)
#define CMD_READ_SFR				(0x2C)
#define SFR_UNLOCK_FLASH			(0x3D)
#define SFR_LOCK_FLASH				(0x7D)	

#define MAX_FLASH_BUF_SIZE			(0x10000)
#define FLASH_PAGE_LEN				(128)
///=========================================================================================///
///  Model Type
///=========================================================================================///
#define MODEL_ZET6221				(0)
#define MODEL_ZET6223				(1)
#define MODEL_ZET6231				(2)
#define MODEL_ZET6241				(3)
#define MODEL_ZET6251				(4)
static u8 ic_model		= MODEL_ZET6231;

#define ROM_TYPE_UNKNOWN			(0x00)
#define ROM_TYPE_SRAM				(0x02)
#define ROM_TYPE_OTP				(0x06)
#define ROM_TYPE_FLASH				(0x0F)

///----------------------------------------------------///
/// FW variables
///----------------------------------------------------///
static u16 pcode_addr[8]	= {0x3DF1,0x3DF4,0x3DF7,0x3DFA,0x3EF6,0x3EF9,0x3EFC,0x3EFF}; ///< default pcode addr: zet6221
static u16 pcode_addr_6221[8]	= {0x3DF1,0x3DF4,0x3DF7,0x3DFA,0x3EF6,0x3EF9,0x3EFC,0x3EFF}; ///< zet6221 pcode_addr[8]
static u16 pcode_addr_6223[8]	= {0x7BFC,0x7BFD,0x7BFE,0x7BFF,0x7C04,0x7C05,0x7C06,0x7C07}; ///< zet6223 pcode_addr[8]
static u8 sfr_data[16]		= {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
				   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

//static u8 zet_tx_data[131] __initdata;
static u8 zet_rx_data[131] __initdata;

#define TRUE 					(1)
#define FALSE 					(0)
#define PROJECT_CODE_MAX_CNT                    (8)
#define ZET_ABS(a) (a > 0 ? a : -a)

u8 zet622x_ts_check_version(void);
static void zet6231_i2c_work_handler(struct work_struct *work);
u8 *flash_buffer 			= NULL;
static u8 pcode[8];  
#define NO_DATA_SLEEP_MSECS	(MSEC_PER_SEC / 4)

//static u8 download_ok 		= FALSE;

struct zet6231_i2c {
	struct i2c_client	*client;
	struct input_dev	*input;
	struct regulator        *power;
	int                     irq;
	int                     irq_gpio;
	struct delayed_work	dwork;
	spinlock_t		lock;
	int touch_mode;
	int last_touch_x;
	int last_touch_y;
	int start_touch_x;
	int start_touch_y;
};

#define TOUCH_MODE_IDEL 0
#define TOUCH_MODE_PRES 1
#define TOUCH_ACTION_PRES 0x81
#define TOUCH_ACTION_UP   0x2

static const struct i2c_device_id zet6231_id[] = {
	{ "zet6231", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, zet6231_id);

///**********************************************************************
///   [function]:  zet622x_i2c_read_tsdata
///   [parameters]: client, data, length
///   [return]: s32
///***********************************************************************
s32 zet622x_i2c_read_tsdata(struct i2c_client *client, u8 *data, u8 length)
{
	struct i2c_msg msg;
	msg.addr     = client->addr;
	msg.flags    = I2C_M_RD;
	msg.len      = length;
	msg.buf      = data;
	//msg.scl_rate = 300*1000;
	return i2c_transfer(client->adapter,&msg, 1);
}

s32 zet622x_i2c_write_tsdata(struct i2c_client *client, u8 *data, u8 length)
{
	struct i2c_msg msg;
	msg.addr     = client->addr;
	msg.flags    = 0;
	msg.len      = length;
	msg.buf      = data;
	//msg.scl_rate = 300*1000;
	return i2c_transfer(client->adapter,&msg, 1);
}

static void ctp_set_reset_low(void)
{
	gpio_request(TS_RST_GPIO, "tp_rst");
	gpio_direction_output(TS_RST_GPIO, 0);
}

static void ctp_set_reset_high(void)
{
	gpio_direction_output(TS_RST_GPIO, 1);
}

///**********************************************************************
///   [function]:  zet622x_cmd_sndpwd
///   [parameters]: client
///   [return]: u8
///**********************************************************************
static u8 zet622x_cmd_sndpwd(struct i2c_client *client)
{
	u8 ts_cmd[3] = {CMD_WRITE_PASSWORD, CMD_PASSWORD_HIBYTE, CMD_PASSWORD_LOBYTE};
	int ret;
	
	ret = zet622x_i2c_write_tsdata(client, ts_cmd, 3);
	return ret;
}

static u8 zet622x_cmd_codeoption(struct i2c_client *client, u8 *romtype)
{
	u8 ts_cmd[1] = {CMD_READ_CODE_OPTION};
	u8 code_option[32] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

#ifdef FEATURE_HIGH_IMPEDENCE_MODE
	u8 ts_code_option_erase[1] = {CMD_ERASE_CODE_OPTION};
	u8 tx_buf[18] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
#endif ///< for FEATURE_HIGH_IMPEDENCE_MODE

	int ret;
	u16 model;
	int i;
	
	printk("\n[ZET] : option write : "); 

	ret = zet622x_i2c_write_tsdata(client, ts_cmd, 1);

	msleep(1);
	
	printk("%02x ",ts_cmd[0]); 
	
	printk("\n[ZET] : read : "); 

	ret = zet622x_i2c_read_tsdata(client, code_option, 16);

	msleep(1);

	for(i = 0 ; i < 16 ; i++)
		{
			printk("%02x ",code_option[i]); 
		}
	printk("\n"); 

	model = 0x0;
	model = code_option[7];
	model = (model << 8) | code_option[6];

	/// Set the rom type	
	*romtype = (code_option[2] & 0xf0)>>4;
	
	printk("model 0x%04x\n", model);

	switch(model)
		{ 
		case 0xFFFF: 
			ret = 1;
			ic_model = MODEL_ZET6221;
			for(i = 0 ; i < 8 ; i++)
				{
					pcode_addr[i] = pcode_addr_6221[i];
				}
			
#ifdef FEATURE_HIGH_IMPEDENCE_MODE
			if(code_option[2] != IMPEDENCE_BYTE)
				{
					///------------------------------------------///
					/// unlock the flash 
					///------------------------------------------///				
					if(zet622x_cmd_sfr_read(client) == 0)
						{
							return 0;
						}
					if(zet622x_cmd_sfr_unlock(client) == 0)
						{
							return 0;
						}
					///------------------------------------------///
					/// Erase Code Option
					///------------------------------------------///							
					ret = zet622x_i2c_write_tsdata(client, ts_code_option_erase, 1);
					msleep(50);

					///------------------------------------------///
					/// Write Code Option
					///------------------------------------------///	
					tx_buf[0] = CMD_WRITE_CODE_OPTION;
					tx_buf[1] = 0xc5;
					for(i = 2 ; i < 18 ; i++)
						{
							tx_buf[i]=code_option[i-2];
						}				
					tx_buf[4] = IMPEDENCE_BYTE;
			
					ret = zet622x_i2c_write_tsdata(client, tx_buf, 18);
					msleep(50);

					///------------------------------------------///
					/// Read Code Option back check
					///------------------------------------------///					
					ret = zet622x_i2c_write_tsdata(client, ts_cmd, 1);
					msleep(5);	
					printk("%02x ",ts_cmd[0]); 	
					printk("\n[ZET] : (2)read : "); 
					ret = zet622x_i2c_read_tsdata(client, code_option, 16);
					msleep(1);
					for(i = 0 ; i < 16 ; i++)
						{
							printk("%02x ",code_option[i]); 
						}
					printk("\n"); 
				
				}
#endif ///< for  FEATURE_HIGH_IMPEDENCE_MODE
			break; 
		case 0x6231: 
		 	ret = 1;
			ic_model = MODEL_ZET6231;
			for(i = 0 ; i < 8 ; i++)
				{
					pcode_addr[i] = pcode_addr_6223[i];
				} 
			break;           
		case 0x6223:
		 	ret = 1;
			ic_model = MODEL_ZET6223;
			for(i = 0 ; i < 8 ; i++)
				{
					pcode_addr[i] = pcode_addr_6223[i];
				}
			break; 
    		case 0x6251:
			ic_model = MODEL_ZET6251;
			for(i = 0 ; i < 8 ; i++)
				{
					pcode_addr[i] = pcode_addr_6223[i];
				}
			break;
		default:
		 	ret = 1;
			ic_model = MODEL_ZET6223;
			for(i = 0 ; i < 8 ; i++)
				{
					pcode_addr[i] = pcode_addr_6223[i];
				}
			break;         
		} 

	return ret;
}
///***********************************************************************
///   [function]:  zet622x_ts_check_version
///   [parameters]: void
///   [return]: void
///************************************************************************
u8 zet622x_ts_check_version(void)
{	
	int i;
	
	for(i = 0 ; i < PROJECT_CODE_MAX_CNT ; i++)
		{
			if(pcode[i] != flash_buffer[pcode_addr[i]])
				{
					printk("[ZET]: Version different\n");
					/// if reload the bin file mode 
					return FALSE;
				}
		}

	printk("[ZET]: Version the same\n");
	return TRUE;
}

static void updater_touch(struct zet6231_i2c *touch, int x, int y, int z){
	struct input_dev *input = touch->input;

	//printk("updater_touch\n");
	input_report_rel(input, REL_X, x);
	input_report_rel(input, REL_Y, y);
	input_report_rel(input, REL_Z, z);
	input_sync(input);
}

static void zet6231_i2c_work_handler(struct work_struct *work){
	struct zet6231_i2c *touch = container_of(work, struct zet6231_i2c, dwork.work);

	u8 tx_buf[32] = {0,};

	int ret = 0;
	int x,y,z = 0;
	//struct timeval time1;
	int rel_x,rel_y = 0;
	int diff_x, diff_y = 0;

	int gpio = gpio_get_value(touch->irq_gpio);
	if (gpio > 0){
		//printk("gpio:%d\n", gpio);
		return;
	}

	ret = zet622x_i2c_read_tsdata(touch->client, tx_buf, 1);
	if (ret < 0){
		printk("zet6231_i2c_irq zet622x_i2c_read_tsdata fail\n");
		return;
	}
  
	if (tx_buf[0] != 0x3C){
		//printk("zet6231_i2c_irq not 0x3C 0x%02x\n", tx_buf[0]);
		return;
	}
	//printk("%02x ", tx_buf[0]);

	ret = zet622x_i2c_read_tsdata(touch->client, tx_buf, 22);

	/* for(i = 0 ; i < 22 ; i++){ */
	/*   printk("%02x ",tx_buf[i]); */
	/* } */
	/* printk("\n"); */

	y = (tx_buf[2]>>4) * 256 + tx_buf[3];
	x = (tx_buf[2]&0xf) * 256 + tx_buf[4];
	z = (tx_buf[0] | tx_buf[1]) > 0 ? TOUCH_ACTION_PRES : TOUCH_ACTION_UP;
	//printk("x:%d y:%d z:%d\n", x, y, z);

	//do_gettimeofday(&time1);
	//printk("sec:%d usec:%d\n", time1.tv_sec, time1.tv_usec);
	if (touch->touch_mode == TOUCH_MODE_IDEL){
		if (z == TOUCH_ACTION_UP){
			return;
		}

		touch->touch_mode = TOUCH_MODE_PRES;

		touch->last_touch_x = x;
		touch->last_touch_y = y;
		touch->start_touch_x = x;
		touch->start_touch_y = y;
		updater_touch(touch, x, y, z);
	}else if (touch->touch_mode == TOUCH_MODE_PRES){
		if (z == TOUCH_ACTION_UP){
			touch->touch_mode = TOUCH_MODE_IDEL;
			updater_touch(touch, 0, 0, z);
			return;
		}

		rel_x = x - touch->last_touch_x;
		rel_y = y - touch->last_touch_y;
		if (touch->touch_mode == TOUCH_MODE_PRES && rel_x == 0 && rel_y == 0){
			return;
		}

		diff_x = x - touch->start_touch_x;
		diff_y = y - touch->start_touch_y;
		touch->last_touch_x = x;
		touch->last_touch_y = y;

		//updater_touch(touch, rel_x, rel_y, z);
		if (ZET_ABS(diff_x) > ZET_ABS(diff_y)){
			updater_touch(touch, rel_x, 0, z);
		}else if (ZET_ABS(diff_x) < ZET_ABS(diff_y)){
			updater_touch(touch, 0, rel_y, z);
		}else{
			updater_touch(touch, rel_x, rel_y, z);
		}
	}

	return;
}

static void zet6231_i2c_reschedule_work(struct zet6231_i2c *touch,
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

static irqreturn_t zet6231_i2c_irq(int irq, void *dev_id)
{
	//printk("zet6231_i2c_irq in %d\n", irq);
	struct zet6231_i2c *touch = dev_id;

	zet6231_i2c_reschedule_work(touch, 0);

	return IRQ_HANDLED;
}
#if 0
///************************************************************************
///   [function]:  zet_mem_init
///   [parameters]: void
///   [return]: void
///************************************************************************
static void zet_mem_init(void)
{
	if(flash_buffer == NULL)
		{
			flash_buffer = kmalloc(MAX_FLASH_BUF_SIZE, GFP_KERNEL);	
		}

}

static void zet_fw_init(void)
{
	int i;
	
	printk("[ZET]: Load from header\n");

	if(ic_model == MODEL_ZET6231){
		for(i = 0 ; i < sizeof(zeitec_zet6231_firmware) ; i++){
			flash_buffer[i] = zeitec_zet6231_firmware[i];
		}
	}
	
	/// Load firmware from bin file
	//zet_fw_load(fw_file_name);
}
#endif
///***********************************************************************
///   [function]:  zet622x_cmd_readpage
///   [parameters]: client, page_id, buf
///   [return]: int
///************************************************************************
int zet622x_cmd_readpage(struct i2c_client *client, int page_id, u8 * buf)
{
	int ret;
	int cmd_len = 3;

	switch(ic_model){
	case MODEL_ZET6223: 
	case MODEL_ZET6231: 
	case MODEL_ZET6251: 
		buf[0] = CMD_PAGE_READ_PROGRAM;
		buf[1] = (u8)(page_id) & 0xff; ///< (pcode_addr[0]/128);
		buf[2] = (u8)(page_id >> 8);   ///< (pcode_addr[0]/128);			
		cmd_len = 3;
		break;
	default: 
		buf[0] = CMD_PAGE_READ_PROGRAM;
		buf[1] = (u8)(page_id) & 0xff; ///< (pcode_addr[0]/128);
		buf[2] = (u8)(page_id >> 8);   ///< (pcode_addr[0]/128);			
		cmd_len = 3;
		break;
	}
	
	ret = zet622x_i2c_write_tsdata(client, buf, cmd_len);
	if(ret <= 0){
		printk("[ZET]: Read page command fail");
		return ret;
	}

	ret = zet622x_i2c_read_tsdata(client, buf, FLASH_PAGE_LEN);
	if(ret <= 0){
		printk("[ZET]: Read page data fail");
		return ret;
	}
	return 1;
}

///************************************************************************
///   [function]:  zet622x_ts_project_code_get
///   [parameters]: client
///   [return]: int
///************************************************************************
int zet622x_ts_project_code_get(struct i2c_client *client)
{
	int i;
	int ret;

	///----------------------------------------///
	/// Read Data page for flash version check#1
	///----------------------------------------///
	ret = zet622x_cmd_readpage(client, (pcode_addr[0]>>7), &zet_rx_data[0]);
	if(ret <= 0){
		return ret;
	}
	printk("[ZET]: page=%3d ",(pcode_addr[0] >> 7)); ///< (pcode_addr[0]/128));
	for(i = 0 ; i < 4 ; i++){
		pcode[i] = zet_rx_data[(pcode_addr[i] & 0x7f)]; ///< [(pcode_addr[i]%128)];
		printk("offset[%04x] = %02x ",i,(pcode_addr[i] & 0x7f));    ///< (pcode_addr[i]%128));
	}
	printk("\n");

	///----------------------------------------///
	/// Read Data page for flash version check#2
	///----------------------------------------///
	ret = zet622x_cmd_readpage(client, (pcode_addr[4]>>7), &zet_rx_data[0]);		
	if(ret <= 0){
		return ret;
	}	

	printk("[ZET]: page=%3d ",(pcode_addr[4] >> 7)); //(pcode_addr[4]/128));
	for(i = 4 ; i < PROJECT_CODE_MAX_CNT ; i++){
		pcode[i] = zet_rx_data[(pcode_addr[i] & 0x7f)]; //[(pcode_addr[i]%128)];
		printk("offset[%04x] = %02x ",i,(pcode_addr[i] & 0x7f));  //(pcode_addr[i]%128));
	}
	printk("\n");
        
	printk("[ZET]: pcode_now : ");
	for(i = 0 ; i < PROJECT_CODE_MAX_CNT ; i++){
		printk("%02x ",pcode[i]);
	}
	printk("\n");
	
	printk("[ZET]: pcode_new : ");
	for(i = 0 ; i < PROJECT_CODE_MAX_CNT ; i++){
		printk("%02x ", flash_buffer[pcode_addr[i]]);
	}
	printk("\n");
        
	return ret;
}

///**********************************************************************
///   [function]:  zet622x_cmd_sfr_read
///   [parameters]: client
///   [return]: u8
///**********************************************************************
u8 zet622x_cmd_sfr_read(struct i2c_client *client)
{

	u8 ts_cmd[1] = {CMD_READ_SFR};
	int ret;
	int i;
	
	printk("\n[ZET] : write : "); 

	ret = zet622x_i2c_write_tsdata(client, ts_cmd, 1);

	msleep(5);
	
	printk("%02x ",ts_cmd[0]); 
	
	printk("\n[ZET] : sfr_read : "); 

	ret = zet622x_i2c_read_tsdata(client, sfr_data, 16);

	msleep(1);

	for(i = 0 ; i < 16 ; i++){
		printk("%02x ",sfr_data[i]); 
	}
	printk("\n"); 

	if((sfr_data[14] != SFR_UNLOCK_FLASH) && 
	   (sfr_data[14] != SFR_LOCK_FLASH)){
		printk("[ZET] : The SFR[14] shall be 0x3D or 0x7D\n"); 
		return FALSE;
	}
	
	return TRUE;
}

///**********************************************************************
///   [function]:  zet622x_cmd_sfr_unlock
///   [parameters]: client
///   [return]: u8
///**********************************************************************
u8 zet622x_cmd_sfr_unlock(struct i2c_client *client)
{
	u8 tx_buf[17] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
	int ret;
	int i;
	
	printk("\nsfr_update : "); 	
	for(i = 0 ; i < 16 ; i++){
		tx_buf[i+1] = sfr_data[i];
		printk("%02x ",sfr_data[i]); 
	}
	printk("\n"); 

	if(sfr_data[14] != SFR_UNLOCK_FLASH){
		tx_buf[0]  = CMD_WRITE_SFR;	
		tx_buf[15] = SFR_UNLOCK_FLASH;
		ret = zet622x_i2c_write_tsdata(client, tx_buf, 17);
	}
	
	return TRUE;
}

///***********************************************************************
///   [function]:  zet622x_cmd_masserase
///   [parameters]: client
///   [return]: u8
///************************************************************************
u8 zet622x_cmd_masserase(struct i2c_client *client)
{
	u8 ts_cmd[1] = {CMD_MASS_ERASE};
	
	int ret;

	ret = zet622x_i2c_write_tsdata(client, ts_cmd, 1);

	return ret;
}

///***********************************************************************
///   [function]:  zet622x_cmd_pageerase
///   [parameters]: client, npage
///   [return]: u8
///************************************************************************
u8 zet622x_cmd_pageerase(struct i2c_client *client, int npage)
{
	u8 ts_cmd[3] = {CMD_PAGE_ERASE, 0x00, 0x00};
	u8 len = 0;
	int ret;

	switch(ic_model){
	case MODEL_ZET6221: ///< 6221
		ts_cmd[1] = npage;
		len = 2;
		break;
	case MODEL_ZET6223: ///< 6223
	case MODEL_ZET6231: ///< 6231
	case MODEL_ZET6251: ///< 6251
	default: 
		ts_cmd[1] = npage & 0xff;
		ts_cmd[2] = npage >> 8;
		len=3;
		break;
	}

	ret = zet622x_i2c_write_tsdata(client, ts_cmd, len);
	printk( " [ZET] : page erase\n");
	return TRUE;
}

///***********************************************************************
///   [function]:  zet622x_cmd_writepage
///   [parameters]: client, page_id, buf
///   [return]: int
///************************************************************************
int zet622x_cmd_writepage(struct i2c_client *client, int page_id, u8 * buf)
{
	int ret;
	int cmd_len = 131;
	int cmd_idx = 3;
	u8 tx_buf[256];
	int i;

	switch(ic_model){
	case MODEL_ZET6221: ///< for 6221
		cmd_len = 130;
			
		tx_buf[0] = CMD_WRITE_PROGRAM;
		tx_buf[1] = page_id;
		cmd_idx = 2;
		break;
	case MODEL_ZET6223: ///< for 6223
	case MODEL_ZET6231: ///< for 6231
	case MODEL_ZET6251: ///< for 6251
	default: 
		cmd_len = 131;
			
		tx_buf[0] = CMD_WRITE_PROGRAM;
		tx_buf[1] = (page_id & 0xff);
		tx_buf[2] = (u8)(page_id >> 8);
		cmd_idx = 3;
		break;
	}

	for(i = 0 ; i < FLASH_PAGE_LEN ; i++){
		tx_buf[i + cmd_idx] = buf[i];
	}
	ret = zet622x_i2c_write_tsdata(client, tx_buf, cmd_len);
	if(ret <= 0){
		printk("[ZET] : write page %d failed!!", page_id);
	}
	return ret;
}

///***********************************************************************
///   [function]:  zet622x_cmd_resetmcu
///   [parameters]: client
///   [return]: u8
///************************************************************************
u8 zet622x_cmd_resetmcu(struct i2c_client *client)
{
	u8 ts_cmd[1] = {CMD_RESET_MCU};
	
	int ret;

	ret = zet622x_i2c_write_tsdata(client, ts_cmd, 1);

	return ret;
}
/*
  static int zet6231_downloader(struct i2c_client *client){
  int ret;
  int i;
  u8 uRomType;
  int flash_total_len 	= 0;
  int flash_rest_len 	= 0;
  int flash_page_id	= 0;	
  int retry_count	= 0;
  int now_flash_rest_len	= 0;
  int now_flash_page_id	= 0;

  download_ok = TRUE;

  zet_mem_init();

  ctp_set_reset_low();

  msleep(1);

  ret = zet622x_cmd_sndpwd(client);
  if(ret <= 0){
  printk("zet6231_TPC_probe zet622x_cmd_sndpwd fail\n");
  return ret;
  }
  msleep(10);

  uRomType = ROM_TYPE_OTP;
  ret = zet622x_cmd_codeoption(client, &uRomType);
  if (ret < 0){
  printk("zet6231_TPC_probe zet622x_cmd_codeoption fail\n");
  return ret;
  }
  msleep(10);

  zet_fw_init();

  memset(zet_rx_data, 0x00, 131);

  zet622x_ts_project_code_get(client);

  if(zet622x_ts_check_version() == TRUE){
  goto LABEL_EXIT_DOWNLOAD;
  }

  ret = zet622x_cmd_sfr_read(client);	
  if(ret <= 0){
  return ret;
  }

  if(zet622x_cmd_sfr_unlock(client) == 0){
  return 0;
  }
  msleep(20);

  printk("uRomType 0x%02x\n", uRomType);

  if(uRomType == ROM_TYPE_FLASH){
  zet622x_cmd_masserase(client);
  msleep(40);
  }

  flash_total_len = 0x8000;
  flash_rest_len = flash_total_len;

  while(flash_rest_len > 0){
  memset(zet_tx_data, 0x00, 131);

  LABEL_DOWNLOAD_PAGE:

  /// Do page erase
  if(retry_count > 0){
  ///------------------------------///
  /// Do page erase
  ///------------------------------///    
  if(uRomType == ROM_TYPE_FLASH){
  zet622x_cmd_pageerase(client, flash_page_id);
  msleep(30);
  }
  }

  //printk( " [ZET] : write page%d\n", flash_page_id);
  now_flash_rest_len = flash_rest_len;
  now_flash_page_id  = flash_page_id;
		
  ///---------------------------------///
  /// Write page
  ///---------------------------------///		
  ret = zet622x_cmd_writepage(client, flash_page_id, &flash_buffer[flash_page_id * FLASH_PAGE_LEN]);
  flash_rest_len -= FLASH_PAGE_LEN;

  if(ic_model != MODEL_ZET6251){
  msleep(5);
  }
		
  ///---------------------------------///
  /// Read page
  ///---------------------------------///
  ret = zet622x_cmd_readpage(client, flash_page_id, &zet_rx_data[0]);		
  if(ret <= 0){
  return ret;
  }
		
  ///--------------------------------------------------------------------------///
  /// 10. compare data
  ///--------------------------------------------------------------------------///
  for(i = 0 ; i < FLASH_PAGE_LEN; i++){
  if(i < now_flash_rest_len){
  if(flash_buffer[flash_page_id * FLASH_PAGE_LEN + i] != zet_rx_data[i]){
  flash_rest_len = now_flash_rest_len;
  flash_page_id = now_flash_page_id;
				
  if(retry_count < 5){
  printk("compare fail\n");
  retry_count++;
  goto LABEL_DOWNLOAD_PAGE;
  }else{
  download_ok = FALSE;
  retry_count = 0;						
  ctp_set_reset_high();
  msleep(20);		
  ctp_set_reset_low();
  msleep(20);
  ctp_set_reset_high();
  msleep(20);
  printk("download fail\n");
  goto LABEL_EXIT_DOWNLOAD;
  }

  }
  }
  }
		
  retry_count = 0;
  flash_page_id += 1;
  }

  LABEL_EXIT_DOWNLOAD:
  if(download_ok == FALSE){
  printk("[ZET] : download failed!\n");
  }

  //zet622x_cmd_resetmcu(client);
  msleep(10);

  ctp_set_reset_high();

  /// download pass then copy the pcode
  for(i = 0 ; i < PROJECT_CODE_MAX_CNT ; i++){      
  pcode[i] = flash_buffer[pcode_addr[i]];
  }

  return 0;
  }
*/
static void zet6231_i2c_set_input_params(struct zet6231_i2c *touch)
{
	struct input_dev *input = touch->input;

	input->name = "touchpanel";
	//input->phys = touch->client->adapter->name;
	input->id.bustype = BUS_I2C;
	input->id.version = 0x0001;
	input->dev.parent = &touch->client->dev;
	input_set_drvdata(input, touch);

	/* Register the device as touch */
	__set_bit(EV_REL, input->evbit);
	__set_bit(REL_X, input->relbit);
	__set_bit(REL_Y, input->relbit);
	__set_bit(REL_Z, input->relbit);
	__set_bit(EV_KEY, input->evbit);
	__set_bit(BTN_LEFT, input->keybit);
	__set_bit(BTN_RIGHT, input->keybit);
}

static void zet6231_TPC_init(struct i2c_client *client){
	int ret = 0;
	u8 uRomType;

	ctp_set_reset_low();

	msleep(1);
  
	ret = zet622x_cmd_sndpwd(client);
	if(ret <= 0){
		printk("zet6231_TPC_probe zet622x_cmd_sndpwd fail\n");
		return;
	}
	msleep(20);
  
	uRomType = ROM_TYPE_OTP;
	ret = zet622x_cmd_codeoption(client, &uRomType);
	if (ret < 0){
		printk("zet6231_TPC_probe zet622x_cmd_codeoption fail\n");
		return;
	}
	msleep(10);

	ctp_set_reset_high();
}

static struct zet6231_i2c *zet6231_i2c_touch_create(struct i2c_client *client)
{
	struct zet6231_i2c *touch;

	touch = kzalloc(sizeof(struct zet6231_i2c), GFP_KERNEL);
	if (!touch)
		return NULL;

	touch->client = client;
	touch->irq = -1;
	touch->touch_mode = TOUCH_MODE_IDEL;
	INIT_DELAYED_WORK(&touch->dwork, zet6231_i2c_work_handler);
	spin_lock_init(&touch->lock);

	touch->power = regulator_get(NULL, "touchpanel");
	if (IS_ERR(touch->power)) {
	        dev_err(&client->dev, "Get touchpanel device failed\n");
		touch->power = NULL;
	} else
		regulator_enable(touch->power);

	return touch;
}

static int zet6231_TPC_probe(struct i2c_client *client, const struct i2c_device_id *id){
	int ret;

	struct zet6231_i2c *touch = NULL;

	//printk("zet6231_TPC_probe in\n");
#if 1
	//zet6231_downloader(client);
	zet6231_TPC_init(client);
#if 1
	touch = zet6231_i2c_touch_create(client);
	if (!touch)
		return -ENOMEM;

	touch->input = input_allocate_device();
	if (!touch->input) {
		ret = -ENOMEM;
		goto err_mem_free;
	}

	zet6231_i2c_set_input_params(touch);

	ret = input_register_device(touch->input);
	if (ret) {
		printk("Input device register failed: %d\n", ret);
		goto err_input_free;
	}

	i2c_set_clientdata(client, touch);

	if (client->irq < 0){
		printk("zet6231 do not have irq\n");
		goto err_input_free;
	}
	printk("irq:%d\n", client->irq);

	if (gpio_request(client->irq, "zet6231_irq") < 0){
		printk("Request GPIO %d error!\n", client->irq);
	}

	if (gpio_direction_input(client->irq) < 0) {
		printk("Config GPIO %d error!\n", client->irq);
	}

	touch->irq_gpio = client->irq;
	touch->irq = gpio_to_irq(client->irq);
	if (touch->irq < 0) {
		printk("GPIO to irq error!\n");
	}

	ret = request_irq(touch->irq, zet6231_i2c_irq,
			  IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING | IRQF_DISABLED,
			  DRIVER_NAME, touch);
#endif

	// printk("zet6231_TPC_probe successful\n");

	return 0;

 err_input_free:
	input_free_device(touch->input);
 err_mem_free:
	kfree(touch);
#endif
	return -1;
}

static int zet6231_TPC_remove(struct i2c_client *client){

	struct zet6231_i2c *touch = i2c_get_clientdata(client);

	input_unregister_device(touch->input);

	if (touch->power) {
		regulator_disable(touch->power);
		regulator_put(touch->power);
	}

	kfree(touch);

	return 0;
}

#ifdef CONFIG_PM
static int zet6231_i2c_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct zet6231_i2c *touch = i2c_get_clientdata(client);

	u8 ts_sleep_cmd[1] = {0xb1}; 

	disable_irq(touch->irq);

	zet622x_i2c_write_tsdata(client, ts_sleep_cmd, 1);

	udelay(5*1000);

	cancel_delayed_work_sync(&touch->dwork);

	if (touch->power)
		regulator_disable(touch->power);

	return 0;
}

static int zet6231_i2c_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct zet6231_i2c *touch = i2c_get_clientdata(client);

	u8 ts_wake_cmd[1] = {0xb4};

	if (touch->power)
		regulator_enable(touch->power);

	zet6231_i2c_reschedule_work(touch, msecs_to_jiffies(NO_DATA_SLEEP_MSECS));

	enable_irq(touch->irq);

	zet622x_i2c_write_tsdata(client, ts_wake_cmd, 1);

	udelay(5*1000);
	
	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(zet6231_i2c_pm, zet6231_i2c_suspend,
			 zet6231_i2c_resume);

static struct i2c_driver zet6231_TPC_driver = {
        .driver     = {
                .name   = "zet6231",
		.owner	= THIS_MODULE,
		.pm	= &zet6231_i2c_pm,
        },
        .probe      = zet6231_TPC_probe,
        .remove     = zet6231_TPC_remove,
	.id_table = zet6231_id,
};

static int __init zet6231_init(void)
{
	return i2c_add_driver(&zet6231_TPC_driver);
}

static void __exit zet6231_exit(void)
{
	i2c_del_driver(&zet6231_TPC_driver);
}
module_init(zet6231_init);
module_exit(zet6231_exit);

MODULE_AUTHOR("hpwang");
MODULE_DESCRIPTION("zet6231 TOUCH PANEL CONTROLLER");
MODULE_LICENSE("GPL");
