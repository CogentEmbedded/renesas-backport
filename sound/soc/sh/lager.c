/*
 * sound/soc/sh/lager.c
 *     This file is ALSA SoC driver for LAGER.
 *
 * Copyright (C) 2013 Renesas Electronics Corporation
 *
 * This file is based on the sound/soc/sh/migor.c
 *
 * ALSA SoC driver for Migo-R
 *
 * Copyright (C) 2009-2010 Guennadi Liakhovetski <g.liakhovetski@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/device.h>
#include <linux/module.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/sh_scu.h>

#undef DEBUG
#ifdef DEBUG
#define FNC_ENTRY	pr_info("entry:%s:%d\n", __func__, __LINE__);
#define FNC_EXIT	pr_info("exit:%s:%d\n", __func__, __LINE__);
#define DBG_POINT()	pr_info("check:%s:%d\n", __func__, __LINE__);
#define DBG_MSG(args...)	pr_info(args)
#else  /* DEBUG */
#define FNC_ENTRY
#define FNC_EXIT
#define DBG_POINT()
#define DBG_MSG(args...)
#endif /* DEBUG */

static struct scu_route_info *routeinfo;

int scu_check_route(int dir, struct scu_route_info *routeinfo)
{
	if (!dir) { /* playback */
		if (routeinfo->p_route != RP_MEM_SSI0 &&
		    routeinfo->p_route != RP_MEM_SRC0_SSI0 &&
		    routeinfo->p_route != RP_MEM_SRC0_DVC0_SSI0) {
			pr_info("scu playback route is invalid.\n");
			return -EPERM;
		}
	} else { /* capture */
		if (routeinfo->c_route != RC_SSI1_MEM &&
		    routeinfo->c_route != RC_SSI1_SRC1_MEM &&
		    routeinfo->c_route != RC_SSI1_SRC1_DVC1_MEM) {
			pr_info("scu capture route is invalid.\n");
			return -EPERM;
		}
	}

	return 0;
}
EXPORT_SYMBOL(scu_check_route);

/************************************************************************

	DAPM

************************************************************************/
#undef EV_PRINT
#ifdef EV_PRINT
static void event_print(int event, char *evt_str)
{
	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		printk(KERN_INFO "%s SND_SOC_DAPM_PRE_PMU\n", evt_str);
		break;
	case SND_SOC_DAPM_POST_PMU:
		printk(KERN_INFO "%s SND_SOC_DAPM_POST_PMU\n", evt_str);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		printk(KERN_INFO "%s SND_SOC_DAPM_PRE_PMD\n", evt_str);
		break;
	case SND_SOC_DAPM_POST_PMD:
		printk(KERN_INFO "%s SND_SOC_DAPM_POST_PMD\n", evt_str);
		break;
	default:
		printk(KERN_INFO "%s unknown event\n", evt_str);
	}
}
#else
#define event_print(a, b)
#endif

static int event_ssi0(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	event_print(event, "ssi0");
	if (event == SND_SOC_DAPM_POST_PMU) {
		routeinfo->pcb.init_ssi = scu_init_ssi_ind_master;
		routeinfo->pcb.deinit_ssi = scu_deinit_ssi_ind_master;
	} else if (event == SND_SOC_DAPM_PRE_PMD) {
		routeinfo->pcb.init_ssi = NULL;
		routeinfo->pcb.deinit_ssi = NULL;
	}
	return 0;
}

static int event_ssi0_src0(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	event_print(event, "ssi0_src0");
	if (event == SND_SOC_DAPM_POST_PMU) {
		routeinfo->pcb.init_ssi_src = scu_init_ssi_master;
		routeinfo->pcb.deinit_ssi_src = scu_deinit_ssi_master;
	} else if (event == SND_SOC_DAPM_PRE_PMD) {
		routeinfo->pcb.init_ssi_src = NULL;
		routeinfo->pcb.deinit_ssi_src = NULL;
	}
	return 0;
}

static int event_ssi0_dvc0(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	event_print(event, "ssi0_dvc0");
	if (event == SND_SOC_DAPM_POST_PMU) {
		routeinfo->pcb.init_ssi_dvc = scu_init_ssi_master;
		routeinfo->pcb.deinit_ssi_dvc = scu_deinit_ssi_master;
	} else if (event == SND_SOC_DAPM_PRE_PMD) {
		routeinfo->pcb.init_ssi_dvc = NULL;
		routeinfo->pcb.deinit_ssi_dvc = NULL;
	}
	return 0;
}

