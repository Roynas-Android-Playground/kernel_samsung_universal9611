/*
 *  Driver for Madera CODECs on Exynos9610
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/input-event-codes.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/wakelock.h>
#include <sound/tlv.h>
#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>

#include <soc/samsung/exynos-pmu.h>
#include <sound/samsung/abox.h>
#include <sound/samsung/sec_audio_debug.h>

#include <linux/mfd/madera/core.h>
#include <linux/extcon/extcon-madera.h>
#include "../codecs/madera.h"
#include "abox/abox.h"

#include "jack_madera_sysfs_cb.h"

#define CHANGE_DEV_PRINT

#define EXYNOS_PMU_PMU_DEBUG_OFFSET		(0x0A00)

#define SYSCLK_RATE_CHECK_PARAM 8000
#define SYSCLK_RATE_48KHZ   98304000
#define SYSCLK_RATE_44_1KHZ 90316800

#define MADERA_BASECLK_48K	49152000
#define MADERA_BASECLK_44K1	45158400

#define MADERA_AMP_RATE	48000
#define MADERA_AMP_BCLK	(MADERA_AMP_RATE * 16 * 4)

#if defined(CONFIG_SND_SOC_RT5510)
#define RT5510_DAI_ID		15
#endif

#define CLK_SRC_SCLK	0
#define CLK_SRC_LRCLK	1
#define CLK_SRC_PDM		2
#define CLK_SRC_SELF	3
#define CLK_SRC_MCLK	4
#define CLK_SRC_SWIRE	5

#define CLK_SRC_DAI 0
#define CLK_SRC_CODEC 1

#define MADERA_DAI_ID			13
#define MADERA_CODEC_MAX		22
#define MADERA_AUX_MAX			2

#define RDMA_COUNT                      8
#define WDMA_COUNT                      5

#define UAIF_START			(RDMA_COUNT + WDMA_COUNT)
#define UAIF_COUNT			4
#define SIFS_START			(RDMA_COUNT + WDMA_COUNT + UAIF_COUNT + 2)
#define SIFS_COUNT			3

#define DSIF_COUNT			1
#define SPDY_START			(RDMA_COUNT + WDMA_COUNT + UAIF_COUNT + DSIF_COUNT)

#define CS47L15_MCLK1_RATE		26000000
#define CS47L15_MCLK2_RATE		32768

#define CS47L15_SYSCLK_RATE		98304000

#define MADERA_FLL1_REFCLK		1
#define MADERA_FLL1_SYNCCLK		5

#define MADERA_FLL_SRC_MCLK1		0
#define MADERA_FLL_SRC_MCLK2		1
#define MADERA_FLL_SRC_AIF1BCLK		8

#define MADERA_CLK_SYSCLK		1

#define MADERA_CLK_SRC_FLL1		0x4

static struct clk *xclkout;

struct clk_conf {
	int id;
	const char *name;
	int source;
	int rate;
	int fout;

	bool valid;
};

#define MADERA_MAX_CLOCKS 10

struct madera_drvdata {
	struct device *dev;
	struct snd_soc_codec *codec;

	struct clk_conf fll1_refclk;
	struct clk_conf fll2_refclk;
	struct clk_conf fllao_refclk;
	struct clk_conf sysclk;
	struct clk_conf asyncclk;
	struct clk_conf dspclk;
	struct clk_conf opclk;
	struct clk_conf outclk;

	struct notifier_block nb;

	int fll2_switch;

	struct clk *clk[MADERA_MAX_CLOCKS];

	int abox_vss_state;
	unsigned int pcm_state;
	struct wake_lock wake_lock;
	unsigned int wake_lock_switch;

	int uaifp_count;
	int uaifc_count;
};

static struct madera_drvdata exynos9610_drvdata;
static struct snd_soc_card exynos9610_madera;

static struct snd_soc_pcm_runtime *madera_get_rtd(struct snd_soc_card *card,
		int id)
{
	struct snd_soc_dai_link *dai_link;
	struct snd_soc_pcm_runtime *rtd = NULL;

	for (dai_link = card->dai_link;
			dai_link - card->dai_link < card->num_links;
			dai_link++) {
		if (id == dai_link->id) {
			rtd = snd_soc_get_pcm_runtime(card, dai_link->name);
			break;
		}
	}

	if (!rtd)
		rtd = snd_soc_get_pcm_runtime(card, card->dai_link[id].name);

	return rtd;
}

static int madera_hw_params(struct snd_pcm_substream *substream,
			       struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	unsigned int rate = params_rate(params);
	struct snd_soc_codec *codec;
	struct snd_soc_dai *codec_dai;
	int ret = 0;

	dev_info(card->dev, "%s\n", __func__);
	dev_info(card->dev, "%s-%d %dch, %dHz, %dbytes, %dbit\n",
			rtd->dai_link->name, substream->stream,
			params_channels(params), params_rate(params),
			params_buffer_bytes(params), params_width(params));

	rtd = snd_soc_get_pcm_runtime(card,
			card->dai_link[MADERA_DAI_ID].name);
	codec_dai = rtd->codec_dai;
	codec = rtd->codec_dai->codec;

	ret = snd_soc_codec_set_pll(codec, MADERA_FLL1_SYNCCLK,
				    MADERA_FLL_SRC_AIF1BCLK, rate * params_width(params) * 2, 0);
	if (ret)
		dev_err(card->dev, "Failed to set FLL syncclk\n");

	return ret;
}

static int madera_prepare(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;

	return snd_soc_dai_set_tristate(rtd->cpu_dai, 0);
}

static void madera_shutdown(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;

	snd_soc_dai_set_tristate(rtd->cpu_dai, 1);
}

static struct snd_soc_ops uaif0_ops = {
	.hw_params = madera_hw_params,
	.prepare = madera_prepare,
	.shutdown = madera_shutdown,
};

static int cs35l41_hw_params(struct snd_pcm_substream *substream,
			       struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct snd_soc_dai **codec_dais = rtd->codec_dais;
	unsigned int clk;
	unsigned int num_codecs = rtd->num_codecs;
	int ret = 0, i;

	/* using bclk for sysclk */
	clk = snd_soc_params_to_bclk(params);
	for (i = 0; i < num_codecs; i++) {
		ret = snd_soc_codec_set_sysclk(codec_dais[i]->codec,
					CLK_SRC_SCLK, 0, clk,
					SND_SOC_CLOCK_IN);
		if (ret < 0)
			dev_err(card->dev, "%s: set codec sysclk failed: %d\n",
					codec_dais[i]->name, ret);
	}
	return ret;
}

