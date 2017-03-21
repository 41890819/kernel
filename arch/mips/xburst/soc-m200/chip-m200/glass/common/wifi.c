#include <linux/platform_device.h>
#include <linux/gpio.h>

#include <board.h>
#include <linux/wlan_plat.h>

#if (defined(CONFIG_BCM4343))
extern struct wifi_platform_data bcmdhd_wlan_pdata;
#endif

static struct resource wlan_resources[] = {
	[0] = {
		.start = WL_WAKE_HOST,
		.end = WL_WAKE_HOST,
		.name = "bcmdhd_wlan_irq",
		.flags  = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL | IORESOURCE_IRQ_SHAREABLE,
	},
};
static struct platform_device wlan_device = {
	.name   = "bcmdhd_wlan",
	.id     = 1,
	.dev    = {

#if (defined(CONFIG_BCM4343))
                .platform_data = &bcmdhd_wlan_pdata,
#else
		.platform_data = NULL,
#endif
	},
	.resource	= wlan_resources,
	.num_resources	= ARRAY_SIZE(wlan_resources),
};

static int __init wlan_device_init(void)
{
	int ret;

	ret = platform_device_register(&wlan_device);

	return ret;
}

late_initcall(wlan_device_init);
MODULE_DESCRIPTION("Broadcomm wlan driver");
MODULE_LICENSE("GPL");
