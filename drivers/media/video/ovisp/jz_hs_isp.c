#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/workqueue.h>

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/bug.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/syscore_ops.h>
#include <mach/jz_libdmmu.h>

#include "jz_hs_isp.h"
#include "ovisp-isp.h"
#include "ovisp-video.h"
#include "ovisp-videobuf.h"
#include "ovisp-debugtool.h"
#include "ovisp-base.h"
#include "isp-debug.h"
#include "ovisp-csi.h"
#include "hsisp-firmware_array.h"

/*
 * global define, don't change.
 * */
#define HS_ISP_TP_READY              _IOR('Z',0,unsigned int)
#define HS_ISP_TP_ENABLE             _IOR('Z',1,unsigned int)

unsigned int hs_isp_available = 1;

enum hs_isp_task_status
{
	HS_ISP_TASK_IDLE,
	HS_ISP_TASK_START,
	HS_ISP_TASK_SUCCESS,
	HS_ISP_TASK_FAILED,
	HS_ISP_TASK_ERROR,
};

enum hs_isp_status
{
	HS_ISP_POWER_ON,
	HS_ISP_CMD_02,
	HS_ISP_CMD_04,
	HS_ISP_CMD_06,
	HS_ISP_POWER_OFF,
};

enum hs_camera_status
{
	HS_CAMERA_PWN_ON,
	HS_CAMERA_STREAM_ON,
	HS_CAMERA_STREAM_OFF,
	HS_CAMERA_PWN_OFF,
};

struct hstp_isp_info {
	unsigned int nv12_width;
	unsigned int nv12_height;
	unsigned int exposure_line;
	unsigned int banding_step_50hz;
	unsigned int sensor_gain;
	unsigned int flags;
};

struct hs_isp_dev {
	int major;
	int minor;
	int nr_devs;
	unsigned char *resource_buf;

	struct timer_list hs_isp_timer;
	struct workqueue_struct *hs_wq;
	struct delayed_work hs_delayed_work;

	wait_queue_head_t hs_isp_wq;
        unsigned char wake_condition;

	unsigned int hs_isp_pending;
	unsigned int hs_isp_enable;
	unsigned int hs_isp_watermark_enable;

	enum hs_isp_task_status hs_isp_task_status;
	enum hs_isp_status hs_isp_status;
	enum hs_camera_status hs_camera_status;
	unsigned int hs_camera_pwn;
	spinlock_t hs_isp_lock;

	unsigned int nv12_width;
	unsigned int nv12_height;
	unsigned int exposure_line;
	unsigned int banding_step_50hz;
	unsigned int sensor_gain;

	/* struct sleep_buffer sleep_buffer; /\* buffer used to store data during suspend to cpu sleep *\/ */

	struct class *class;
	struct cdev cdev;
	struct device *dev;
	struct isp_device *isp;    
};

extern void register_hs_isp_irq(irqreturn_t (*handle_irq)(int ,void*));
extern void unregister_hs_isp_irq(void);
extern int getCameraKey_WakeUp(void);
extern void setCameraKey_WakeUp(int value);
extern int xb_hstp_snd_write(void);
#ifdef CONFIG_TOUCHPANEL_IT7236_KEY
extern int hstp_is_tp_forward(void);
#endif

#ifdef CONFIG_VIDEO_OV5648
extern struct isp_reg_t ov5648_setting[];
#endif
#ifdef CONFIG_VIDEO_OV8858
extern struct isp_reg_t ov8858_r2a_setting[];
#endif
#ifdef CONFIG_VIDEO_OV8865
extern struct isp_reg_t ov8865_setting[];
#endif
#ifdef CONFIG_VIDEO_OV8856
extern struct isp_reg_t ov8856_setting[];
#endif
#ifdef CONFIG_VIDEO_OV4689
extern struct isp_reg_t ov4689_setting[];
#endif

static const struct hs_isp_sensor_setting hs_isp_sensor_settings[] = {
#ifdef CONFIG_VIDEO_OV5648
    {
	.chip_id = 0x5648,
	.hs_isp_setting = ov5648_setting,
    },
#endif
#ifdef CONFIG_VIDEO_OV8858
    {
	.chip_id = 0x8858,
	.hs_isp_setting = ov8858_r2a_setting,
    },
#endif
#ifdef CONFIG_VIDEO_OV8865
    {
	.chip_id = 0x8865,
	.hs_isp_setting = ov8865_setting,
    },
#endif
#ifdef CONFIG_VIDEO_OV8856
    {
	.chip_id = 0x8856,
	.hs_isp_setting = ov8856_setting,
    },
#endif
#ifdef CONFIG_VIDEO_OV4689
    {
	.chip_id = 0x4689,
	.hs_isp_setting = ov4689_setting,
    },
#endif

};

#define HSTP_SETTING_SIZES (ARRAY_SIZE(hs_isp_sensor_settings))

extern struct sensor_info *hstp_camera_info;

static struct hs_isp_dev *g_hs_isp_dev;

static int hs_isp_take_photo(void);

static void hs_work_handler(struct delayed_work *data)
{
	printk("hs_isp_take_photo START !!!!!\n");
	hs_isp_take_photo();
	printk("hs_isp_take_photo  END  !!!!!\n");
}
static int hs_isp_open(struct inode *inode, struct file *filp)
{
	struct cdev *cdev = inode->i_cdev;
        struct hs_isp_dev *ptn = container_of(cdev,struct hs_isp_dev,cdev);
     
        filp->private_data = ptn;
        /* printk("jz_hs_isp inode open sucessfully..\n"); */
	return 0;
}
static int hs_isp_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
	return 0;
}
static int hs_isp_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
	return 0;
}
static int hs_isp_close(struct inode *inode, struct file *filp)
{
	return 0;
}


static long hs_isp_ioctl(struct file *filp, unsigned int cmd, unsigned long args)
{
	struct hs_isp_dev *hs_isp_dev = filp->private_data;
	unsigned int *ret;
	int hstp_snd_state = 0;
	struct hstp_isp_info *isp_info;

        switch(cmd){

	case HS_ISP_TP_ENABLE:
		  ret = (unsigned int *)args;
		  *ret = hs_isp_dev->hs_isp_enable;
	  	  break;
	case HS_ISP_TP_READY:
		  isp_info = (struct hstp_isp_info *)args;
                  wait_event_interruptible(hs_isp_dev->hs_isp_wq, hs_isp_dev->wake_condition);
                  hs_isp_dev->wake_condition = 0;
		  if(HS_ISP_TASK_SUCCESS == hs_isp_dev->hs_isp_task_status){
			  printk("hs isp task success..replay sound!!\n");
			  hstp_snd_state = xb_hstp_snd_write(); //replay hstp sound
			  if(hstp_snd_state < 0)
				  printk("jz hstp replay sound error ..\n");
			  hs_isp_dev->hs_isp_task_status = HS_ISP_TASK_IDLE;
			  isp_info->nv12_width = hstp_camera_info->hstp_photo_setting->width;
			  isp_info->nv12_height = hstp_camera_info->hstp_photo_setting->height;
			  isp_info->exposure_line = hs_isp_dev->exposure_line;
			  isp_info->banding_step_50hz = hs_isp_dev->banding_step_50hz;
			  isp_info->sensor_gain = hs_isp_dev->sensor_gain;
			  isp_info->flags = 0x1;
			  if(hs_isp_dev->hs_isp_watermark_enable == HS_ISP_ENABLE)isp_info->flags |= HS_ISP_WM_OFFSET;
                  }else if(HS_ISP_TASK_ERROR == hs_isp_dev->hs_isp_task_status){
			  printk("hs isp task occure unknown error ..\n");
			  isp_info->flags = 0x80;
		  }else{
			  printk("hs isp task failed ..\n");
			  hs_isp_dev->hs_isp_task_status = HS_ISP_TASK_IDLE;
			  isp_info->flags = 0x0;
		  }
                  break;	
	  default:
		  printk(" No avilable order to use!!...\n");
                  return -1;
        }

	return 0;
}

