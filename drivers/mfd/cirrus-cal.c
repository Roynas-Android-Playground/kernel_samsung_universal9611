/*
 * Calibration support for Cirrus Logic CS35L41 codec
 *
 * Copyright 2017 Cirrus Logic
 *
 * Author:	David Rhodes	<david.rhodes@cirrus.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/file.h>
#include <linux/fcntl.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <asm/io.h>
#include <linux/firmware.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>
#include <linux/power_supply.h>
#include <linux/fs.h>

#include <sound/cs35l41.h>
#include <linux/mfd/cs35l41/core.h>
#include <linux/mfd/cs35l41/registers.h>
#include <linux/mfd/cs35l41/calibration.h>
#include <linux/mfd/cs35l41/wmfw.h>

#define CIRRUS_CAL_VERSION "5.01.18"

#define CIRRUS_CAL_DIR_NAME			"cirrus_cal"
#define CIRRUS_CAL_CONFIG_FILENAME_SUFFIX	"-dsp1-spk-prot-calib.bin"
#define CIRRUS_CAL_PLAYBACK_FILENAME_SUFFIX	"-dsp1-spk-prot.bin"
#define CIRRUS_CAL_RDC_SAVE_LOCATION		"/efs/cirrus/rdc_cal"
#define CIRRUS_CAL_TEMP_SAVE_LOCATION		"/efs/cirrus/temp_cal"

#define CS35L41_CAL_COMPLETE_DELAY_MS	1250
#define CS34L40_CAL_AMBIENT_DEFAULT	23
#define CS34L40_CAL_RDC_DEFAULT		8580

struct cirrus_cal_t {
	struct class *cal_class;
	struct device *dev;
	struct cirrus_mfd_amp *amps;
	int num_amps;
	bool cal_running;
	struct mutex lock;
	struct delayed_work cal_complete_work;
	unsigned int efs_cache_temp;
	int efs_cache_read[CIRRUS_MAX_AMPS];
	unsigned int efs_cache_rdc[CIRRUS_MAX_AMPS];
	unsigned int v_validation[CIRRUS_MAX_AMPS];
	unsigned int dsp_input1_cache[CIRRUS_MAX_AMPS];
	unsigned int dsp_input2_cache[CIRRUS_MAX_AMPS];
#ifdef CS35L41_FACTORY_RECOVERY_SYSFS
	struct snd_soc_codec *codecs[CIRRUS_MAX_AMPS];
#endif /* CS35L41_FACTORY_RECOVERY_SYSFS */
};

static struct cirrus_cal_t *cirrus_cal;
static struct attribute_group cirrus_cal_attr_grp;

struct cs35l41_dsp_buf {
	struct list_head list;
	void *buf;
};

struct cirrus_mfd_amp *cirrus_cal_get_amp_from_suffix(const char *suffix)
{
	int i;
	struct cirrus_mfd_amp *ret = NULL;

	if (cirrus_cal == NULL || cirrus_cal->amps == NULL)
		return NULL;

	dev_dbg(cirrus_cal->dev, "%s: suffix = %s\n", __func__, suffix);

	for (i = 0; i < cirrus_cal->num_amps; i++) {
		dev_dbg(cirrus_cal->dev, "comparing %s & %s\n",
				cirrus_cal->amps[i].mfd_suffix,
				suffix);
		if (strcmp(cirrus_cal->amps[i].mfd_suffix, suffix) == 0)
			ret = &cirrus_cal->amps[i];
	}

	return ret;
}

#ifdef CS35L41_FACTORY_RECOVERY_SYSFS
int cirrus_cal_codec_add(struct snd_soc_codec *codec, const char *mfd_suffix)
{
	struct cirrus_mfd_amp *amp = cirrus_cal_get_amp_from_suffix(mfd_suffix);

	if (cirrus_cal){
		if (amp) {
			dev_info(cirrus_cal->dev,
				"Codec added, suffix: %s\n",
				mfd_suffix);
			cirrus_cal->codecs[amp->index] = codec;
		} else {
			dev_err(cirrus_cal->dev,
				"No amp with suffix %s registered\n",
				mfd_suffix);
		}
	} else {
		return -EINVAL;
	}

	return 0;
}
#endif /* CS35L41_FACTORY_RECOVERY_SYSFS */

int cirrus_cal_amp_add(struct regmap *regmap_new, const char *mfd_suffix,
					const char *dsp_part_name)
{
	struct cirrus_mfd_amp *amp = cirrus_cal_get_amp_from_suffix(mfd_suffix);

	if (cirrus_cal){
		if (amp) {
			dev_info(cirrus_cal->dev,
				"Amp added, suffix: %s dsp_part_name: %s\n",
				mfd_suffix, dsp_part_name);
			amp->regmap = regmap_new;
			amp->dsp_part_name = dsp_part_name;
		} else {
			dev_err(cirrus_cal->dev,
				"No amp with suffix %s registered\n",
				mfd_suffix);
		}
	} else {
		return -EINVAL;
	}

	return 0;
}

static struct cs35l41_dsp_buf *cs35l41_dsp_buf_alloc(const void *src, size_t len,
					     struct list_head *list)
{
	struct cs35l41_dsp_buf *buf = kzalloc(sizeof(*buf), GFP_KERNEL);

	if (buf == NULL)
		return NULL;

	buf->buf = vmalloc(len);
	if (!buf->buf) {
		kfree(buf);
		return NULL;
	}
	memcpy(buf->buf, src, len);

	if (list)
		list_add_tail(&buf->list, list);

	return buf;
}

static void cs35l41_dsp_buf_free(struct list_head *list)
{
	while (!list_empty(list)) {
		struct cs35l41_dsp_buf *buf = list_first_entry(list,
							   struct cs35l41_dsp_buf,
							   list);
		list_del(&buf->list);
		vfree(buf->buf);
		kfree(buf);
	}
}

static unsigned long long int cs35l41_rdc_to_ohms(unsigned long int rdc)
{
	return ((rdc * CS35L41_CAL_AMP_CONSTANT_NUM) /
		CS35L41_CAL_AMP_CONSTANT_DENOM);
}

static unsigned int cirrus_cal_vpk_to_mv(unsigned int vpk)
{
	return (vpk * CIRRUS_CAL_VFS_MV) >> 19;
}

static unsigned int cirrus_cal_ipk_to_ma(unsigned int ipk)
{
	return (ipk * CIRRUS_CAL_IFS_MA) >> 19;
}

