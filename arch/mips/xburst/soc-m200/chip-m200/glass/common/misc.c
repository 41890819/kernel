
#include <linux/platform_device.h>
#include <linux/power/jz_battery.h>
#include <linux/power/li_ion_charger.h>
#include <linux/jz_adc.h>
#include <mach/jz_efuse.h>
#include <linux/jz_dwc.h>
#include "board_base.h"

#ifdef CONFIG_JZ_EFUSE_V12
struct jz_efuse_platform_data jz_efuse_pdata = {
	/* supply 2.5V to VDDQ */
	.gpio_vddq_en_n = GPIO_EFUSE_VDDQ,
};
#endif

#ifdef CONFIG_CHARGER_LI_ION
/* li-ion charger */
static struct li_ion_charger_platform_data jz_li_ion_charger_pdata = {
	.gpio_charge = GPIO_LI_ION_CHARGE,
	.gpio_ac = GPIO_LI_ION_AC,
	.gpio_active_low = GPIO_ACTIVE_LOW,
};

static struct platform_device jz_li_ion_charger_device = {
	.name = "li-ion-charger",
	.dev = {
		.platform_data = &jz_li_ion_charger_pdata,
	},
};
#endif

#ifdef CONFIG_MFD_JZ_SADC_V12
struct jz_adc_platform_data adc_platform_data;
#endif

#if defined(CONFIG_USB_DWC2) || defined(CONFIG_USB_DWC_OTG)
#if defined(GPIO_USB_ID) && defined(GPIO_USB_ID_LEVEL)
struct jzdwc_pin dwc2_id_pin = {
	.num = GPIO_USB_ID,
	.enable_level = GPIO_USB_ID_LEVEL,
};
#endif

#if defined(GPIO_USB_DETE) && defined(GPIO_USB_DETE_LEVEL)
struct jzdwc_pin dwc2_dete_pin = {
	.num = GPIO_USB_DETE,
	.enable_level = GPIO_USB_DETE_LEVEL,
};
#endif

#if defined(GPIO_USB_DRVVBUS) && defined(GPIO_USB_DRVVBUS_LEVEL) && !defined(USB_DWC2_DRVVBUS_FUNCTION_PIN)
struct jzdwc_pin dwc2_drvvbus_pin = {
	.num = GPIO_USB_DRVVBUS,
	.enable_level = GPIO_USB_DRVVBUS_LEVEL,
};
#endif
#endif /*CONFIG_USB_DWC2 || CONFIG_USB_DWC_OTG*/