static int hs_isp_mmap(struct file *file, struct vm_area_struct *vma)
{
	/* struct jzfb *jzfb = info->par; */
	unsigned long start;
	unsigned long off;
	u32 len;

	off = vma->vm_pgoff << PAGE_SHIFT;
	/* frame buffer memory */
	start =  0x0f000000;
	len   =  0x01000000;
	/* start &= PAGE_MASK; */

	if ((vma->vm_end - vma->vm_start + off) > len)
	    return -EINVAL;

	off += start;

	vma->vm_pgoff = off >> PAGE_SHIFT;
	vma->vm_flags |= VM_IO;
	/* Uncacheable */
//	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

 	pgprot_val(vma->vm_page_prot) &= ~_CACHE_MASK;
// 	pgprot_val(vma->vm_page_prot) |= _CACHE_UNCACHED; /* Uncacheable */
	/* Write-Back */
	//pgprot_val(vma->vm_page_prot) |= _CACHE_CACHABLE_NONCOHERENT;
	/* Write-Acceleration */
	/* pgprot_val(vma->vm_page_prot) |= _CACHE_CACHABLE_WA; */

	/* printk("vma= %08x, vma->vm_start= %08x, off >> PAGE_SHIFT = %08x\n",vma, vma->vm_start, off >> PAGE_SHIFT); */
	/* printk("vma->vm_end = %08x\n",vma->vm_end); */
	if (io_remap_pfn_range(vma, vma->vm_start, off >> PAGE_SHIFT,
			       vma->vm_end - vma->vm_start,
			       vma->vm_page_prot)) {
	    return -EAGAIN;
	}

	return 0;
}

static struct file_operations hs_isp_ops = {
	.owner = THIS_MODULE,
	.write = hs_isp_write,
	.read = hs_isp_read,
	.open = hs_isp_open,
	.mmap = hs_isp_mmap,
	.release = hs_isp_close,
	.unlocked_ioctl = hs_isp_ioctl,
};


static void hs_isp_timer_handler(unsigned long data)
{

}

/************************* sysfs for hs_isp *****************************/
/* static ssize_t hs_isp_state_store(struct device *dev, */
/* 		struct device_attribute *attr, */
/* 		const char *buf, size_t count) */
/* { */
/* 	/\* set hs_isp resource *\/ */
/* 	return count; */
/* } */
static ssize_t hs_isp_state_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct hs_isp_dev *hs_isp_dev = g_hs_isp_dev;
	if(hs_isp_dev == NULL){
	    printk("hs_isp_dev == NULL..\n");
	    return 0;
	}

	switch(hs_isp_dev->hs_isp_task_status){
	case HS_ISP_TASK_IDLE:
	    printk("hs_isp_task_status = HS_ISP_TASK_IDLE\n");
	    break;
	case HS_ISP_TASK_START:
	    printk("hs_isp_task_status = HS_ISP_TASK_START\n");
	    break;
	case HS_ISP_TASK_SUCCESS:
	    printk("hs_isp_task_status = HS_ISP_TASK_SUCCESS\n");
	    break;
	case HS_ISP_TASK_FAILED:
	    printk("hs_isp_task_status = HS_ISP_TASK_FAILED\n");
	    break;
	case HS_ISP_TASK_ERROR:
	    printk("hs_isp_task_status = HS_ISP_TASK_ERROR\n");
	    break;
	default:	    
	    printk("hs_isp_task_status = %d, unknown!\n",hs_isp_dev->hs_isp_task_status);
	}

	switch(hs_isp_dev->hs_isp_status){
	case HS_ISP_POWER_ON:
	    printk("hs_isp_status = HS_ISP_POWER_ON\n");
	    break;
	case HS_ISP_CMD_02:
	    printk("hs_isp_status = HS_ISP_CMD_02\n");
	    break;
	case HS_ISP_CMD_04:
	    printk("hs_isp_status = HS_ISP_CMD_04\n");
	    break;
	case HS_ISP_CMD_06:
	    printk("hs_isp_status = HS_ISP_CMD_06\n");
	    break;
	case HS_ISP_POWER_OFF:
	    printk("hs_isp_status = HS_ISP_POWER_OFF\n");
	    break;
	default:	    
	    printk("hs_isp_status = %d, unknown!\n",hs_isp_dev->hs_isp_status);
	}

	switch(hs_isp_dev->hs_camera_status){
	case HS_CAMERA_PWN_ON:
	    printk("hs_camera_status = HS_CAMERA_PWN_ON\n");
	    break;
	case HS_CAMERA_STREAM_ON:
	    printk("hs_camera_status = HS_CAMERA_STREAM_ON\n");
	    break;
	case HS_CAMERA_STREAM_OFF:
	    printk("hs_camera_status = HS_CAMERA_STREAM_OFF\n");
	    break;
	case HS_CAMERA_PWN_OFF:
	    printk("hs_camera_status = HS_CAMERA_PWN_OFF\n");
	    break;
	default:	    
	    printk("hs_camera_status = %d, unknown!\n",hs_isp_dev->hs_camera_status);
	}
	return 0;
}

static ssize_t hs_isp_enable_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct hs_isp_dev *hs_isp_dev = g_hs_isp_dev;
	/* unsigned long flags; */
	unsigned long ctl;
	int rc;
	if(hs_isp_dev == NULL){
	    printk("hs_isp_dev == NULL..\n");
	    return 0;
	}

	rc = strict_strtoul(buf, 0, &ctl);
	if(rc){
		printk("error input type ..\n");
		return rc;
	}
	if(ctl == 0){
		hs_isp_dev->hs_isp_enable = HS_ISP_DISABLE;
		hs_isp_available = 0;
		printk("hs_isp_dev->hs_isp_enable = HS_ISP_DISABLE\n");
	}else {
		hs_isp_dev->hs_isp_enable = HS_ISP_ENABLE;
		hs_isp_available = 1;
		printk("hs_isp_dev->hs_isp_enable = HS_ISP_ENABLE\n");
	}
	return count;
}
static ssize_t hs_isp_enable_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct hs_isp_dev *hs_isp_dev = g_hs_isp_dev;
	if(hs_isp_dev == NULL){
	    printk("hs_isp_dev == NULL..\n");
	    return 0;
	}
	printk("hs_isp_dev = %d\n",hs_isp_dev->hs_isp_enable);
	return 0;
}

static ssize_t hs_isp_watermark_enable_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct hs_isp_dev *hs_isp_dev = g_hs_isp_dev;
	/* unsigned long flags; */
	unsigned long ctl;
	int rc;
	if(hs_isp_dev == NULL){
	    printk("hs_isp_dev == NULL..\n");
	    return 0;
	}

	rc = strict_strtoul(buf, 0, &ctl);
	if(rc){
		printk("error input type ..\n");
		return rc;
	}
	if(ctl == 0){
	    hs_isp_dev->hs_isp_watermark_enable = HS_ISP_DISABLE;
		printk("hs_isp_dev->hs_isp_watermark_enable = HS_ISP_DISABLE\n");
	}else {
	    hs_isp_dev->hs_isp_watermark_enable = HS_ISP_ENABLE;
		printk("hs_isp_dev->hs_isp_watermark_enable = HS_ISP_ENABLE\n");
	}
	return count;
}
static ssize_t hs_isp_watermark_enable_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct hs_isp_dev *hs_isp_dev = g_hs_isp_dev;
	if(hs_isp_dev == NULL){
	    printk("hs_isp_dev == NULL..\n");
	    return 0;
	}
	printk("hs_isp_watermark_enable = %d\n",hs_isp_dev->hs_isp_watermark_enable);
	return 0;
}

static DEVICE_ATTR(hs_isp_state , 0666, hs_isp_state_show , NULL);
static DEVICE_ATTR(hs_isp_enable, 0666, hs_isp_enable_show, hs_isp_enable_store);
static DEVICE_ATTR(hs_isp_watermark_enable, 0666, hs_isp_watermark_enable_show, hs_isp_watermark_enable_store);

static struct attribute *hs_isp_attributes[] = {
	&dev_attr_hs_isp_state.attr,
	&dev_attr_hs_isp_enable.attr,
	&dev_attr_hs_isp_watermark_enable.attr,
	NULL
};

static const struct attribute_group hs_isp_attr_group = {
	.attrs = hs_isp_attributes,
};


