/* drivers/input/touchpanel/ite7236_tp.c
 *
 * FocalTech ite7236 TouchPanel driver.
 *
 * Copyright (c) 2010  Focal tech Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/cdev.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/earlysuspend.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/semaphore.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <soc/gpio.h>
#include <linux/syscalls.h>
#include <linux/unistd.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/regulator/consumer.h>
#include <linux/device.h> 
#include <linux/tsc.h>
#include <jz_notifier.h>
#include <linux/irq.h>
#include <linux/workqueue.h>
#include <linux/pm.h>

#include "IT7236-Slider-Button-V02.h"

#define GPIO_INT                        GPIO_PA(29)

static u8 wTemp[128] = {0x00};
static int fw_upgrade_success = 0;
static char config_id[10];
static struct ite7236_tp_data *gl_ts;

struct ite7236_tp_data {
        unsigned int irq;
        unsigned char tp_resume_status;
	int last_x;
	int status;
        struct i2c_client *client;
        struct input_dev *input_dev;
	struct regulator *ite7236_power;
        struct work_struct  work;
        struct workqueue_struct *workqueue;
	spinlock_t lock;
};

#ifdef CONFIG_VIDEO_JZ_HS_ISP
extern int getCameraKey_WakeUp(void);
#endif

/* static int get_config_ver(void); */

static int i2cReadFromIt7280(struct i2c_client *client, unsigned char bufferIndex, unsigned char dataBuffer[], unsigned short dataLength)
{
	int ret, retry = 3;
	struct i2c_msg msgs[2] = { {
		.addr = client->addr,
		.flags = 0,
		.len = 1,
		.buf = &bufferIndex
		}, {
		.addr = client->addr,
		.flags = I2C_M_RD,
		.len = dataLength,
		.buf = dataBuffer
		}
	};
	memset(dataBuffer, 0x56, dataLength);

	do {
		ret = i2c_transfer(client->adapter, msgs, 2);
		retry--;
	} while((ret != 2) && (retry > 0));

	if(ret != 2) {
		pr_err("%s : i2c_transfer error\n", __func__);
		return -1;
	}
	return 0;
}

static int i2cWriteToIt7280(struct i2c_client *client, unsigned char bufferIndex, unsigned char *dataBuffer, unsigned short dataLength)
{
	unsigned char buffer4Write[256];
	struct i2c_msg msg;

	if (dataLength < 256) {

		buffer4Write[0] = bufferIndex;
		memcpy(&(buffer4Write[1]), dataBuffer, dataLength);

		msg.addr = client->addr;
		msg.flags = client->flags & I2C_M_TEN;
		msg.len = dataLength + 1;
		msg.buf = buffer4Write;

		return i2c_transfer(client->adapter, &msg, 1);
	} else {
		pr_err("%s : i2c_transfer error , out of size\n", __func__);
		return -1;
	}
}

static void updater_touch(struct ite7236_tp_data *ite7236_tp, int event)
{
	struct input_dev *input = ite7236_tp->input_dev;

	input_event(input, EV_KEY, event, 1);
	input_event(input, EV_KEY, event, 0);
	input_sync(input);
	// printk("Report : %4d %4d %4x\n", x, y, z);
}

/* Event handle
 * ---------------------------------------------------------
 * | Area0 | Area1 | Area2 | Area3 | Area4 | Area5 | Area6 |
 * ---------------------------------------------------------
 *   BACK   39  .......................................  03
 *
 * Buffer[0] : 0x40 means touch back area, other no means. 0x00 no press.
 * pucBuffer[1] : position value from 39 to 03.
 */

#define ITE_STATUS_FREE 0
#define ITE_STATUS_BACK 1
#define ITE_STATUS_TOUCH 2
#define SCALE 14
static void ite7236_work_handler(struct work_struct *work){
	struct ite7236_tp_data *tp = container_of(work, struct ite7236_tp_data, work);
	unsigned char pucBuffer[2];
	unsigned char  delta;

	i2cReadFromIt7280(tp->client, 0xfd, pucBuffer, 2);
	printk("Raw : %4d 0x%02x\n", pucBuffer[0]);

	delta = pucBuffer[0];

	if (delta == 0x11){//tap
	  updater_touch(tp, KEY_P);
	}else if (delta == 0x5){//font
#ifdef CONFIG_VIDEO_JZ_HS_ISP
	    if(0 == getCameraKey_WakeUp())updater_touch(tp, KEY_L);
#else
	       updater_touch(tp, KEY_L);
#endif
	}else if (delta == 0x6){//back
	  updater_touch(tp, KEY_R);
	}else if (delta == 0x14){//long
	  updater_touch(tp, KEY_Q);
	}
}

