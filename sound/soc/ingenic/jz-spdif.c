/* sound/soc/ingenic/jz-spdif.c
 *
 * ALSA SoC Audio Layer - Ingenic S/PDIF Controller driver
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
#include <linux/delay.h>

#include <sound/soc.h>
#include <sound/soc-dai.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <sound/jz-aic.h>

#include "jz-aic-dma.h"

static int jz_spdif_debug = 0;

module_param(jz_spdif_debug, int, 0644);
#define SPDIF_DEBUG_MSG(msg...)			\
	do {					\
		if (jz_spdif_debug)		\
			printk("SPDIF: " msg);	\
	} while(0)

static struct device *parent = NULL;
static int is_playing = 0;

static struct jz_dma_client jz_dma_client_out = {
	.name = "SPDIF PCM Stereo out"
};

static struct jz_pcm_dma_params jz_spdif_stereo_out = {
	.client		= &jz_dma_client_out,
	.channel	= -1,
	.dma_addr	= 0,
	.dma_size	= 2,
};

static int set_sample_bit(struct device *parent, unsigned int smp_wl,
		unsigned int max_wl)
{
	unsigned int reg_tmp = 0;

	reg_tmp = jz_aic_read_reg(parent, SPCFG2);
	reg_tmp &= ~(SPCFG2_SAMPL_WL_MASK);

	if (max_wl == 0) {
		__spdif_set_max_wl(parent, 0);

		switch(smp_wl) {
			case 16:
				reg_tmp |= (SPCFG2_SAMPL_WL_16BIT);
				break;
			case 17:
				reg_tmp |= (SPCFG2_SAMPL_WL_17BIT);
				break;
			case 18:
				reg_tmp |= (SPCFG2_SAMPL_WL_18BIT);
				break;
			case 19:
				reg_tmp |= (SPCFG2_SAMPL_WL_19BIT);
				break;
			case 20:
				reg_tmp |= (SPCFG2_SAMPL_WL_20BITL);
				break;
			default:
				pr_err("SPDIF: Don't support this sample"
						"word length(%d)\n", smp_wl);
				return -EINVAL;
		}
	} else {
		__spdif_set_max_wl(parent, 1);

		switch(smp_wl) {
			case 20:
				reg_tmp |= (SPCFG2_SAMPL_WL_20BITM);
				break;
			case 21:
				reg_tmp |= (SPCFG2_SAMPL_WL_21BIT);
				break;
			case 22:
				reg_tmp |= (SPCFG2_SAMPL_WL_22BIT);
				break;
			case 23:
				reg_tmp |= (SPCFG2_SAMPL_WL_23BIT);
				break;
			case 24:
				reg_tmp |= (SPCFG2_SAMPL_WL_24BIT);
				break;
			default:
				pr_err("SPDIF: Don't support this sample"
						"word length(%d)\n", smp_wl);
				return -EINVAL;
		}
	}
	jz_aic_write_reg(parent, SPCFG2, reg_tmp);

	return 0;
}

static void calculate_rate(unsigned long cpu_rate, unsigned long sampling_clk,
		unsigned int *i2s_dv, unsigned int *i2scdr)
{
	/*i2s_dv <=511, i2scdr <=255*/
	unsigned long tmp1, i2s_dv_tmp=0, i2scdr_tmp=0;
	unsigned long sampling_clk_tmp, divation_tmp;
	unsigned int divation = sampling_clk;
	tmp1 = cpu_rate / (sampling_clk * 64);

	for (i2scdr_tmp = 0;i2scdr_tmp < 255; i2scdr_tmp++) {
		i2s_dv_tmp = tmp1/(i2scdr_tmp+1) - 1;
		if ((i2s_dv_tmp >= 511) ||
				(100000000 <= (cpu_rate / (i2scdr_tmp + 1))))
			continue;
		if (i2s_dv_tmp == 0)
			break;
		sampling_clk_tmp = cpu_rate / ((i2scdr_tmp+1)*(i2s_dv_tmp+1)*64);
		if (sampling_clk_tmp == sampling_clk) {
			*i2s_dv = i2s_dv_tmp;
			*i2scdr = i2scdr_tmp;
			return;
		}
		if (sampling_clk < sampling_clk_tmp) {
			divation_tmp = sampling_clk_tmp - (sampling_clk);
		} else {
			divation_tmp = (sampling_clk) - sampling_clk_tmp;
		}
		if (divation_tmp < divation) {
			*i2s_dv = i2s_dv_tmp;
			*i2scdr = i2scdr_tmp;
			divation = divation_tmp;
		}
	}
	for (i2s_dv_tmp = 0; i2s_dv_tmp < 511; i2s_dv_tmp++) {
		i2scdr_tmp = tmp1/(i2s_dv_tmp);
		if ((i2scdr_tmp >= 255) ||
				((cpu_rate / (i2scdr_tmp + 1) >= 100000000)))
			continue;
		if (i2scdr_tmp == 0)
			break;
		sampling_clk_tmp =
			cpu_rate / ((i2scdr_tmp+1)*(i2s_dv_tmp+1)*64);
		if (sampling_clk_tmp == sampling_clk) {
			*i2s_dv = i2s_dv_tmp;
			*i2scdr = i2scdr_tmp;
			return;
		}
		if (sampling_clk < sampling_clk_tmp) {
			divation_tmp = sampling_clk_tmp - (sampling_clk);
		} else {
			divation_tmp = sampling_clk - sampling_clk_tmp;
		}
		if (divation_tmp < divation) {
			*i2s_dv = i2s_dv_tmp;
			*i2scdr = i2scdr_tmp;
			divation = divation_tmp;
		}
	}
}