/* PM */
static int hs_isp_suspend(void)
{
	/* alloc dma buffer. and build dma desc, hs_isp_module_set_desc */
	/* struct hs_isp_dev * hs_isp = g_hs_isp; */
	/* struct sleep_buffer *sleep_buffer = &hs_isp->sleep_buffer; */
	/* int i; */
	/* int ret; */
	/* if(hs_isp->hs_isp_enable == 0) { */
	/* 	return 0; */
	/* } */

	/* if(hs_isp->hs_isp_pending == 0) { */
	/* 	/\* voice identified during suspend *\/ */
	/* 	return -1; */
	/* } */
	/* sleep_buffer->nr_buffers = NR_BUFFERS; */
	/* sleep_buffer->total_len	 = SLEEP_BUFFER_SIZE; */
	/* for(i = 0; i < sleep_buffer->nr_buffers; i++) { */
	/* 	sleep_buffer->buffer[i] = kmalloc(sleep_buffer->total_len/sleep_buffer->nr_buffers, GFP_KERNEL); */
	/* 	if(sleep_buffer->buffer[i] == NULL) { */
	/* 		printk("failed to allocate buffer for sleep!!\n"); */
	/* 		goto _allocate_failed; */
	/* 	} */
	/* } */
	/* dma_cache_wback_inv(&sleep_buffer->buffer[0], sleep_buffer->total_len); */
	/* ret = hs_isp_module_set_sleep_buffer(&hs_isp->sleep_buffer); */

	/* return 0; */

/* _allocate_failed: */
	/* for(i = i-1; i>0; i--) { */
	/* 	kfree(sleep_buffer->buffer[i]); */
	/* 	sleep_buffer->buffer[i] = NULL; */
	/* } */

	return 0;
}

static void hs_isp_resume(void)
{
	struct hs_isp_dev *hs_isp_dev = g_hs_isp_dev;
	struct isp_device *isp;
	unsigned int shutter_time;
	/* struct isp_device *isp; */
	if (hs_isp_dev == NULL) {
		return;
	}

	if (hs_isp_dev->hs_isp_task_status != HS_ISP_TASK_START) {
		return;
	}

	isp = hs_isp_dev->isp;
	if (isp->done_frames >= HS_ISP_DROP_FRAMES) {
		shutter_time = 0;
	} else {
		unsigned short fps = hstp_camera_info->hstp_preview_setting->fps;
		if (fps > 0) {
			shutter_time = (HS_ISP_DROP_FRAMES - isp->done_frames) * 1000/fps;
		} else {
			shutter_time = (HS_ISP_DROP_FRAMES - isp->done_frames) * HS_ISP_FRAME_INTERVAL;
		}
	}
	printk("isp->done_frames = %d hstp do after %d ms\n",isp->done_frames, shutter_time);
	schedule_delayed_work(&hs_isp_dev->hs_delayed_work, msecs_to_jiffies(shutter_time));
}

struct syscore_ops hs_isp_pm_ops = {
	.suspend = hs_isp_suspend,
	.resume = hs_isp_resume,
};

static int hs_camera_regrw(struct isp_device *isp, struct isp_i2c_cmd * cmd){
	unsigned char sccb_cmd = 0;
	isp_reg_writeb(isp, cmd->addr, 0x63601);

	/*sensore reg*/
	if(!(cmd->flags & I2C_CMD_NO_ADDR)){
	    isp_reg_writeb(isp, (cmd->reg >> 8) & 0xff, 0x63602);
	    isp_reg_writeb(isp, cmd->reg & 0xff, 0x63603);
	}

	if(cmd->flags & I2C_CMD_DATA_16BIT)
		sccb_cmd |= (0x01 << 1);
	if(cmd->flags & I2C_CMD_ADDR_16BIT)
		sccb_cmd |= 0x01;
	isp_reg_writeb(isp, sccb_cmd, 0x63606);/*16bit addr enable*/

	if(!(cmd->flags & I2C_CMD_READ)) {
		if(cmd->flags & I2C_CMD_DATA_16BIT) {
			isp_reg_writeb(isp, (cmd->data >> 8) & 0xff, 0x63604);
			isp_reg_writeb(isp, cmd->data & 0xff, 0x63605);
			/**/
		} else {
			/*write data*/
			isp_reg_writeb(isp, (cmd->data >> 8) & 0xff, 0x63604);
			isp_reg_writeb(isp, cmd->data & 0xff, 0x63605);
		}
		if(cmd->flags & I2C_CMD_NO_ADDR){
		    isp_reg_writeb(isp, 0x35, 0x63609);
		}else{
		    isp_reg_writeb(isp, 0x37, 0x63609);
		}
		mdelay(1);
	}
	if (cmd->flags & I2C_CMD_READ) {

		if(cmd->flags & I2C_CMD_NO_ADDR){
		    isp_reg_writeb(isp, 0x31, 0x63609);
		    mdelay(1);
		    isp_reg_writeb(isp, 0xf9, 0x63609);
		    mdelay(1);
		}else{
		    isp_reg_writeb(isp, 0x33, 0x63609);
		    mdelay(1);
		    isp_reg_writeb(isp, 0xf9, 0x63609);
		    mdelay(1);
		}
		if(cmd->flags & I2C_CMD_DATA_16BIT) {
			cmd->data = (isp_reg_readb(isp, 0x63607)<<8) + isp_reg_readb(isp, 0x63608); /*read data*/
		}
		else {
			cmd->data = isp_reg_readb(isp, 0x63608); /*read data*/
		}
	}
	return 0;
}

static int hs_isp_intc_disable(struct isp_device *isp, unsigned int mask)
{

	unsigned long flags;
	isp_intc_regs_t intr;
	/* unsigned char h,l; */
	/* l = mask & 0xff; */
	/* h = (mask >> 0x08) & 0xff; */

	intr.intc = mask;
	spin_lock_irqsave(&isp->lock, flags);
	if (intr.bits.c3) {
		isp->intr.bits.c3 &= ~(intr.bits.c3);
		isp_reg_writeb(isp, isp->intr.bits.c3, REG_ISP_INT_EN_C3);
	}
	if (intr.bits.c2) {
		isp->intr.bits.c2 &= ~(intr.bits.c2);
		isp_reg_writeb(isp, isp->intr.bits.c2, REG_ISP_INT_EN_C2);
	}
	if (intr.bits.c1) {
		isp->intr.bits.c1 &= ~(intr.bits.c1);
		isp_reg_writeb(isp, isp->intr.bits.c1, REG_ISP_INT_EN_C1);
	}
	if (intr.bits.c0) {
		isp->intr.bits.c0 &= ~(intr.bits.c0);
		isp_reg_writeb(isp, isp->intr.bits.c0, REG_ISP_INT_EN_C0);
	}
	spin_unlock_irqrestore(&isp->lock, flags);

	return 0;
}
static int hs_isp_intc_enable(struct isp_device * isp, unsigned int mask)
{
	unsigned long flags;
	isp_intc_regs_t intr;

	intr.intc = mask;
	spin_lock_irqsave(&isp->lock, flags);
	if (intr.bits.c3) {
		isp->intr.bits.c3 |= intr.bits.c3;
		isp_reg_writeb(isp, isp->intr.bits.c3, REG_ISP_INT_EN_C3);
	}
	if (intr.bits.c2) {
		isp->intr.bits.c2 |= intr.bits.c2;
		isp_reg_writeb(isp, isp->intr.bits.c2, REG_ISP_INT_EN_C2);
	}
	if (intr.bits.c1) {
		isp->intr.bits.c1 |= intr.bits.c1;
		isp_reg_writeb(isp, isp->intr.bits.c1, REG_ISP_INT_EN_C1);
	}
	if (intr.bits.c0) {
		isp->intr.bits.c0 |= intr.bits.c0;
		isp_reg_writeb(isp, isp->intr.bits.c0, REG_ISP_INT_EN_C0);
	}
	spin_unlock_irqrestore(&isp->lock, flags);

	return 0;
}

static unsigned int hs_isp_intc_state(struct isp_device *isp)
{
	isp_intc_regs_t intr;
	intr.bits.c3 = isp_reg_readb(isp, REG_ISP_INT_STAT_C3);
	intr.bits.c2 = isp_reg_readb(isp, REG_ISP_INT_STAT_C2);
	intr.bits.c1 = isp_reg_readb(isp, REG_ISP_INT_STAT_C1);
	intr.bits.c0 = isp_reg_readb(isp, REG_ISP_INT_STAT_C0);
	return intr.intc;
}