static irqreturn_t ite7236_tp_interrupt(int irq, void *dev_id)
{
        struct ite7236_tp_data *ite7236_tp = dev_id;
	unsigned long flags;

	// jz_notifier_call(NOTEFY_PROI_NORMAL, JZ_CLK_CHANGING, NULL);
        // disable_irq_nosync(ite7236_tp->irq);

	spin_lock_irqsave(&ite7236_tp->lock, flags);

	queue_work(ite7236_tp->workqueue, &ite7236_tp->work);

	spin_unlock_irqrestore(&ite7236_tp->lock, flags);

	// enable_irq(ite7236_tp->irq);

        return IRQ_HANDLED;

}

int hstp_is_tp_forward(void){
	struct ite7236_tp_data *tp = gl_ts;
	if(tp == NULL)return 0;
 
	if(tp->tp_resume_status == 0x5)return 1;
	else return 0;
}

static int ite7236_tp_suspend(struct device *dev)
{
	char pucBuffer[3],data[2];
	int count=0;

	printk("ite7236_tp_suspend enter !\n");

	if(gl_ts != NULL)enable_irq_wake(gl_ts->irq);
#if 0
	// disable_IRQ
	disable_irq_nosync(gl_ts->client->irq);
	
	do{
	  	i2cReadFromIt7280(gl_ts->client, 0xFA, data, 2);
	  	pucBuffer[0] = 0x00;
	  	i2cWriteToIt7280(gl_ts->client, 0xF0, pucBuffer, 1);//set Pate to Page0
	  	
		pucBuffer[0] = 0x80;
		i2cWriteToIt7280(gl_ts->client, 0xF1, pucBuffer, 1);//set 0XF1 bit7 t0 1
	      
	      
		i2cReadFromIt7280(gl_ts->client, 0xFA, data, 2);
		count++;
	} while( ((data[0]&0X01) != 0x01) && (count < 5));//0xFA  bit0  =1?
	
        pucBuffer[0] = 0x01;//C/R
     	pucBuffer[1] = 0x30;//CMD=0x30,Set power mode
    	pucBuffer[2] = 0x02;//sub cmd =0x02,sleep mode
	i2cWriteToIt7280(gl_ts->client, 0x40, pucBuffer, 3);//write commond buffer at 0X40
             
	pucBuffer[0] = 0x40;
	i2cWriteToIt7280(gl_ts->client, 0xF1, pucBuffer, 1);//set Pate to Page0
	  	
	i2cReadFromIt7280(gl_ts->client, 0xF3, data, 2);
	  	
	pucBuffer[0] = 0x00;
	i2cWriteToIt7280(gl_ts->client, 0xF0, pucBuffer, 1);//set Pate to Page0
	  	  	
	pucBuffer[0] = 0x80;
	i2cWriteToIt7280(gl_ts->client, 0xF1, pucBuffer, 1);//set 0XF1 bit7 t0 1
	   	  	
	i2cReadFromIt7280(gl_ts->client, 0xFA, data, 2);
#endif	
	return 0;
}

