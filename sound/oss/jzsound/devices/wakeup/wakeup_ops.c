#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>   /* printk() */
#include <linux/slab.h>     /* kmalloc() */
#include <linux/fs.h>       /* everything... */
#include <linux/errno.h>    /* error codes */
#include <linux/types.h>    /* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h>    /* O_ACCMODE */
#include <linux/seq_file.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/sched.h>
#include <linux/io.h>
#include <linux/memory.h>
#include <linux/mm.h>
#include <asm/cacheops.h>
#include <linux/dma-mapping.h>
#include <linux/syscalls.h>
#include <asm/system.h>
#include <asm/uaccess.h>    /* copy_*_user */

#include <mach/jzdma.h>
#include <mach/jztcu.h>


#include "ivDefine.h"
#include "ivIvwDefine.h"
#include "ivIvwErrorCode.h"
#include "ivIVW.h"
#include "ivPlatform.h"

#include "wakeup_ops.h"
#include "timer.h"

//#define WAKEUP_TIMEOUT_VAL	(3000)	/*3000ms*/
#define WAKEUP_TIMEOUT_VAL	(1000)	/*1000ms*/

#define VOICE_RES_SIZE	(512 * 1024)	/*512KBytes*/

static int wakeup_prepare_dma_desc(struct wakeup_module *wakeup);
static int wakeup_close_dma(struct wakeup_module *wakeup);
static int wakeup_start_dma_3(struct wakeup_module *wakeup);
static int wakeup_wait_done(struct wakeup_module *wakeup);

/******************sysfs************************************/
static ssize_t voice_wakeup_set_res(struct wakeup_module *wakeup,
						   const char *buf, size_t count)
{

	if(wakeup->IvwObjCreated == true) {
		/* reset wakeup resource. */
		wakeup->pResKeyoffset = 0;
		wakeup->IvwObjCreated = false;
		//return count;
	}

	if(wakeup->pResKeyoffset >= WAKEUP_RES_SIZE) {
		printk("resource file is too large, voice wakeup may failed\n");
	} else  {
		//printk("wakeup->pResKeyoffset:%d\n", wakeup->pResKeyoffset);
		memcpy(wakeup->sys_pReskey + wakeup->pResKeyoffset, buf, count);
		wakeup->pResKeyoffset += count;
	}
	return count;
}

#define SYSFS_CTL_CREATE_OBJECT		(0x1)
#define SYSFS_CTL_ENABLE_WAKEUP		(0x2)
#define SYSFS_CTL_DISABEL_WAKEUP	(0x3)
static ssize_t voice_wakeup_ctl(struct wakeup_module *wakeup,
					const char *buf, size_t count)
{
	//struct wakeup_dev * wakeup_dev = (struct wakeup_dev *)dev_get_drvdata(dev);
	/*get buf, switch buf, create ivwObject*/
	unsigned long ctl;
	int rc;
	ivStatus iStatus;

	rc = strict_strtoul(buf, 0, &ctl);
	if(rc)
		return rc;
	printk("ctl is :%ld\n", ctl);
	switch(ctl) {
		case SYSFS_CTL_CREATE_OBJECT:
			/* need to call only once */
			if(wakeup->IvwObjCreated == false) {
				iStatus = IvwCreate(wakeup->pIvwObj, &wakeup->nIvwObjSize,
						wakeup->pResidentRAM, &wakeup->nResidentRAMSize,
						wakeup->sys_pReskey, wakeup->nWakeupNetworkID);
				if( IvwErrID_OK != iStatus ){
					//printk("IvwVreate Error: %x\n", iStatus);
				} else {
					printk("IvwCreate wakeup object create ok!\n");
					IvwSetParam( wakeup->pIvwObj, IVW_CM_THRESHOLD, 10, 0 ,0);

					IvwSetParam( wakeup->pIvwObj, IVW_CM_THRESHOLD, 20, 1 ,0);

					IvwSetParam( wakeup->pIvwObj, IVW_CM_THRESHOLD, 15, 2 ,0);
				}
				wakeup->IvwObjCreated = true;
			} else {
				printk("IvwObject has already created.\n");
			}
			break;
		case SYSFS_CTL_ENABLE_WAKEUP:
			printk("enable wakeup\n");
			//set_bit(F_WAKEUP_ENABLED, &wakeup->flags);

			break;
		case SYSFS_CTL_DISABEL_WAKEUP:
			printk("disable wakeup\n");
			clear_bit(F_WAKEUP_ENABLED, &wakeup->flags);
			if(test_bit(F_WAITING_WAKEUP, &wakeup->flags)) {
				wakeup_wait_done(wakeup);
			}
			break;

		default:
			break;
	}
	return count;
}