static int hs_isp_mac_int_mask(struct isp_device *isp, unsigned short mask)
{
	unsigned char mask_l = mask & 0x00ff;
	unsigned char mask_h = (mask >> 8) & 0x00ff;
	unsigned long flags;

	spin_lock_irqsave(&isp->lock, flags);
	if (mask_l) {
		isp->mac_intr.bits.c0 &= ~mask_l;
		isp_reg_writeb(isp, isp->mac_intr.bits.c0, REG_ISP_MAC_INT_EN_L);
	}
	if (mask_h) {
		isp->mac_intr.bits.c1 &= ~mask_h;
		isp_reg_writeb(isp, isp->mac_intr.bits.c1, REG_ISP_MAC_INT_EN_H);
	}
	spin_unlock_irqrestore(&isp->lock, flags);

	return 0;
}

static int hs_isp_mac_int_unmask(struct isp_device *isp, unsigned short mask)
{
	unsigned char mask_l = mask & 0x00ff;
	unsigned char mask_h = (mask >> 8) & 0x00ff;
	unsigned long flags;

	spin_lock_irqsave(&isp->lock, flags);
	if (mask_l) {
		isp->mac_intr.bits.c0 |= mask_l;
		isp_reg_writeb(isp, isp->mac_intr.bits.c0, REG_ISP_MAC_INT_EN_L);
	}
	if (mask_h) {
		isp->mac_intr.bits.c1 |= mask_h;
		isp_reg_writeb(isp, isp->mac_intr.bits.c1, REG_ISP_MAC_INT_EN_H);
	}
	spin_unlock_irqrestore(&isp->lock, flags);

	return 0;
}

static unsigned short hs_isp_mac_int_state(struct isp_device *isp)
{
	unsigned short state_l;
	unsigned short state_h;
	state_l = isp_reg_readb(isp, REG_ISP_MAC_INT_STAT_L);
	state_h = isp_reg_readb(isp, REG_ISP_MAC_INT_STAT_H);

	return (state_h << 8) | state_l;
}

static int hs_isp_get_exif_info(struct isp_device *isp)
{

	struct hs_isp_dev *hs_isp_dev = g_hs_isp_dev;

	if(hs_isp_dev != NULL){
	    hs_isp_dev->exposure_line = (unsigned int)(isp_firmware_readb(isp,0x1e030)<<24) | (isp_firmware_readb(isp,0x1e031) <<16) | (isp_firmware_readb(isp,0x1e032) << 8) | isp_firmware_readb(isp,0x1e0c3);
	    hs_isp_dev->banding_step_50hz = (unsigned int)(isp_firmware_readb(isp,0x1f07c)<<8) | isp_firmware_readb(isp,0x1f07d);
	    hs_isp_dev->sensor_gain = (unsigned int)(isp_firmware_readb(isp,0x1e034)<<8) | isp_firmware_readb(isp,0x1e035);	
	}
	return 0;
}

static int hs_isp_set_window_size(struct isp_device *isp, struct sensor_win_setting *win, bool is_process_raw)
{
	unsigned char width_H = win->width >> 8;
	unsigned char width_L = win->width & 0xff;
	unsigned char height_H = win->height >> 8;
	unsigned char height_L = win->height & 0xff;

	isp_firmware_writeb(isp, width_H, 0x1f002);
	isp_firmware_writeb(isp, width_L, 0x1f003);
	isp_firmware_writeb(isp, height_H, 0x1f004);
	isp_firmware_writeb(isp, height_L, 0x1f005);
	isp_firmware_writeb(isp, width_H, 0x1f008);
	isp_firmware_writeb(isp, width_L, 0x1f009);
	isp_firmware_writeb(isp, height_H, 0x1f00a);
	isp_firmware_writeb(isp, height_L, 0x1f00b);

	isp_firmware_writeb(isp, width_H, 0x1f024);
	isp_firmware_writeb(isp, width_L, 0x1f025);
	isp_firmware_writeb(isp, height_H, 0x1f026);
	isp_firmware_writeb(isp, height_L, 0x1f027);
	isp_firmware_writeb(isp, width_H, 0x1f028);
	isp_firmware_writeb(isp, width_L, 0x1f029);
	if(is_process_raw){
	    width_H = win->width >> 9;
	    width_L = (win->width >> 1) & 0xff;
	}
	isp_firmware_writeb(isp, width_H, 0x1f02a);
	isp_firmware_writeb(isp, width_L, 0x1f02b);
	return 0;
}

static int hs_isp_set_banding_parameters(struct isp_device *isp, struct sensor_win_setting *win, bool is_preview_mode)
{
	/* the default fps is 24, if others ,wrong . banding step=（sensor_sys_pclk / hts / 100）*0x10*/
	unsigned int value_fifty = 0;
	unsigned int value_sixty = 0;
	value_fifty = (16000000 * (unsigned int)win->sysclk)/(100 * (unsigned int)win->hts);
	value_sixty = (16000000 * (unsigned int)win->sysclk)/(120 * (unsigned int)win->hts);
	if(is_preview_mode){
	    /* in preview mode ,  reg ISP_BANDING_50HZ do not work*/
	    isp_firmware_writeb(isp, (value_fifty >> 8) & 0xff, 0x1e046);
	    isp_firmware_writeb(isp, value_fifty & 0xff, 0x1e047);
	    isp_firmware_writeb(isp, (value_sixty >> 8) & 0xff, 0x1e044);
	    isp_firmware_writeb(isp, value_sixty & 0xff, 0x1e045);
	}else{
	    isp_firmware_writeb(isp, (value_fifty >> 8) & 0xff, ISP_BANDING_50HZ);
	    isp_firmware_writeb(isp, value_fifty & 0xff, ISP_BANDING_50HZ + 1);
	    isp_firmware_writeb(isp, (value_sixty >> 8) & 0xff, ISP_BANDING_60HZ);
	    isp_firmware_writeb(isp, value_sixty & 0xff, ISP_BANDING_60HZ + 1);
	}
	return 0;
}

static int hs_isp_wait_cmd_done(struct isp_device *isp, unsigned long timeout)
{
	unsigned long tm;
	int ret = 0;
	tm = wait_for_completion_timeout(&isp->completion,
			msecs_to_jiffies(timeout));
	if (!tm && !isp->completion.done) {
		ret = -ETIMEDOUT;
	}
	return ret;
}

static int hs_isp_send_cmd(struct isp_device *isp, unsigned char id)
{
	int ret = 0;
	int max_cmd_sendtimes = 5;

	isp_reg_writeb(isp, 0x0, COMMAND_RESULT);
	isp_reg_writeb(isp, 0x04, REG_ISP_INT_EN_C0);
	isp_reg_writeb(isp, id, COMMAND_REG0);
	udelay(50);
	while(id != isp_firmware_readb(isp, 0x1c010)){
	    if(max_cmd_sendtimes-- == 0){
		printk("SSSSSSSSSS error\n");
		break;
	    }	    
	    isp_reg_writeb(isp, id, COMMAND_REG0);
	    udelay(50);
	}
	/* Command set done interrupt. */
	if(id == CMD_SET_FORMAT)return 0;

	INIT_COMPLETION(isp->completion);
	ret = hs_isp_wait_cmd_done(isp, 1000);

	/* determine whether setting cammand successfully */
	if ((CMD_SET_SUCCESS != isp_reg_readb(isp, COMMAND_RESULT))
			|| (id != isp_reg_readb(isp, COMMAND_FINISHED))) {
		printk("Failed to send command (%02x:%02x:%02x:%02x)\n",
				isp_reg_readb(isp, COMMAND_RESULT),
				isp_reg_readb(isp, COMMAND_FINISHED), isp_reg_readb(isp, 0x6390f), isp_firmware_readb(isp, 0x1c21b));
		ret = -EINVAL;
	}

	return ret;
}

static int hs_camera_stream_off(struct hs_isp_dev *hs_isp_dev)
{
	struct isp_device *isp;
	struct isp_i2c_cmd i2c_cmd;

	isp = hs_isp_dev->isp;

	i2c_cmd.flags = I2C_CMD_ADDR_16BIT;
	i2c_cmd.addr = HS_CAMERA_I2C_ADDR;
	if((hstp_camera_info->chip_id != 0x8856) && (hstp_camera_info->chip_id != 0x4689)){
		i2c_cmd.reg = 0x4202;
		i2c_cmd.data = 0x0f;
		hs_camera_regrw(isp, &i2c_cmd);
	}
	i2c_cmd.reg = 0x0100;
	i2c_cmd.data = 0x0;
	hs_camera_regrw(isp, &i2c_cmd);

	return 0;
}

