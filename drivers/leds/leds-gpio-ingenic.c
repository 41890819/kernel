#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/delay.h>

struct ingenic_led {
	struct list_head header;

	const char *name;

	int gpio;
	int status;
	int active_low;
	unsigned long rate;
	struct timer_list timer;
};

struct list_head led_list;

static struct delayed_work dwork;

static struct kobject * leds_kobj;

static int supply_percent = 50;
void set_power_supply(int percent) {
	if (percent > 0 && percent < 101) {
		supply_percent = percent;
		printk("set_power_supply : %d\n", supply_percent);
	}
}
EXPORT_SYMBOL(set_power_supply);

#define POWER_STATUS_CHARGING 1
#define POWER_STATUS_DISCHARGING 0
static int power_status = POWER_STATUS_DISCHARGING;
void set_power_status(int status) {
	power_status = status;
	printk("set_power_status : %d\n", status);
}
EXPORT_SYMBOL(set_power_status);

static void ingenic_leds_work_handler(struct work_struct *work) {
#ifdef CONFIG_BOARD_KEPTER
	if (supply_percent == 100 && power_status == POWER_STATUS_CHARGING) {
	}else{
	}
#endif
}

#define INGENIC_LED_ON            0
#define INGENIC_LED_OFF           1
#define INGENIC_LED_FLICKER_ON    2
#define INGENIC_LED_FLICKER_OFF   3
#define INGENIC_LED_FLICKER_RATE  4

static ssize_t set_led_status(const char *name, const char *buf, size_t count)
{
	struct ingenic_led *led;
	list_for_each_entry(led, &led_list, header)
		if (strcmp(led->name, name) == 0) {
			int value = -1, rate = -1;
	
			sscanf(buf, "%d", &value);
			switch (value) {
			case INGENIC_LED_OFF :
			case INGENIC_LED_FLICKER_OFF :
				del_timer_sync(&led->timer);
				gpio_direction_output(led->gpio, led->active_low);
				led->status = 0;
				break;

			case INGENIC_LED_ON :
				del_timer_sync(&led->timer);
				gpio_direction_output(led->gpio, !led->active_low);
				led->status = 1;
				break;

			case INGENIC_LED_FLICKER_ON :
				if ( (led->status & 0x2) == 0 ) {
					add_timer(&led->timer);
					led->status |= 0x2;
				}
				break;

			case INGENIC_LED_FLICKER_RATE :
				sscanf(buf, "%d %d", &value, &rate);
				if (rate > 0)
					led->rate = rate * HZ / 100;
				if (led->status & 0x2)
					mod_timer(&led->timer, jiffies + led->rate);
				break;

			default :
				printk("Unsupport handle for %s led\n", name);
				break;
			}

			return count;
		}

	return -1;
}

static void led_flicker(unsigned long arg)
{
	struct ingenic_led * led = (struct ingenic_led *) arg;

	if (led->status & 0x1) {
		gpio_direction_output(led->gpio, led->active_low);
		led->status &= ~0x1;
	} else {
		gpio_direction_output(led->gpio, !led->active_low);
		led->status |= 0x1;
	}

	led->timer.expires = jiffies + led->rate;
	add_timer(&led->timer);
}

static int ingenic_leds_probe(struct platform_device *pdev)
{
	int ret = 0, i = 0;
	struct gpio_led_platform_data *dev = pdev->dev.platform_data;

	for (i = 0; i < dev->num_leds; i++) {
		const struct gpio_led *led = &dev->leds[i];

		ret = gpio_request(led->gpio, led->name);
		if (ret == 0) {
			struct ingenic_led * iled;
			
			iled = kmalloc(sizeof(struct ingenic_led), GFP_KERNEL);
			if (iled == NULL)
				return -ENOMEM;

			iled->gpio = led->gpio;
			iled->name = led->name;
			iled->active_low = led->active_low;
			if (led->default_state == LEDS_GPIO_DEFSTATE_OFF) {
				gpio_direction_output(iled->gpio, iled->active_low);
				iled->status = 0;
			} else {
				gpio_direction_output(iled->gpio, !iled->active_low);
				iled->status = 1;
			}

			init_timer(&iled->timer);
			iled->rate = HZ;
			iled->timer.data = (unsigned long) iled;
			iled->timer.function = led_flicker;
			iled->timer.expires = jiffies + iled->rate;

			INIT_LIST_HEAD(&iled->header);
			list_add_tail(&iled->header, &led_list);
		} else
			printk("Request %d gpio for %s fail\n", led->gpio, led->name);
	}

	INIT_DELAYED_WORK(&dwork, ingenic_leds_work_handler);
	schedule_delayed_work(&dwork, msecs_to_jiffies(8000));

#if defined(CONFIG_BOARD_CRUISE) || defined(CONFIG_BOARD_CRUISE_UMS)
	set_led_status("red", "2", 1);
#endif
#ifdef CONFIG_BOARD_M200S_ZBRZFY_D
	set_led_status("blue", "0", 1);
#endif

	return 0;
}