static int send_data_to_wakeup_engine(struct wakeup_module *wakeup, unsigned char *pdata, unsigned int size)
{
	unsigned int nSize_PCM = size;
	unsigned int tmp;
	int i;
	ivUInt16 CMScore  =  0;
	ivUInt16 ResID = 0;
	ivUInt16 KeywordID = 0;
	ivUInt32 StartMS = 0;
	ivUInt32 EndMS = 0;
	unsigned int Samples;
	unsigned int BytesPerSample;
	unsigned char *pData = pdata;
	unsigned int nSamples_count;
	ivStatus iStatus1;
	ivStatus iStatus2;

	Samples = 110;

	BytesPerSample = 2;
	nSamples_count = nSize_PCM / (Samples * BytesPerSample);
	/*process 16k, 16bit samples*/
	for(i = 0; i <= nSamples_count; i++ )
	{
		if(i == nSamples_count) {
			tmp = nSize_PCM % (Samples*BytesPerSample);
			if(!tmp) {
				break;
			}
			Samples = tmp / BytesPerSample;
		}
		iStatus1 = IvwAppendAudioData(wakeup->pIvwObj,(ivCPointer)pData, Samples);
		pData = pData + Samples * BytesPerSample;
		if( iStatus1 != IvwErrID_OK )
		{
			if( iStatus1 == IvwErr_BufferFull ){
				printk("IvwAppendAudioData Error:IvwErr_BufferFull\n");
			}else if( iStatus1 ==  IvwErr_InvArg ){
				printk("IvwAppendAudioData Error:IvwErr_InvArg\n");
			}else if( iStatus1 ==  IvwErr_InvCal ){
				printk("IvwAppendAudioData Error:IvwErr_InvCal\n");
			}
			//printk("IvwAppendAudioData Error: %d\n", iStatus1);
		} else {
			//printk("IvwAppendAudioData Ok!!!!!!!!\n");
		}
		iStatus2 = IvwRunStepEx( wakeup->pIvwObj, &CMScore,  &ResID, &KeywordID,  &StartMS, &EndMS );
		if( IvwErr_WakeUp == iStatus2 ){
			printk("%d: #########System is Wake Up\n", i+1);
			return STATUS_WAKEUP;
		}
#if 0
		else if( iStatus2 == IvwErrID_OK )
			printk("%d: iStatus2 == IvwErrID_OK\n", i);
		else if( iStatus2 == IvwErr_InvArg )
			printk("%d: iStatus2 == IvwErr_InvArg\n", i);
		else if( iStatus2 == IvwErr_ReEnter )
			printk("%d: iStatus2 == IvwErr_ReEnter\n", i);
		else if( iStatus2 == IvwErr_InvCal )
			printk("%d: iStatus2 == IvwErr_InvCal\n", i);
		else if( iStatus2 == IvwErr_BufferEmpty )
			printk("%d: iStatus2 == IvwErr_BufferEmpty\n", i);
#endif
	}
	return 0;
}