static int event_src0(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	event_print(event, "src0");
	if (event == SND_SOC_DAPM_POST_PMU) {
		routeinfo->pcb.init_src = scu_init_src;
		routeinfo->pcb.deinit_src = scu_deinit_src;
	} else if (event == SND_SOC_DAPM_PRE_PMD) {
		routeinfo->pcb.init_src = NULL;
		routeinfo->pcb.deinit_src = NULL;
	}
	return 0;
}

static int event_dvc0(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	event_print(event, "dvc0");
	if (event == SND_SOC_DAPM_POST_PMU) {
		routeinfo->pcb.init_dvc = scu_init_dvc;
		routeinfo->pcb.deinit_dvc = scu_deinit_dvc;
	} else if (event == SND_SOC_DAPM_PRE_PMD) {
		routeinfo->pcb.init_dvc = NULL;
		routeinfo->pcb.deinit_dvc = NULL;
	}
	return 0;
}

static int event_ssi1(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	event_print(event, "ssi1");
	if (event == SND_SOC_DAPM_POST_PMU) {
		routeinfo->ccb.init_ssi = scu_init_ssi_ind_slave;
		routeinfo->ccb.deinit_ssi = scu_deinit_ssi_ind_slave;
	} else if (event == SND_SOC_DAPM_PRE_PMD) {
		routeinfo->ccb.init_ssi = NULL;
		routeinfo->ccb.deinit_ssi = NULL;
	}
	return 0;
}

static int event_ssi1_src1(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	event_print(event, "ssi1_src1");
	if (event == SND_SOC_DAPM_POST_PMU) {
		routeinfo->ccb.init_ssi_src = scu_init_ssi_slave;
		routeinfo->ccb.deinit_ssi_src = scu_deinit_ssi_slave;
	} else if (event == SND_SOC_DAPM_PRE_PMD) {
		routeinfo->ccb.init_ssi_src = NULL;
		routeinfo->ccb.deinit_ssi_src = NULL;
	}
	return 0;
}

static int event_ssi1_dvc1(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	event_print(event, "ssi1_dvc1");
	if (event == SND_SOC_DAPM_POST_PMU) {
		routeinfo->ccb.init_ssi_dvc = scu_init_ssi_slave;
		routeinfo->ccb.deinit_ssi_dvc = scu_deinit_ssi_slave;
	} else if (event == SND_SOC_DAPM_PRE_PMD) {
		routeinfo->ccb.init_ssi_dvc = NULL;
		routeinfo->ccb.deinit_ssi_dvc = NULL;
	}
	return 0;
}

static int event_src1(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	event_print(event, "src1");
	if (event == SND_SOC_DAPM_POST_PMU) {
		routeinfo->ccb.init_src = scu_init_src;
		routeinfo->ccb.deinit_src = scu_deinit_src;
	} else if (event == SND_SOC_DAPM_PRE_PMD) {
		routeinfo->ccb.init_src = NULL;
		routeinfo->ccb.deinit_src = NULL;
	}
	return 0;
}

static int event_src1_dvc1(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	event_print(event, "src1_dvc1");
	if (event == SND_SOC_DAPM_POST_PMU) {
		routeinfo->ccb.init_src_dvc = scu_init_src;
		routeinfo->ccb.deinit_src_dvc = scu_deinit_src;
	} else if (event == SND_SOC_DAPM_PRE_PMD) {
		routeinfo->ccb.init_src_dvc = NULL;
		routeinfo->ccb.deinit_src_dvc = NULL;
	}
	return 0;
}

static int event_dvc1(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	event_print(event, "dvc1");
	if (event == SND_SOC_DAPM_POST_PMU) {
		routeinfo->ccb.init_dvc = scu_init_dvc;
		routeinfo->ccb.deinit_dvc = scu_deinit_dvc;
	} else if (event == SND_SOC_DAPM_PRE_PMD) {
		routeinfo->ccb.init_dvc = NULL;
		routeinfo->ccb.deinit_dvc = NULL;
	}
	return 0;
}