static int ite7236_tp_resume(struct device *dev)
{
	/* char pucBuffer[2],data[2]; */
	/* int count=0; */


	struct ite7236_tp_data *tp = gl_ts;
	unsigned char pucBuffer[2];
	unsigned char  delta;

	printk("ite7236_tp_resume enter \n");
	if(gl_ts != NULL)disable_irq_wake(gl_ts->irq);

	i2cReadFromIt7280(tp->client, 0xfd, pucBuffer, 2);
	printk("Raw : %4d 0x%02x\n", pucBuffer[0]);

	tp->tp_resume_status = pucBuffer[0];

#if 0
	pucBuffer[0] = 0x55; 
	i2cWriteToIt7280(gl_ts->client, 0xFB, pucBuffer, 1); 
	i2cWriteToIt7280(gl_ts->client, 0xFB, pucBuffer, 1); 

	do{
	  	i2cReadFromIt7280(gl_ts->client, 0xFA, data, 2);
	  	
	  	pucBuffer[0] = 0x00;
	  	i2cWriteToIt7280(gl_ts->client, 0xF0, pucBuffer, 1);//set Pate to Page0
	  	
		pucBuffer[0] = 0x80;
		i2cWriteToIt7280(gl_ts->client, 0xF1, pucBuffer, 1);//set 0XF1 bit7 t0 1
	      	      
		i2cReadFromIt7280(gl_ts->client, 0xFA, data, 2);
		count++;
	} while( ((data[0]&0X01) != 0x01) && (count < 5));//0xFA  bit0  =1?

	pucBuffer[0] = 0x01;//C/R
 	pucBuffer[1] = 0xF0;//CMD=0x30,Set power mode
	i2cWriteToIt7280(gl_ts->client, 0x40, pucBuffer,2);//write commond buffer at 0X40

	pucBuffer[0] = 0x40;
  	i2cWriteToIt7280(gl_ts->client, 0xF1, pucBuffer, 1);//set Pate to Page0
  		
	pucBuffer[0] = 0x00;
  	i2cWriteToIt7280(gl_ts->client, 0xF0, pucBuffer, 1);//set Pate to Page0
  	
	pucBuffer[0] = 0x80;
	i2cWriteToIt7280(gl_ts->client, 0xF1, pucBuffer, 1);//set 0XF1 bit7 t0 1
  	
	i2cReadFromIt7280(gl_ts->client, 0xFA, data, 2);
      
        enable_irq(gl_ts->client->irq);
#endif

	return 0;
}

static bool fnFirmwareReinitialize(void)
{
	int count = 0;
	u8 data[1];
	char pucBuffer[1];

	pucBuffer[0] = 0xFF;
	i2cWriteToIt7280(gl_ts->client, 0xF6, pucBuffer, 1);
	pucBuffer[0] = 0x64;
	i2cWriteToIt7280(gl_ts->client, 0xF0, pucBuffer, 1);
	pucBuffer[0] = 0x3F;
	i2cWriteToIt7280(gl_ts->client, 0x00, pucBuffer, 1);
	pucBuffer[0] = 0x7C;
	i2cWriteToIt7280(gl_ts->client, 0x01, pucBuffer, 1);
	pucBuffer[0] = 0x00;
	i2cWriteToIt7280(gl_ts->client, 0x00, pucBuffer, 1);
	pucBuffer[0] = 0x00;
	i2cWriteToIt7280(gl_ts->client, 0x01, pucBuffer, 1);

	do{
		i2cReadFromIt7280(gl_ts->client, 0xFA, data, 1);
		count++;
	} while( (data[0] != 0x80) && (count < 20));

	pucBuffer[0] = 0x00;
	i2cWriteToIt7280(gl_ts->client, 0xF1, pucBuffer, 1);

	return true;
}

static bool waitCommandDone(void)
{
	unsigned char ucQuery = 0x00;
	unsigned int count = 0;

	mdelay(200);
	
	do {
		if(!i2cReadFromIt7280(gl_ts->client, 0xFA, &ucQuery, 1)) {
			ucQuery = 0x00;
		}
		count++;
	} while((ucQuery != 0x80) && (count < 10));

	if( ucQuery == 0x80)
		return  true;
	else
		return  false;
}

static int get_config_ver(void)
{
	char pucBuffer[1];
	int ret;

	// 1. Request Full Authority of All Registers
	pucBuffer[0] = 0x01;
	i2cWriteToIt7280(gl_ts->client, 0xF1, pucBuffer, 1);

	// 2. Assert Reset of MCU
	pucBuffer[0] = 0x64;
	i2cWriteToIt7280(gl_ts->client, 0xF0, pucBuffer, 1);
	pucBuffer[0] = 0x04;
	i2cWriteToIt7280(gl_ts->client, 0x01, pucBuffer, 1);

	// 3. Assert EF enable & reset
	pucBuffer[0] = 0x10;
	i2cWriteToIt7280(gl_ts->client, 0x2B, pucBuffer, 1);
	pucBuffer[0] = 0x00;
	i2cWriteToIt7280(gl_ts->client, 0xF0, pucBuffer, 1);

	// 4. Test Mode Enable
	pucBuffer[0] = 0x07;
	gl_ts->client->addr = 0x7F;
	i2cWriteToIt7280(gl_ts->client, 0xF4, pucBuffer, 1);
	gl_ts->client->addr = 0x46;

	// 5. Page switch to 4
	pucBuffer[0] = 0x04;
	i2cWriteToIt7280(gl_ts->client, 0xF0, pucBuffer, 1);

	// 6. Write EF Read Mode Cmd
	pucBuffer[0] = 0x01;
	i2cWriteToIt7280(gl_ts->client, 0xF7, pucBuffer, 1);

	gl_ts->client->addr = 0x7F;
	ret = i2cReadFromIt7280(gl_ts->client, 0x00, wTemp, 10);
	gl_ts->client->addr = 0x46;

	// 7. Write EF Standby Mode Cmd
	pucBuffer[0] = 0x00;
	i2cWriteToIt7280(gl_ts->client, 0xF7, pucBuffer, 1);

	// 8. Power On Reset
	fnFirmwareReinitialize();
	memcpy(config_id, wTemp+6, 4);
	pr_info("IT7236 version:%d.%d.%d.%d\n", config_id[0], config_id[1], config_id[2], config_id[3]);

	return ret;
}

