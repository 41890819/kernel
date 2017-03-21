#include <linux/module.h>	/* kernel module definitions */
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>

#include <linux/irq.h>
#include <linux/param.h>
#include <linux/bitops.h>
#include <linux/input.h>
#include <linux/termios.h>
#include <linux/wakelock.h>
#include <linux/gpio.h>
#include <linux/serial_core.h>

#ifdef CONFIG_VIDEO_CAMERA_FLASH_LAMP
extern void flash_lamp_on(void);
extern void flash_lamp_off(void);
#endif

static ssize_t flashlamp_show(struct device *dev, struct device_attribute *attr, char *buf){
  printk("flashlamp_show in\n");
  return 0;
}

static ssize_t flashlamp_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count){
  int flashlampnum = 0;


  flashlampnum = buf[0];
  printk("flashlamp_store in %d\n", flashlampnum);

  if (flashlampnum == 48){
    flash_lamp_off();
  }else{
    flash_lamp_on();
  }

  return count;
}

static DEVICE_ATTR(flashlamp, 0666, flashlamp_show, flashlamp_store);

static int /*__init*/ flashlamp_probe(struct platform_device *pdev)
{
  int ret = 0;

  printk("flashlamp_probe in\n");

  ret = device_create_file(&pdev->dev, &dev_attr_flashlamp);
  if (ret != 0) {
    printk("Failed to create flashlamp sysfs files: %d\n", ret);
    return ret;
  }else{
    printk("succ to create flashlamp sysfs\n");
  }

  return 0;
}

static int flashlamp_remove(struct platform_device *pdev)
{
  printk("flashlamp_remove in\n");
  return 0;
}

static struct platform_driver flashlamp_driver = {
	.probe = flashlamp_probe,
	.remove = flashlamp_remove,
	.suspend = NULL,
	.resume = NULL,
	.driver = {
		   .name = "flashlamp",
		   .owner = THIS_MODULE,
		   },
};

static int __init flashlamp_init(void)
{
	int retval;

	printk("flashlamp_init in\n");
	retval = platform_driver_register(&flashlamp_driver);
	if (retval)
		return retval;
	printk("flashlamp_init succ\n");
	return 0;
}

module_init(flashlamp_init);

static void __exit flashlamp_exit(void)
{
	platform_driver_unregister(&flashlamp_driver);

}

module_exit(flashlamp_exit);

MODULE_LICENSE("GPL");