/* if the camera have vcm driver , pls pw on or off here */
static int hs_power_on_camera(struct hs_isp_dev *hs_isp_dev){
	/* default PWN_D is pa 13 */
	int tmp[4];
	tmp[0] = *(volatile int*)(HS_BASE_CAMERA_EN + 0x10);
	tmp[1] = *(volatile int*)(HS_BASE_CAMERA_EN + 0x20);
	tmp[2] = *(volatile int*)(HS_BASE_CAMERA_EN + 0x30);
	tmp[3] = *(volatile int*)(HS_BASE_CAMERA_EN + 0x40);

	*(volatile int*)(HS_BASE_CAMERA_EN + 0x10) = tmp[0] & ~HS_OFFSET_CAMERA_EN;
	*(volatile int*)(HS_BASE_CAMERA_EN + 0x20) = tmp[1] | HS_OFFSET_CAMERA_EN;
	*(volatile int*)(HS_BASE_CAMERA_EN + 0x30) = tmp[2] & ~HS_OFFSET_CAMERA_EN;
	*(volatile int*)(HS_BASE_CAMERA_EN + 0x40) = tmp[3] | HS_OFFSET_CAMERA_EN;
#ifdef HS_BASE_AF_EN
	/* vcm power on pd 28*/
	tmp[0] = *(volatile int*)(HS_BASE_AF_EN + 0x10);
	tmp[1] = *(volatile int*)(HS_BASE_AF_EN + 0x20);
	tmp[2] = *(volatile int*)(HS_BASE_AF_EN + 0x30);
	tmp[3] = *(volatile int*)(HS_BASE_AF_EN + 0x40);

	*(volatile int*)(HS_BASE_AF_EN + 0x10) = tmp[0] & ~HS_OFFSET_AF_EN;
	*(volatile int*)(HS_BASE_AF_EN + 0x20) = tmp[1] | HS_OFFSET_AF_EN;
	*(volatile int*)(HS_BASE_AF_EN + 0x30) = tmp[2] & ~HS_OFFSET_AF_EN;
	*(volatile int*)(HS_BASE_AF_EN + 0x40) = tmp[3] | HS_OFFSET_AF_EN;
#endif
	return 0;
}

static int hs_power_off_camera(struct hs_isp_dev *hs_isp_dev){
	/* default PWN_D is pa 13 */
	int tmp[4];
#ifndef HS_PWDN_NO_POWER_OFF
	tmp[0] = *(volatile int*)(HS_BASE_CAMERA_EN + 0x10);
	tmp[1] = *(volatile int*)(HS_BASE_CAMERA_EN + 0x20);
	tmp[2] = *(volatile int*)(HS_BASE_CAMERA_EN + 0x30);
	tmp[3] = *(volatile int*)(HS_BASE_CAMERA_EN + 0x40);

	*(volatile int*)(HS_BASE_CAMERA_EN + 0x10) = tmp[0] & ~HS_OFFSET_CAMERA_EN;
	*(volatile int*)(HS_BASE_CAMERA_EN + 0x20) = tmp[1] | HS_OFFSET_CAMERA_EN;
	*(volatile int*)(HS_BASE_CAMERA_EN + 0x30) = tmp[2] & ~HS_OFFSET_CAMERA_EN;
	*(volatile int*)(HS_BASE_CAMERA_EN + 0x40) = tmp[3] & ~HS_OFFSET_CAMERA_EN;
#endif
#ifdef HS_BASE_AF_EN
	/* vcm power off pd 28*/
	tmp[0] = *(volatile int*)(HS_BASE_AF_EN + 0x10);
	tmp[1] = *(volatile int*)(HS_BASE_AF_EN + 0x20);
	tmp[2] = *(volatile int*)(HS_BASE_AF_EN + 0x30);
	tmp[3] = *(volatile int*)(HS_BASE_AF_EN + 0x40);

	*(volatile int*)(HS_BASE_AF_EN + 0x10) = tmp[0] & ~HS_OFFSET_AF_EN;
	*(volatile int*)(HS_BASE_AF_EN + 0x20) = tmp[1] | HS_OFFSET_AF_EN;
	*(volatile int*)(HS_BASE_AF_EN + 0x30) = tmp[2] & ~HS_OFFSET_AF_EN;
	*(volatile int*)(HS_BASE_AF_EN + 0x40) = tmp[3] & ~HS_OFFSET_AF_EN;
#endif
	return 0;
}

static int hs_isp_initialize(struct isp_device *isp)
{
	isp->boot = 0;
	isp->poweron = 1;
	isp->snapshot = 0;
	isp->bypass = false;
	isp->running = 0;
	isp->format_active = 0;
	isp->bracket_end = 0;
	isp->parm.out_videos = 1; // its default value is 1.
	isp->parm.c_video = 0; // the first video.

	return 0;
}

static int hs_isp_poweroff(struct isp_device *isp)
{
	int tmp;

	/*clk should be disabled here, but error happens, add by pzqi*/
	/* wait for mac wirte ram finish */
	if (!isp->poweron)
		return -ENODEV;
	/*  */
	csi_phy_stop(0);
	/* isp_clk_disable(isp, ISP_CLK_GATE_CSI); */
	tmp = *(volatile int *)(0xb0000020);
	tmp |= 0x02000000;
	*(volatile int *)(0xb0000020) = tmp;

	/*disable interrupt*/
	isp_reg_writeb(isp, 0x00, REG_ISP_INT_EN_C3);
	isp_reg_writeb(isp, 0x00, REG_ISP_INT_EN_C2);
	isp_reg_writeb(isp, 0x00, REG_ISP_INT_EN_C1);
	isp_reg_writeb(isp, 0x00, REG_ISP_INT_EN_C0);
	isp_reg_writeb(isp, 0x00, REG_ISP_MAC_INT_EN_H);
	isp_reg_writeb(isp, 0x00, REG_ISP_MAC_INT_EN_L);

	/* isp_powerdown(isp); //isp should first power down */
	tmp = *(volatile int *)(0xb0000004);
	tmp |= 0x08000000;
	*(volatile int *)(0xb0000004) = tmp;

	/* isp_clk_disable(isp, ISP_CLK_GATE_ISP); */
	tmp = *(volatile int *)(0xb0000020);
	tmp |= 0x00800000;
	*(volatile int *)(0xb0000020) = tmp;

	/* isp_clk_disable(isp, ISP_CLK_CGU_ISP); */
	*(volatile int *)(0xb0000080) = 0xd0000004;

	return 0;
}

static void hs_fw_copy(unsigned int *dst,unsigned int *src, int cnt)
{
	int i = 0;

	for (i = 0; i < cnt; i++) {
		volatile unsigned char *dt = (volatile unsigned char *)dst;
		volatile unsigned char *st = (volatile unsigned char *)src;

		dt[3] = st[0];
		dt[2] = st[1];
		dt[1] = st[2];
		dt[0] = st[3];

		dst++;
		src++;
	}
}