static const struct snd_soc_ops cs35l41_ops = {
	.hw_params = cs35l41_hw_params,
};

static int vogue_dai_ops_hw_params(struct snd_pcm_substream *substream,
			       struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct madera_drvdata *drvdata = card->drvdata;
	struct snd_soc_dai_link *dai_link = rtd->dai_link;
	int ret = 0;

	drvdata->pcm_state |= (1 << dai_link->id);

	dev_info(card->dev,
		"%s: %s-%d: 0x%x: hw_params: %dch, %dHz, %dbytes, %dbit (pcm_state: 0x%07x)\n",
		__func__, rtd->dai_link->name, substream->stream, dai_link->id,
		params_channels(params), params_rate(params),
		params_buffer_bytes(params), params_width(params),
		drvdata->pcm_state);

	return ret;
}

static int vogue_dai_ops_hw_free(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_card *card = rtd->card;
	struct madera_drvdata *drvdata = card->drvdata;
	struct snd_soc_dai_link *dai_link = rtd->dai_link;
	int ret = 0;

	drvdata->pcm_state &= (~(1 << dai_link->id));

	dev_info(card->dev, "%s: %s-%d (pcm_state: 0x%07x)\n",
			__func__, rtd->dai_link->name, substream->stream,
			drvdata->pcm_state);

	return ret;
}

static const struct snd_soc_ops uaif_ops = {
	.hw_params = vogue_dai_ops_hw_params,
	.hw_free = vogue_dai_ops_hw_free,
};

static const struct snd_soc_ops rdma_ops = {
	.hw_params = vogue_dai_ops_hw_params,
	.hw_free = vogue_dai_ops_hw_free,
};

static const struct snd_soc_ops wdma_ops = {
	.hw_params = vogue_dai_ops_hw_params,
	.hw_free = vogue_dai_ops_hw_free,
};

static int dsif_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *hw_params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int tx_slot[] = {0, 1};

	/* bclk ratio 64 for DSD64, 128 for DSD128 */
	snd_soc_dai_set_bclk_ratio(cpu_dai, 64);

	/* channel map 0 1 if left is first, 1 0 if right is first */
	snd_soc_dai_set_channel_map(cpu_dai, 2, tx_slot, 0, NULL);
	return 0;
}

static const struct snd_soc_ops dsif_ops = {
	.hw_params = dsif_hw_params,
};

static int madera_set_bias_level(struct snd_soc_card *card,
				  struct snd_soc_dapm_context *dapm,
				  enum snd_soc_bias_level level)
{
	struct snd_soc_pcm_runtime *rtd;
	struct snd_soc_dai *codec_dai;
	struct snd_soc_codec *codec;
	int ret;

	rtd = snd_soc_get_pcm_runtime(card,
			card->dai_link[MADERA_DAI_ID].name);
	codec_dai = rtd->codec_dai;
	codec = rtd->codec_dai->codec;

	if (dapm->dev != codec_dai->dev)
		return 0;

	switch (level) {
	case SND_SOC_BIAS_STANDBY:
		if (dapm->bias_level != SND_SOC_BIAS_OFF)
			break;

		codec = codec_dai->codec;
		ret = snd_soc_codec_set_pll(codec, MADERA_FLL1_REFCLK,
					    MADERA_FLL_SRC_MCLK1,
					    CS47L15_MCLK1_RATE,
					    CS47L15_SYSCLK_RATE);
		if (ret)
			dev_err(card->dev, "Failed to start FLL\n");
		break;
	default:
		break;
	}

	return 0;
}

static int madera_set_bias_level_post(struct snd_soc_card *card,
					 struct snd_soc_dapm_context *dapm,
					 enum snd_soc_bias_level level)
{
	struct snd_soc_pcm_runtime *rtd;
	struct snd_soc_dai *codec_dai;
	struct snd_soc_codec *codec;
	int ret;

	rtd = snd_soc_get_pcm_runtime(card,
			card->dai_link[MADERA_DAI_ID].name);
	codec_dai = rtd->codec_dai;
	codec = rtd->codec_dai->codec;

	if (dapm->dev != codec_dai->dev)
		return 0;