static void cirrus_cal_unmute_dsp_inputs(struct regmap *regmap, int cache_index)
{
	regmap_write(regmap, CS35L41_DSP1_RX1_SRC,
			cirrus_cal->dsp_input1_cache[cache_index]);
	regmap_write(regmap, CS35L41_DSP1_RX2_SRC,
			cirrus_cal->dsp_input2_cache[cache_index]);
}

static void cirrus_cal_mute_dsp_inputs(struct regmap *regmap, int cache_index)
{
	unsigned int src1, src2;

	regmap_read(regmap, CS35L41_DSP1_RX1_SRC, &src1);
	if (src1) cirrus_cal->dsp_input1_cache[cache_index] = src1;
	regmap_read(regmap, CS35L41_DSP1_RX2_SRC, &src2);
	if (src2) cirrus_cal->dsp_input2_cache[cache_index] = src2;

	regmap_write(regmap, CS35L41_DSP1_RX1_SRC, 0);
	regmap_write(regmap, CS35L41_DSP1_RX2_SRC, 0);
}

static int cs35l41_load_config(const char *file, struct regmap *regmap)
{
	LIST_HEAD(buf_list);
	struct wmfw_coeff_hdr *hdr;
	struct wmfw_coeff_item *blk;
	const struct firmware *firmware;
	const char *region_name;
	int ret, pos, blocks, type, offset, reg;
	struct cs35l41_dsp_buf *buf;

	ret = request_firmware(&firmware, file, cirrus_cal->dev);

	if (ret != 0) {
		dev_err(cirrus_cal->dev, "Failed to request '%s'\n", file);
		ret = 0;
		goto out;
	}
	ret = -EINVAL;

	if (sizeof(*hdr) >= firmware->size) {
		dev_err(cirrus_cal->dev, "%s: file too short, %zu bytes\n",
			file, firmware->size);
		goto out_fw;
	}

	hdr = (void *)&firmware->data[0];
	if (memcmp(hdr->magic, "WMDR", 4) != 0) {
		dev_err(cirrus_cal->dev, "%s: invalid magic\n", file);
		goto out_fw;
	}

	switch (be32_to_cpu(hdr->rev) & 0xff) {
	case 1:
		break;
	default:
		dev_err(cirrus_cal->dev, "%s: Unsupported coefficient file format %d\n",
			 file, be32_to_cpu(hdr->rev) & 0xff);
		ret = -EINVAL;
		goto out_fw;
	}

	dev_dbg(cirrus_cal->dev, "%s: v%d.%d.%d\n", file,
		(le32_to_cpu(hdr->ver) >> 16) & 0xff,
		(le32_to_cpu(hdr->ver) >>  8) & 0xff,
		le32_to_cpu(hdr->ver) & 0xff);

	pos = le32_to_cpu(hdr->len);

	blocks = 0;
	while (pos < firmware->size &&
	       pos - firmware->size > sizeof(*blk)) {
		blk = (void *)(&firmware->data[pos]);

		type = le16_to_cpu(blk->type);
		offset = le16_to_cpu(blk->offset);

		dev_dbg(cirrus_cal->dev, "%s.%d: %x v%d.%d.%d\n",
			 file, blocks, le32_to_cpu(blk->id),
			 (le32_to_cpu(blk->ver) >> 16) & 0xff,
			 (le32_to_cpu(blk->ver) >>  8) & 0xff,
			 le32_to_cpu(blk->ver) & 0xff);
		dev_dbg(cirrus_cal->dev, "%s.%d: %d bytes at 0x%x in %x\n",
			 file, blocks, le32_to_cpu(blk->len), offset, type);

		reg = 0;
		region_name = "Unknown";
		switch (type) {
		case WMFW_ADSP2_YM:
			dev_dbg(cirrus_cal->dev, "%s.%d: %d bytes in %x for %x\n",
				 file, blocks, le32_to_cpu(blk->len),
				 type, le32_to_cpu(blk->id));

			if (le32_to_cpu(blk->id) == 0xcd) {
				reg = CS35L41_YM_CONFIG_ADDR;
				reg += offset - 0x8;
			}
			break;

		case WMFW_HALO_YM_PACKED:
			dev_dbg(cirrus_cal->dev, "%s.%d: %d bytes in %x for %x\n",
				 file, blocks, le32_to_cpu(blk->len),
				 type, le32_to_cpu(blk->id));

			if (le32_to_cpu(blk->id) == 0xcd) {
				/*     config addr packed + 1        */
				/* config size (config[0]) is not at 24bit packed boundary */
				/* so that fist word gets written by itself to unpacked mem */
				/* then the rest of it starts here */
				/* offset = 3 (groups of 4 24bit words) * 3 (packed words) * 4 bytes */
				reg = CS35L41_DSP1_YMEM_PACK_0 + 3 * 4 * 3;
			}
			break;

		default:
			dev_dbg(cirrus_cal->dev, "%s.%d: region type %x at %d\n",
				 file, blocks, type, pos);
			break;
		}

		if (reg) {
			if ((pos + le32_to_cpu(blk->len) + sizeof(*blk)) >
			    firmware->size) {
				dev_err(cirrus_cal->dev,
					 "%s.%d: %s region len %d bytes exceeds file length %zu\n",
					 file, blocks, region_name,
					 le32_to_cpu(blk->len),
					 firmware->size);
				ret = -EINVAL;
				goto out_fw;
			}

			buf = cs35l41_dsp_buf_alloc(blk->data,
						le32_to_cpu(blk->len),
						&buf_list);
			if (!buf) {
				dev_err(cirrus_cal->dev, "Out of memory\n");
				ret = -ENOMEM;
				goto out_fw;
			}

			dev_dbg(cirrus_cal->dev, "%s.%d: Writing %d bytes at %x\n",
				 file, blocks, le32_to_cpu(blk->len),
				 reg);
			ret = regmap_raw_write_async(regmap, reg, buf->buf,
						     le32_to_cpu(blk->len));
			if (ret != 0) {
				dev_err(cirrus_cal->dev,
					"%s.%d: Failed to write to %x in %s: %d\n",
					file, blocks, reg, region_name, ret);
			}
		}

		pos += (le32_to_cpu(blk->len) + sizeof(*blk) + 3) & ~0x03;
		blocks++;
	}

	ret = regmap_async_complete(regmap);
	if (ret != 0)
		dev_err(cirrus_cal->dev, "Failed to complete async write: %d\n", ret);

	if (pos > firmware->size)
		dev_err(cirrus_cal->dev, "%s.%d: %zu bytes at end of file\n",
			  file, blocks, pos - firmware->size);

	dev_info(cirrus_cal->dev, "%s load complete\n", file);

out_fw:
	regmap_async_complete(regmap);
	release_firmware(firmware);
	cs35l41_dsp_buf_free(&buf_list);
out:
	return ret;
}

