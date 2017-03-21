/*
 * mensa.c  --  SoC audio for MENSA
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/timer.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <linux/gpio.h>

#include "jz47xx-pcm.h"
#include "jz47xx-i2s.h"
#include "include/dma.h"
#include "include/jz4775aic.h"
#include "include/jz4775pdma.h"
#include "include/jz4775cpm.h"

static int mensa_hw_params(struct snd_pcm_substream *substream,
                           struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret = 0;

	/* set cpu DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai,
				  SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
				  SND_SOC_DAIFMT_CBM_CFM);
	if (ret < 0)
		return ret;

	/* set the codec system clock for DAC and ADC */
	ret = snd_soc_dai_set_sysclk(codec_dai, 0, JZ47XX_I2S_SYSCLK,
				     SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;
	/* we do not have to set clkdiv for internal codec */

#if 0
	/* set codec DAI configuration */
#if 0
	ret = codec_dai->ops->set_fmt(codec_dai, SND_SOC_DAIFMT_I2S |
		SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;
#endif

	/* set cpu DAI configuration */
	ret = cpu_dai->ops->set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S |
		SND_SOC_DAIFMT_NB_NF | SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	/* set the codec system clock for DAC and ADC */
	ret = codec_dai->ops->set_sysclk(codec_dai, 0, 111,
		SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;

	/* set the I2S system clock as input (unused) */
	ret = cpu_dai->ops->set_sysclk(cpu_dai, JZ47XX_I2S_SYSCLK, 0,
		SND_SOC_CLOCK_IN);
	if (ret < 0)
		return ret;
#endif

	return 0;
}

static struct snd_soc_ops mensa_ops = {
	.hw_params = mensa_hw_params,
};

/* define the scenarios */
#define MENSA_MODE_HP_BIT		BIT0
#define MENSA_MODE_LINEOUT_BIT		BIT1
#define MENSA_MODE_MIC1_BIT		BIT2
#define MENSA_MODE_MIC2_BIT		BIT3
#define MENSA_MODE_LINEBY1_BIT		BIT4
#define MENSA_MODE_LINEBY2_BIT		BIT5


#define MENSA_AUDIO_OFF			0
#define MENSA_STEREO_TO_HP       	1
#define MENSA_STEREO_TO_LINEOUT		2
#define MENSA_STEREO_TO_HP_LINEOUT	3
#define MENSA_CAPTURE_MIC1		4
#define MENSA_CAPTURE_LINEIN		5

#define MENSA_MODE_HP_MIC1		6
#define MENSA_MODE_HP_LINEIN		7
#define MENSA_MODE_HP_BYPASS_MIC1       8
#define MENSA_MODE_HP_BYPASS_LINEIN	9

#define MENSA_MODE_LO_MIC1		10
#define MENSA_MODE_LO_LINEIN		11
#define MENSA_MODE_LO_BYPASS_MIC1  	12
#define MENSA_MODE_LO_BYPASS_LINEIN	13


#define MENSA_MODE_HP_LO_MIC1		14
#define MENSA_MODE_HP_LO_LINEIN  	15
#define MENSA_MODE_HP_LO_BYPASS_MIC1	16
#define MENSA_MODE_HP_LO_BYPASS_LINEIN  17

static const char *mensa_scenarios[] = {
	"Off",
	"Headphone",
	"Lineout",
	"HP+Lineout",
	"Mic1",
	"Linein",

	"HP+Mic1",           //Mic1 record and HP replay
	"HP+Linein",         //Linein record and HP replay
	"HP~Mic1",          // Mic1   input bypass to HP output
	"HP~Linein",        // Linein input bypass to HP output


	"Lineout+Mic1",
	"Lineout+Linein",
	"Lineout~Mic1",
	"Lineout~Linein",

	"HP+LO+Mic1",
	"HP+LO+Linein",
	"HP+LO~Mic1",
	"HP+LO~Linein",

   /* we can add another route, which can recording and bypassing to HP in the sametime */
};

static const struct soc_enum mensa_scenario_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(mensa_scenarios), mensa_scenarios),
};

static int mensa_scenario;

static int mensa_get_scenario(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = mensa_scenario;
	return 0;
}

/* lineout SHUTDOWN_PIN, if your board have, you can add it here */
#define LM4890_SHUTDOWN_PIN	(32 * 6 + 14)