	switch (level) {
	case SND_SOC_BIAS_OFF:
		codec = codec_dai->codec;
		ret = snd_soc_codec_set_pll(codec, MADERA_FLL1_REFCLK, 0, 0, 0);
		if (ret)
			dev_err(card->dev, "Failed to stop FLL\n");

		break;
	default:
		break;
	}

	return 0;
}

void vogue_madera_hpdet_cb(unsigned int meas)
{
	struct snd_soc_card *card = &exynos9610_madera;
	int jack_det;

	if (meas == (INT_MAX / 100))
		jack_det = 0;
	else
		jack_det = 1;

	madera_jack_det = jack_det;

	dev_info(card->dev, "%s(%d) meas(%d)\n", __func__, jack_det, meas);
}

void vogue_madera_micd_cb(bool mic)
{
	struct snd_soc_card *card = &exynos9610_madera;

	madera_ear_mic = mic;
	dev_info(card->dev, "%s: madera_ear_mic = %d\n",
			__func__, madera_ear_mic);
}

static int madera_notify(struct notifier_block *nb,
				   unsigned long event, void *data)
{
	const struct madera_hpdet_notify_data *hp_inf;
	const struct madera_micdet_notify_data *md_inf;
	const struct madera_voice_trigger_info *vt_inf;
	const struct madera_drvdata *drvdata =
		container_of(nb, struct madera_drvdata, nb);

	switch (event) {
	case MADERA_NOTIFY_VOICE_TRIGGER:
		vt_inf = data;
		dev_info(drvdata->dev, "Voice Triggered (core_num=%d)\n",
			 vt_inf->core_num);
		break;
	case MADERA_NOTIFY_HPDET:
		hp_inf = data;
		vogue_madera_hpdet_cb(hp_inf->impedance_x100 / 100);
		dev_info(drvdata->dev, "HPDET val=%d.%02d ohms\n",
			 hp_inf->impedance_x100 / 100,
			 hp_inf->impedance_x100 % 100);
		break;
	case MADERA_NOTIFY_MICDET:
		md_inf = data;
		vogue_madera_micd_cb(md_inf->present);
		dev_info(drvdata->dev, "MICDET present=%c val=%d.%02d ohms\n",
			 md_inf->present ? 'Y' : 'N',
			 md_inf->impedance_x100 / 100,
			 md_inf->impedance_x100 % 100);
		break;
	default:
		dev_info(drvdata->dev, "notifier event=0x%lx data=0x%p\n",
			 event, data);
		break;
	}

	return NOTIFY_DONE;
}

static int madera_put_impedance_volsw(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct snd_soc_card *card = codec->component.card;
	struct madera_drvdata *drvdata = card->drvdata;
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int shift = mc->shift;
	int max = mc->max;
	unsigned int mask = (1 << fls(max)) - 1;
	unsigned int invert = mc->invert;
	int err;
	unsigned int val, val_mask;

	val = (unsigned int) (ucontrol->value.integer.value[0] & mask);

	if (invert)
		val = max - val;
	val_mask = mask << shift;
	val = val << shift;

	err = snd_soc_update_bits(codec, reg, val_mask, val);
	if (err < 0) {
		dev_err(drvdata->dev,
				"impedance volume update err %d\n", err);
		return err;
	}

	return err;
}