static void cirrus_cal_complete_work(struct work_struct *work)
{
	int rdc, status, checksum, temp;
	unsigned long long int ohms;
	unsigned int cal_state;
	char *playback_config_filename;
	int timeout = 100, amp, index;
	struct regmap *regmap;
	const char *dsp_part_name;



	mutex_lock(&cirrus_cal->lock);

	for (amp = 0; amp < cirrus_cal->num_amps; amp++) {
		regmap = cirrus_cal->amps[amp].regmap;
		dsp_part_name = cirrus_cal->amps[amp].dsp_part_name;
		index = cirrus_cal->amps[amp].index;

		playback_config_filename = kzalloc(PAGE_SIZE, GFP_KERNEL);
		snprintf(playback_config_filename,
			PAGE_SIZE, "%s%s",
			dsp_part_name,
			CIRRUS_CAL_PLAYBACK_FILENAME_SUFFIX);

		regmap_read(regmap, CS35L41_CAL_STATUS, &status);
		regmap_read(regmap, CS35L41_CAL_RDC, &rdc);
		regmap_read(regmap, CS35L41_CAL_AMBIENT, &temp);
		regmap_read(regmap, CS35L41_CAL_CHECKSUM, &checksum);

		ohms = cs35l41_rdc_to_ohms((unsigned long int)rdc);

		regmap_read(regmap, CS35L41_CSPL_STATE, &cal_state);
		if (cal_state == CS35L41_CSPL_STATE_ERROR) {
			dev_err(cirrus_cal->dev,
			    "Error during calibration, invalidating results\n");
			rdc = status = checksum = 0;
		}

		dev_info(cirrus_cal->dev, "Calibration finished: cs35l41%s\n",
				cirrus_cal->amps[amp].mfd_suffix);
		dev_info(cirrus_cal->dev, "Duration:\t%d ms\n",
					    CS35L41_CAL_COMPLETE_DELAY_MS);
		dev_info(cirrus_cal->dev, "Status:\t%d\n", status);
		if (status == CS35L41_CSPL_STATUS_OUT_OF_RANGE)
			dev_err(cirrus_cal->dev, "Calibration out of range\n");
		if (status == CS35L41_CSPL_STATUS_INCOMPLETE)
			dev_err(cirrus_cal->dev, "Calibration incomplete\n");
		dev_info(cirrus_cal->dev, "R :\t\t%d (%llu.%04llu Ohms)\n",
				rdc, ohms >> CS35L41_CAL_RDC_RADIX,
				    (ohms & (((1 << CS35L41_CAL_RDC_RADIX) - 1))) *
				    10000 / (1 << CS35L41_CAL_RDC_RADIX));
		dev_info(cirrus_cal->dev, "Checksum:\t%d\n", checksum);
		dev_info(cirrus_cal->dev, "Ambient:\t%d\n", temp);

		usleep_range(5000, 5500);

		/* Send STOP_PRE_REINIT command and poll for response */
		regmap_write(regmap, CS35L41_CSPL_MBOX_CMD_DRV,
				CSPL_MBOX_CMD_STOP_PRE_REINIT);
		timeout = 100;
		do {
			dev_info(cirrus_cal->dev, "waiting for REINIT ready...\n");
			usleep_range(1000, 1500);
			regmap_read(regmap, CS35L41_CSPL_MBOX_STS,
					&cal_state);
		} while ((cal_state != CSPL_MBOX_STS_RDY_FOR_REINIT) &&
				--timeout > 0);

		usleep_range(5000, 5500);

		cs35l41_load_config(playback_config_filename, regmap);

		regmap_update_bits(regmap,
			CS35L41_MIXER_NGATE_CH1_CFG,
			CS35L41_NG_ENABLE_MASK,
			CS35L41_NG_ENABLE_MASK);
		regmap_update_bits(regmap,
			CS35L41_MIXER_NGATE_CH2_CFG,
			CS35L41_NG_ENABLE_MASK,
			CS35L41_NG_ENABLE_MASK);
		dev_dbg(cirrus_cal->dev, "NOISE GATE ENABLE\n");

		regmap_write(regmap, CS35L41_CAL_STATUS, status);
		regmap_write(regmap, CS35L41_CAL_RDC, rdc);
		regmap_write(regmap, CS35L41_CAL_AMBIENT, temp);
		regmap_write(regmap, CS35L41_CAL_CHECKSUM, checksum);

		/* Send REINIT command and poll for response */
		regmap_write(regmap, CS35L41_CSPL_MBOX_CMD_DRV,
				CSPL_MBOX_CMD_REINIT);
		timeout = 100;
		do {
			dev_info(cirrus_cal->dev, "waiting for REINIT done...\n");
			usleep_range(1000, 1500);
			regmap_read(regmap, CS35L41_CSPL_MBOX_STS,
					&cal_state);

		} while ((cal_state != CSPL_MBOX_STS_RUNNING) &&
				--timeout > 0);

		regmap_read(regmap, CS35L41_CSPL_STATE, &cal_state);
		if (cal_state == CS35L41_CSPL_STATE_ERROR)
			dev_err(cirrus_cal->dev,
				"Playback config load error\n");

		cirrus_cal_unmute_dsp_inputs(regmap, index);
		dev_dbg(cirrus_cal->dev, "DSP Inputs unmuted\n");

		cirrus_cal->cal_running = 0;
		cirrus_cal->efs_cache_read[index] = 0;

		kfree(playback_config_filename);
	}

	dev_dbg(cirrus_cal->dev, "Calibration complete\n");
	mutex_unlock(&cirrus_cal->lock);
}