static int it7236_upgrade(u8* InputBuffer, int fileSize)
{
	int i, j, k;
	int StartPage = 0;
	int registerI, registerJ;
	int page, pageres;
	int Addr;
	int nErasePage;
	u8 result = 1;
	u8 DATABuffer1[128] = {0x00};
	u8 DATABuffer2[128] = {0x00};
	u8 OutputDataBuffer[8192] = {0x00};
	u8 WriteDATABuffer[128] = {0x00};
	char pucBuffer[1];
	int retry = 0;
	int ret1, ret2;
	
	struct regulator *power = regulator_get(NULL, "touchpanel");
	regulator_disable(power);	
	mdelay(200);
	
	regulator_enable(power);
	mdelay(200);

	printk("ite7236 upgrade\n");

	// Request Full Authority of All Registers
	pucBuffer[0] = 0x01;
	i2cWriteToIt7280(gl_ts->client, 0xF1, pucBuffer, 1);

	// 1. Assert Reset of MCU
	pucBuffer[0] = 0x64;
	i2cWriteToIt7280(gl_ts->client, 0xF0, pucBuffer, 1);
	pucBuffer[0] = 0x04;
	i2cWriteToIt7280(gl_ts->client, 0x01, pucBuffer, 1);

	// 2. Assert EF enable & reset
	pucBuffer[0] = 0x10;
	i2cWriteToIt7280(gl_ts->client, 0x2B, pucBuffer, 1);
	pucBuffer[0] = 0x00;
	i2cWriteToIt7280(gl_ts->client, 0xF0, pucBuffer, 1);

	// 3. Test Mode Enable
	pucBuffer[0] = 0x07;
	gl_ts->client->addr = 0x7F;
	i2cWriteToIt7280(gl_ts->client, 0xF4, pucBuffer, 1);
	gl_ts->client->addr = 0x46;
	nErasePage = fileSize/256;
	if(fileSize % 256 == 0)
		nErasePage -= 1;

	// 4. EF HVC Flow (Erase Flash)
	for( i = 0 ; i < nErasePage + 1 ; i++ ){
		// EF HVC Flow
		pucBuffer[0] = i+StartPage;
		i2cWriteToIt7280(gl_ts->client, 0xF0, pucBuffer, 1);	// Select axa of EF E/P Mode
		pucBuffer[0] = 0x00;
		i2cWriteToIt7280(gl_ts->client, 0xF9, pucBuffer, 1);	// Select axa of EF E/P Mode Fail
		pucBuffer[0] = 0xB2;
		i2cWriteToIt7280(gl_ts->client, 0xF7, pucBuffer, 1);	// Select efmode of EF E/P Mode(all erase)
		pucBuffer[0] = 0x80;
		i2cWriteToIt7280(gl_ts->client, 0xF9, pucBuffer, 1);	// Pump Enable
		mdelay(10);
		pucBuffer[0] = 0xB6;
		i2cWriteToIt7280(gl_ts->client, 0xF7, pucBuffer, 1);	// Write EF CHVPL Mode Cmd
		pucBuffer[0] = 0x00;
		i2cWriteToIt7280(gl_ts->client, 0xF7, pucBuffer, 1);	// Write EF Standby Mode Cmd
	}

	// 5. EFPL Flow - Write EF
	for (i = 0; i < fileSize; i += 256)
	{
		pucBuffer[0] = 0x05;
		i2cWriteToIt7280(gl_ts->client, 0xF7, pucBuffer, 1);	// Write EF PL Mode Cmd
		// Write EF Data - half page(128 bytes)
		for(registerI = 0 ; registerI < 128; registerI++)
		{
			if(( i + registerI ) < fileSize ) {
				DATABuffer1[registerI] = InputBuffer[i+registerI];
			}
			else {
				DATABuffer1[registerI] = 0x00;
			}
		}
		for(registerI = 128 ; registerI < 256; registerI++)
		{
			if(( i + registerI ) < fileSize ) {
				DATABuffer2[registerI - 128] = InputBuffer[i+registerI];
			}
			else {
				DATABuffer2[registerI - 128] = 0x00;
			}
		}
		registerJ = i & 0x00FF;
		page = ((i & 0x3F00)>>8) + StartPage;
		pageres = i % 256;
		retry = 0;
		do {
			pucBuffer[0] = page;
			i2cWriteToIt7280(gl_ts->client, 0xF0, pucBuffer, 1);
			i2cReadFromIt7280(gl_ts->client, 0xF0, pucBuffer, 1);;
			retry++;
		} while((pucBuffer[0] != page) && (retry < 10));

		/* write 256 bytes once */
		retry = 0;

		gl_ts->client->addr = 0x7F;
		do {
			ret1 = i2cWriteToIt7280(gl_ts->client, 0x00, DATABuffer1, 128);
			ret2 = i2cWriteToIt7280(gl_ts->client, 0x00 + 128, DATABuffer2, 128);
			retry++;
		} while(((ret1 * ret2) != 1) && (retry < 10));
		gl_ts->client->addr = 0x46;
		pucBuffer[0] = 0x00;
		i2cWriteToIt7280(gl_ts->client, 0xF7, pucBuffer, 1);	// Write EF Standby Mode Cmd
		pucBuffer[0] = 0x00;
		i2cWriteToIt7280(gl_ts->client, 0xF9, pucBuffer, 1);	// Select axa of EF E/P Mode Fail
		pucBuffer[0] = 0xE2;
		i2cWriteToIt7280(gl_ts->client, 0xF7, pucBuffer, 1);	// Select efmode of EF E/P Mode(all erase)
		pucBuffer[0] = 0x80;
		i2cWriteToIt7280(gl_ts->client, 0xF9, pucBuffer, 1);	// Pump Enable
		mdelay(10);
		pucBuffer[0] = 0xE6;
		i2cWriteToIt7280(gl_ts->client, 0xF7, pucBuffer, 1);	// Write EF CHVPL Mode Cmd
		pucBuffer[0] = 0x00;
		i2cWriteToIt7280(gl_ts->client, 0xF7, pucBuffer, 1);	// Write EF Standby Mode Cmd
	}
	// 6. Page switch to 0
	pucBuffer[0] = 0x00;
	i2cWriteToIt7280(gl_ts->client, 0xF0, pucBuffer, 1);

	// 7. Write EF Read Mode Cmd
	pucBuffer[0] = 0x01;
	i2cWriteToIt7280(gl_ts->client, 0xF7, pucBuffer, 1);		// Write EF Standby Mode Cmd

	// 8. Read EF Data, Compare the firmware and input data. for j loop
	for ( j = 0; j < fileSize; j+=128)
	{
		page = ((j & 0x3F00)>>8) + StartPage;			// 3F = 0011 1111, at most 32 pages
		pageres = j % 256;
		pucBuffer[0] = page;
		i2cWriteToIt7280(gl_ts->client, 0xF0, pucBuffer, 1);
	
		gl_ts->client->addr = 0x7F;
		i2cReadFromIt7280(gl_ts->client, 0x00 + pageres, wTemp, 128); // use 0x7f to read data	
		gl_ts->client->addr = 0x46;
		// Compare Flash Data
		for( k = 0 ; k < 128 ; k++ )
		{
			if( j+k >= fileSize )
				break;

			pageres = (j + k) % 256;
			OutputDataBuffer[j+k] = wTemp[k];
			WriteDATABuffer[k] = InputBuffer[j+k];

			if(OutputDataBuffer[j+k] != WriteDATABuffer[k])
			{
				Addr = page << 8 | pageres;
				result = 0;
			}
		}
	}
	if(!result)
	{
		fw_upgrade_success = 1;
		return -1;
	}

	// 9. Write EF Standby Mode Cmd
	pucBuffer[0] = 0x00;
	i2cWriteToIt7280(gl_ts->client, 0xF7, pucBuffer, 1);

	// 10. Power On Reset
	pr_info("[IT7236] : success to upgrade firmware\n\n");
	get_config_ver();
	fw_upgrade_success = 0;

	return 0;
}