static int exynos9610_late_probe(struct snd_soc_card *card)
{
	struct snd_soc_dai *aif1_dai;
	struct snd_soc_codec *codec;
	struct snd_soc_component *cpu;
	struct snd_soc_dapm_context *dapm;
	char name[SZ_32];
	const char *prefix;
	int i;
	struct madera_drvdata *drvdata = snd_soc_card_get_drvdata(card);
#if defined(CONFIG_SND_SOC_RT5510)
	struct snd_soc_codec *spk_amp;
	struct snd_soc_dai *aif2_dai;
#endif

	dev_info(card->dev, "%s\n", __func__);

	aif1_dai = madera_get_rtd(card, 0)->cpu_dai;
	cpu = aif1_dai->component;

	aif1_dai = madera_get_rtd(card, MADERA_DAI_ID)->codec_dai;
	codec = aif1_dai->codec;

#if defined(CONFIG_SND_SOC_RT5510)
	aif2_dai = madera_get_rtd(card, RT5510_DAI_ID)->codec_dai;
	spk_amp = aif2_dai->codec;
#endif

	/* close codec device immediately when pcm is closed */
	snd_soc_dapm_ignore_suspend(&card->dapm, "VOUTPUT");
	snd_soc_dapm_ignore_suspend(&card->dapm, "VINPUT1");
	snd_soc_dapm_ignore_suspend(&card->dapm, "VINPUT2");
	snd_soc_dapm_ignore_suspend(&card->dapm, "VOUTPUTCALL");
	snd_soc_dapm_ignore_suspend(&card->dapm, "VINPUTCALL");
	snd_soc_dapm_ignore_suspend(&card->dapm, "HEADSETMIC");
	snd_soc_dapm_ignore_suspend(&card->dapm, "MAINMIC");
	snd_soc_dapm_ignore_suspend(&card->dapm, "SUBMIC");
	snd_soc_dapm_ignore_suspend(&card->dapm, "RECEIVER");
	snd_soc_dapm_ignore_suspend(&card->dapm, "HEADPHONE");
	snd_soc_dapm_ignore_suspend(&card->dapm, "SPEAKER");
	snd_soc_dapm_sync(&card->dapm);

	snd_soc_dapm_ignore_suspend(snd_soc_codec_get_dapm(codec), "AIF1 Playback");
	snd_soc_dapm_ignore_suspend(snd_soc_codec_get_dapm(codec), "AIF1 Capture");
	snd_soc_dapm_sync(snd_soc_codec_get_dapm(codec));

#if defined(CONFIG_SND_SOC_RT5510)
	snd_soc_dapm_ignore_suspend(snd_soc_codec_get_dapm(spk_amp), "aif_playback");
	snd_soc_dapm_ignore_suspend(snd_soc_codec_get_dapm(spk_amp), "aif_capture");
	snd_soc_dapm_sync(snd_soc_codec_get_dapm(spk_amp));
#endif

	for (i = 0; i < RDMA_COUNT; i++) {
		aif1_dai = madera_get_rtd(card, i)->cpu_dai;
		cpu = aif1_dai->component;
		dapm = snd_soc_component_get_dapm(cpu);
		prefix = dapm->component->name_prefix;
		snprintf(name, sizeof(name), "%s RDMA%d Playback", prefix, i);
		snd_soc_dapm_ignore_suspend(dapm, name);
		snd_soc_dapm_sync(dapm);
	}

	for (i = 0; i < WDMA_COUNT; i++) {
		aif1_dai = madera_get_rtd(card, RDMA_COUNT + i)->cpu_dai;
		cpu = aif1_dai->component;
		dapm = snd_soc_component_get_dapm(cpu);
		prefix = dapm->component->name_prefix;
		snprintf(name, sizeof(name), "%s WDMA%d Capture", prefix, i);
		snd_soc_dapm_ignore_suspend(dapm, name);
		snd_soc_dapm_sync(dapm);
	}

	for (i = 0; i < UAIF_COUNT; i++) {
		aif1_dai = madera_get_rtd(card, UAIF_START + i)->cpu_dai;
		cpu = aif1_dai->component;
		dapm = snd_soc_component_get_dapm(cpu);
		prefix = dapm->component->name_prefix;
		snprintf(name, sizeof(name), "%s UAIF%d Capture", prefix, i);
		snd_soc_dapm_ignore_suspend(dapm, name);
		snprintf(name, sizeof(name), "%s UAIF%d Playback", prefix, i);
		snd_soc_dapm_ignore_suspend(dapm, name);
		snd_soc_dapm_sync(dapm);
	}

	for (i = 0; i < SIFS_COUNT; i++) {
		aif1_dai = madera_get_rtd(card, SIFS_START + i)->cpu_dai;
		cpu = aif1_dai->component;
		dapm = snd_soc_component_get_dapm(cpu);
		prefix = dapm->component->name_prefix;
		snprintf(name, sizeof(name), "%s SIFS%d Capture", prefix, i);
		snd_soc_dapm_ignore_suspend(dapm, name);
		snprintf(name, sizeof(name), "%s SIFS%d Playback", prefix, i);
		snd_soc_dapm_ignore_suspend(dapm, name);
		snd_soc_dapm_sync(dapm);
	}

	aif1_dai = madera_get_rtd(card, SPDY_START)->cpu_dai;
	cpu = aif1_dai->component;
	dapm = snd_soc_component_get_dapm(cpu);
	prefix = dapm->component->name_prefix;
	snprintf(name, sizeof(name), "%s SPDY Capture", prefix);
	snd_soc_dapm_ignore_suspend(dapm, name);
	snd_soc_dapm_sync(dapm);

	wake_lock_init(&drvdata->wake_lock, WAKE_LOCK_SUSPEND, "vogue-sound");
	drvdata->wake_lock_switch = 0;

	register_debug_mixer(card);

	return 0;
}

static int vogue_headsetmic(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;

	dev_info(card->dev, "%s ev: %d\n", __func__, event);

	return 0;
}

static int vogue_mainmic(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;

	dev_info(card->dev, "%s ev: %d\n", __func__, event);

	return 0;
}

static int vogue_submic(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_card *card = w->dapm->card;

	dev_info(card->dev, "%s ev: %d\n", __func__, event);

	return 0;
}

static int vogue_get_sound_wakelock(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct madera_drvdata *drvdata = &exynos9610_drvdata;

	ucontrol->value.integer.value[0] = drvdata->wake_lock_switch;

	return 0;
}

static int vogue_set_sound_wakelock(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct madera_drvdata *drvdata = &exynos9610_drvdata;
	unsigned int val;

	val = (unsigned int)ucontrol->value.integer.value[0];
	drvdata->wake_lock_switch = val;

	dev_info(drvdata->dev, "%s: %d\n", __func__, drvdata->wake_lock_switch);

	if (drvdata->wake_lock_switch)
		wake_lock(&drvdata->wake_lock);
	else
		wake_unlock(&drvdata->wake_lock);

	return 0;
}

static const char * const abox_vss_state_enum_texts[] = {
	"NORMAL",
	"INIT FAIL",
	"SUSPENDED",
	"PCM OPEN FAIL",
	"ABOX STUCK",
};

static SOC_ENUM_SINGLE_DECL(abox_vss_state_enum, SND_SOC_NOPM, 0,
				abox_vss_state_enum_texts);