static void wakeup_timer_func_2(unsigned long data)
{

	struct wakeup_module *wakeup = (struct wakeup_module *)data;
	struct dma_fifo_2 *wakeup_fifo = wakeup->wakeup_fifo_2;
	struct circ_buf *xfer = &wakeup_fifo->xfer;
	int ret = 0;

	dma_addr_t trans_addr = wakeup->chan_rx->device->get_current_trans_addr(wakeup->chan_rx,\
			NULL, NULL,									\
			wakeup->dma_config.direction);

	xfer->head = (char *)KSEG1ADDR(trans_addr) - xfer->buf;

	/*process data*/
	while(1) {
		int nread;

		nread = CIRC_CNT(xfer->head, xfer->tail, wakeup_fifo->n_size);
		if(nread > CIRC_CNT_TO_END(xfer->head, xfer->tail, wakeup_fifo->n_size)) {
			nread = CIRC_CNT_TO_END(xfer->head, xfer->tail, wakeup_fifo->n_size);
		} else if(nread == 0) {
			//printk("nread = 0, do another mod timer\n");
			break;
		}
		/*send data to wakeupengine*/

		if(test_bit(F_WAKEUP_ENABLED, &wakeup->flags)) {
			ret = send_data_to_wakeup_engine(wakeup, xfer->buf + xfer->tail, nread);
			if(ret == STATUS_WAKEUP) {
				if(test_bit(F_WAKEUP_ENABLED, &wakeup->flags) && test_bit(F_WAITING_WAKEUP, &wakeup->flags)) {
					wakeup_wait_done(wakeup);
				}

			}
		}

		xfer->tail += nread;
		xfer->tail %= wakeup_fifo->n_size;

		//printk("xfer->tail:%d, xfer->head:%d, nread:%d\n", xfer->tail, xfer->head, nread);
		if(ret == STATUS_WAKEUP) {
			break;
		}
	}
	if(ret != STATUS_WAKEUP) {
		mod_timer(&wakeup->wakeup_timer, jiffies + msecs_to_jiffies(30));
	}
}

static bool dma_chan_filter(struct dma_chan *chan, void *filter_param)
{
	struct wakeup_module *wakeup = filter_param;
	return wakeup->dma_type == (enum jzdma_type)chan->private;
}
static int wakeup_init_dma(struct wakeup_module *wakeup)
{
		struct dma_fifo_2 *wakeup_fifo_2;
		struct circ_buf *xfer;

		dma_addr_t trans_addr = wakeup->chan_rx->device->get_current_trans_addr(wakeup->chan_rx,\
				NULL, NULL,									\
				wakeup->dma_config.direction);
		printk("trans_addr:%x#####\n", trans_addr);

		wakeup_fifo_2 = wakeup->wakeup_fifo_2;
		xfer = &wakeup_fifo_2->xfer;

		xfer->buf = (char *)TCSM_BANK_5_V;

		xfer->head = (char *)KSEG1ADDR(trans_addr) - xfer->buf;
		xfer->tail = xfer->head;

		wakeup_fifo_2->n_size = TCSM_BUF_SIZE;

		printk("%s, xfer->head:%d, xfer->tail:%d\n", __func__, xfer->head, xfer->tail);

		return 0;
}

static int wakeup_close_dma(struct wakeup_module *wakeup)
{
	printk("wakeup close dma!!!!!!!\n");

	return 0;
}

static int wakeup_prepare_dma_desc(struct wakeup_module *wakeup)
{
	if(wakeup->dma_config.direction == DMA_FROM_DEVICE) {

		wakeup->desc_rx = wakeup->chan_rx->device->device_prep_dma_cyclic(wakeup->chan_rx, TCSM_BANK_5_P, TCSM_BUF_SIZE,\
				wakeup->bytes_per_buf, wakeup->dma_config.direction);

		if(wakeup->desc_rx == NULL) {
			printk("prepare desc failed\n");
			return -EFAULT;
		}

		wakeup->desc_rx->callback = NULL;
		wakeup->desc_rx->callback_param = NULL;

		dmaengine_submit(wakeup->desc_rx);
	}
	return 0;
}

static int wakeup_start_dma_3(struct wakeup_module *wakeup)
{
	printk("start dma 3\n");
	wakeup->mod_time_ms = 20;
	wakeup->wakeup_timeout = 0;

	mod_timer(&wakeup->wakeup_timer, jiffies + msecs_to_jiffies(wakeup->mod_time_ms));

	return 0;
}

static int wakeup_start_dma_2(struct wakeup_module *wakeup)
{
	printk("start dma 2, wakeup->wakeup_timeout = %d \n", wakeup->wakeup_timeout);
	wakeup->mod_time_ms = 30;
	wakeup->wakeup_timeout = 0;

	return	mod_wakeup_timer(wakeup->wakeup_timer_hw, ms_to_count(wakeup->mod_time_ms));

}