static int hs_isp_boot(struct isp_device *isp)
{
	unsigned char val;
	int max_cmd_sendtimes = 200 ;

	/* Mask all interrupts. */
	hs_isp_intc_disable(isp, 0xffffffff);
	hs_isp_mac_int_mask(isp, 0xffff);

	/* Reset ISP.  */
	isp_reg_writeb(isp, DO_SOFT_RST, REG_ISP_SOFT_RST);
	printk("REG_ISP_SOFT_RST:%x\n", isp_reg_readb(isp, REG_ISP_SOFT_RST));
	/* soft reset must wait for 3 ms */
	/* udelay(3000); */
	/* Enable interrupt (only set_cmd_done interrupt). */
	hs_isp_intc_enable(isp, MASK_INT_CMDSET);

	isp_reg_writeb(isp, DO_SOFTWARE_STAND_BY, REG_ISP_SOFT_STANDBY);

	/* ISP_PRINT(ISP_INFO,"REG_ISP_SOFT_STANDBY:%x\n", isp_reg_readb(isp, REG_ISP_SOFT_STANDBY)); */
	/* Enable the clk used by mcu. */
	isp_reg_writeb(isp, 0xf1, REG_ISP_CLK_USED_BY_MCU);
	/* ISP_PRINT(ISP_INFO,"REG_ISP_CLK_USED_BY_MCU:%x\n", isp_reg_readb(isp, REG_ISP_CLK_USED_BY_MCU)); */

	/* Download firmware to ram of mcu. */
	hs_fw_copy((unsigned int *)(isp->base + FIRMWARE_BASE), (unsigned int *)hs_isp_firmware,
		ARRAY_SIZE(hs_isp_firmware) / 4);

	/* printk("REG_ISP_CLK_USED_BY_MCU:%x\n", isp_reg_readb(isp, REG_ISP_CLK_USED_BY_MCU)); */

	hs_isp_intc_state(isp);
	/* MCU initialize. */
	isp_reg_writeb(isp, 0xf0, REG_ISP_CLK_USED_BY_MCU);

	udelay(1000);

	while (!(hs_isp_intc_state(isp) & MASK_INT_CMDSET) && max_cmd_sendtimes != 0) {
	    udelay(1000);
	    max_cmd_sendtimes--;
	}

	/* printk("max_cmd_sendtimes = %d \n",max_cmd_sendtimes); */

	val = isp_reg_readb(isp, COMMAND_FINISHED);
	if (val != CMD_FIRMWARE_DOWNLOAD) {
		ISP_PRINT(ISP_ERROR, KERN_ERR "Failed to download isp firmware (%02x)\n", val);
		return -EINVAL;
	}

	isp_reg_writeb(isp, DO_SOFTWARE_STAND_BY, REG_ISP_SOFT_STANDBY);

	/* isp_i2c_config(isp); */
	isp_reg_writeb(isp, I2C_SPEED_200, REG_SCCB_MAST1_SPEED);
	
	ISP_PRINT(ISP_INFO,"versionh  %08x\n",isp_reg_readb(isp, 0x6303d));
	ISP_PRINT(ISP_INFO,"versionl  %08x\n",isp_reg_readb(isp, 0x6303e));

	return 0;
}

static int hs_isp_start_on(struct isp_device *isp)
{
	int i = 0;
	int ret = 0;
	int tmp = 200;

	/* isp_clk_enable(isp, ISP_CLK_CGU_ISP); */
	*(volatile int *)(0xb0000080) = 0xa0000004;
	while( (*(volatile int *)(0xb0000080) & 0x10000000) && tmp != 0 ){
	    udelay(20);
	    tmp--;
	}
	*(volatile int *)(0xb0000080) = 0x80000004;

	/* isp_clk_enable(isp, ISP_CLK_GATE_ISP); */
	tmp = *(volatile int *)(0xb0000020);
	tmp &= 0xff7fffff;
	*(volatile int *)(0xb0000020) = tmp;

	/* isp_clk_enable(isp, ISP_PWC_GATE_ISP); */
	tmp = *(volatile int *)(0xb0000004);
	tmp &= 0xf7ffffff;
	*(volatile int *)(0xb0000004) = tmp;

	/* ISP_PRINT(ISP_INFO,"cpm cpccr:%x\n", *((volatile unsigned int *)0xB0000000)); */
	/* ISP_PRINT(ISP_INFO,"isp cpm regs:%08x\n", *((volatile unsigned int *)0xB0000080)); */
	/* take care , you must delay 3000 us here */
	udelay(3000);
	if (!isp->boot) {
		ret = hs_isp_boot(isp);
		if (ret)
			return ret;
		isp->boot = 1;
	}

	/* isp_clk_enable(isp, ISP_CLK_GATE_CSI); */
	tmp = *(volatile int *)(0xb0000020);
	tmp &= 0xfdffffff;
	*(volatile int *)(0xb0000020) = tmp;

	/* ret = isp_mipi_init(isp); */
	for (i = 0; i < ARRAY_SIZE(hs_isp_mipi_regs_init); i++) {
		isp_reg_writeb(isp, hs_isp_mipi_regs_init[i].value,
				hs_isp_mipi_regs_init[i].reg);
	}

	isp->input = 0;
	isp->snapshot = 0;
	isp->format_active = 0;
	memset(&isp->fmt_data, 0, sizeof(isp->fmt_data));
	/* init the parameters of isp */
	memset(&(isp->parm), 0 , sizeof(struct isp_parm));
	isp->parm.out_videos = 1; // its default value is 1.
	isp->parm.c_video = 0; // the first video.

	isp->first_init = true;
	isp->done_frames = 0;
	isp->frame_need_drop = 0;
	isp->wait_eof = false;
	isp->capture_raw_enable = false;

	return ret;
}


static int hs_isp_fill_parm_registers(struct isp_device *isp, struct isp_reg_t *regs)
{
	int i = 0;
	for(i = 0; i < 100 && regs[i].reg != OVISP_REG_END; i++){
		isp_firmware_writeb(isp, regs[i].value, regs[i].reg);
		if( regs[i].reg & 0x10000 )
			isp_firmware_writeb(isp, regs[i].value, regs[i].reg);
		else if( regs[i].reg & 0x60000 )
			isp_reg_writeb(isp, regs[i].value, regs[i].reg);
	}
	if(i >= 100)
		return -EINVAL;
	else
		return 0;

}

static int hs_isp_fill_i2c_group(struct isp_device *isp, struct regval_list *camera_regs)
{
	struct regval_list *sensor_regs;
	unsigned char i2c_flag = 0;
	int j = 0;

	i2c_flag = HS_CAMERA_I2C_FLAG;

	sensor_regs = camera_regs;
	for (j = 0; j < 150; j++) {
	    if (sensor_regs[j].reg_num == SENSOR_REG_END) {
		break;
	    }
	    isp_firmware_writeb(isp, sensor_regs[j].reg_num >> 8, COMMAND_BUFFER + j * 4);
	    isp_firmware_writeb(isp, sensor_regs[j].reg_num & 0xff, COMMAND_BUFFER + j * 4 + 1);
	    isp_firmware_writeb(isp, sensor_regs[j].value, COMMAND_BUFFER + j * 4 + 2);
	    isp_firmware_writeb(isp, 0xff, COMMAND_BUFFER + j * 4 + 3);
	}
	isp_reg_writeb(isp, i2c_flag, COMMAND_REG1);
	isp_reg_writeb(isp, HS_CAMERA_I2C_ADDR, COMMAND_REG2);
	isp_reg_writeb(isp, j, COMMAND_REG3);
	return 0;
}

static int hs_isp_setting_init(struct isp_device *isp)
{
	struct hs_isp_sensor_setting* hs_isp_sensor_setting;
	struct isp_reg_t* hs_isp_setting = NULL;
	int i = 0;

	for (i = 0; i < HSTP_SETTING_SIZES; i++) {
	    hs_isp_sensor_setting = &hs_isp_sensor_settings[i];
	    if(hstp_camera_info->chip_id == hs_isp_sensor_setting->chip_id){
		hs_isp_setting = hs_isp_sensor_setting->hs_isp_setting;
		break;
	    }
	}
	
	if(hs_isp_setting == NULL)return -1;
	
	for (i = 0; hs_isp_setting[i].reg != OVISP_REG_END; i++) {
	    if( hs_isp_setting[i].reg & 0x10000 ){
			isp_firmware_writeb(isp, hs_isp_setting[i].value, hs_isp_setting[i].reg);
	    }
	    else if( hs_isp_setting[i].reg & 0x60000 ){
			isp_reg_writeb(isp, hs_isp_setting[i].value, hs_isp_setting[i].reg);
	    }
	}
	return 0;
}

static int hs_isp_set_af_parameters(struct isp_device *isp)
{
	/* set auto focus window size */
	unsigned int startX = 0, startY = 0, windowWidth = 0, windowHeight = 0;
	windowWidth = (HS_CAMERA_PREVIEW_WIDTH/3) & ~0xf;
	windowHeight = (HS_CAMERA_PREVIEW_HEIGHT/3) & ~0xf;
	startX = windowWidth;
	startY = windowHeight;
	isp_reg_writeb(isp, (startX >> 8) & 0xff, 0x66304);
	isp_reg_writeb(isp, startX & 0xff, 0x66305);
	isp_reg_writeb(isp, (startY >> 8) & 0xff, 0x66306);
	isp_reg_writeb(isp, startY & 0xff, 0x66307);
	isp_reg_writeb(isp, (windowWidth >> 8) & 0xff, 0x66308);
	isp_reg_writeb(isp, windowWidth & 0xff, 0x66309);
	isp_reg_writeb(isp, (windowHeight >> 8) & 0xff, 0x6630a);
	isp_reg_writeb(isp, windowHeight & 0xff, 0x6630b);
	return 0;
}