static int abox_vss_state_get(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct madera_drvdata *drvdata = &exynos9610_drvdata;

	ucontrol->value.enumerated.item[0] = drvdata->abox_vss_state;

	return 0;
}

static int abox_vss_state_put(struct snd_kcontrol *kcontrol,
					 struct snd_ctl_elem_value *ucontrol)
{
	struct madera_drvdata *drvdata = &exynos9610_drvdata;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	const unsigned int item = ucontrol->value.enumerated.item[0];

	if (item >= e->items) {
		dev_err(drvdata->dev, "%s item %d overflow\n", __func__, item);
		return -EINVAL;
	}

	drvdata->abox_vss_state = item;
	dev_info(drvdata->dev, "VSS STATE: %s\n", abox_vss_state_enum_texts[item]);

	return 0;
}

static const struct snd_kcontrol_new exynos9610_madera_controls[] = {
	SOC_DAPM_PIN_SWITCH("HEADPHONE"),
	SOC_DAPM_PIN_SWITCH("SPEAKER"),
	SOC_DAPM_PIN_SWITCH("RECEIVER"),
	SOC_DAPM_PIN_SWITCH("MAINMIC"),
	SOC_DAPM_PIN_SWITCH("SUBMIC"),
	SOC_DAPM_PIN_SWITCH("HEADSETMIC"),
	SOC_ENUM_EXT("ABOX VSS State", abox_vss_state_enum, abox_vss_state_get, abox_vss_state_put),
	SOC_SINGLE_BOOL_EXT("Sound Wakelock", 0,
			vogue_get_sound_wakelock, vogue_set_sound_wakelock),
};

const struct snd_soc_dapm_widget exynos9610_madera_dapm_widgets[] = {
	SND_SOC_DAPM_OUTPUT("VOUTPUT"),
	SND_SOC_DAPM_INPUT("VINPUT1"),
	SND_SOC_DAPM_INPUT("VINPUT2"),
	SND_SOC_DAPM_OUTPUT("VOUTPUTCALL"),
	SND_SOC_DAPM_INPUT("VINPUTCALL"),
	SND_SOC_DAPM_MIC("HEADSETMIC", vogue_headsetmic),
	SND_SOC_DAPM_MIC("MAINMIC", vogue_mainmic),
	SND_SOC_DAPM_MIC("SUBMIC", vogue_submic),
	SND_SOC_DAPM_SPK("RECEIVER", NULL),
	SND_SOC_DAPM_HP("HEADPHONE", NULL),
	SND_SOC_DAPM_SPK("SPEAKER", NULL),
	SND_SOC_DAPM_MIC("BLUETOOTH MIC", NULL),
	SND_SOC_DAPM_SPK("BLUETOOTH SPK", NULL),
};

static const struct snd_kcontrol_new madera_codec_controls[] = {
	SOC_SINGLE_EXT_TLV("HPOUT1L Impedance Volume",
		MADERA_DAC_DIGITAL_VOLUME_1L,
		MADERA_OUT1L_VOL_SHIFT, 0xbf, 0,
		snd_soc_get_volsw, madera_put_impedance_volsw,
		madera_digital_tlv),

	SOC_SINGLE_EXT_TLV("HPOUT1R Impedance Volume",
		MADERA_DAC_DIGITAL_VOLUME_1R,
		MADERA_OUT1L_VOL_SHIFT, 0xbf, 0,
		snd_soc_get_volsw, madera_put_impedance_volsw,
		madera_digital_tlv),

	SOC_SINGLE_BOOL_EXT("Sound Wakelock",
		0, vogue_get_sound_wakelock, vogue_set_sound_wakelock),
};

static int vogue_uaif0_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_card *card = rtd->card;
	struct madera_drvdata *drvdata = snd_soc_card_get_drvdata(card);
	struct snd_soc_dai *codec_dai;
	int ret = 0;

	dev_info(drvdata->dev, "%s\n", __func__);

	rtd = snd_soc_get_pcm_runtime(card,
			card->dai_link[MADERA_DAI_ID].name);
	codec_dai = rtd->codec_dai;
	codec = rtd->codec_dai->codec;

	ret = snd_soc_add_codec_controls(codec, madera_codec_controls,
					ARRAY_SIZE(madera_codec_controls));
	if (ret < 0) {
		dev_err(drvdata->dev,
			"Failed to add controls to codec: %d\n", ret);
		return ret;
	}

	ret = snd_soc_codec_set_sysclk(codec, drvdata->sysclk.id,
				       drvdata->sysclk.source,
				       drvdata->sysclk.rate,
				       SND_SOC_CLOCK_IN);
	if (ret != 0) {
		dev_err(drvdata->dev, "Failed to set SYSCLK: %d\n", ret);
		return ret;
	}

	ret = snd_soc_codec_set_pll(codec, MADERA_FLL1_SYNCCLK,
				    MADERA_FLL_SRC_MCLK1,
				    CS47L15_MCLK1_RATE, 0);
	if (ret)
		dev_err(card->dev, "Failed to init FLL syncclk\n");

	snd_soc_dapm_ignore_suspend(snd_soc_codec_get_dapm(codec), "AIF1 Playback");
	snd_soc_dapm_ignore_suspend(snd_soc_codec_get_dapm(codec), "AIF1 Capture");
	snd_soc_dapm_ignore_suspend(snd_soc_codec_get_dapm(codec), "AUXPDM1");
	snd_soc_dapm_sync(snd_soc_codec_get_dapm(codec));

	drvdata->nb.notifier_call = madera_notify;
	madera_register_notifier(codec, &drvdata->nb);

	register_madera_jack_cb(codec);


	return 0;
}