static void scu_playback_route_control(struct snd_soc_dapm_context *dapm)
{
	snd_soc_dapm_disable_pin(dapm, "SSI0_OUT0");
	snd_soc_dapm_disable_pin(dapm, "SSI0_OUT1");
	snd_soc_dapm_disable_pin(dapm, "SSI0_OUT2");

	switch (routeinfo->p_route) {
	case RP_MEM_SSI0:
		snd_soc_dapm_enable_pin(dapm, "SSI0_OUT0");
		break;
	case RP_MEM_SRC0_SSI0:
		snd_soc_dapm_enable_pin(dapm, "SSI0_OUT1");
		break;
	case RP_MEM_SRC0_DVC0_SSI0:
		snd_soc_dapm_enable_pin(dapm, "SSI0_OUT2");
		break;
	default:
		break;
	};
}

static void scu_capture_route_control(struct snd_soc_dapm_context *dapm)
{
	snd_soc_dapm_disable_pin(dapm, "SSI1_IN0");
	snd_soc_dapm_disable_pin(dapm, "SSI1_IN1");
	snd_soc_dapm_disable_pin(dapm, "SSI1_IN2");

	switch (routeinfo->c_route) {
	case RC_SSI1_MEM:
		snd_soc_dapm_enable_pin(dapm, "SSI1_IN0");
		break;
	case RC_SSI1_SRC1_MEM:
		snd_soc_dapm_enable_pin(dapm, "SSI1_IN1");
		break;
	case RC_SSI1_SRC1_DVC1_MEM:
		snd_soc_dapm_enable_pin(dapm, "SSI1_IN2");
		break;
	default:
		break;
	};
}

static int scu_get_ssi0_route(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = routeinfo->route_ssi[0];

	return 0;
}

static int scu_set_ssi0_route(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);

	if (routeinfo->route_ssi[0] == ucontrol->value.integer.value[0])
		return 0;

	routeinfo->route_ssi[0] = ucontrol->value.integer.value[0];
	if (routeinfo->route_ssi[0])
		routeinfo->p_route |= W_SSI0;
	else
		routeinfo->p_route &= ~W_SSI0;
	scu_playback_route_control(&card->dapm);

	return 1;
}

static int scu_get_src0_route(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = routeinfo->route_src[0];

	return 0;
}

static int scu_set_src0_route(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);

	if (routeinfo->route_src[0] == ucontrol->value.integer.value[0])
		return 0;

	routeinfo->route_src[0] = ucontrol->value.integer.value[0];
	if (routeinfo->route_src[0])
		routeinfo->p_route |= W_SRC0;
	else
		routeinfo->p_route &= ~W_SRC0;
	scu_playback_route_control(&card->dapm);

	return 1;
}

static int scu_get_dvc0_route(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = routeinfo->route_dvc[0];

	return 0;
}

static int scu_set_dvc0_route(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);

	if (routeinfo->route_dvc[0] == ucontrol->value.integer.value[0])
		return 0;

	routeinfo->route_dvc[0] = ucontrol->value.integer.value[0];
	if (routeinfo->route_dvc[0])
		routeinfo->p_route |= W_DVC0;
	else
		routeinfo->p_route &= ~W_DVC0;
	scu_playback_route_control(&card->dapm);

	return 1;
}

static int scu_get_ssi1_route(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = routeinfo->route_ssi[1];

	return 0;
}

static int scu_set_ssi1_route(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);

	if (routeinfo->route_ssi[1] == ucontrol->value.integer.value[0])
		return 0;

	routeinfo->route_ssi[1] = ucontrol->value.integer.value[0];
	if (routeinfo->route_ssi[1])
		routeinfo->c_route |= W_SSI1;
	else
		routeinfo->c_route &= ~W_SSI1;
	scu_capture_route_control(&card->dapm);

	return 1;
}

static int scu_get_src1_route(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = routeinfo->route_src[1];

	return 0;
}

static int scu_set_src1_route(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);

	if (routeinfo->route_src[1] == ucontrol->value.integer.value[0])
		return 0;

	routeinfo->route_src[1] = ucontrol->value.integer.value[0];
	if (routeinfo->route_src[1])
		routeinfo->c_route |= W_SRC1;
	else
		routeinfo->c_route &= ~W_SRC1;
	scu_capture_route_control(&card->dapm);

	return 1;
}

static int scu_get_dvc1_route(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = routeinfo->route_dvc[1];

	return 0;
}