static int wakeup_wait_done(struct wakeup_module *wakeup)
{
	complete(&wakeup->completion);
	return 0;
}

static int wakeup_wait_for_complete(struct wakeup_module * wakeup)
{
	printk("#######wake wait for completion!!!!!\n");
	set_bit(F_WAKEUP_ENABLED, &wakeup->flags);

	init_completion(&wakeup->completion);
	set_bit(F_WAITING_WAKEUP, &wakeup->flags);

	wait_for_completion_interruptible(&wakeup->completion);

	clear_bit(F_WAITING_WAKEUP, &wakeup->flags);
	del_timer_sync(&wakeup->wakeup_timer);

	return 0;
}

/*******************record *************************/
void wakeup_record_timer_func_2(unsigned long data)
{
	struct wakeup_module *wakeup = (struct wakeup_module *)data;
	unsigned long flags;

	struct dma_fifo_2 *record_fifo = wakeup->record_fifo;
	struct circ_buf *xfer = &record_fifo->xfer;
	dma_addr_t trans_addr = wakeup->chan_rx->device->get_current_trans_addr(wakeup->chan_rx,\
												NULL, NULL,									\
												wakeup->dma_config.direction);

	spin_lock_irqsave(&wakeup->lock, flags);
	/*
	 * we can't controll dma transfer,
	 * so we just change the fifo info according to trans_addr.
	 *
	 * */
	xfer->head = (char *)KSEG1ADDR(trans_addr) - xfer->buf;
	spin_unlock_irqrestore(&wakeup->lock, flags);

	if(test_bit(F_READBLOCK, &wakeup->flags)) {
		complete(&wakeup->read_completion);
	}

	//printk("record_timer:xfer->head:%d\n", xfer->head);


	mod_timer(&wakeup->record_timer, jiffies + msecs_to_jiffies(30));
}

ssize_t wakeup_read_2(struct wakeup_module *wakeup, char __user *buffer,\
		size_t count, loff_t *ppos)
{

	int mcount = count;
	struct dma_fifo_2 *record_fifo = wakeup->record_fifo;
	struct circ_buf *xfer = &record_fifo->xfer;
	dma_addr_t trans_addr;

	if(wakeup->recording == false) {
		wakeup->recording = true;

		/*init dma trans info*/
		trans_addr = wakeup->chan_rx->device->get_current_trans_addr(wakeup->chan_rx,\
				NULL, NULL,                                 \
				wakeup->dma_config.direction);

		xfer->head = (char *)KSEG1ADDR(trans_addr) - xfer->buf;
		xfer->tail = xfer->head;
		mod_timer(&wakeup->record_timer, jiffies + msecs_to_jiffies(10));
	}

	while( mcount > 0) {

		unsigned long flags;

		while(1) {
			int nread;
			spin_lock_irqsave(&wakeup->lock, flags);
			nread = CIRC_CNT(xfer->head, xfer->tail, record_fifo->n_size);
			//printk("nread:%d, xfer->head:%d, xfer->tail:%d, mcount:%d\n", nread, xfer->head, xfer->tail, mcount);
			if(nread > CIRC_CNT_TO_END(xfer->head, xfer->tail, record_fifo->n_size)) {
				/* head to end, start to tail */
				nread = CIRC_CNT_TO_END(xfer->head, xfer->tail, record_fifo->n_size);
			}
			if(nread > mcount) {
				/* done a request */
				//printk("nread:%d > mcount:%d\n", nread, mcount);
				nread = mcount;
			}
			if(nread == 0) {
				//printk("no data avalile\n");
				/*nodata in fifo, then break to wait */
				spin_unlock_irqrestore(&wakeup->lock, flags);
				break;
			}
			spin_unlock_irqrestore(&wakeup->lock, flags);

			copy_to_user(buffer, xfer->buf + xfer->tail, nread);

			spin_lock_irqsave(&wakeup->lock, flags);
			xfer->tail += nread;
			xfer->tail %= record_fifo->n_size;
			spin_unlock_irqrestore(&wakeup->lock, flags);

			buffer	+= nread;
			mcount -= nread;
			if(mcount == 0) {
				/*read done*/
				break;
			}
		}
		//printk("mcount:%d, count:%d\n", mcount, count);
		if(mcount > 0) {
			/*means data not complete yet, we block here until ready*/
			//printk("wait for data complete!!!!!!!\n");
			set_bit(F_READBLOCK, &wakeup->flags);
			wait_for_completion(&wakeup->read_completion);
			clear_bit(F_READBLOCK, &wakeup->flags);
		}

	}

	return count - mcount;
}