static int set_scenario_endpoints(struct snd_soc_codec *codec, int scenario)
{
	unsigned int mode_bits = 0;

	switch (mensa_scenario) {
	case MENSA_STEREO_TO_HP:
		mode_bits |= MENSA_MODE_HP_BIT;
		break;
	case MENSA_STEREO_TO_LINEOUT:
		mode_bits |= MENSA_MODE_LINEOUT_BIT;
		break;
	case MENSA_STEREO_TO_HP_LINEOUT:
		mode_bits |= MENSA_MODE_HP_BIT;
		mode_bits |= MENSA_MODE_LINEOUT_BIT;
		break;
	case MENSA_CAPTURE_MIC1:
		mode_bits |= MENSA_MODE_MIC1_BIT;
		break;
	case MENSA_CAPTURE_LINEIN:
		mode_bits |= MENSA_MODE_MIC1_BIT;
		mode_bits |= MENSA_MODE_MIC2_BIT;
		break;

	case MENSA_MODE_HP_MIC1:
		mode_bits |= MENSA_MODE_HP_BIT;
		mode_bits |= MENSA_MODE_MIC1_BIT;
		break;
	case MENSA_MODE_HP_LINEIN:
		mode_bits |= MENSA_MODE_HP_BIT;
		mode_bits |= MENSA_MODE_MIC1_BIT;
		mode_bits |= MENSA_MODE_MIC2_BIT;
	        break;
	case MENSA_MODE_HP_BYPASS_MIC1:
		mode_bits |= MENSA_MODE_HP_BIT;
		mode_bits |= MENSA_MODE_LINEBY1_BIT;
		break;
	case MENSA_MODE_HP_BYPASS_LINEIN:
		mode_bits |= MENSA_MODE_HP_BIT;
		mode_bits |= MENSA_MODE_LINEBY1_BIT;
		mode_bits |= MENSA_MODE_LINEBY2_BIT;
		break;

	case MENSA_MODE_LO_MIC1:
		mode_bits |= MENSA_MODE_LINEOUT_BIT;
		mode_bits |= MENSA_MODE_MIC1_BIT;
		break;
	case MENSA_MODE_LO_LINEIN:
		mode_bits |= MENSA_MODE_LINEOUT_BIT;
		mode_bits |= MENSA_MODE_MIC1_BIT;
		mode_bits |= MENSA_MODE_MIC2_BIT;
		break;
        case MENSA_MODE_LO_BYPASS_MIC1:
		mode_bits |= MENSA_MODE_LINEOUT_BIT;
		mode_bits |= MENSA_MODE_LINEBY1_BIT;
		break;
	case MENSA_MODE_LO_BYPASS_LINEIN:
		mode_bits |= MENSA_MODE_LINEOUT_BIT;
		mode_bits |= MENSA_MODE_LINEBY1_BIT;
		mode_bits |= MENSA_MODE_LINEBY2_BIT;
                break;

	case MENSA_MODE_HP_LO_MIC1:
		mode_bits |= MENSA_MODE_HP_BIT;
		mode_bits |= MENSA_MODE_LINEOUT_BIT;
		mode_bits |= MENSA_MODE_MIC1_BIT;
		break;
	case MENSA_MODE_HP_LO_LINEIN:
		mode_bits |= MENSA_MODE_HP_BIT;
		mode_bits |= MENSA_MODE_LINEOUT_BIT;
		mode_bits |= MENSA_MODE_MIC1_BIT;
		mode_bits |= MENSA_MODE_MIC2_BIT;
		break;
        case MENSA_MODE_HP_LO_BYPASS_MIC1:
		mode_bits |= MENSA_MODE_HP_BIT;
		mode_bits |= MENSA_MODE_LINEOUT_BIT;
		mode_bits |= MENSA_MODE_LINEBY1_BIT;
		break;
        case MENSA_MODE_HP_LO_BYPASS_LINEIN:
		mode_bits |= MENSA_MODE_HP_BIT;
		mode_bits |= MENSA_MODE_LINEOUT_BIT;
		mode_bits |= MENSA_MODE_LINEBY1_BIT;
		mode_bits |= MENSA_MODE_LINEBY2_BIT;
		break;
	default:
		;
	}

	if (mode_bits & MENSA_MODE_HP_BIT)
		snd_soc_dapm_enable_pin(&codec->dapm, "Headphone Jack");
	else
		snd_soc_dapm_disable_pin(&codec->dapm, "Headphone Jack");

	if (mode_bits & MENSA_MODE_LINEOUT_BIT) {
		snd_soc_dapm_enable_pin(&codec->dapm, "Lineout Jack");
		gpio_direction_output(LM4890_SHUTDOWN_PIN, 1);
	} else {
		snd_soc_dapm_disable_pin(&codec->dapm, "Lineout Jack");
                gpio_direction_output(LM4890_SHUTDOWN_PIN, 0);
	}

	if (mode_bits & MENSA_MODE_MIC1_BIT)
		snd_soc_dapm_enable_pin(&codec->dapm, "Mic1 Jack");
	else
		snd_soc_dapm_disable_pin(&codec->dapm, "Mic1 Jack");

	if (mode_bits & MENSA_MODE_MIC2_BIT)
		snd_soc_dapm_enable_pin(&codec->dapm, "Mic2 Jack");
	else
		snd_soc_dapm_disable_pin(&codec->dapm, "Mic2 Jack");

	if (mode_bits & MENSA_MODE_LINEBY1_BIT)
		snd_soc_dapm_enable_pin(&codec->dapm, "Lineby1 Jack");
	else
		snd_soc_dapm_disable_pin(&codec->dapm, "Lineby1 Jack");

	if (mode_bits & MENSA_MODE_LINEBY2_BIT)
		snd_soc_dapm_enable_pin(&codec->dapm, "Lineby2 Jack");
	else
		snd_soc_dapm_disable_pin(&codec->dapm, "Lineby2 Jack");

	snd_soc_dapm_sync(&codec->dapm);

	return 0;
}

