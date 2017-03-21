/* sound/soc/ingenic/mensa_spdif.c
 * mensa_spdif.c  --  S/PDIF audio driver for MENSA
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

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <sound/soc.h>

static int mensa_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	return 0;
}

static struct snd_soc_ops mensa_spdif_ops = {
	.hw_params = mensa_hw_params,
};

static struct snd_soc_dai_link mensa_dai = {
	.name = "S/PDIF",
	.stream_name = "S/PDIF PCM Playback",
	.platform_name = "jz-audio-dma",
	.cpu_dai_name = "jz-aic-spdif",
	.codec_dai_name = "dit-hifi",
	.codec_name = "spdif-dit",
	.ops = &mensa_spdif_ops,
};

static struct snd_soc_card mensa = {
	.name = "MENSA-S/PDIF",
	.dai_link = &mensa_dai,
	.num_links = 1,
};

static struct platform_device *mensa_snd_spdif_dit_device;
static struct platform_device *mensa_snd_spdif_device;

static int __init mensa_init(void)
{
	int ret;

	mensa_snd_spdif_dit_device = platform_device_alloc("spdif-dit", -1);
	if (!mensa_snd_spdif_dit_device)
		return -ENOMEM;

	ret = platform_device_add(mensa_snd_spdif_dit_device);
	if (ret)
		goto err1;

	mensa_snd_spdif_device = platform_device_alloc("soc-audio", -1);
	if (!mensa_snd_spdif_device) {
		ret = -ENOMEM;
		goto err2;
	}

	platform_set_drvdata(mensa_snd_spdif_device, &mensa);

	ret = platform_device_add(mensa_snd_spdif_device);
	if (ret)
		goto err3;

	/* Set audio clock hierarchy manually */

	return 0;

err3:
	platform_device_put(mensa_snd_spdif_device);
err2:
	platform_device_del(mensa_snd_spdif_dit_device);
err1:
	platform_device_put(mensa_snd_spdif_dit_device);
	return ret;
}

static void __exit mensa_exit(void)
{
	platform_device_unregister(mensa_snd_spdif_device);
	platform_device_unregister(mensa_snd_spdif_dit_device);
}

module_init(mensa_init);
module_exit(mensa_exit);

MODULE_AUTHOR("Sun Jiwei<jwsun@ingenic.cn>");
MODULE_DESCRIPTION("ALSA SoC MENSA+S/PDIF");
MODULE_LICENSE("GPL");