int wakeup_open(struct wakeup_module *wakeup)
{
	int ret = 0;
	struct dma_fifo_2 *record_fifo = wakeup->record_fifo;
	printk("wakeup open!!\n");

	/* voice wake up res */
	wakeup->pResKeyoffset = 0;

	wakeup->flags = 0;
	wakeup->recording = false;

	record_fifo->xfer.buf = (char *)TCSM_BANK_5_V;
	record_fifo->xfer.head = 0;
	record_fifo->xfer.tail = 0;
	record_fifo->n_size = TCSM_BUF_SIZE;


	return ret;
}

int wakeup_close(struct wakeup_module *wakeup)
{
	int ret = 0;
	printk("wakeup close!!\n");

	del_timer_sync(&wakeup->record_timer);

	del_timer_sync(&wakeup->wakeup_timer);


	wakeup->recording = false;

	return ret;
}

static int idle_timer_hw_handler(struct wakeup_module *wakeup)
{
	int ret = 0;
	struct dma_fifo_2 *wakeup_fifo = wakeup->wakeup_fifo_2;
	struct circ_buf *xfer = &wakeup_fifo->xfer;
	dma_addr_t trans_addr;

	wakeup->wakeup_timeout += wakeup->mod_time_ms;
	//printk("%s, ------------timeout:%d\n", __func__, wakeup->wakeup_timeout);
	if(wakeup->wakeup_timeout >= WAKEUP_TIMEOUT_VAL) {
		printk("wakeup timeout!!\n");
		//release_wakeup_timer(wakeup->wakeup_timer_hw);
		return SYS_WAKEUP_TIMEOUT;
	}

	trans_addr = wakeup->chan_rx->device->get_current_trans_addr(wakeup->chan_rx,\
			NULL, NULL,									\
			wakeup->dma_config.direction);

	xfer->head = (char *)KSEG1ADDR(trans_addr) - xfer->buf;


	/*process data*/
	while(1) {
		int nread;

		nread = CIRC_CNT(xfer->head, xfer->tail, wakeup_fifo->n_size);
		if(nread > CIRC_CNT_TO_END(xfer->head, xfer->tail, wakeup_fifo->n_size)) {
			nread = CIRC_CNT_TO_END(xfer->head, xfer->tail, wakeup_fifo->n_size);
		} else if(nread == 0) {
			//printk("nread = 0, do another mod timer\n");
			break;
		}
		/*sned data to wakeupengine*/

		if(test_bit(F_WAKEUP_ENABLED, &wakeup->flags)) {
			ret = send_data_to_wakeup_engine(wakeup, xfer->buf + xfer->tail, nread);
			if(ret == STATUS_WAKEUP) {
				if(test_bit(F_WAKEUP_ENABLED, &wakeup->flags) && test_bit(F_WAITING_WAKEUP, &wakeup->flags)) {
					wakeup_close_dma(wakeup);
				}

			}
		}

		xfer->tail += nread;
		xfer->tail %= wakeup_fifo->n_size;

		//printk("xfer->tail:%d, xfer->head:%d, nread:%d\n", xfer->tail, xfer->head, nread);
		if(ret == STATUS_WAKEUP) {
			stoptimer();
			return SYS_WAKEUP;
		}
	}
	if(ret != STATUS_WAKEUP) {
		wakeup->mod_time_ms = 30;
		mod_wakeup_timer(wakeup->wakeup_timer_hw, ms_to_count(wakeup->mod_time_ms));
	}

	return 0;
}