static int __devinit ingenic_leds_remove(struct platform_device *pdev)
{
	struct ingenic_led *led;

	while ( led_list.next != &led_list ) {
		led = list_first_entry(&led_list, typeof(*led), header);
		list_del(&led->header);
		del_timer_sync(&led->timer);
		gpio_free(led->gpio);
		kfree(led);
	}

	return 0;
}

#ifdef CONFIG_PM
static int ingenic_leds_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct ingenic_led *led;
#if (defined(CONFIG_BOARD_M200S_ZBRZFY) || defined(CONFIG_BOARD_M200S_ZBRZFY_D) || defined(CONFIG_BOARD_M200S_ZBRZFY_C))
#else
	list_for_each_entry(led, &led_list, header) {
		del_timer_sync(&led->timer);
		gpio_direction_output(led->gpio, led->active_low);
		led->status = 0;
	}
#endif

	return 0;
}

static int ingenic_leds_resume(struct platform_device *pdev)
{
	return 0;
}
#else
#define ingenic_leds_suspend	NULL
#define ingenic_leds_resume	NULL
#endif

static struct platform_driver ingenic_leds_driver = {
	.driver		= {
		.name	= "ingenic-leds",
		.owner	= THIS_MODULE,
	},
	.probe		= ingenic_leds_probe,
	.remove		= ingenic_leds_remove,
	.suspend	= ingenic_leds_suspend,
	.resume		= ingenic_leds_resume,
};

void red_led_interface(int status) {
	if (status == 1) { // ON
		set_led_status("red", "0", 1);
	} else if (status == 2) { // OFF
		set_led_status("red", "1", 1);
	} else if (status == 3) { // flicker ON
		set_led_status("red", "2", 1);
	}
}

EXPORT_SYMBOL(red_led_interface);

static ssize_t led_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t led_store(struct kobject *kobj, struct kobj_attribute *attr,
				 const char *buf, size_t count)
{
	return set_led_status(attr->attr.name, buf, count);
}

#define leds_attr(_name)				\
	static struct kobj_attribute _name##_attr = {	\
		.attr	= {				\
			.name = __stringify(_name),	\
			.mode = 0666,			\
		},					\
		.show	= led_show,			\
		.store	= led_store,			\
	}

leds_attr(red);
leds_attr(green);
leds_attr(blue);
leds_attr(orange);
leds_attr(yellow);
leds_attr(red1);
leds_attr(red2);
leds_attr(red3);

static struct attribute * leds_attr[] = {
	&red_attr.attr,
	&red1_attr.attr,
	&red2_attr.attr,
	&red3_attr.attr,
	&green_attr.attr,
	&blue_attr.attr,
	&orange_attr.attr,
	&yellow_attr.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = leds_attr,
};

static int __init ingenic_leds_init(void)
{
	INIT_LIST_HEAD(&led_list);

	leds_kobj = kobject_create_and_add("ingenic_leds", NULL);
	if (!leds_kobj)
		return -ENOMEM;

	if (sysfs_create_group(leds_kobj, &attr_group)) {
		printk("Create ingenic_leds error!\n");
		kobject_del(leds_kobj);
	}

	return platform_driver_register(&ingenic_leds_driver);
}

static void __exit ingenic_leds_exit(void)
{
	sysfs_remove_group(leds_kobj, &attr_group);
	kobject_del(leds_kobj);

	return platform_driver_unregister(&ingenic_leds_driver);
}

module_init(ingenic_leds_init);
module_exit(ingenic_leds_exit);

MODULE_AUTHOR("Derrick.kznan");
MODULE_LICENSE("GPL");
