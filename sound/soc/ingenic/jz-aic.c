/* sound/soc/xburst/jz-aic.c
 * The AIC(Audio Interface Controller, include I2S, SPDIF, AC97) ALSA
 * core driver for Ingenic SoC, such as m200, m150, JZ4780, JZ4775,
 * and so on.
 *
 * Copyright (C) 2014 Ingenic Semiconductor Co., Ltd.
 *	http://www.ingenic.com
 * Author: Sun Jiwei <jwsun@ingenic.cn>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <sound/jz-aic.h>

void jz_aic_dump_reg(struct device *parent)
{
	printk("\n==========AIC dump reg=========\n");
	printk("AIC_FR:0x%08x\n", jz_aic_read_reg(parent, AICFR));
	printk("AIC_CR:0x%08x\n", jz_aic_read_reg(parent, AICCR));
	printk("AIC_ACCR1:0x%08x\n", jz_aic_read_reg(parent, ACCR1));
	printk("AIC_ACCR2:0x%08x\n", jz_aic_read_reg(parent, ACCR2));
	printk("AIC_I2SCR:0x%08x\n", jz_aic_read_reg(parent, I2SCR));
	printk("AIC_SR:0x%08x\n", jz_aic_read_reg(parent, AICSR));
	printk("AIC_ACSR:0x%08x\n", jz_aic_read_reg(parent, ACSR));
	printk("AIC_I2SSR:0x%08x\n", jz_aic_read_reg(parent, I2SSR));
	printk("AIC_ACCAR:0x%08x\n", jz_aic_read_reg(parent, ACCAR));
	printk("AIC_ACCDR:0x%08x\n", jz_aic_read_reg(parent, ACCDR));
	printk("AIC_ACSAR:0x%08x\n", jz_aic_read_reg(parent, ACSAR));
	printk("AIC_ACSDR:0x%08x\n", jz_aic_read_reg(parent, ACSDR));
	printk("AIC_I2SDIV:0x%08x\n", jz_aic_read_reg(parent, I2SDIV));
	printk("AIC_DR:0x%08x\n", jz_aic_read_reg(parent, AICDR));

	printk("\n==========SPDIF dump reg=========\n");
	printk("SPDIF_ENA:0x%08x\n", jz_aic_read_reg(parent, SPENA));
	printk("SPDIF_CTRL:0x%08x\n", jz_aic_read_reg(parent, SPCTRL));
	printk("SPDIF_STATE:0x%08x\n", jz_aic_read_reg(parent, SPSTATE));
	printk("SPDIF_CFG1:0x%08x\n", jz_aic_read_reg(parent, SPCFG1));
	printk("SPDIF_CFG2:0x%08x\n", jz_aic_read_reg(parent, SPCFG2));
	printk("SPDIF_FIFO:0x%08x\n", jz_aic_read_reg(parent, SPFIFO));
}
EXPORT_SYMBOL_GPL(jz_aic_dump_reg);

unsigned int jz_aic_get_fifo_addr(struct device *dev, unsigned int addr_offset)
{
	struct jz_aic *jz_aic = dev_get_drvdata(dev);
	if ((addr_offset != SPFIFO) && (addr_offset != AICDR))
		return 0;
	return (unsigned int)(jz_aic->addr_base + addr_offset);
}
EXPORT_SYMBOL_GPL(jz_aic_get_fifo_addr);

void jz_aic_write_reg(struct device *dev, unsigned int reg,
		unsigned int val)
{
	struct jz_aic *jz_aic = dev_get_drvdata(dev);

	writel(val, jz_aic->addr_base + reg);
}
EXPORT_SYMBOL_GPL(jz_aic_write_reg);

unsigned int jz_aic_read_reg(struct device *dev, unsigned int reg)
{
	struct jz_aic *jz_aic = dev_get_drvdata(dev);

	return readl(jz_aic->addr_base + reg);
}
EXPORT_SYMBOL_GPL(jz_aic_read_reg);

unsigned long get_i2s_clk_rate(struct device *dev)
{
	struct jz_aic *jz_aic = dev_get_drvdata(dev);

	return clk_get_rate(jz_aic->i2s_clk);
}
EXPORT_SYMBOL_GPL(get_i2s_clk_rate);

unsigned long get_i2s_clk_parent_rate(struct device *dev)
{
	struct jz_aic *jz_aic = dev_get_drvdata(dev);
	struct clk *parent_clk = clk_get_parent(jz_aic->i2s_clk);
	unsigned long clk_tmp;

	clk_tmp = clk_get_rate(parent_clk);

	return clk_tmp;
}
EXPORT_SYMBOL_GPL(get_i2s_clk_parent_rate);

int set_i2s_clk_rate(struct device *dev, unsigned long rate)
{
	struct jz_aic *jz_aic = dev_get_drvdata(dev);

	return clk_set_rate(jz_aic->i2s_clk, rate);
}
EXPORT_SYMBOL_GPL(set_i2s_clk_rate);

int enable_aic_clk(struct device *dev)
{
	struct jz_aic *jz_aic = dev_get_drvdata(dev);
	int ret = 0;

	if (!clk_is_enabled(jz_aic->aic_clk))
		ret = clk_enable(jz_aic->aic_clk);

	return ret;
}
EXPORT_SYMBOL_GPL(enable_aic_clk);

int enable_i2s_clk(struct device *dev)
{
	struct jz_aic *jz_aic = dev_get_drvdata(dev);
	int ret = 0;

	if (!clk_is_enabled(jz_aic->i2s_clk))
		ret = clk_enable(jz_aic->i2s_clk);

	return ret;
}
EXPORT_SYMBOL_GPL(enable_i2s_clk);

void disable_aic_clk(struct device *dev)
{
	struct jz_aic *jz_aic = dev_get_drvdata(dev);

	if (clk_is_enabled(jz_aic->aic_clk))
		clk_disable(jz_aic->aic_clk);
}
EXPORT_SYMBOL_GPL(disable_aic_clk);

void disable_i2s_clk(struct device *dev)
{
	struct jz_aic *jz_aic = dev_get_drvdata(dev);

//	if (clk_is_enabled(jz_aic->i2s_clk))
		 clk_disable(jz_aic->i2s_clk);
}
EXPORT_SYMBOL_GPL(disable_i2s_clk);

int set_aic_work_mode(struct device *dev, unsigned int wkmd)
{
	struct jz_aic *jz_aic = dev_get_drvdata(dev);

	spin_lock(&jz_aic->lock);
	if (jz_aic->wk_md) {
		spin_unlock(&jz_aic->lock);
		pr_err("Only set one work mode at the same time\n");
		return -EPERM;
	}
	jz_aic->wk_md |= wkmd;
	spin_unlock(&jz_aic->lock);
	return 0;
}
EXPORT_SYMBOL_GPL(set_aic_work_mode);

void clear_aic_work_mode(struct device *dev, unsigned int wkmd)
{
	struct jz_aic *jz_aic = dev_get_drvdata(dev);

	spin_lock(&jz_aic->lock);
	jz_aic->wk_md &= ~wkmd;
	spin_unlock(&jz_aic->lock);
}
EXPORT_SYMBOL_GPL(clear_aic_work_mode);

static int jz_aic_add_subdevs(struct jz_aic *jz_aic, struct jz_aic_platform_data *pdata)
{
	struct jz_aic_subdev_info *subdev;
	struct platform_device *pdev;
	int i,ret = 0;

	for (i = 0; i < pdata->num_subdevs; i++) {
		subdev = &pdata->subdevs[i];

		pdev = platform_device_alloc(subdev->name, subdev->id);
		if (!pdev) {
			printk("add device(%s) error\n", subdev->name);
			continue;
		}

		pdev->dev.parent = jz_aic->dev;
		pdev->dev.platform_data = subdev->platform_data;

		ret = platform_device_add(pdev);
		if (ret) {
			printk("add device(%s) error, ret is %d \n", subdev->name, ret);
			continue;
		}
	}

	return ret;
}

static int __devinit jz_aic_probe(struct platform_device *pdev)
{
	struct jz_aic *jz_aic;
	struct resource *mem_base;
	int ret;

	jz_aic = kzalloc(sizeof(struct jz_aic), GFP_KERNEL);
	if (!jz_aic) {
		dev_err(&pdev->dev, "Failed to allocate memory for driver struct\n");
		return -ENOMEM;
	}

	jz_aic->dev = &pdev->dev;

	dev_set_drvdata(jz_aic->dev, jz_aic);
	mem_base = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem_base) {
		ret = -EBUSY;
		dev_err(&pdev->dev, "Failed to get platform mmio resource");
		goto ERR1;
	}

	jz_aic->mem = request_mem_region(mem_base->start, 0x94, pdev->name);
	if (!jz_aic->mem) {
		ret = -EBUSY;
		dev_err(&pdev->dev, "Failed to request mmio memory region\n");
		goto ERR1;
	}

	jz_aic->addr_base = ioremap_nocache(jz_aic->mem->start,
			resource_size(jz_aic->mem));
	if (!jz_aic->addr_base) {
		ret = -EBUSY;
		dev_err(&pdev->dev, "Failed to ioremap mmio memory\n");
		goto ERR2;
	}

	jz_aic->aic_clk = clk_get(&pdev->dev, "aic0");
	if (IS_ERR(jz_aic->aic_clk)) {
		ret = PTR_ERR(jz_aic->aic_clk);
		dev_err(&pdev->dev, "Failed to get clock: %d\n", ret);
		goto ERR3;
	}
//	printk("aic0.flags :%d\n",jz_aic->aic_clk->flags);

	jz_aic->i2s_clk = clk_get(&pdev->dev, "cgu_aic");
	if (IS_ERR(jz_aic->i2s_clk)) {
		ret = PTR_ERR(jz_aic->i2s_clk);
		dev_err(&pdev->dev, "Failed to get clock: %d\n", ret);
		goto ERR4;
	}
//	printk("cgu_aic.flags :%d\n",jz_aic->i2s_clk->flags);

	jz_aic_add_subdevs(jz_aic, pdev->dev.platform_data);

	spin_lock_init(&jz_aic->lock);

	return 0;

//ERR5:
	clk_put(jz_aic->i2s_clk);
ERR4:
	clk_put(jz_aic->aic_clk);
ERR3:
	platform_set_drvdata(pdev, NULL);
	iounmap(jz_aic->addr_base);
ERR2:
	release_mem_region(jz_aic->mem->start, resource_size(jz_aic->mem));
ERR1:
	kfree(jz_aic);

	return ret;
}

static int __devexit jz_aic_remove(struct platform_device *pdev)
{
	return 0;
}

struct platform_driver jz_aic_driver = {
	.probe  = jz_aic_probe,
	.remove = jz_aic_remove,
	.driver = {
		.name   = "jz-aic",
		.owner  = THIS_MODULE,
	},
};

static int __init jz_aic_init(void)
{
	return platform_driver_register(&jz_aic_driver);
}
module_init(jz_aic_init);

static void __exit jz_aic_exit(void)
{
	platform_driver_unregister(&jz_aic_driver);
}
module_exit(jz_aic_exit);

MODULE_DESCRIPTION("JZ SOC AIC core driver");
MODULE_AUTHOR("Sun Jiwei<jwsun@ingenic.cn>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:jz-aic");