static struct snd_soc_dai_link exynos9610_dai[] = {
	{
		.name = "RDMA0",
		.stream_name = "RDMA0",
		.platform_name = "14a51000.abox_rdma",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE, SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
		.id = 0,
	},
	{
		.name = "RDMA1",
		.stream_name = "RDMA1",
		.platform_name = "14a51100.abox_rdma",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE, SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
		.id = 1,
	},
	{
		.name = "RDMA2",
		.stream_name = "RDMA2",
		.platform_name = "14a51200.abox_rdma",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE, SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
		.id = 2,
	},
	{
		.name = "RDMA3",
		.stream_name = "RDMA3",
		.platform_name = "14a51300.abox_rdma",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE, SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
		.id = 3,
	},
	{
		.name = "RDMA4",
		.stream_name = "RDMA4",
		.platform_name = "14a51400.abox_rdma",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE, SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
		.id = 4,
	},
	{
		.name = "RDMA5",
		.stream_name = "RDMA5",
		.platform_name = "14a51500.abox_rdma",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE, SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
		.id = 5,
	},
	{
		.name = "RDMA6",
		.stream_name = "RDMA6",
		.platform_name = "14a51600.abox_rdma",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE, SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
		.id = 6,
	},
	{
		.name = "RDMA7",
		.stream_name = "RDMA7",
		.platform_name = "14a51700.abox_rdma",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE, SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &rdma_ops,
		.dpcm_playback = 1,
		.id = 7,
	},
	{
		.name = "WDMA0",
		.stream_name = "WDMA0",
		.platform_name = "14a52000.abox_wdma",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE, SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &wdma_ops,
		.dpcm_capture = 1,
		.id = 8,
	},
	{
		.name = "WDMA1",
		.stream_name = "WDMA1",
		.platform_name = "14a52100.abox_wdma",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE, SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &wdma_ops,
		.dpcm_capture = 1,
		.id = 9,
	},
	{
		.name = "WDMA2",
		.stream_name = "WDMA2",
		.platform_name = "14a52200.abox_wdma",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE, SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &wdma_ops,
		.dpcm_capture = 1,
		.id = 10,
	},
	{
		.name = "WDMA3",
		.stream_name = "WDMA3",
		.platform_name = "14a52300.abox_wdma",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE, SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &wdma_ops,
		.dpcm_capture = 1,
		.id = 11,
	},
	{
		.name = "WDMA4",
		.stream_name = "WDMA4",
		.platform_name = "14a52400.abox_wdma",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.dynamic = 1,
		.ignore_suspend = 1,
		.trigger = {SND_SOC_DPCM_TRIGGER_POST_PRE, SND_SOC_DPCM_TRIGGER_PRE_POST},
		.ops = &wdma_ops,
		.dpcm_capture = 1,
		.id = 12,
	},
#if IS_ENABLED(SND_SOC_SAMSUNG_DISPLAYPORT)
	{
		.name = "DP Audio",
		.stream_name = "DP Audio",
		.cpu_dai_name = "audio_cpu_dummy",
		.platform_name = "dp_dma",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
	},
#endif
	{
		.name = "UAIF0",
		.stream_name = "UAIF0",
		.cpu_dai_name = "UAIF0",
		.platform_name = "snd-soc-dummy",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.init = vogue_uaif0_init,
		.ops = &uaif0_ops,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
		.id = MADERA_DAI_ID,
	},
	{
		.name = "UAIF1",
		.stream_name = "UAIF1",
		.platform_name = "snd-soc-dummy",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.ops = &uaif_ops,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "UAIF2",
		.stream_name = "UAIF2",
		.platform_name = "snd-soc-dummy",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.ops = &uaif_ops,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "UAIF4",
		.stream_name = "UAIF4",
		.platform_name = "snd-soc-dummy",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.ops = &uaif_ops,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "DSIF",
		.stream_name = "DSIF",
		.cpu_dai_name = "DSIF",
		.platform_name = "snd-soc-dummy",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.ops = &dsif_ops,
		.dpcm_playback = 1,
	},
	{
		.name = "SPDY",
		.stream_name = "SPDY",
		.cpu_dai_name = "SPDY",
		.platform_name = "snd-soc-dummy",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.ops = &uaif_ops,
		.dpcm_capture = 1,
	},
	{
		.name = "SIFS0",
		.stream_name = "SIFS0",
		.cpu_dai_name = "SIFS0",
		.platform_name = "snd-soc-dummy",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "SIFS1",
		.stream_name = "SIFS1",
		.cpu_dai_name = "SIFS1",
		.platform_name = "snd-soc-dummy",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
	{
		.name = "SIFS2",
		.stream_name = "SIFS2",
		.cpu_dai_name = "SIFS2",
		.platform_name = "snd-soc-dummy",
		.codec_name = "snd-soc-dummy",
		.codec_dai_name = "snd-soc-dummy-dai",
		.no_pcm = 1,
		.ignore_suspend = 1,
		.ignore_pmdown_time = 1,
		.be_hw_params_fixup = abox_hw_params_fixup_helper,
		.dpcm_playback = 1,
		.dpcm_capture = 1,
	},
};

static int read_platform(struct device_node *np, const char * const prop,
				  struct device_node **dai)
{
	int ret = 0;