/***************************************************************
 *  Use codec slave mode clock rate list
 *  We do not hope change EPLL,so we use 270MHz (fix) epllclk
 *  for minimum error
 *  270M ---    M:203 N:9 OD:1
 *       rate    spdifdr         cguclk          spdifdv.div    samplerate/error
 *      |192000 |1              |135.335M       |10                     |+0.12%
 *      |96000  |3              |67.6675M       |10                     |+0.12%
 *      |48000  |7              |33.83375M      |10                     |-0.11%
 *      |44100  |7              |33.83375M      |11                     |-0.10%
 *      |32000  |11             |22.555833M     |10                     |+0.12%
 *      |24000  |15             |16.916875M     |10                     |+0.12%
 *      |22050  |15             |16.916875M     |11                     |-0.12%
 *      |16000  |23             |11.277916M     |10                     |+0.12%
 *      |12000  |31             |8.458437M      |10                     |+0.12%
 *      |11025  |31             |8.458437M      |11                     |-0.10%
 *      |8000   |47             |5.523877M      |10                     |+0.12%
 *      HDMI:
 *      sysclk 11.2896M (theoretical)
 *      spdifdr  23
 *      cguclk 11.277916M (practical)
 *      error  -0.10%
 ***************************************************************/
static unsigned long calculate_cgu_spdif_rate(unsigned int *rate)
{
#if 0
	int i;
	unsigned long mrate[10] = {
		8000, 11025,16000,22050,24000,
		32000,44100,48000,96000,192000,
	};
	unsigned long mcguclk[10] = {
		8192000, 11333338, 8192000, 11333338, 12288000,
		8192000, 11333338, 12288000,12288000, 25500000,
	};
	for (i=0; i<10; i++) {
		if (*rate <= mrate[i]) {
			*rate = mrate[i];
			break;
		}
	}
	if (i >= 10) {
		*rate = 44100; /*unsupport rate use default*/
		return mcguclk[6];
	}

	return mcguclk[i];
#else
       unsigned long i2s_parent_clk;
       unsigned int i2s_dv=0, i2scdr=0;
       i2s_parent_clk = get_i2s_clk_parent_rate(parent);
       if (i2s_parent_clk < (*rate * 64)) {
               pr_err("SPDIF:i2s parent clk rate is error, "
                               "please chose right\n");
               return i2s_parent_clk;
       }
       calculate_rate(i2s_parent_clk, (*rate), &i2s_dv, &i2scdr);
       *rate =i2s_parent_clk / ((i2scdr + 1) * (i2s_dv + 1) * 64);
       if (jz_spdif_debug)
	       printk("i2scdr is 0x%08x, i2s_dv is 0x%08x, i2s_clk is %ld,"
                       "and calculate smp rate is %d\n",
                       i2scdr, i2s_dv, i2s_parent_clk/(i2scdr + 1),
                       *rate);

       return i2s_parent_clk / (i2scdr + 1);
#endif
}


