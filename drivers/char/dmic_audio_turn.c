#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/fs.h>

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/fcntl.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/sched.h>
#include <linux/io.h>
#include <linux/memory.h>
#include <linux/mm.h>
#include <linux/syscalls.h>

#include <asm/system.h>
#include <asm/uaccess.h>

#include <linux/voice_wakeup_module.h>
#include "voice_wakeup/wakeup_module/src/jz_dmic.h"
#include "voice_wakeup/wakeup_module/include/common.h"

#define  SWITCH_STREREO_MODE           (1 << 5)       //(REG_DMIC_CR0 | (1 << 5))  
#define  SINGLE_STREREO_MODE            (~(1 << 5))   //(REG_DMIC_CR0 & (~(1 << 5)))
#define  SPLIT_LR_MODE                        (1 << 10)      //(REG_DMIC_CR0 | (1 << 11))
#define  DEFAULT_SHARE_MODE             (~(1 << 10))
#define  DEFAULT_LR_MODE                   (~(1 << 11))
#define  TURN_LR_EXCHANGE_MODE       (1 << 11)  

#define  SWITCH_STREREO_CMD                  0x1
#define  SINGLE_STREREO_CMD                   0x2
#define  TURN_SPLIT_MODE_CMD                 0x3
#define  RESET_DEFALUT                             0x4
#define  TURN_LR_EXCHANGE_CMD               0x5

struct dmic_device_info {
	struct class *dmic_turn_class;
        struct cdev dmic_cdev;
	int major;
        int minor;
        dev_t mic_dev;
};

static struct dmic_device_info *ptn = NULL;


static int sakura_dmic_open(struct inode *inode,struct file *filp){

	struct cdev  *cdev = inode->i_cdev;
        struct dmic_device_info * sakura = container_of(cdev,struct dmic_device_info,dmic_cdev);
        filp->private_data = sakura;
        ptn = sakura;
        printk(" Dmic test open Sucessful\n");
    
        return 0;

}

static long sakura_dmic_ioctl(struct file *filp,unsigned int cmd,unsigned long args){

        struct dmic_device_info * sakura_ioctl = filp->private_data;
        void __user *argp = (void __user *)args;
       
        switch(cmd){

	 case SWITCH_STREREO_CMD :
		printk(" turn strereo sound mode\n");  
                REG_DMIC_CR0 |= SWITCH_STREREO_MODE;
                break;
    
         case SINGLE_STREREO_CMD:   
		printk("turn defalut stereo mode\n");
                REG_DMIC_CR0 &= SINGLE_STREREO_MODE;
                break;
     
	 case TURN_LR_EXCHANGE_CMD:
		 printk("turn exchage the LR channel \n");
                 REG_DMIC_CR0 |= TURN_LR_EXCHANGE_MODE; 
         case TURN_SPLIT_MODE_CMD:
		 printk("turn the splt mode\n");
                 REG_DMIC_CR0 |= SPLIT_LR_MODE;
                break;

	 case RESET_DEFALUT:
                 REG_DMIC_CR0 &= SINGLE_STREREO_MODE;
                 REG_DMIC_CR0 &= DEFAULT_SHARE_MODE;
                 REG_DMIC_CR0 &= DEFAULT_LR_MODE;
	 default:
                printk(" No avilable order...\n");  
                return -1;

	}

        return 0;
}

static int sakura_dmic_close(struct inode *inode,struct file *filp)
{
          REG_DMIC_CR0 &= DEFAULT_LR_MODE;
          REG_DMIC_CR0 &= SINGLE_STREREO_MODE;
          REG_DMIC_CR0 &= DEFAULT_SHARE_MODE;
          printk("The device has turn defalut mode\n"); 
          return 0;         

}

static  struct file_operations  sakura_test_ops = {

	.owner             = THIS_MODULE,
        .open               = sakura_dmic_open,
        .release           = sakura_dmic_close,
        .unlocked_ioctl = sakura_dmic_ioctl,
};

static int __init dmic_strereo_init(void){

        unsigned char ret;
        struct dmic_device_info * sakura = kmalloc(sizeof(struct dmic_device_info),GFP_KERNEL);
        if(!sakura){
		printk(" kalloc mem failed!\n");
                goto __error_handle;
        }
       
        sakura->minor = 0;
        ret = alloc_chrdev_region(&sakura->mic_dev,sakura->minor,1,"dmic_turn_test");
        if(ret){
		printk("alloc chrdev failed\n");
                goto __error_chrdev;
        }
        sakura->major = MAJOR(sakura->mic_dev);
        cdev_init(&sakura->dmic_cdev,&sakura_test_ops);
        sakura->dmic_cdev.owner = THIS_MODULE;
        cdev_add(&sakura->dmic_cdev,sakura->mic_dev,1);

        sakura->dmic_turn_class = class_create(THIS_MODULE,"dmic_turn_test");        
        device_create(sakura->dmic_turn_class,NULL, sakura->mic_dev,NULL,"dmic_turn_test");
        ptn = sakura;


	return 0;

 __error_chrdev:
 __error_handle:
	kfree(sakura);
        return -EFAULT;
}

static void __exit dmic_strereo_exit(void){
 
	device_destroy(ptn->dmic_turn_class,ptn->mic_dev);
        class_destroy(ptn->dmic_turn_class);
        cdev_del(&ptn->dmic_cdev);
        unregister_chrdev_region(ptn->mic_dev,1);
  
        kfree(ptn);
        ptn = NULL;

}

module_init(dmic_strereo_init);
module_exit(dmic_strereo_exit);


MODULE_AUTHOR("Sakura_Mx<aric.pzqi@ingenic.com>");
MODULE_DESCRIPTION("jzdmic driver for turn strereo_mode");
MODULE_LICENSE("GPL");