	np = of_get_child_by_name(np, prop);
	if (!np)
		return -ENOENT;

	*dai = of_parse_phandle(np, "sound-dai", 0);
	if (!*dai) {
		ret = -ENODEV;
		goto out;
	}
out:
	of_node_put(np);

	return ret;
}

static int read_cpu(struct device_node *np, struct device *dev,
		struct snd_soc_dai_link *dai_link)
{
	int ret = 0;

	np = of_get_child_by_name(np, "cpu");
	if (!np)
		return -ENOENT;

	dai_link->cpu_of_node = of_parse_phandle(np, "sound-dai", 0);
	if (!dai_link->cpu_of_node) {
		ret = -ENODEV;
		goto out;
	}

	if (dai_link->cpu_dai_name == NULL) {
		/* Ignoring the return as we don't register DAIs to the platform */
		ret = snd_soc_of_get_dai_name(np, &dai_link->cpu_dai_name);
		if (ret)
			goto out;
	}
out:
	of_node_put(np);

	return ret;
}

static int read_codec(struct device_node *np, struct device *dev,
		struct snd_soc_dai_link *dai_link)
{
	np = of_get_child_by_name(np, "codec");
	if (!np)
		return -ENOENT;

	return snd_soc_of_get_dai_link_codecs(dev, np, dai_link);
}

static struct snd_soc_codec_conf codec_conf[MADERA_CODEC_MAX];

static struct snd_soc_aux_dev aux_dev[MADERA_AUX_MAX];

static struct snd_soc_card exynos9610_madera = {
	.name = "Exynos9610-Madera",
	.owner = THIS_MODULE,
	.dai_link = exynos9610_dai,
	.num_links = ARRAY_SIZE(exynos9610_dai),

	.late_probe = exynos9610_late_probe,

	.controls = exynos9610_madera_controls,
	.num_controls = ARRAY_SIZE(exynos9610_madera_controls),
	.dapm_widgets = exynos9610_madera_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(exynos9610_madera_dapm_widgets),

	.set_bias_level = madera_set_bias_level,
	.set_bias_level_post = madera_set_bias_level_post,

	.drvdata = (void *)&exynos9610_drvdata,

	.codec_conf = codec_conf,
	.num_configs = ARRAY_SIZE(codec_conf),

	.aux_dev = aux_dev,
	.num_aux_devs = ARRAY_SIZE(aux_dev),
};

static int read_clk_conf(struct device_node *np,
				   const char * const prop,
				   struct clk_conf *conf)
{
	u32 tmp;
	int ret;

	/*Truncate "cirrus," from prop_name to fetch clk_name*/
	conf->name = &prop[7];

	ret = of_property_read_u32_index(np, prop, 0, &tmp);
	if (ret)
		return ret;

	conf->id = tmp;

	ret = of_property_read_u32_index(np, prop, 1, &tmp);
	if (ret)
		return ret;

	if (tmp < 0xffff)
		conf->source = tmp;
	else
		conf->source = -1;

	ret = of_property_read_u32_index(np, prop, 2, &tmp);
	if (ret)
		return ret;

	conf->rate = tmp;
	conf->valid = true;

	return 0;
}

static void exynos9610_mic_always_on(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = madera_get_rtd(card, MADERA_DAI_ID)->codec_dai->codec;
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(codec);

	dev_info(&pdev->dev, "%s\n", __func__);

	if (of_find_property(pdev->dev.of_node, "mic-always-on", NULL) != NULL){
		snd_soc_dapm_force_enable_pin(dapm, "MICBIAS1B");
		snd_soc_dapm_force_enable_pin(dapm, "MICBIAS1C");
		snd_soc_dapm_sync(dapm);
	} else {
		dev_info(&pdev->dev, "Invalid mic_always_on\n");
		return;
	}

	dev_info(&pdev->dev, "Success to enable mic-always-on\n");
}

static void control_xclkout(bool on)
{
	if (on) {
		clk_prepare_enable(xclkout);
	} else {
		clk_disable_unprepare(xclkout);
	}
}

static int exynos9610_audio_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &exynos9610_madera;
	struct madera_drvdata *drvdata = card->drvdata;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *dai;
	int nlink = 0;
	int i, rc, ret;

	dev_info(&pdev->dev, "%s\n", __func__);

	card->dev = &pdev->dev;
	drvdata->dev = card->dev;
	snd_soc_card_set_drvdata(card, drvdata);

	xclkout = devm_clk_get(&pdev->dev, "xclkout");
	if (IS_ERR(xclkout)) {
		dev_err(&pdev->dev, "xclkout get failed\n");
		xclkout = NULL;
		goto err_clk_get;
	}
	control_xclkout(true);
	dev_info(&pdev->dev, "xclkout is enabled\n");

	ret = read_clk_conf(np, "cirrus,sysclk", &drvdata->sysclk);
	if (ret) {
		dev_err(card->dev, "Failed to parse sysclk: %d\n", ret);
		goto err_clk_prepare;
	}

	ret = read_clk_conf(np, "cirrus,asyncclk", &drvdata->asyncclk);
	if (ret)
		dev_info(card->dev, "Failed to parse asyncclk: %d\n", ret);