static int wakeup_tcu_int_handler(struct wakeup_module *wakeup)
{
	int ret = 0;
	int tcu_int_flag = 0;

	tcu_int_flag = timer_irq_handler(wakeup->wakeup_timer_hw);

	if(tcu_int_flag & (1 << 2)) {
		ret = idle_timer_hw_handler(wakeup);
	}
	return ret;
}
static int mod_tcu_timer(struct wakeup_module *wakeup, unsigned long timer_count)
{

	return	mod_wakeup_timer(wakeup->wakeup_timer_hw, timer_count);

}

static struct wakeup_ops voice_wakeup_ops = {
	.init_dma		= wakeup_init_dma,
	.prepare_dma	= wakeup_prepare_dma_desc,
	.start_dma		= wakeup_start_dma_2,
	.start_dma_3	= wakeup_start_dma_3,
	.release_dma	= wakeup_close_dma,

	/*normal record*/
	.open			= wakeup_open,
	.read			= wakeup_read_2,
	.close			= wakeup_close,

	.wakeup_set_res = voice_wakeup_set_res,
	.wakeup_ctl		= voice_wakeup_ctl,
	/*user wakeup settings*/
	.wakeup_wait_for_complete = wakeup_wait_for_complete,

	.idle_tcu_int	= wakeup_tcu_int_handler,
	.mod_timer		= mod_tcu_timer,
	/*suspend resume*/
};

int voice_wakeup_init(struct wakeup_module * wakeup)
{
	/*do some init to voice wakeup engine*/

	dma_cap_mask_t mask;

	init_timer(&wakeup->wakeup_timer);

	wakeup->wakeup_timer.function = wakeup_timer_func_2;
	wakeup->wakeup_timer.data = (unsigned long)wakeup;
	wakeup->mod_time_ms = 10;	/*10ms*/

	wakeup->nr_buf		  = TCSM_NR_BUF;
	wakeup->bytes_per_buf = TCSM_BUF_SIZE/TCSM_NR_BUF;

	spin_lock_init(&wakeup->lock);

	init_completion(&wakeup->read_completion);
	/* wakeup init */
	wakeup->nIvwObjSize = 20 * 1024;

	wakeup->nResidentRAMSize = 38;
	wakeup->pResidentRAM = kmalloc(wakeup->nResidentRAMSize, GFP_KERNEL);
	wakeup->nWakeupNetworkID = 0;
	/*sysfs test*/
	wakeup->pResKeyoffset = 0;
	wakeup->sys_pReskey = kmalloc(VOICE_RES_SIZE, GFP_KERNEL);
	wakeup->IvwObjCreated = false;

	wakeup->pIvwObj = kmalloc(wakeup->nIvwObjSize, GFP_KERNEL);

	/*dma request*/
	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);

	wakeup->chan_rx = dma_request_channel(mask, dma_chan_filter, (void *)wakeup);
	if(wakeup->chan_rx == NULL) {
		printk("request dma channel failed\n");
		return -ENXIO;
	}

	dmaengine_slave_config(wakeup->chan_rx, &wakeup->dma_config);

	wakeup_prepare_dma_desc(wakeup);

	dma_async_issue_pending(wakeup->chan_rx);
	/*dma started, when dmic has data, will transfer to tcsm, never stop dma*/

	init_timer(&wakeup->record_timer);
	wakeup->record_timer.function = wakeup_record_timer_func_2;
	wakeup->record_timer.data = (unsigned long)wakeup;


	wakeup->wakeup_fifo_2 = kmalloc(sizeof(struct dma_fifo_2), GFP_KERNEL);
	wakeup->record_fifo = kmalloc(sizeof(struct dma_fifo_2), GFP_KERNEL);

	wakeup->ops = &voice_wakeup_ops;

	wakeup->wakeup_timer_hw = kmalloc(sizeof(struct wakeup_timer), GFP_KERNEL);

	request_wakeup_timer(wakeup->wakeup_timer_hw);

	wakeup->wakeup_timeout = 0;

	//enable_wakeup_timer(wakeup->wakeup_timer_hw);
	return 0;
}
EXPORT_SYMBOL(voice_wakeup_init);

static int __init wakeup_init(void)
{

	return 0;
}
static void __exit wakeup_exit(void)
{

}

module_init(wakeup_init);
module_exit(wakeup_exit);
MODULE_LICENSE("GPL");

