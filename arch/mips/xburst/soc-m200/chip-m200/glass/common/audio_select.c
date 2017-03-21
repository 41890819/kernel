#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <board_base.h>

#if defined (SPK_REGULATOR_EN)
struct amp_ctrl
{
	struct regulator *vdd;
	struct mutex lock;
};
static struct amp_ctrl *ampctrl = NULL;
#endif

int ampvdd_power_init(struct device *dev)
{
	int ret = 0;

#if defined (SPK_REGULATOR_EN)
	if(ampctrl == NULL) {
		ampctrl = kmalloc(sizeof(struct amp_ctrl), GFP_KERNEL);
		if (!ampctrl)
			return -ENOMEM;

		ampctrl->vdd = regulator_get(dev, SPK_REGULATOR_EN);
		if (IS_ERR(ampctrl->vdd)) {
			pr_err("can not get regulator amp_vdd\n");
			kfree(ampctrl);
			ampctrl = NULL;
			return -ENODEV;
		}

		mutex_init(&ampctrl->lock);
	}
#endif
	return ret;
}
EXPORT_SYMBOL(ampvdd_power_init);

int ampvdd_power_on(void) {
#if defined (SPK_REGULATOR_EN) 
	if(ampctrl != NULL) {
		mutex_lock(&ampctrl->lock);   

		if (regulator_is_enabled(ampctrl->vdd) == 0)
			regulator_enable(ampctrl->vdd);

		mutex_unlock(&ampctrl->lock);
	}
#endif
	return 0;
}
EXPORT_SYMBOL(ampvdd_power_on);

int ampvdd_power_off(void) {
#if defined (SPK_REGULATOR_EN)
	if(ampctrl != NULL) {
		mutex_lock(&ampctrl->lock); 

		if (regulator_is_enabled(ampctrl->vdd))
			regulator_disable(ampctrl->vdd);

		mutex_unlock(&ampctrl->lock);
	}
#endif
	return 0;
}
EXPORT_SYMBOL(ampvdd_power_off);

int usb_audio_select(void) {	
	if ((GPIO_AUDIO_USB_SEL != -1) && (GPIO_USB_DETE != -1)){
	        int val = 0; 
		int ret_val = 0;
		
		ret_val = gpio_request(GPIO_AUDIO_USB_SEL,"gpio_usb_sel");
		if(ret_val < 0){
			gpio_free(GPIO_AUDIO_USB_SEL);
			gpio_request(GPIO_AUDIO_USB_SEL,"gpio_usb_sel");
		}
		gpio_direction_output(GPIO_AUDIO_USB_SEL, GPIO_AUDIO_USB_SEL_LEVEL); //select HSD
		val = gpio_get_value(GPIO_USB_DETE);
		if(!val){
			gpio_direction_output(GPIO_AUDIO_USB_SEL, !GPIO_AUDIO_USB_SEL_LEVEL);//select other HSD
		}
	}	
	return 0;
}
EXPORT_SYMBOL(usb_audio_select);