static void cirrus_cal_v_val_complete(void)
{
	char *playback_config_filename;
	unsigned int cal_state;
	int timeout = 100, amp, index;
	struct regmap *regmap;
	const char *dsp_part_name;

	for (amp = 0; amp < cirrus_cal->num_amps; amp++) {
		regmap = cirrus_cal->amps[amp].regmap;
		dsp_part_name = cirrus_cal->amps[amp].dsp_part_name;
		index = cirrus_cal->amps[amp].index;

		playback_config_filename = kzalloc(PAGE_SIZE, GFP_KERNEL);
		snprintf(playback_config_filename,
			PAGE_SIZE, "%s%s",
			dsp_part_name,
			CIRRUS_CAL_PLAYBACK_FILENAME_SUFFIX);

		/* Send STOP_PRE_REINIT command and poll for response */
		regmap_write(regmap, CS35L41_CSPL_MBOX_CMD_DRV,
				CSPL_MBOX_CMD_STOP_PRE_REINIT);
		timeout = 100;
		do {
			dev_info(cirrus_cal->dev, "waiting for REINIT ready...\n");
			usleep_range(1000, 1500);
			regmap_read(regmap, CS35L41_CSPL_MBOX_STS,
					&cal_state);

		} while ((cal_state != CSPL_MBOX_STS_RDY_FOR_REINIT) &&
				--timeout > 0);

		usleep_range(5000, 5500);

		cs35l41_load_config(playback_config_filename,
					regmap);

		regmap_update_bits(regmap,
			CS35L41_MIXER_NGATE_CH1_CFG,
			CS35L41_NG_ENABLE_MASK,
			CS35L41_NG_ENABLE_MASK);
		regmap_update_bits(regmap,
			CS35L41_MIXER_NGATE_CH2_CFG,
			CS35L41_NG_ENABLE_MASK,
			CS35L41_NG_ENABLE_MASK);
		dev_dbg(cirrus_cal->dev, "NOISE GATE ENABLE\n");

		/* Send REINIT command and poll for response */
		regmap_write(regmap, CS35L41_CSPL_MBOX_CMD_DRV,
				CSPL_MBOX_CMD_REINIT);

		timeout = 100;
		do {
			dev_info(cirrus_cal->dev, "waiting for REINIT done...\n");
			usleep_range(1000, 1500);
			regmap_read(regmap, CS35L41_CSPL_MBOX_STS,
					&cal_state);

		} while ((cal_state != CSPL_MBOX_STS_RUNNING) &&
				--timeout > 0);

		cirrus_cal_unmute_dsp_inputs(regmap, index);
		dev_dbg(cirrus_cal->dev, "DSP Inputs unmuted\n");

		regmap_read(regmap, CS35L41_CSPL_STATE, &cal_state);
		if (cal_state == CS35L41_CSPL_STATE_ERROR)
			dev_err(cirrus_cal->dev,
				"Playback config load error\n");

		kfree(playback_config_filename);
	}
	
	dev_info(cirrus_cal->dev, "V validation complete\n");
}

static int cirrus_cal_get_power_temp(void)
{
	union power_supply_propval value = {0};
	struct power_supply *psy;

	psy = power_supply_get_by_name("battery");
	if (!psy) {
		dev_warn(cirrus_cal->dev, "failed to get battery, assuming %d\n",
				CS34L40_CAL_AMBIENT_DEFAULT);
		return CS34L40_CAL_AMBIENT_DEFAULT;
	}

	power_supply_get_property(psy, POWER_SUPPLY_PROP_TEMP, &value);

	return DIV_ROUND_CLOSEST(value.intval, 10);
}

static void cirrus_cal_start(struct cirrus_mfd_amp *amp)
{
	int ambient;
	unsigned int global_en;
	unsigned int halo_state;
	char *cal_config_filename;
	int timeout = 50;
	struct regmap *regmap = amp->regmap;
	const char *dsp_part_name = amp->dsp_part_name;

	cal_config_filename = kzalloc(PAGE_SIZE, GFP_KERNEL);
	snprintf(cal_config_filename,
		PAGE_SIZE, "%s%s",
		dsp_part_name,
		CIRRUS_CAL_CONFIG_FILENAME_SUFFIX);

	dev_info(cirrus_cal->dev, "Calibration prepare start\n");

	regmap_read(regmap,
			CS35L41_PWR_CTRL1, &global_en);
	while ((global_en & 1) == 0) {
		usleep_range(1000, 1500);
		regmap_read(regmap,
			CS35L41_PWR_CTRL1, &global_en);
	}

	do {
		dev_info(cirrus_cal->dev, "waiting for HALO start...\n");
		usleep_range(10000, 15500);

		regmap_read(regmap, CS35L41_HALO_STATE,
			&halo_state);
		timeout--;
	} while ((halo_state == 0) && timeout > 0);

	if (timeout == 0) {
		dev_err(cirrus_cal->dev, "Failed to setup calibration\n");
		kfree(cal_config_filename);
		return;
	}

	/* Send STOP_PRE_REINIT command and poll for response */
	regmap_write(regmap, CS35L41_CSPL_MBOX_CMD_DRV,
			CSPL_MBOX_CMD_STOP_PRE_REINIT);
	timeout = 100;
	do {
		dev_info(cirrus_cal->dev, "waiting for REINIT ready...\n");
		usleep_range(1000, 1500);
		regmap_read(regmap, CS35L41_CSPL_MBOX_STS,
				&halo_state);

	} while ((halo_state != CSPL_MBOX_STS_RDY_FOR_REINIT) &&
			--timeout > 0);

	dev_dbg(cirrus_cal->dev, "load %s\n", dsp_part_name);
	cs35l41_load_config(cal_config_filename, regmap);

	cirrus_cal_mute_dsp_inputs(regmap, amp->index);
	dev_dbg(cirrus_cal->dev, "DSP Inputs muted\n");

	regmap_update_bits(regmap,
		CS35L41_MIXER_NGATE_CH1_CFG,
		CS35L41_NG_ENABLE_MASK, 0);
	regmap_update_bits(regmap,
		CS35L41_MIXER_NGATE_CH2_CFG,
		CS35L41_NG_ENABLE_MASK, 0);
	dev_dbg(cirrus_cal->dev, "NOISE GATE DISABLE\n");

	ambient = cirrus_cal_get_power_temp();
	regmap_write(regmap, CS35L41_CAL_AMBIENT, ambient);

	/* Send REINIT command and poll for response */
	regmap_write(regmap, CS35L41_CSPL_MBOX_CMD_DRV,
			CSPL_MBOX_CMD_REINIT);
	timeout = 100;
	do {
		dev_info(cirrus_cal->dev, "waiting for REINIT done...\n");
		usleep_range(1000, 1500);
		regmap_read(regmap, CS35L41_CSPL_MBOX_STS,
				&halo_state);

	} while ((halo_state != CSPL_MBOX_STS_RUNNING) &&
			--timeout > 0);

	kfree(cal_config_filename);
}

