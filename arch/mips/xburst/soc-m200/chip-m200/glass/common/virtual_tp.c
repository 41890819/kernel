#include <linux/platform_device.h>

struct platform_device virtual_tp_device = {
	.name		= "virtual-tp",
	.dev		= {
		.platform_data	= NULL,
	},
};