static int mensa_set_scenario(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);

	if (mensa_scenario == ucontrol->value.integer.value[0])
		return 0;

	mensa_scenario = ucontrol->value.integer.value[0];
	set_scenario_endpoints(codec, mensa_scenario);
	return 1;
}

/* mensa machine dapm widgets */
static const struct snd_soc_dapm_widget jz_icdc_dapm_widgets[] = {
	SND_SOC_DAPM_HP("Headphone Jack", NULL),
	SND_SOC_DAPM_LINE("Lineout Jack", NULL),
	SND_SOC_DAPM_MIC("Mic1 Jack", NULL),
	SND_SOC_DAPM_MIC("Mic2 Jack", NULL),
	SND_SOC_DAPM_MIC("Lineby1 Jack", NULL),
	SND_SOC_DAPM_MIC("Lineby2 Jack", NULL),
};

/* mensa machine audio map (connections to the codec pins) */
static const struct snd_soc_dapm_route audio_map [] = {
	/* Destination Widget(sink)  <=== Path Name <=== Source Widget */

	/* headphone connected to LHPOUT/RHPOUT */
	{"Headphone Jack", NULL, "LHPOUT"},
	{"Headphone Jack", NULL, "RHPOUT"},

	{ "Lineout Jack", NULL, "LOUT" },
	{ "Lineout Jack", NULL, "ROUT" },

	/* mic is connected to MICIN (via right channel of headphone jack) */

	{"AIP1", NULL, "Mic Bias"},
	{"AIN1", NULL, "Mic Bias"},       /* no such connection, but not harm */
	{"Mic Bias", NULL, "Mic1 Jack"},

	{"AIP2", NULL, "Mic1 Jack"},
	{"AIP3", NULL, "Mic2 Jack"},

	{"LINE1BY", NULL, "Mic Bias"},
	{"Mic Bias", NULL, "Lineby1 Jack"},
	{"LINE2BY", NULL, "Lineby2 Jack"},
};

/* If you have any amplifiers, you can add its controls here */
static const struct snd_kcontrol_new jz_icdc_mensa_controls[] = {
	SOC_ENUM_EXT("Mensa Mode", mensa_scenario_enum[0],
		     mensa_get_scenario, mensa_set_scenario),
};

/*
 * MENSA for a jz_icdc as connected on jz4775 Device
 */
static int mensa_jz_icdc_init(struct snd_soc_pcm_runtime *rtd)
{
	int err;
	struct snd_soc_codec *codec = rtd->codec;

        gpio_request(LM4890_SHUTDOWN_PIN, "LM4890");
	/* Add mensa specific controls */
	err = snd_soc_add_controls(codec, jz_icdc_mensa_controls,
				   ARRAY_SIZE(jz_icdc_mensa_controls));
	if (err < 0)
		return err;

	/* Add mensa specific widgets */
	snd_soc_dapm_new_controls(&codec->dapm, jz_icdc_dapm_widgets, ARRAY_SIZE(jz_icdc_dapm_widgets));

	snd_soc_dapm_add_routes(&codec->dapm, audio_map, ARRAY_SIZE(audio_map));

	snd_soc_dapm_sync(&codec->dapm);

	/* set endpoints to default mode */
	set_scenario_endpoints(codec, MENSA_AUDIO_OFF);

	return 0;
}

/* mensa digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link mensa_dai = {
	.name = "JZ_ICDC",
	.stream_name = "JZ_ICDC",
	.cpu_dai_name = "jz47xx-i2s",
	.platform_name = "jz47xx-pcm-audio",
	.codec_dai_name = "jz4775-hifi",
	.codec_name = "jz4775-codec",

	.init = mensa_jz_icdc_init,
	.ops = &mensa_ops,
};

/* mensa audio machine driver */
static struct snd_soc_card snd_soc_card_mensa = {
	.name = "Mensa",      /* used by state file, use aplay -l or arecord -l will see this name */
	.dai_link = &mensa_dai,
	.num_links = 1,	      /* ARRAY_SIZE(mensa_dai), though mensa_dai is not an array *n* */
};

static struct platform_device *mensa_snd_device;

static int __init mensa_init(void)
{
	int ret;

	mensa_snd_device = platform_device_alloc("soc-audio", -1);

	if (!mensa_snd_device){
		printk("platform_device_alloc error\n");
		return -ENOMEM;
	}

	platform_set_drvdata(mensa_snd_device, &snd_soc_card_mensa);
	ret = platform_device_add(mensa_snd_device);

	if (ret){
		printk("platform_device_add error\n");
		platform_device_put(mensa_snd_device);
	}
	return ret;
}

module_init(mensa_init);

static void __exit mensa_exit(void)
{
	platform_device_unregister(mensa_snd_device);
}

module_exit(mensa_exit);

/* Module information */
MODULE_AUTHOR("Lutts Wolf <slcao@ingenic.cn>");
MODULE_DESCRIPTION("ALSA SoC Internel Codec Mensa");
MODULE_LICENSE("GPL");