static u32 get_firmware_ver(void)
{
	char pucBuffer[1];
        u8  wTemp[4];
        u32  fw_version;

	waitCommandDone();
	pucBuffer[0] = 0x00;
	i2cWriteToIt7280(gl_ts->client, 0xF0, pucBuffer, 1);
	pucBuffer[0] = 0x80;
	i2cWriteToIt7280(gl_ts->client, 0xF1, pucBuffer, 1);
	pucBuffer[0] = 0x01;
	i2cWriteToIt7280(gl_ts->client, 0x40, pucBuffer, 1);
	pucBuffer[0] = 0x01;
	i2cWriteToIt7280(gl_ts->client, 0x41, pucBuffer, 1);
	pucBuffer[0] = 0x40;
	i2cWriteToIt7280(gl_ts->client, 0xF1, pucBuffer, 1);
	pucBuffer[0] = 0x00;
	i2cWriteToIt7280(gl_ts->client, 0xF0, pucBuffer, 1);
	pucBuffer[0] = 0x80;
	i2cWriteToIt7280(gl_ts->client, 0xF1, pucBuffer, 1);
	
	i2cReadFromIt7280(gl_ts->client, 0x48, wTemp, 4);
	fw_version = (wTemp[0] << 24) | (wTemp[1] << 16) | (wTemp[2] << 8) | (wTemp[3]);

	printk("IT7236 version:%d.%d.%d.%d\n", wTemp[0], wTemp[1], wTemp[2], wTemp[3]);

	return fw_version;
}