static int set_smp_freq(struct device *parent, unsigned int rate)
{
	unsigned long cgu_spdif_clk = 0;
	unsigned int fs_tmp, org_frq_tmp;

	switch(rate) {
	case 44100:
		fs_tmp = 0;
		org_frq_tmp = 0xf;
		break;
	case 48000:
		fs_tmp = 0x2;
		org_frq_tmp = 0xd;
		break;
	case 32000:
		fs_tmp = 0x3;
		org_frq_tmp = 0xc;
		break;
	case 96000:
		fs_tmp = 0xa;
		org_frq_tmp = 0x5;
		break;
	case 192000:
		fs_tmp = 0xe;
		org_frq_tmp = 0x1;
		break;
	default:
		pr_err("SPDIF:Invalid sampling rate %d\n", rate);
		return -EINVAL;
	}

	cgu_spdif_clk = calculate_cgu_spdif_rate(&rate);

	if (get_i2s_clk_rate(parent) != cgu_spdif_clk * 2)
		set_i2s_clk_rate(parent, cgu_spdif_clk * 2);
	if (get_i2s_clk_rate(parent) > cgu_spdif_clk * 2) {
		printk("external codec set rate fail.\n");
		return -EINVAL;
	}

	__i2s_set_sample_rate(parent, cgu_spdif_clk, rate);

	__spdif_set_ori_sample_freq(parent, org_frq_tmp);

	__spdif_set_sample_freq(parent, fs_tmp);

	return 0;
}

static void spdif_set_i2sdeinit(struct device *parent)
{
	__i2s_disable_transmit_dma(parent);
	__i2s_disable_receive_dma(parent);
	__i2s_disable_replay(parent);
	__i2s_disable_record(parent);
	__i2s_disable_loopback(parent);
#if 0
	__i2s_enable_sysclk_output(parent);
	__i2s_play_lastsample(parent);
	__i2s_set_transmit_trigger(parent, 4);
	__i2s_set_receive_trigger(parent, 3);
	/*__i2s_disable_sysclk_output(parent);*/
#endif
	__i2s_disable(parent);
}

static inline void spdif_set_transmit_trigger(int n)
{
	unsigned int div;
	div = n / 4;
	div -= 1;
	if (div > 1 && div < 7)
		div = 2;
	else if(div >= 7)
		div = 3;
	else if (div < 0)
		div = 0;

	__spdif_set_trigger(parent, div);
}

static void spdif_replay_init(struct device *parent)
{
	__i2s_stop_bitclk(parent);
	__i2s_external_codec(parent);
	__i2s_bclk_output(parent);
	__i2s_sync_output(parent);
	__aic_select_i2s(parent);
	__i2s_send_rfirst(parent);

	__spdif_set_dtype(parent, 0);
	__spdif_set_ch1num(parent, 0);
	__spdif_set_ch2num(parent, 1);
	__spdif_set_srcnum(parent, 0);

	/*select spdif trans*/
	__interface_select_spdif(parent);
	__spdif_play_lastsample(parent);
	__spdif_init_set_low(parent);
	__spdif_choose_consumer(parent);
	__spdif_clear_audion(parent);
	__spdif_set_copyn(parent);
	__spdif_clear_pre(parent);
	__spdif_choose_chmd(parent);
	__spdif_set_category_code_normal(parent);
	__spdif_set_clkacu(parent, 0);
	__spdif_set_sample_size(parent, 1);
	__spdif_set_valid(parent);
	__spdif_mask_trig(parent);
	__spdif_disable_underrun_intr(parent);
}

static int jz_spdif_startup(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	int ret = 0;
	unsigned int reg_tmp = 0;

	SPDIF_DEBUG_MSG("enter %s, startup spdif\n", __func__);
	ret = set_aic_work_mode(parent, AIC_WORK_SPDIF);
	if (ret) {
		pr_err("SPDIF:Cann't startup spdif, "
				"maybe the AIC is used by else device!\n");
		return ret;
	}

	ret = enable_aic_clk(parent);
	if (ret) {
		pr_err("SPDIF:Cann't enable aic clock\n");
		return ret;
	}

	spdif_set_i2sdeinit(parent);

	reg_tmp = jz_aic_read_reg(parent, SPCTRL);
	reg_tmp |= (SPCTRL_SPDIF_I2S_MASK | SPCTRL_M_TRIG_MASK |
			SPCTRL_M_FFUR_MASK | SPCTRL_INVALID_MASK);
	jz_aic_write_reg(parent, SPCTRL, reg_tmp);

	spdif_replay_init(parent);

	return 0;
}