static int cirrus_cal_read_file(char *filename, int *value)
{
	struct file *cal_filp;
	mm_segment_t old_fs = get_fs();
	char str[12] = {0};
	int ret;

	set_fs(get_ds());

	cal_filp = filp_open(filename, O_RDONLY, 0660);
	if (IS_ERR(cal_filp)) {
		ret = PTR_ERR(cal_filp);
		dev_err(cirrus_cal->dev, "Failed to open calibration file %s: %d\n",
			filename, ret);
		goto err_open;
	}

	ret = vfs_read(cal_filp, (char __user *)str, sizeof(str),
						&cal_filp->f_pos);
	if (ret != sizeof(str)) {
		dev_err(cirrus_cal->dev, "Failed to read calibration file %s\n",
			filename);
		ret = -EIO;
		goto err_read;
	}

	ret = 0;

	if (kstrtoint(str, 0, value)) {
		dev_err(cirrus_cal->dev, "Failed to parse calibration.\n");
		ret = -EINVAL;
	}

err_read:
	filp_close(cal_filp, current->files);
err_open:
	set_fs(old_fs);
	return ret;
}

int cirrus_cal_apply(const char *mfd_suffix)
{
	unsigned int temp, rdc, status, checksum;
	int ret1 = 0;
	int ret2 = 0;
	struct cirrus_mfd_amp *amp = cirrus_cal_get_amp_from_suffix(mfd_suffix);
	struct regmap *regmap = amp->regmap;
	char *efs_name;

	if (!cirrus_cal)
		return 0;

	if (!amp)
		return 0;

	efs_name = kzalloc(PAGE_SIZE, GFP_KERNEL);
	snprintf(efs_name,
		PAGE_SIZE, "%s%s",
		CIRRUS_CAL_RDC_SAVE_LOCATION,
		mfd_suffix);

	if (cirrus_cal->efs_cache_read[amp->index] == 1) {
		rdc = cirrus_cal->efs_cache_rdc[amp->index];
		temp = cirrus_cal->efs_cache_temp;
	} else {
		ret1 = cirrus_cal_read_file(efs_name, &rdc);
		ret2 = cirrus_cal_read_file(CIRRUS_CAL_TEMP_SAVE_LOCATION,
						&temp);

		if (ret1 < 0 || ret2 < 0) {
			dev_err(cirrus_cal->dev,
				"No saved calibration, writing defaults\n");
			rdc = CS34L40_CAL_RDC_DEFAULT;
			temp = CS34L40_CAL_AMBIENT_DEFAULT;
		}

		cirrus_cal->efs_cache_rdc[amp->index] = rdc;
		cirrus_cal->efs_cache_temp = temp;
		cirrus_cal->efs_cache_read[amp->index] = 1;
	}

	status = 1;
	checksum = status + rdc;

	dev_info(cirrus_cal->dev, "Writing calibration (%s): to cs35l41%s\n",
			efs_name, mfd_suffix);
	dev_info(cirrus_cal->dev,
		"RDC = %d, Temp = %d, Status = %d Checksum = %d\n",
		rdc, temp, status, checksum);

	regmap_write(regmap, CS35L41_CAL_RDC, rdc);
	regmap_write(regmap, CS35L41_CAL_AMBIENT, temp);
	regmap_write(regmap, CS35L41_CAL_STATUS, status);
	regmap_write(regmap, CS35L41_CAL_CHECKSUM,
						checksum);

	kfree(efs_name);
	return ret1 | ret2;
}
EXPORT_SYMBOL_GPL(cirrus_cal_apply);

/***** SYSFS Interfaces *****/

static ssize_t cirrus_cal_version_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return sprintf(buf, CIRRUS_CAL_VERSION "\n");
}

static ssize_t cirrus_cal_version_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	return 0;
}

static ssize_t cirrus_cal_status_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return sprintf(buf, "%s\n",
			cirrus_cal->cal_running ? "Enabled" : "Disabled");
}

static ssize_t cirrus_cal_status_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	int prepare;
	int ret = kstrtos32(buf, 10, &prepare);
	int delay = msecs_to_jiffies(CS35L41_CAL_COMPLETE_DELAY_MS);
	int retries = 5;
	unsigned int cal_state;
	int amp, index;
	struct regmap *regmap;

	if (cirrus_cal->cal_running) {
		dev_err(cirrus_cal->dev,
			"cirrus_cal measurement in progress\n");
		return size;
	}

	mutex_lock(&cirrus_cal->lock);

	if (ret == 0 && prepare == 1) {
		cirrus_cal->cal_running = 1;

		for (amp = 0; amp < cirrus_cal->num_amps; amp++) {
			regmap = cirrus_cal->amps[amp].regmap;
			index = cirrus_cal->amps[amp].index;

			regmap_write(regmap, CS35L41_CAL_STATUS,0);
			regmap_write(regmap, CS35L41_CAL_RDC, 0);
			regmap_write(regmap, CS35L41_CAL_AMBIENT, 0);
			regmap_write(regmap, CS35L41_CAL_CHECKSUM,0);

			cirrus_cal_start(&cirrus_cal->amps[amp]);
			usleep_range(80000, 90000);

			regmap_read(regmap, CS35L41_CSPL_STATE,
				&cal_state);

			while (cal_state == CS35L41_CSPL_STATE_ERROR &&
				retries > 0) {
				if (cal_state == CS35L41_CSPL_STATE_ERROR) {
					dev_err(cirrus_cal->dev,
					    "Calibration load error\n");
				}

				cirrus_cal_start(&cirrus_cal->amps[amp]);
				usleep_range(80000, 90000);
				regmap_read(regmap, CS35L41_CSPL_STATE,
					&cal_state);
				retries--;
			}

			if (retries == 0) {
				dev_err(cirrus_cal->dev,
					"Calibration setup fail @ %d\n", amp);
				mutex_unlock(&cirrus_cal->lock);
				cirrus_cal_unmute_dsp_inputs(regmap, index);
				cirrus_cal->cal_running = 0;
				return size;
			}
		}

		dev_dbg(cirrus_cal->dev,
			"Calibration prepare complete\n");

		queue_delayed_work(system_unbound_wq,
					&cirrus_cal->cal_complete_work,
					delay);

	}

	mutex_unlock(&cirrus_cal->lock);
	return size;
}