static ssize_t IT7280_upgrade_store(void)
{
        u32 read_version ;
	u32 raw_version = 0x12000206;
	int retval = 0;
	u8 data[2];
	
	retval = i2cReadFromIt7280(gl_ts->client, 0xFA, data, 1);
	if (retval == -1){
	  printk("force upgrade\n");
	  //it7236_upgrade((u8 *)rawData,sizeof(rawData));
	  return 0;
	}
        read_version = get_firmware_ver();
	printk("read_version:%x\n", read_version);
        if (read_version != raw_version ){
	  printk("need upgrade\n");
	  it7236_upgrade((u8 *)rawData,sizeof(rawData));
	}
	return 0;
}

static struct ite7236_tp_data *ite7236_i2c_touch_create(struct i2c_client *client)
{
	struct ite7236_tp_data *ite7236_tp = NULL;
	
	int err = 0;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
                err = -ENODEV;
        }

        ite7236_tp = kzalloc(sizeof(struct ite7236_tp_data), GFP_KERNEL);
        if (!ite7236_tp) {
                dev_err(&client->dev, "failed to allocate input driver data\n");
                err = -ENOMEM;
        }

	ite7236_tp->client = client;
	ite7236_tp->irq = -1;
	ite7236_tp->workqueue = create_singlethread_workqueue(/* dev_name(&client->dev) */"ite7236_tp_i2c");
	ite7236_tp->last_x = 0;
	ite7236_tp->status = 0;
        INIT_WORK(&ite7236_tp->work, ite7236_work_handler);
	
	ite7236_tp->ite7236_power = regulator_get(NULL, "touchpanel");
	regulator_enable(ite7236_tp->ite7236_power);

	return ite7236_tp;
}