/*
* Set Jz Clock source
*/
static int jz_spdif_set_dai_sysclk(struct snd_soc_dai *cpu_dai,
		int clk_id, unsigned int freq, int dir)
{
	return 0;
}

static int jz_snd_tx_ctrl(int on)
{
	unsigned int time_t = 0xfffff;

	SPDIF_DEBUG_MSG("enter %s, on:%d\n", __func__, on);

	if (on) {
		is_playing = 1;

		enable_i2s_clk(parent);
		if (jz_spdif_debug)
			printk("\n\n%s %d, I2SDIV:0x%08x\n", __func__, __LINE__,
				*((volatile unsigned int *)0xb0000060));

		__i2s_start_bitclk(parent);

		/*rest spdif*/
		__spdif_reset(parent);
		while ((jz_aic_read_reg(parent, SPCTRL) & SPCTRL_SFT_RST_MASK)
				&& (time_t--)) {
			if (time_t == 0) {
				pr_err("SPDIF: reset spdif timeout\n");
				return -ETIMEDOUT;
			}
		}

		spdif_set_transmit_trigger(32);
		__spdif_clear_underrun(parent);
		__spdif_enable_transmit_dma(parent);
		__spdif_enable(parent);
	} else if (is_playing) {
		is_playing = 0;

		jz_aic_write_reg(parent, SPFIFO, 0x0);
		jz_aic_write_reg(parent, SPFIFO, 0x0);
		while (!(jz_aic_read_reg(parent, SPSTATE) & SPSTATE_F_FFUR_MASK));

		__spdif_disable_transmit_dma(parent);
		__i2s_disable_replay(parent);

		__spdif_disable(parent);
	}

	if(jz_spdif_debug)
		jz_aic_dump_reg(parent);

	return 0;
}

static int jz_spdif_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0;

	SPDIF_DEBUG_MSG("enter %s, spdif set hw params\n", __func__);
	SPDIF_DEBUG_MSG("hw params:format(0x%x),rate(%d)\n",
			params_format(params), params_rate(params));

	if (substream->stream != SNDRV_PCM_STREAM_PLAYBACK) {
		pr_err("SPDIF:%s, Don't support capture\n", __func__);
		return -EINVAL;
	}

	jz_spdif_stereo_out.dma_addr = jz_aic_get_fifo_addr(parent, SPFIFO);
	if (!jz_spdif_stereo_out.dma_addr) {
		pr_warn("SPDIF: the dma_addr is 0\n");
		return -EINVAL;
	}

	cpu_dai->playback_dma_data = &jz_spdif_stereo_out;

#ifdef CONFIG_SUPPORT_AIC_MSB
	__aic_disable_msb(parent);
#endif /* CONFIG_SUPPORT_AIC_MSB */

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		__spdif_clear_signn(parent);
		ret = set_sample_bit(parent, 16, 0);
		break;
	case SNDRV_PCM_FORMAT_U16_LE:
		__spdif_set_signn(parent);
		ret = set_sample_bit(parent, 16, 0);
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		__spdif_clear_signn(parent);
		ret = set_sample_bit(parent, 24, 0);
		break;
	case SNDRV_PCM_FORMAT_U24_LE:
		__spdif_set_signn(parent);
		ret = set_sample_bit(parent, 24, 0);
		break;
#ifdef CONFIG_SUPPORT_AIC_MSB
	case SNDRV_PCM_FORMAT_S16_BE:
		__aic_enable_msb(parent);

		__spdif_clear_signn(parent);
		ret = set_sample_bit(parent, 16, 1);
		break;
	case SNDRV_PCM_FORMAT_U16_BE:
		__aic_enable_msb(parent);

		__spdif_set_signn(parent);
		ret = set_sample_bit(parent, 16, 1);
		break;
	case SNDRV_PCM_FORMAT_S24_BE:
		__aic_enable_msb(parent);

		__spdif_clear_signn(parent);
		ret = set_sample_bit(parent, 24, 1);
		break;
	case SNDRV_PCM_FORMAT_U24_BE:
		__aic_enable_msb(parent);

		__spdif_set_signn(parent);
		ret = set_sample_bit(parent, 24, 1);
		break;
#endif /* CONFIG_SUPPORT_AIC_MSB */
	default:
		pr_err("SPDIF: Don't support the sample formats(%d)\n",
				params_format(params));
		return -EINVAL;
	}
	if (ret < 0) {
		pr_err("SPDIF: Don't support the sample bit\n");
		return ret;
	}

	ret = set_smp_freq(parent, params_rate(params));

	return ret;
}