static ssize_t cirrus_cal_v_status_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return sprintf(buf, "%s\n",
			cirrus_cal->cal_running ? "Enabled" : "Disabled");
}

static ssize_t cirrus_cal_v_status_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	int prepare;
	int ret = kstrtos32(buf, 10, &prepare);
	int retries = 5;
	unsigned int cal_state;
	int i, reg, amp, index;
	struct regmap *regmap;
	unsigned int vmax[CIRRUS_MAX_AMPS];
	unsigned int vmin[CIRRUS_MAX_AMPS];
	unsigned int imax[CIRRUS_MAX_AMPS];
	unsigned int imin[CIRRUS_MAX_AMPS];

	if (cirrus_cal->cal_running) {
		dev_err(cirrus_cal->dev,
			"cirrus_cal measurement in progress\n");
		return size;
	}

	mutex_lock(&cirrus_cal->lock);

	if (ret == 0 && prepare == 1) {
		cirrus_cal->cal_running = 1;

		for (amp = 0; amp < cirrus_cal->num_amps; amp++) {
			regmap = cirrus_cal->amps[amp].regmap;
			index = cirrus_cal->amps[amp].index;

			regmap_write(regmap, CIRRUS_PWR_CSPL_V_PEAK, 0);
			regmap_write(regmap, CIRRUS_PWR_CSPL_I_PEAK, 0);

			vmax[amp] = 0;
			vmin[amp] = INT_MAX;
			imax[amp] = 0;
			imin[amp] = INT_MAX;

			cirrus_cal_start(&cirrus_cal->amps[amp]);
			usleep_range(80000, 90000);

			regmap_read(regmap, CS35L41_CSPL_STATE,
				&cal_state);
			
			while (cal_state == CS35L41_CSPL_STATE_ERROR &&
				retries > 0) {
				if (cal_state == CS35L41_CSPL_STATE_ERROR) {
					dev_err(cirrus_cal->dev,
					 "Calibration load error\n");
				}

				cirrus_cal_start(&cirrus_cal->amps[amp]);
				usleep_range(80000, 90000);
				regmap_read(regmap, CS35L41_CSPL_STATE,
					&cal_state);
				retries--;
			}

			if (retries == 0) {
				dev_err(cirrus_cal->dev,
					"Calibration setup fail @ %d\n", amp);
				mutex_unlock(&cirrus_cal->lock);
				cirrus_cal_unmute_dsp_inputs(regmap, index);
				cirrus_cal->cal_running = 0;
				return size;
			}
		}

		dev_info(cirrus_cal->dev,
			 "V validation prepare complete\n");

		for (i = 0; i < 400; i++) {
			for (amp = 0; amp < cirrus_cal->num_amps; amp++) {
				regmap = cirrus_cal->amps[amp].regmap;
				regmap_read(regmap,
					CIRRUS_PWR_CSPL_V_PEAK, &reg);
				if (reg > vmax[amp]) vmax[amp] = reg;
				if (reg < vmin[amp]) vmin[amp] = reg;

				regmap_read(regmap,
					CIRRUS_PWR_CSPL_I_PEAK, &reg);
				if (reg > imax[amp]) imax[amp] = reg;
				if (reg < imin[amp]) imin[amp] = reg;

				usleep_range(50, 150);
			}
		}

		for (amp = 0; amp < cirrus_cal->num_amps; amp++) {

			dev_info(cirrus_cal->dev,
				"V Validation results for cs35l41%s\n",
				cirrus_cal->amps[amp].mfd_suffix);

			dev_dbg(cirrus_cal->dev, "V Max: 0x%x\n", vmax[amp]);
			vmax[amp] = cirrus_cal_vpk_to_mv(vmax[amp]);
			dev_info(cirrus_cal->dev, "V Max: %d mV\n", vmax[amp]);

			dev_dbg(cirrus_cal->dev, "V Min: 0x%x\n", vmin[amp]);
			vmin[amp] = cirrus_cal_vpk_to_mv(vmin[amp]);
			dev_info(cirrus_cal->dev, "V Min: %d mV\n", vmin[amp]);

			dev_dbg(cirrus_cal->dev, "I Max: 0x%x\n", imax[amp]);
			imax[amp] = cirrus_cal_ipk_to_ma(imax[amp]);
			dev_info(cirrus_cal->dev, "I Max: %d mA\n", imax[amp]);

			dev_dbg(cirrus_cal->dev, "I Min: 0x%x\n", imin[amp]);
			imin[amp] = cirrus_cal_ipk_to_ma(imin[amp]);
			dev_info(cirrus_cal->dev, "I Min: %d mA\n", imin[amp]);

			if (vmax[amp] < CIRRUS_CAL_V_VAL_UB_MV &&
			    vmax[amp] > CIRRUS_CAL_V_VAL_LB_MV) {
				cirrus_cal->v_validation[amp] = 1;
				dev_info(cirrus_cal->dev,
					"V validation success\n");
			} else {
				cirrus_cal->v_validation[amp] = 0xCC;
				dev_err(cirrus_cal->dev,
					"V validation failed\n");
			}

		}

		cirrus_cal_v_val_complete();

		cirrus_cal->cal_running = 0;

	}

	mutex_unlock(&cirrus_cal->lock);
	return size;
}

#ifdef CS35L41_FACTORY_RECOVERY_SYSFS
static ssize_t cirrus_cal_reinit_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return sprintf(buf, "\n");
}

static ssize_t cirrus_cal_reinit_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	int reinit, i;
	int ret = kstrtos32(buf, 10, &reinit);
	struct cirrus_mfd_amp *amp;

	if (cirrus_cal->cal_running) {
		dev_err(cirrus_cal->dev,
			"cirrus_cal measurement in progress\n");
		return size;
	}

	mutex_lock(&cirrus_cal->lock);

	if (ret == 0 && reinit == 1) {
		for (i = 0; i < cirrus_cal->num_amps; i++) {
			amp = &cirrus_cal->amps[i];
			if (amp && cirrus_cal->codecs[amp->index])
				cs35l41_reinit(cirrus_cal->codecs[amp->index]);
		}
	}

	mutex_unlock(&cirrus_cal->lock);

	return size;
}
#endif /* CS35L41_FACTORY_RECOVERY_SYSFS*/