static int ite7236_tp_probe(struct i2c_client *client,
                const struct i2c_device_id *id)
{
        struct ite7236_tp_data *ite7236_tp = NULL;

        int err = 0;
	int retval = 0;
	printk("ite7236_tp_probe\n");
	ite7236_tp = ite7236_i2c_touch_create(client);
	if (!ite7236_tp) {
	        retval = -ENOMEM;
		printk("touch_create error\n");
		goto exit_check_retval_err;
	}

        i2c_set_clientdata(client, ite7236_tp);
	
	if (gpio_request(client->irq, "ite7236_irq") < 0) {
		printk("gpio_request error!!!\n");
		goto exit_check_retval_err;
	}
	if (gpio_direction_input(client->irq) < 0) {
		printk("gpio_direction_input error!!!\n");
		goto exit_check_retval_err;
	}

	ite7236_tp->irq = gpio_to_irq(client->irq);
	if (ite7236_tp->irq < 0) {
		printk("gpio_to_irq error!!!\n");
		goto exit_check_retval_err;
	}

	retval = request_irq(ite7236_tp->irq, ite7236_tp_interrupt,
		    IRQF_TRIGGER_FALLING | IRQF_DISABLED/* pdata->irqflags */, "ite7236_irq",
		    ite7236_tp);
	if (retval != 0) {
	        printk("request_irq error!!!\n");
		goto exit_request_irq_err;
	}

	gl_ts = ite7236_tp;

	gpio_direction_output(GPIO_INT, 1);
	disable_irq_nosync(ite7236_tp->irq);
	IT7280_upgrade_store();//key upgrade
        gpio_direction_input(GPIO_INT);
        enable_irq(ite7236_tp->irq);

	ite7236_tp->input_dev = input_allocate_device();
        if (!ite7236_tp->input_dev) {
                err = -ENOMEM;
                goto exit_input_dev_alloc_err;
        }

	ite7236_tp->input_dev->name = "touchpanel";
	ite7236_tp->input_dev->id.bustype = BUS_I2C;

	ite7236_tp->input_dev->dev.parent = &ite7236_tp->client->dev;
	input_set_drvdata(ite7236_tp->input_dev, ite7236_tp);

	/* Register the device as mouse */
	__set_bit(EV_REL, ite7236_tp->input_dev->evbit);
	__set_bit(REL_X, ite7236_tp->input_dev->relbit);
	__set_bit(REL_Y, ite7236_tp->input_dev->relbit);
	__set_bit(REL_Z, ite7236_tp->input_dev->relbit);
	__set_bit(EV_KEY, ite7236_tp->input_dev->evbit);
	__set_bit(BTN_LEFT, ite7236_tp->input_dev->keybit);
	__set_bit(KEY_L, ite7236_tp->input_dev->keybit);
	__set_bit(KEY_P, ite7236_tp->input_dev->keybit);
	__set_bit(KEY_Q, ite7236_tp->input_dev->keybit);
	__set_bit(KEY_R, ite7236_tp->input_dev->keybit);
	__set_bit(KEY_A, ite7236_tp->input_dev->keybit);
	__set_bit(KEY_B, ite7236_tp->input_dev->keybit);
	__set_bit(BTN_RIGHT, ite7236_tp->input_dev->keybit);

        err = input_register_device(ite7236_tp->input_dev);
        if (err) {
                dev_err(&client->dev,
                                "ite7236_tp_probe: failed to register input device: %s\n",
                                dev_name(&client->dev));
                goto exit_input_register_device_err;
        }

        return 0;

exit_input_register_device_err:
	input_free_device(ite7236_tp->input_dev);
exit_input_dev_alloc_err:
	free_irq(ite7236_tp->irq, ite7236_tp);
exit_request_irq_err:

exit_check_retval_err:

	if (ite7236_tp->ite7236_power) {
		regulator_disable(ite7236_tp->ite7236_power);
		regulator_put(ite7236_tp->ite7236_power);
	}

	kfree(ite7236_tp);

	return retval;
}

static int __devexit ite7258_ts_remove(struct i2c_client *client)
{
        return 0;
}

static const struct dev_pm_ops ite7236_tp_pm_ops = {
	.suspend        = ite7236_tp_suspend,
	.resume		= ite7236_tp_resume,
};

static const struct i2c_device_id ite7236_tp_id[] = {
        {"ite7236_tp_i2c", 0},
        {}
};

MODULE_DEVICE_TABLE(i2c, ite7236_tp_id);

static struct i2c_driver ite7236_tp_driver = {
        .driver = {
                .name = "ite7236_tp_i2c",
                .owner = THIS_MODULE,
		.pm	= &ite7236_tp_pm_ops,
        },

        .probe = ite7236_tp_probe,
        .remove = __devexit_p(ite7258_ts_remove),
        .id_table = ite7236_tp_id,
};

static int __init ite7236_tp_init(void)
{
        int ret;
        ret = i2c_add_driver(&ite7236_tp_driver);
        return ret;
}

static void __exit ite7236_tp_exit(void)
{
        i2c_del_driver(&ite7236_tp_driver);
}

module_init(ite7236_tp_init);
module_exit(ite7236_tp_exit);

MODULE_AUTHOR("cyan");
MODULE_DESCRIPTION("FocalTech ite7236 TouchPanel driver");
MODULE_LICENSE("GPL");