static int scu_set_dvc1_route(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_card *card = snd_kcontrol_chip(kcontrol);

	if (routeinfo->route_dvc[1] == ucontrol->value.integer.value[0])
		return 0;

	routeinfo->route_dvc[1] = ucontrol->value.integer.value[0];
	if (routeinfo->route_dvc[1])
		routeinfo->c_route |= W_DVC1;
	else
		routeinfo->c_route &= ~W_DVC1;
	scu_capture_route_control(&card->dapm);

	return 1;
}

static const char * const widget_switch[] = {"Off", "On"};

static const struct soc_enum widget_switch_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(widget_switch), widget_switch);

static const struct snd_kcontrol_new playback_controls[] = {
	SOC_ENUM_EXT("SSI0 Control", widget_switch_enum,
		scu_get_ssi0_route, scu_set_ssi0_route),
	SOC_ENUM_EXT("SRC0 Control", widget_switch_enum,
		scu_get_src0_route, scu_set_src0_route),
	SOC_ENUM_EXT("DVC0 Control", widget_switch_enum,
		scu_get_dvc0_route, scu_set_dvc0_route),
};

static const struct snd_kcontrol_new capture_controls[] = {
	SOC_ENUM_EXT("SSI1 Control", widget_switch_enum,
		scu_get_ssi1_route, scu_set_ssi1_route),
	SOC_ENUM_EXT("SRC1 Control", widget_switch_enum,
		scu_get_src1_route, scu_set_src1_route),
	SOC_ENUM_EXT("DVC1 Control", widget_switch_enum,
		scu_get_dvc1_route, scu_set_dvc1_route),
};