static ssize_t cirrus_cal_vval_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	const char *suffix = &(attr->attr.name[strlen("v_validation")]);
	struct cirrus_mfd_amp *amp = cirrus_cal_get_amp_from_suffix(suffix);
	dev_info(dev, "%s\n", __func__);

	return sprintf(buf, "%d", cirrus_cal->v_validation[amp->index]);
}

static ssize_t cirrus_cal_vval_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	dev_info(dev, "%s\n", __func__);
	return 0;
}

static ssize_t cirrus_cal_rdc_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	unsigned int rdc;
	const char *suffix = &(attr->attr.name[strlen("rdc")]);
	struct cirrus_mfd_amp *amp = cirrus_cal_get_amp_from_suffix(suffix);
	struct regmap *regmap = amp->regmap;

	if (amp) {
		regmap_read(regmap, CS35L41_CAL_RDC, &rdc);
		return sprintf(buf, "%d", rdc);
	} else
		return 0;
}

static ssize_t cirrus_cal_rdc_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	int rdc, ret;
	const char *suffix = &(attr->attr.name[strlen("rdc")]);
	struct cirrus_mfd_amp *amp = cirrus_cal_get_amp_from_suffix(suffix);
	struct regmap *regmap = amp->regmap;

	ret = kstrtos32(buf, 10, &rdc);
	if (ret == 0 && amp)
		regmap_write(regmap, CS35L41_CAL_RDC, rdc);
	return size;
}

static ssize_t cirrus_cal_temp_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	unsigned int temp;
	const char *suffix = &(attr->attr.name[strlen("temp")]);
	struct cirrus_mfd_amp *amp = cirrus_cal_get_amp_from_suffix(suffix);
	struct regmap *regmap = amp->regmap;

	if (amp) {
		regmap_read(regmap, CS35L41_CAL_AMBIENT, &temp);
		return sprintf(buf, "%d", temp);
	} else
		return 0;
}

static ssize_t cirrus_cal_temp_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	int temp, ret;
	const char *suffix = &(attr->attr.name[strlen("temp")]);
	struct cirrus_mfd_amp *amp = cirrus_cal_get_amp_from_suffix(suffix);
	struct regmap *regmap = amp->regmap;

	ret = kstrtos32(buf, 10, &temp);
	if (ret == 0 && amp)
		regmap_write(regmap, CS35L41_CAL_AMBIENT, temp);
	return size;
}

static ssize_t cirrus_cal_checksum_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	unsigned int checksum;
	const char *suffix = &(attr->attr.name[strlen("checksum")]);
	struct cirrus_mfd_amp *amp = cirrus_cal_get_amp_from_suffix(suffix);
	struct regmap *regmap = amp->regmap;

	if (amp) {
		regmap_read(regmap, CS35L41_CAL_CHECKSUM, &checksum);
		return sprintf(buf, "%d", checksum);
	} else
		return 0;
}

static ssize_t cirrus_cal_checksum_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	int checksum, ret;
	const char *suffix = &(attr->attr.name[strlen("checksum")]);
	struct cirrus_mfd_amp *amp = cirrus_cal_get_amp_from_suffix(suffix);
	struct regmap *regmap = amp->regmap;

	ret = kstrtos32(buf, 10, &checksum);
	if (ret == 0 && amp)
		regmap_write(regmap, CS35L41_CAL_CHECKSUM, checksum);
	return size;
}

static ssize_t cirrus_cal_set_status_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	unsigned int set_status;
	const char *suffix = &(attr->attr.name[strlen("set_status")]);
	struct cirrus_mfd_amp *amp = cirrus_cal_get_amp_from_suffix(suffix);
	struct regmap *regmap = amp->regmap;

	if (amp) {
		regmap_read(regmap, CS35L41_CAL_SET_STATUS, &set_status);
		return sprintf(buf, "%d", set_status);
	} else
		return 0;
}

static ssize_t cirrus_cal_set_status_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	return 0;
}

static ssize_t cirrus_cal_rdc_stored_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	unsigned int rdc = 0;
	const char *suffix = &(attr->attr.name[strlen("rdc_stored")]);
	char *efs_name;
	
	efs_name = kzalloc(PAGE_SIZE, GFP_KERNEL);
	snprintf(efs_name,
		PAGE_SIZE, "%s%s",
		CIRRUS_CAL_RDC_SAVE_LOCATION,
		suffix);

	cirrus_cal_read_file(efs_name, &rdc);
	return sprintf(buf, "%d", rdc);
}

static ssize_t cirrus_cal_rdc_stored_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	return 0;
}

static ssize_t cirrus_cal_temp_stored_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	unsigned int temp_stored = 0;

	cirrus_cal_read_file(CIRRUS_CAL_TEMP_SAVE_LOCATION, &temp_stored);
	return sprintf(buf, "%d", temp_stored);
}

static ssize_t cirrus_cal_temp_stored_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t size)
{
	return 0;
}

static DEVICE_ATTR(version, 0444, cirrus_cal_version_show,
				cirrus_cal_version_store);
static DEVICE_ATTR(status, 0664, cirrus_cal_status_show,
				cirrus_cal_status_store);
static DEVICE_ATTR(v_status, 0664, cirrus_cal_v_status_show,
				cirrus_cal_v_status_store);
static DEVICE_ATTR(temp_stored, 0444, cirrus_cal_temp_stored_show,
				cirrus_cal_temp_stored_store);
#ifdef CS35L41_FACTORY_RECOVERY_SYSFS
static DEVICE_ATTR(reinit, 0664, cirrus_cal_reinit_show,
				cirrus_cal_reinit_store);
#endif /* CS35L41_FACTORY_RECOVERY_SYSFS */