static int jz_spdif_trigger(struct snd_pcm_substream *substream, int cmd,
		struct snd_soc_dai *dai)
{
	int ret = 0;

	SPDIF_DEBUG_MSG("enter %s, spdif trigger, cmd is %d \n", __func__, cmd);

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		pr_err("SPDIF:%s, Don't support capture\n", __func__);
		return -EINVAL;
	}

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		ret = jz_snd_tx_ctrl(1);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		ret = jz_snd_tx_ctrl(0);
		break;
	default:
		pr_err("SPDIF:%s, Don't support the cmd(%d)\n", __func__, cmd);
		ret = -EINVAL;
	}

	return ret;
}

static void jz_spdif_shutdown(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	SPDIF_DEBUG_MSG("enter %s, close spdif\n", __func__);

	__i2s_stop_bitclk(parent);
	disable_aic_clk(parent);
	disable_i2s_clk(parent);

	clear_aic_work_mode(parent, AIC_WORK_SPDIF);
}

static int jz_spdif_probe(struct snd_soc_dai *dai)
{
	__spdif_disable_transmit_dma(parent);
	__spdif_disable(parent);
	return 0;
}

static struct snd_soc_dai_ops jz_spdif_dai_ops = {
	.startup 	= jz_spdif_startup,
	.shutdown 	= jz_spdif_shutdown,
	.trigger 	= jz_spdif_trigger,
	.hw_params 	= jz_spdif_hw_params,
	.set_sysclk 	= jz_spdif_set_dai_sysclk,
};

#ifdef CONFIG_PM
static int jz_spdif_suspend(struct snd_soc_dai *dai)
{
	return 0;
}

static int jz_spdif_resume(struct snd_soc_dai *dai)
{
	return 0;
}
#else
#define jz_spdif_suspend	NULL
#define jz_spdif_resume		NULL
#endif

#ifdef CONFIG_SUPPORT_AIC_MSB
#define JZ_SPDIF_FORMATS (SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_U24_LE \
		| SNDRV_PCM_FMTBIT_S24_BE | SNDRV_PCM_FMTBIT_U24_BE	\
		| SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_U16_LE \
		| SNDRV_PCM_FMTBIT_S16_BE | SNDRV_PCM_FMTBIT_U16_BE \
		| SNDRV_PCM_FMTBIT_S8 )
#else
#define JZ_SPDIF_FORMATS (SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_U24_LE \
		| SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_U16_LE )
//		| SNDRV_PCM_FMTBIT_S8 )
#endif
#define JZ_SPDIF_RATES (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_11025 |	\
		SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_16000 | \
		SNDRV_PCM_RATE_22050 | SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_96000 | \
		SNDRV_PCM_RATE_192000 )


static struct snd_soc_dai_driver jz_spdif_dai = {
	.probe   = jz_spdif_probe,
	.suspend = jz_spdif_suspend,
	.resume  = jz_spdif_resume,
	.playback = {
		.channels_min = 2,
		.channels_max = 2,
		.rates = JZ_SPDIF_RATES,
		.formats = JZ_SPDIF_FORMATS,
	},
	.ops = &jz_spdif_dai_ops,
};

static  __devinit int  jz_spdif_dev_probe(struct platform_device *pdev)
{
	parent = pdev->dev.parent;
	return snd_soc_register_dai(&pdev->dev, &jz_spdif_dai);
}

static __devexit int jz_spdif_dev_remove(struct platform_device *pdev)
{
	snd_soc_unregister_dai(&pdev->dev);
	parent = NULL;
	return 0;
}

static struct platform_driver jz_spdif_plat_driver = {
	.probe  = jz_spdif_dev_probe,
	.remove = jz_spdif_dev_remove,
	.driver = {
		.name = "jz-aic-spdif",
		.owner = THIS_MODULE,
	},
};

static int __init jz_spdif_init(void)
{
	return platform_driver_register(&jz_spdif_plat_driver);
}
module_init(jz_spdif_init);

static void __exit jz_spdif_exit(void)
{
	platform_driver_unregister(&jz_spdif_plat_driver);
}
module_exit(jz_spdif_exit);

MODULE_AUTHOR("Sun Jiwei<jwsun@ingenic.cn>");
MODULE_DESCRIPTION("jz SPDIF SoC Interface");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:jz-spdif");