static const struct snd_soc_dapm_widget lager_dapm_widgets[] = {
	/* Playback */
	SND_SOC_DAPM_OUTPUT("SSI0_OUT0"),
	SND_SOC_DAPM_OUTPUT("SSI0_OUT1"),
	SND_SOC_DAPM_OUTPUT("SSI0_OUT2"),

	SND_SOC_DAPM_DAC("MEM_OUT", "HiFi Playback", SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_MIXER_E("SSI0", SND_SOC_NOPM, 0, 0, NULL, 0,
		event_ssi0,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_MIXER_E("SSI0_SRC0", SND_SOC_NOPM, 0, 0, NULL, 0,
		event_ssi0_src0,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_MIXER_E("SSI0_DVC0", SND_SOC_NOPM, 0, 0, NULL, 0,
		event_ssi0_dvc0,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_MIXER_E("SRC0", SND_SOC_NOPM, 0, 0, NULL, 0,
		event_src0,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_MIXER_E("DVC0", SND_SOC_NOPM, 0, 0, NULL, 0,
		event_dvc0,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),

	/* Capture */
	SND_SOC_DAPM_ADC("MEM_IN", "HiFi Capture", SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_INPUT("SSI1_IN0"),
	SND_SOC_DAPM_INPUT("SSI1_IN1"),
	SND_SOC_DAPM_INPUT("SSI1_IN2"),

	SND_SOC_DAPM_MIXER_E("SSI1", SND_SOC_NOPM, 0, 0, NULL, 0,
		event_ssi1,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_MIXER_E("SSI1_SRC1", SND_SOC_NOPM, 0, 0, NULL, 0,
		event_ssi1_src1,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_MIXER_E("SSI1_DVC1", SND_SOC_NOPM, 0, 0, NULL, 0,
		event_ssi1_dvc1,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_MIXER_E("SRC1", SND_SOC_NOPM, 0, 0, NULL, 0,
		event_src1,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_MIXER_E("SRC1_DVC1", SND_SOC_NOPM, 0, 0, NULL, 0,
		event_src1_dvc1,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_MIXER_E("DVC1", SND_SOC_NOPM, 0, 0, NULL, 0,
		event_dvc1,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
};

static const struct snd_soc_dapm_route audio_map[] = {
	/*
	 * Playback route
	 */
	/* SSI<-MEM */
	{"SSI0", NULL, "MEM_OUT"},
	{"SSI0_OUT0", NULL, "SSI0"},
	/* SSI<-SRC<-MEM */
	{"SRC0", NULL, "MEM_OUT"},
	{"SSI0_SRC0", NULL, "SRC0"},
	{"SSI0_OUT1", NULL, "SSI0_SRC0"},
	/* SSI<-DVC<-SRC<-MEM */
	{"DVC0", NULL, "SRC0"},
	{"SSI0_DVC0", NULL, "DVC0"},
	{"SSI0_OUT2", NULL, "SSI0_DVC0"},

	/*
	 * Capture route
	 */
	/* MEM<-SSI */
	{"SSI1", NULL, "SSI1_IN0"},
	{"MEM_IN", NULL, "SSI1"},
	/* MEM<-SRC<-SSI */
	{"SSI1_SRC1", NULL, "SSI1_IN1"},
	{"SRC1", NULL, "SSI1_SRC1"},
	{"MEM_IN", NULL, "SRC1"},
	/* MEM<-DVC<-SRC<-SSI */
	{"SSI1_DVC1", NULL, "SSI1_IN2"},
	{"SRC1_DVC1", NULL, "SSI1_DVC1"},
	{"DVC1", NULL, "SRC1_DVC1"},
	{"MEM_IN", NULL, "DVC1"},
};

/************************************************************************

	ALSA SoC

************************************************************************/
static int lager_hw_params(struct snd_pcm_substream *substream,
			   struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	int ret;

	/* set PLL clock */
	ret = snd_soc_dai_set_sysclk(codec_dai, 0, 11289600, SND_SOC_CLOCK_IN);
	if (ret) {
		pr_err("snd_soc_dai_set_sysclk err=%d\n", ret);
		return ret;
	}

	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_CBS_CFS |
					 SND_SOC_DAIFMT_I2S);
	if (ret) {
		pr_err("snd_soc_dai_set_fmt err=%d\n", ret);
		return ret;
	}

	return ret;
}

static int lager_hw_free(struct snd_pcm_substream *substream)
{
	return 0;
}

static struct snd_soc_ops lager_dai_ops = {
	.hw_params = lager_hw_params,
	.hw_free = lager_hw_free,
};

static int lager_dai_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	int ret;

	FNC_ENTRY
	/* Add controls */
	ret = snd_soc_add_card_controls(rtd->card, playback_controls,
					ARRAY_SIZE(playback_controls));
	if (ret) {
		pr_err("snd_soc_add_card_controls(playback) err=%d\n", ret);
		return ret;
	}

	ret = snd_soc_add_card_controls(rtd->card, capture_controls,
					ARRAY_SIZE(capture_controls));
	if (ret) {
		pr_err("snd_soc_add_card_controls(capture) err=%d\n", ret);
		return ret;
	}

	/* Add widget and route for scu */
	ret = snd_soc_dapm_new_controls(dapm, lager_dapm_widgets,
				  ARRAY_SIZE(lager_dapm_widgets));
	if (ret) {
		pr_err("snd_soc_dapm_new_controls err=%d\n", ret);
		return ret;
	}

	ret = snd_soc_dapm_add_routes(dapm, audio_map, ARRAY_SIZE(audio_map));
	if (ret) {
		pr_err("snd_soc_dapm_add_routes err=%d\n", ret);
		return ret;
	}

	scu_playback_route_control(dapm);
	scu_capture_route_control(dapm);

	FNC_EXIT
	return ret;
}

/* lager digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link lager_dai = {
	.name		= "ak4643",
	.stream_name	= "AK4643",
	.cpu_dai_name	= "scu-ssi-dai",
	.codec_dai_name	= "ak4642-hifi",
	.platform_name	= "scu-pcm-audio.0",
	.codec_name	= "ak4642-codec.2-0012",
	.ops		= &lager_dai_ops,
	.init		= lager_dai_init,
};

/* lager audio machine driver */
static struct snd_soc_card snd_soc_lager = {
	.name = "lager-ak4643",
	.owner = THIS_MODULE,
	.dai_link = &lager_dai,
	.num_links = 1,
};

static int __devinit lager_probe(struct platform_device *pdev)
{
	int ret = -ENOMEM;

	FNC_ENTRY
	routeinfo = scu_get_route_info();

	snd_soc_lager.dev = &pdev->dev;
	ret = snd_soc_register_card(&snd_soc_lager);
	if (ret)
		pr_err("Unable to register sourd card\n");

	FNC_EXIT
	return ret;
}

static struct platform_driver lager_alsa_driver = {
	.driver = {
		.name = "lager_alsa_soc_platform",
		.owner = THIS_MODULE,
	},
	.probe = lager_probe,
};

module_platform_driver(lager_alsa_driver);

MODULE_AUTHOR("Shunsuke Kataoka <shunsuke.kataoka.df@renesas.com>");
MODULE_DESCRIPTION("ALSA SoC LAGER");
MODULE_LICENSE("GPL v2");
