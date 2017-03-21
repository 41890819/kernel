#ifndef __LINUX_JZ_ADC_H__
#define __LINUX_JZ_ADC_H__

#include <linux/power/jz_battery.h>

#define CONFIG_CMD_AUX1    1
#define CONFIG_CMD_AUX2    2

extern int jz_adc_set_config(struct device *dev, uint32_t cmd);

struct jz_adc_platform_data {
	struct jz_battery_info battery_info;
};

#endif