static struct device_attribute generic_amp_attrs[CIRRUS_CAL_NUM_ATTRS_AMP] = {
	{
		.attr = {.mode = VERIFY_OCTAL_PERMISSIONS(0444)},
		.show = cirrus_cal_vval_show,
		.store = cirrus_cal_vval_store,
	},
	{
		.attr = {.mode = VERIFY_OCTAL_PERMISSIONS(0664)},
		.show = cirrus_cal_rdc_show,
		.store = cirrus_cal_rdc_store,
	},
	{
		.attr = {.mode = VERIFY_OCTAL_PERMISSIONS(0664)},
		.show = cirrus_cal_temp_show,
		.store = cirrus_cal_temp_store,
	},
	{
		.attr = {.mode = VERIFY_OCTAL_PERMISSIONS(0664)},
		.show = cirrus_cal_checksum_show,
		.store = cirrus_cal_checksum_store,
	},
	{
		.attr = {.mode = VERIFY_OCTAL_PERMISSIONS(0444)},
		.show = cirrus_cal_set_status_show,
		.store = cirrus_cal_set_status_store,
	},
	{
		.attr = {.mode = VERIFY_OCTAL_PERMISSIONS(0444)},
		.show = cirrus_cal_rdc_stored_show,
		.store = cirrus_cal_rdc_stored_store,
	},
};

static const char *generic_amp_attr_names[CIRRUS_CAL_NUM_ATTRS_AMP] = {
	"v_validation",
	"rdc",
	"temp",
	"checksum",
	"set_status",
	"rdc_stored"
};

static struct attribute *cirrus_cal_attr_base[] = {
	&dev_attr_version.attr,
	&dev_attr_status.attr,
	&dev_attr_v_status.attr,
	&dev_attr_temp_stored.attr,
#ifdef CS35L41_FACTORY_RECOVERY_SYSFS
	&dev_attr_reinit.attr,
#endif /* CS35L41_FACTORY_RECOVERY_SYSFS */
	NULL,
};

/* Kernel does not allow attributes to be dynamically allocated */
static struct device_attribute
		amp_attrs_prealloc[CIRRUS_MAX_AMPS][CIRRUS_CAL_NUM_ATTRS_AMP];
static char attr_names_prealloc[CIRRUS_MAX_AMPS][CIRRUS_CAL_NUM_ATTRS_AMP][20];

struct device_attribute *cirrus_cal_create_amp_attrs(const char *mfd_suffix,
							int index)
{
	struct device_attribute *amp_attrs_new;
	int i, suffix_len = strlen(mfd_suffix);

	amp_attrs_new = &(amp_attrs_prealloc[index][0]);
	if (amp_attrs_new == NULL)
		return amp_attrs_new;

	memcpy(amp_attrs_new, &generic_amp_attrs,
		sizeof(struct device_attribute) *
		CIRRUS_CAL_NUM_ATTRS_AMP);

	for (i = 0; i < CIRRUS_CAL_NUM_ATTRS_AMP; i++) {
		amp_attrs_new[i].attr.name = attr_names_prealloc[index][i];
		snprintf((char *)amp_attrs_new[i].attr.name,
			strlen(generic_amp_attr_names[i]) + suffix_len + 1,
			"%s%s", generic_amp_attr_names[i], mfd_suffix);
	}

	return amp_attrs_new;
}

int cirrus_cal_init(struct class *cirrus_amp_class, int num_amps,
					const char **mfd_suffixes)
{
	int ret, i, j;
	struct device_attribute *new_attrs;

	cirrus_cal = kzalloc(sizeof(struct cirrus_cal_t), GFP_KERNEL);
	if (cirrus_cal == NULL)
		return -ENOMEM;

	cirrus_cal->amps = kzalloc(sizeof(struct cirrus_mfd_amp) * num_amps,
								GFP_KERNEL);
	if (cirrus_cal->amps == NULL) {
		kfree(cirrus_cal);
		return -ENOMEM;
	}

	cirrus_cal->num_amps = num_amps;

	for (i = 0; i < num_amps; i++)
	{
		cirrus_cal->amps[i].mfd_suffix = mfd_suffixes[i];
		cirrus_cal->amps[i].index = i;

		cirrus_cal->efs_cache_read[i] = 0;
		cirrus_cal->v_validation[i] = 0;
	}

	cirrus_cal->cal_class = cirrus_amp_class;
	if (IS_ERR(cirrus_cal->cal_class)) {
		pr_err("Failed to register cirrus_cal\n");
		ret = -EINVAL;
		goto err;
	}

	cirrus_cal->dev = device_create(cirrus_cal->cal_class, NULL, 1, NULL,
						CIRRUS_CAL_DIR_NAME);
	if (IS_ERR(cirrus_cal->dev)) {
		ret = PTR_ERR(cirrus_cal->dev);
		pr_err("Failed to create cirrus_cal device\n");
		class_destroy(cirrus_cal->cal_class);
		goto err;
	}

	cirrus_cal_attr_grp.attrs = kzalloc(sizeof(struct attribute *) *
					(CIRRUS_CAL_NUM_ATTRS_AMP * num_amps +
					CIRRUS_CAL_NUM_ATTRS_BASE + 1),
								GFP_KERNEL);
	for (i = 0; i < num_amps; i++) {
		new_attrs = cirrus_cal_create_amp_attrs(mfd_suffixes[i], i);
		for (j = 0; j < CIRRUS_CAL_NUM_ATTRS_AMP; j++) {
			dev_dbg(cirrus_cal->dev, "New attribute: %s\n",
				new_attrs[j].attr.name);
			cirrus_cal_attr_grp.attrs[i * CIRRUS_CAL_NUM_ATTRS_AMP
						  + j] = &new_attrs[j].attr;
		}
	}

	memcpy(&cirrus_cal_attr_grp.attrs[num_amps * CIRRUS_CAL_NUM_ATTRS_AMP],
		cirrus_cal_attr_base, sizeof(struct attribute *) *
					CIRRUS_CAL_NUM_ATTRS_BASE);
	cirrus_cal_attr_grp.attrs[num_amps * CIRRUS_CAL_NUM_ATTRS_AMP +
			CIRRUS_CAL_NUM_ATTRS_BASE] = NULL;

	ret = sysfs_create_group(&cirrus_cal->dev->kobj, &cirrus_cal_attr_grp);
	if (ret) {
		dev_err(cirrus_cal->dev, "Failed to create sysfs group\n");
		goto err;
	}


	mutex_init(&cirrus_cal->lock);
	INIT_DELAYED_WORK(&cirrus_cal->cal_complete_work,
						cirrus_cal_complete_work);

	return 0;
err:
	kfree(cirrus_cal->amps);
	kfree(cirrus_cal);
	return ret;
}

void cirrus_cal_exit(void)
{
	kfree(cirrus_cal->amps);
	kfree(cirrus_cal);
}