	ret = read_clk_conf(np, "cirrus,dspclk", &drvdata->dspclk);
	if (ret) {
		dev_info(card->dev, "Failed to parse dspclk: %d\n", ret);
	} else if (drvdata->dspclk.source == drvdata->sysclk.source &&
		   drvdata->dspclk.rate / 3 != drvdata->sysclk.rate / 2) {
		/* DSPCLK & SYSCLK, if sharing a source, should be an
		 * appropriate ratio of one another, or both be zero (which
		 * signifies "automatic" mode).
		 */
		dev_err(card->dev, "DSPCLK & SYSCLK share src but request incompatible frequencies: %d vs %d\n",
			drvdata->dspclk.rate, drvdata->sysclk.rate);
		goto err_clk_prepare;
	}

	ret = read_clk_conf(np, "cirrus,fll1-refclk", &drvdata->fll1_refclk);
	if (ret)
		dev_info(card->dev, "Failed to parse fll1-refclk: %d\n", ret);

	ret = read_clk_conf(np, "cirrus,fll2-refclk", &drvdata->fll2_refclk);
	if (ret)
		dev_info(card->dev, "Failed to parse fll2-refclk: %d\n", ret);

	ret = read_clk_conf(np, "cirrus,outclk", &drvdata->outclk);
	if (ret)
		dev_info(card->dev, "Failed to parse outclk: %d\n", ret);

	for_each_child_of_node(np, dai) {
		struct snd_soc_dai_link *link = &exynos9610_dai[nlink];

		if (!link->name)
			link->name = dai->name;
		if (!link->stream_name)
			link->stream_name = dai->name;

		if (!link->cpu_name) {
			ret = read_cpu(dai, card->dev, link);
			if (ret) {
				dev_err(card->dev, "Failed to parse cpu DAI for %s: %d\n",
						dai->name, ret);
				return ret;
			}
		}

		if (!link->platform_name) {
			ret = read_platform(dai, "platform",
				&link->platform_of_node);
			if (ret) {
				link->platform_of_node = link->cpu_of_node;
				dev_info(card->dev, "Cpu node is used as platform for %s: %d\n",
						dai->name, ret);
			}
		}

		if (!link->codec_name) {
			ret = read_codec(dai, card->dev, link);
			if (ret) {
				dev_err(card->dev, "Failed to parse codec DAI for %s: %d\n",
						dai->name, ret);
				return ret;
			}

			if (link->codecs && strstr(link->codecs[0].dai_name,
					"cs35l41")) 
				link->ops = &cs35l41_ops;

		}

		link->dai_fmt = snd_soc_of_parse_daifmt(dai, NULL, NULL, NULL);

		if (++nlink == card->num_links)
			break;
	}

	if (!nlink) {
		dev_err(card->dev, "No DAIs specified\n");
		return -EINVAL;
	}

	if (of_property_read_bool(np, "samsung,routing")) {
		ret = snd_soc_of_parse_audio_routing(card, "samsung,routing");
		if (ret)
			return ret;
	}

	for (i = 0; i < ARRAY_SIZE(codec_conf); i++) {
		codec_conf[i].of_node = of_parse_phandle(np, "samsung,codec", i);
		if (IS_ERR_OR_NULL(codec_conf[i].of_node)) {
			exynos9610_madera.num_configs = i;
			break;
		}

		rc = of_property_read_string_index(np, "samsung,prefix", i,
				&codec_conf[i].name_prefix);
		if (rc < 0)
			codec_conf[i].name_prefix = "";
	}

	for (i = 0; i < ARRAY_SIZE(aux_dev); i++) {
		aux_dev[i].codec_of_node = of_parse_phandle(np, "samsung,aux", i);
		if (IS_ERR_OR_NULL(aux_dev[i].codec_of_node)) {
			exynos9610_madera.num_aux_devs = i;
			break;
		}
	}

	ret = devm_snd_soc_register_card(card->dev, card);
	if (ret) {
		dev_err(card->dev, "%s: snd_soc_register_card() failed:%d\n", __func__, ret);
		sec_audio_bootlog(3, "%s: snd_soc_register_card() failed:%d\n", __func__, ret);
	} else {
		exynos9610_mic_always_on(pdev);
		dev_info(card->dev, "%s done\n", __func__);
		sec_audio_bootlog(6, "%s done\n", __func__);
	}

	return ret;

err_clk_prepare:
	clk_unprepare(xclkout);
err_clk_get:
	devm_clk_put(&pdev->dev, xclkout);

	return ret;
}

static int exynos9610_audio_remove(struct platform_device *pdev)
{
	clk_unprepare(xclkout);
	devm_clk_put(&pdev->dev, xclkout);

	return 0;
}

static const struct of_device_id exynos9610_audio_of_match[] = {
	{.compatible = "samsung,exynos9610-audio-cs47l15",},
	{},
};
MODULE_DEVICE_TABLE(of, exynos9610_audio_of_match);

static struct platform_driver exynos9610_audio_driver = {
	.driver = {
		.name = "Exynos9610-audio-cs47l15",
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = of_match_ptr(exynos9610_audio_of_match),
	},

	.probe		= exynos9610_audio_probe,
	.remove		= exynos9610_audio_remove,
};

module_platform_driver(exynos9610_audio_driver);

MODULE_DESCRIPTION("ALSA SoC Exynos9610 Audio Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:exynos9610-audio-cs47l15");