static int hs_set_af_status(struct isp_device *isp)
{
    /* just for dw9714, with i2c addr 0x18 */
	if(isp_firmware_readb(isp, 0x1ec6e) ==  0x01){
		isp_firmware_writeb(isp, 0x00, 0x1ec6e);
		isp_reg_writeb(isp, 0x18, 0x63601);
		isp_reg_writeb(isp, 0x02, 0x63606);
		isp_reg_writeb(isp, 0x10, 0x63604);
		isp_reg_writeb(isp, 0x4f, 0x63605);
		isp_reg_writeb(isp, 0x35, 0x63609);
	}
	return 0;
}

static irqreturn_t hs_isp_irq(int this_irq, void *dev_id)
{
	struct hs_isp_dev *hs_isp_dev = g_hs_isp_dev;
	struct isp_device *isp;
	unsigned int hs_irq_status;
	unsigned short hs_mac_irq_status = 0;
	unsigned char hs_cmd;

	if(hs_isp_dev == NULL){
		unregister_hs_isp_irq();
		return 1;
	}
	isp = hs_isp_dev->isp;

	hs_irq_status = hs_isp_intc_state(isp);
	hs_mac_irq_status = hs_isp_mac_int_state(isp);
	hs_cmd = isp_reg_readb(isp, COMMAND_REG0);

	/* printk("hs_cmd = %d, hs_irq_status = %08x, hs_mac_irq_status = %04x.\n",hs_cmd, hs_irq_status, hs_mac_irq_status); */
	/* Command set done interrupt. */
	if (hs_irq_status & MASK_INT_CMDSET) {
		if(hs_cmd == CMD_SET_FORMAT){
			hs_isp_dev->hs_isp_status = HS_ISP_CMD_02;
			hs_isp_dev->hs_camera_status = HS_CAMERA_STREAM_ON;
			hs_isp_fill_parm_registers(isp, hs_isp_aecagc_init);
		}else if(hs_cmd == CMD_CAPTURE){
			complete(&isp->completion);
			hs_isp_dev->hs_isp_status = HS_ISP_CMD_04;
		}else if(hs_cmd == CMD_OFFLINE_PROCESS){
			complete(&isp->completion);
			hs_isp_dev->hs_isp_status = HS_ISP_CMD_06;
		}
	}
	if(hs_isp_dev->hs_isp_status == HS_ISP_CMD_02 && (hs_irq_status & MASK_INT_MAC)){
		if(isp->done_frames == 1){
			isp_firmware_writeb(isp, 0x00, 0x1e022);
			isp_reg_writeb(isp, 0x00, 0x65320);
#ifdef HS_BASE_AF_EN 
				hs_set_af_status(isp);
#endif
		}
		isp->done_frames++;
	}
	return 1;
}

static int hs_isp_start_preview(struct isp_device *isp)
{
	int ret;
	csi_phy_init();
	ret = csi_phy_start(0, 72, 2);
	if(ret < 0){
	    printk("hs_csi ERROR ERROR ERROR ERROR ERROR ERROR ERROR\n");
	    return ret;
	}
	ret = hs_isp_setting_init(isp);
	if (ret < 0) {
	    printk("isp setting init failed !!!\n");
	    return ret;
	}
	/* if vcm module exist */
	hs_isp_set_af_parameters(isp);
	/* set bggr */
	isp_reg_writeb(isp, 0x02, 0x65006);
	/* set aec manual */
	isp_firmware_writeb(isp, 0x01, 0x1e022);
	/* set awb manual */
	isp_reg_writeb(isp, 0x01, 0x65320);
	isp_firmware_writeb(isp, 0x01, 0x1c07c);
	/* camera is AF */
#ifdef HS_BASE_AF_EN 
		hs_isp_set_af_parameters(isp);
#endif
	hs_isp_fill_i2c_group(isp, hstp_camera_info->hstp_preview_setting->regs);
	
	hs_isp_fill_parm_registers(isp, hs_isp_aecagc_init);
	
	hs_isp_set_banding_parameters(isp, hstp_camera_info->hstp_preview_setting, true);

	hs_isp_set_window_size(isp, hstp_camera_info->hstp_preview_setting, false);

	hs_isp_fill_parm_registers(isp, hs_isp_preview);

	/* Set Max Exposure */
	unsigned short vts = hstp_camera_info->hstp_preview_setting->vts;
	isp_firmware_writeb(isp, ((vts - 0x10) >> 8) & 0xff, 0x1f072);
	isp_firmware_writeb(isp, (vts - 0x10) & 0xff, 0x1f073);

	/*read to clear interrupt*/
	hs_isp_intc_state(isp);
	hs_isp_mac_int_state(isp);
	/* we don not check cmd 02 */
	hs_isp_send_cmd(isp, CMD_SET_FORMAT);

	hs_isp_intc_disable(isp, 0xffffffff);
	hs_isp_mac_int_mask(isp, 0xffff);

	hs_isp_intc_enable(isp, MASK_INT_MAC);
	hs_isp_intc_enable(isp, MASK_INT_CMDSET);
	hs_isp_intc_enable(isp, MASK_ISP_INT_EOF);
	hs_isp_mac_int_unmask(isp,
			MASK_INT_WRITE_DONE0 | MASK_INT_WRITE_DONE1 |
			MASK_INT_OVERFLOW0 | MASK_INT_OVERFLOW1 |
			MASK_INT_DROP0 | MASK_INT_DROP1 |
			MASK_INT_WRITE_START0 | MASK_INT_WRITE_START1
			);

	return 0;
}

static int hs_reset_isp_status(struct hs_isp_dev *hs_isp_dev)
{
	struct isp_device *isp;
	isp = hs_isp_dev->isp;

	if(hs_isp_dev->hs_isp_status != HS_ISP_POWER_OFF){
	    hs_isp_poweroff(isp);
	    hs_isp_dev->hs_isp_status = HS_ISP_POWER_OFF;
	}
	unregister_hs_isp_irq();
	return 0;
}


static int hs_reset_camera_status(struct hs_isp_dev *hs_isp_dev)
{
	struct isp_device *isp;
	isp = hs_isp_dev->isp;

	if(hs_isp_dev->hs_camera_status != HS_CAMERA_PWN_OFF){
	    hs_camera_stream_off(hs_isp_dev);
	    hs_power_off_camera(hs_isp_dev);
	}
	hs_isp_dev->hs_camera_status = HS_CAMERA_PWN_OFF;
	return 0;
}

int hs_isp_quickcapture_start(void){
	struct hs_isp_dev *hs_isp_dev = g_hs_isp_dev;
	struct isp_device *isp;
	int ret;
	if(hs_isp_dev == NULL){
	    setCameraKey_WakeUp(0);
	    return 0;
	}
	isp = hs_isp_dev->isp;
	/* add for debug */
	if(hs_isp_dev->hs_isp_status != HS_ISP_POWER_OFF || hs_isp_dev->hs_camera_status != HS_CAMERA_PWN_OFF){
		printk("hs_isp occure error initial state on isp or camera, ~~~ \n");
		hs_isp_dev->hs_isp_task_status = HS_ISP_TASK_ERROR;
		setCameraKey_WakeUp(0);
		return 0;
	}
	/*  */
	hs_isp_initialize(isp);

	hs_power_on_camera(hs_isp_dev);
	hs_isp_dev->hs_camera_status = HS_CAMERA_PWN_ON;

	ret = hs_isp_start_on(isp);
	if(ret < 0){
	    printk("hs_isp_firmware boot failed~~\n");
	    hs_power_off_camera(hs_isp_dev);
	    hs_isp_dev->hs_camera_status = HS_CAMERA_PWN_OFF;
	    hs_isp_poweroff(isp);
	    hs_isp_dev->hs_isp_status = HS_ISP_POWER_OFF;
	    hs_isp_dev->hs_isp_task_status = HS_ISP_TASK_ERROR;
	    setCameraKey_WakeUp(0);
	    return ret;
	}
	ret = hs_isp_start_preview(isp);
	if(ret < 0){
	    printk("csi init failed~~\n");
	    hs_power_off_camera(hs_isp_dev);
	    hs_isp_dev->hs_camera_status = HS_CAMERA_PWN_OFF;
	    hs_isp_poweroff(isp);
	    hs_isp_dev->hs_isp_status = HS_ISP_POWER_OFF;
	    hs_isp_dev->hs_isp_task_status = HS_ISP_TASK_ERROR;
	    setCameraKey_WakeUp(0);
	    return ret;
	}

	hs_isp_dev->hs_isp_task_status = HS_ISP_TASK_START;
	register_hs_isp_irq(hs_isp_irq);

	return 0;
}

