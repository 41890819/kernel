/* 
 *
 * Description:
 * 		Bluetooth power driver with rfkill interface and bluetooth host wakeup support.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/rfkill.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <mach/jzmmc.h>

#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>

struct gps_ctrl
{
	struct rfkill *rfkill;
	struct regulator *power;
	struct mutex lock;
	bool poweron;
};

static struct gps_ctrl *gpsctrl = NULL;

extern void enable_clk32k(void);
extern void disable_clk32k(void);

static int gps_rfkill_set_block(void *data, bool blocked)
{
	mutex_lock(&gpsctrl->lock);
	if (blocked && gpsctrl->poweron) {
		regulator_disable(gpsctrl->power); 
		disable_clk32k();
		gpsctrl->poweron = false;

	} else if (!blocked && !gpsctrl->poweron) {
		regulator_enable(gpsctrl->power); 
		enable_clk32k();
		gpsctrl->poweron = true;
	}
	mutex_unlock(&gpsctrl->lock);

	return 0;
}

static const struct rfkill_ops gps_rfkill_ops = {
	.set_block = gps_rfkill_set_block,
};

static int __init gps_power_init(void)
{
	int ret = 0;

	gpsctrl = kmalloc(sizeof(struct gps_ctrl), GFP_KERNEL);
	if (!gpsctrl) {
		ret = -ENOMEM;
		goto err_nomem;
	}

	gpsctrl->power = regulator_get(NULL, "gps");
	if (IS_ERR(gpsctrl->power)) {
		ret = -EINVAL;
		goto err_nopower;
	}
	
	gpsctrl->rfkill = rfkill_alloc("gps", NULL, RFKILL_TYPE_GPS,
			&gps_rfkill_ops, NULL);

	if (!gpsctrl->rfkill) {
		ret = -ENOMEM;
		goto err_power;
	}

	mutex_init(&gpsctrl->lock);

	ret = rfkill_register(gpsctrl->rfkill);

	if (regulator_is_enabled(gpsctrl->power))
		gpsctrl->poweron = true;
	else
		gpsctrl->poweron = false;

	return ret;

err_power:
	regulator_put(gpsctrl->power);
err_nopower:
	kfree(gpsctrl);
err_nomem:
	return ret;
}

static void __exit gps_power_exit(void)
{
	rfkill_unregister(gpsctrl->rfkill);
	kfree(gpsctrl->rfkill);
	regulator_put(gpsctrl->power);
	kfree(gpsctrl);
}

module_init(gps_power_init);
module_exit(gps_power_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Gps power control driver");
MODULE_VERSION("1.0");
