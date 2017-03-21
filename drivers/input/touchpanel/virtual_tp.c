/* Virtual TouchPanel driver.
 *
 * Copyright (c) 2015  Ingenic Glass Development.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/platform_device.h>

struct virtual_tp_data {
        struct input_dev *input_dev;
};

static int virtual_tp_probe(struct platform_device *pdev)
{
        struct virtual_tp_data *virtual_tp = NULL;

        int err = 0;
	
        virtual_tp = kzalloc(sizeof(struct virtual_tp_data), GFP_KERNEL);
        if (!virtual_tp)
		return -ENOMEM;

	dev_set_drvdata(&pdev->dev, virtual_tp);

	virtual_tp->input_dev = input_allocate_device();
        if (!virtual_tp->input_dev) {
	        kfree(virtual_tp);
                return -ENOMEM;
        }

	virtual_tp->input_dev->name = "touchpanel";
	virtual_tp->input_dev->id.bustype = BUS_I2C;

	virtual_tp->input_dev->dev.parent = &pdev->dev;
	input_set_drvdata(virtual_tp->input_dev, virtual_tp);

	/* Register the device as mouse */
	__set_bit(EV_REL, virtual_tp->input_dev->evbit);
	__set_bit(REL_X, virtual_tp->input_dev->relbit);
	__set_bit(REL_Y, virtual_tp->input_dev->relbit);
	__set_bit(REL_Z, virtual_tp->input_dev->relbit);
	__set_bit(EV_KEY, virtual_tp->input_dev->evbit);
	__set_bit(BTN_LEFT, virtual_tp->input_dev->keybit);
	__set_bit(BTN_RIGHT, virtual_tp->input_dev->keybit);

        err = input_register_device(virtual_tp->input_dev);
        if (err) {
                dev_err(&pdev->dev, "[ Virtual TP ] failed to register input device: %s\n",
                                dev_name(&pdev->dev));
		input_free_device(virtual_tp->input_dev);
		kfree(virtual_tp);
		return err;
        }

        return 0;
}

static int virtual_tp_remove(struct platform_device *pdev)
{
	struct virtual_tp_data *virtual_tp = dev_get_drvdata(&pdev->dev);
	
	input_free_device(virtual_tp->input_dev);
	kfree(virtual_tp);

        return 0;
}

static struct platform_driver virtual_driver = {
	.driver		= {
		.name	= "virtual-tp",
		.owner	= THIS_MODULE,
	},
	.probe		= virtual_tp_probe,
	.remove		= virtual_tp_remove,
};

static int __init virtual_tp_init(void)
{
	return platform_driver_register(&virtual_driver);;
}

static void __exit virtual_tp_exit(void)
{
	platform_driver_unregister(&virtual_driver);
}

module_init(virtual_tp_init);
module_exit(virtual_tp_exit);

MODULE_DESCRIPTION("Virtual TouchPanel driver");
MODULE_AUTHOR("Kznan <Derric.kznan@ingenic.com>");
MODULE_LICENSE("GPL");