static int hs_isp_take_photo(void)
{
	struct hs_isp_dev *hs_isp_dev = g_hs_isp_dev;
	struct isp_device *isp;
	unsigned char priv[ISP_OUTPUT_INFO_LENS];
	int ret = 0;
	int i = 0;

	if(hs_isp_dev == NULL){
	    return 0;
	}
#ifdef CONFIG_TOUCHPANEL_IT7236_KEY
	if(hstp_is_tp_forward() != 1 && getCameraKey_WakeUp() == 1){
	    hs_isp_dev->hs_isp_task_status = HS_ISP_TASK_FAILED;
	    goto hs_reset;
	}
#endif
	isp = hs_isp_dev->isp;
	if(hs_isp_dev->hs_isp_status != HS_ISP_CMD_02){
		printk("isp preview failed ..\n");
		hs_isp_dev->hs_isp_task_status = HS_ISP_TASK_FAILED;
		goto hs_reset;
	}

	init_completion(&isp->completion);

	isp_reg_writeb(isp, 0x02, 0x65006);
	isp_firmware_writeb(isp, 0x01, 0x1e022);
	for(i = 0; i < ISP_OUTPUT_INFO_LENS; i++){
		priv[i] = isp_firmware_readb(isp, 0x1f200 + i);
	}

	hs_isp_fill_i2c_group(isp, hstp_camera_info->hstp_photo_setting->regs);

	hs_isp_set_banding_parameters(isp, hstp_camera_info->hstp_photo_setting, false);

	hs_isp_set_window_size(isp, hstp_camera_info->hstp_photo_setting, false);

	hs_isp_fill_parm_registers(isp, hs_isp_capture_raw);

	ret = hs_isp_send_cmd(isp, CMD_CAPTURE);
	if(ret < 0){
		printk("hs_capture_raw failed\n");
		hs_isp_dev->hs_isp_task_status = HS_ISP_TASK_FAILED;
		goto hs_reset;
	}

	hs_isp_get_exif_info(isp);//get exif info
	/* camera steam off and pull low pwn*/
	hs_reset_camera_status(hs_isp_dev);


	for(i = 0; i < ISP_OUTPUT_INFO_LENS; i++){
		isp_firmware_writeb(isp, priv[i], 0x1f300 + i); 
	}

	hs_isp_set_window_size(isp, hstp_camera_info->hstp_photo_setting, true);

	hs_isp_fill_parm_registers(isp, hs_isp_process_raw);
	
	isp_reg_writeb(isp, 0x01, COMMAND_REG2);

	ret = hs_isp_send_cmd(isp, CMD_OFFLINE_PROCESS);
	if(ret < 0){
		printk("hs_offline_process failed\n");
		hs_isp_dev->hs_isp_task_status = HS_ISP_TASK_FAILED;
		goto hs_reset;
	}
	hs_isp_dev->hs_isp_task_status = HS_ISP_TASK_SUCCESS;
	/* hs_isp_dev->wake_condition = 1; */
	/* wake_up(&hs_isp_dev->hs_isp_wq); */
	/* return 0; */
 hs_reset:
	hs_reset_camera_status(hs_isp_dev);
	hs_reset_isp_status(hs_isp_dev);
	hs_isp_dev->wake_condition = 1;
	wake_up(&hs_isp_dev->hs_isp_wq);
	setCameraKey_WakeUp(0);
	return 0;
}
static char *hs_isp_devnode(struct device *dev, mode_t *mode)
{
	*mode = 0666;
	return NULL;
}

/************************** sysfs end  **********************************/
static int __init hs_isp_init(void)
{
	dev_t dev = 0;
	int ret, dev_no;
	struct hs_isp_dev *hs_isp_dev;
	struct isp_device *isp;

	printk("ADD HSTP DRIVER, TAKE CARE ...\n");
	hs_isp_dev = kzalloc(sizeof(struct hs_isp_dev), GFP_KERNEL);
	if(!hs_isp_dev) {
		printk("hs_isp_dev dev alloc failed\n");
		goto __err_hs_isp;
	}
	isp = kzalloc(sizeof(*isp), GFP_KERNEL);
	if(!isp) {
		printk("isp dev alloc failed\n");
		goto __err_isp;
	}
	hs_isp_dev->isp = isp;
	isp->boot = 0;
	isp->base = ioremap(0x13300000, 0x50000);
	if (!isp->base) {
	    printk(KERN_ERR "Unable to ioremap registers.n");
		goto __err_chrdev;
	}

	hs_isp_dev->class = class_create(THIS_MODULE, "jz-hs_isp");
	hs_isp_dev->class->devnode = hs_isp_devnode;
	hs_isp_dev->minor = 0;
	hs_isp_dev->nr_devs = 1;
	hs_isp_dev->hs_isp_task_status = HS_ISP_TASK_IDLE;
	hs_isp_dev->hs_isp_status = HS_ISP_POWER_OFF;
	hs_isp_dev->hs_camera_status = HS_CAMERA_PWN_OFF;
	hs_isp_dev->hs_isp_enable = HS_ISP_ENABLE;
	hs_isp_dev->hs_isp_watermark_enable = HS_ISP_DISABLE;
	hs_isp_dev->wake_condition = 0;

	ret = alloc_chrdev_region(&dev, hs_isp_dev->minor, hs_isp_dev->nr_devs, "jz-hs_isp");
	if(ret) {
		printk("alloc chrdev failed\n");
		goto __err_chrdev;
	}
	hs_isp_dev->major = MAJOR(dev);

	dev_no = MKDEV(hs_isp_dev->major, hs_isp_dev->minor);
	cdev_init(&hs_isp_dev->cdev, &hs_isp_ops);
	hs_isp_dev->cdev.owner = THIS_MODULE;
	cdev_add(&hs_isp_dev->cdev, dev_no, 1);

	init_timer(&hs_isp_dev->hs_isp_timer);
	hs_isp_dev->hs_isp_timer.function = hs_isp_timer_handler;
	hs_isp_dev->hs_isp_timer.data	= (unsigned long)hs_isp_dev;

	hs_isp_dev->dev = device_create(hs_isp_dev->class, NULL, dev_no, NULL, "jz-hs_isp");

	register_syscore_ops(&hs_isp_pm_ops);

	ret = sysfs_create_group(&hs_isp_dev->dev->kobj, &hs_isp_attr_group);
	if (ret < 0) {
		    printk("cannot create sysfs interface\n");
	}

	hs_isp_dev->hs_wq = create_singlethread_workqueue("hs_isp_workqueue");
	if(!hs_isp_dev->hs_wq){
	    goto __err_chrdev;
	}

	INIT_DELAYED_WORK(&hs_isp_dev->hs_delayed_work, hs_work_handler);

	init_waitqueue_head(&hs_isp_dev->hs_isp_wq);
	hs_isp_dev->hs_isp_pending = 0;

	spin_lock_init(&hs_isp_dev->hs_isp_lock);

	dev_set_drvdata(hs_isp_dev->dev, hs_isp_dev);
	g_hs_isp_dev = hs_isp_dev;

	return 0;

__err_chrdev:
	kfree(isp);
 __err_isp:
	kfree(hs_isp_dev);
__err_hs_isp:

	return -EFAULT;
}
static void __exit hs_isp_exit(void)
{
	struct hs_isp_dev *hs_isp_dev = g_hs_isp_dev;
	struct isp_device *isp;
	if(hs_isp_dev == NULL){
	    return;
	}
	isp = hs_isp_dev->isp;
	destroy_workqueue(hs_isp_dev->hs_wq);
	kfree(isp);
	kfree(hs_isp_dev);
}

module_init(hs_isp_init);
module_exit(hs_isp_exit);


MODULE_AUTHOR("sunjianfeng<forest.jfsun@ingenic.com>");
MODULE_DESCRIPTION("hs_isp driver using for quick capture");
MODULE_LICENSE("GPL");

