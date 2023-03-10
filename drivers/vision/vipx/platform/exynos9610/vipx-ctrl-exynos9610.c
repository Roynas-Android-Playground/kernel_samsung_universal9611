/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/io.h>
#include <linux/delay.h>

#include "vipx-log.h"
#include "vipx-system.h"
#include "platform/vipx-ctrl.h"

enum vipx_reg_ss1_id {
	REG_SS1_VERSION_ID,
	REG_SS1_QCHANNEL,
	REG_SS1_IRQ_FROM_DEVICE,
	REG_SS1_IRQ0_TO_DEVICE,
	REG_SS1_IRQ1_TO_DEVICE,
	REG_SS1_GLOBAL_CTRL,
	REG_SS1_CORTEX_CONTROL,
	REG_SS1_CPU_CTRL,
	REG_SS1_PROGRAM_COUNTER,
	REG_SS1_DEBUG0,
	REG_SS1_DEBUG1,
	REG_SS1_DEBUG2,
	REG_SS1_DEBUG3,
	REG_SS1_VIP_STATE,
	REG_SS1_VIP_PWR_DWN,
	REG_SS1_VIP_PC,
	REG_SS1_VIP_LR,
	REG_SS1_VIP_SP,
	REG_SS1_VIP_SPV,
	REG_SS1_VIP_SR,
	REG_SS1_VIP_ILR,
	REG_SS1_VIP_IM,
	REG_SS1_VIP_IC0,
	REG_SS1_VIP_IC1,
	REG_SS1_VIP_IC2,
	REG_SS1_VIP_IS0,
	REG_SS1_VIP_IS1,
	REG_SS1_VIP_IS2,
	REG_SS1_VIP_LE0,
	REG_SS1_VIP_LE1,
	REG_SS1_VIP_LE2,
	REG_SS1_CC_REQ_TO_VIP0,
	REG_SS1_CC_REQ_TO_VIP1,
	REG_SS1_INT_CC_REQ_MASK0,
	REG_SS1_INT_CC_REQ_MASK1,
	REG_SS1_CC_REQ_MASK0,
	REG_SS1_CC_REQ_MASK1,
	REG_SS1_VIP_DONE0,
	REG_SS1_VIP_DONE1,
	REG_SS1_INT_VIP_DONE_CC_MASK0,
	REG_SS1_INT_VIP_DONE_CC_MASK1,
	REG_SS1_INT_VIP_DONE_HOST_MASK0,
	REG_SS1_INT_VIP_DONE_HOST_MASK1,
	REG_SS1_SDMA_ERR_CODES,
	REG_SS1_SDMA_STATUS_SE,
	REG_SS1_SDMA_STATUS_INT_SOURCE,
	REG_SS1_SDMA_STATUS_VC_INFO,
	REG_SS1_MAX
};

enum vipx_reg_ss2_id {
	REG_SS2_QCHANNEL,
	REG_SS2_GLOBAL_CTRL,
	REG_SS2_VIP_STATE,
	REG_SS2_VIP_PWR_DWN,
	REG_SS2_VIP_PC,
	REG_SS2_VIP_LR,
	REG_SS2_VIP_SP,
	REG_SS2_VIP_SPV,
	REG_SS2_VIP_SR,
	REG_SS2_VIP_ILR,
	REG_SS2_VIP_IM,
	REG_SS2_VIP_IC0,
	REG_SS2_VIP_IC1,
	REG_SS2_VIP_IC2,
	REG_SS2_VIP_IS0,
	REG_SS2_VIP_IS1,
	REG_SS2_VIP_IS2,
	REG_SS2_VIP_LE0,
	REG_SS2_VIP_LE1,
	REG_SS2_VIP_LE2,
	REG_SS2_CC_REQ_TO_VIP0,
	REG_SS2_CC_REQ_TO_VIP1,
	REG_SS2_INT_CC_REQ_MASK0,
	REG_SS2_INT_CC_REQ_MASK1,
	REG_SS2_CC_REQ_MASK0,
	REG_SS2_CC_REQ_MASK1,
	REG_SS2_VIP_DONE0,
	REG_SS2_VIP_DONE1,
	REG_SS2_INT_VIP_DONE_CC_MASK0,
	REG_SS2_INT_VIP_DONE_CC_MASK1,
	REG_SS2_INT_VIP_DONE_HOST_MASK0,
	REG_SS2_INT_VIP_DONE_HOST_MASK1,
	REG_SS2_SDMA_ERR_CODES,
	REG_SS2_SDMA_STATUS_SE,
	REG_SS2_SDMA_STATUS_INT_SOURCE,
	REG_SS2_SDMA_STATUS_VC_INFO,
	REG_SS2_MAX
};

enum vipx_reg_sysreg1_id {
	REG_SYSREG1_SFR_APB,
	REG_SYSREG1_BUS_COMPONENT_DRCG_EN,
	REG_SYSREG1_MEMCLK,
	REG_SYSREG1_AxCACHE,
	REG_SYSREG1_AxQOS,
	REG_SYSREG1_AxPROT,
	REG_SYSREG1_AxCNTL,
	REG_SYSREG1_CM7_STATUS0,
	REG_SYSREG1_CM7_STATUS1,
	REG_SYSREG1_CM7_STATUS2,
	REG_SYSREG1_CM7_STATUS3,
	REG_SYSREG1_SHARABILITY_CTRL,
	REG_SYSREG1_VIPX1_DBG_MODE,
	REG_SYSREG1_VIPX1_QCH_MON,
	REG_SYSREG1_XIU_D_VIPX_S0,
	REG_SYSREG1_XIU_D_VIPX1_S1,
	REG_SYSREG1_XIU_D_VIPX_M0,
	REG_SYSREG1_MAX
};

enum vipx_reg_sysreg2_id {
	REG_SYSREG2_SFR_APB,
	REG_SYSREG2_BUS_COMPONENT_DRCG_EN,
	REG_SYSREG2_MEMCLK,
	REG_SYSREG2_AxCACHE,
	REG_SYSREG2_AxQOS,
	REG_SYSREG2_AxPROT,
	REG_SYSREG2_AxCNTL,
	REG_SYSREG2_SHARABILITY_CTRL,
	REG_SYSREG2_VIPX1_DBG_MODE,
	REG_SYSREG2_VIPX1_QCH_MON,
	REG_SYSREG2_MAX
};

static const struct vipx_reg regs_ss1[] = {
	{ 0x0000, "SS1_VERSION_ID" },
	{ 0x0004, "SS1_QCHANNEL" },
	{ 0x0008, "SS1_IRQ_FROM_DEVICE" },
	{ 0x000C, "SS1_IRQ0_TO_DEVICE" },
	{ 0x0010, "SS1_IRQ1_TO_DEVICE" },
	{ 0x0014, "SS1_GLOBAL_CTRL" },
	{ 0x0018, "SS1_CORTEX_CONTROL" },
	{ 0x001C, "SS1_CPU_CTRL" },
	{ 0x0044, "SS1_PROGRAM_COUNTER" },
	{ 0x0048, "SS1_DEBUG0" },
	{ 0x004C, "SS1_DEBUG1" },
	{ 0x0050, "SS1_DEBUG2" },
	{ 0x0054, "SS1_DEBUG3" },
	{ 0x8048, "SS1_VIP_STATE" },
	{ 0x8058, "SS1_VIP_PWR_DWN" },
	{ 0x8060, "SS1_VIP_PC" },
	{ 0x8064, "SS1_VIP_LR" },
	{ 0x8068, "SS1_VIP_SP" },
	{ 0x806C, "SS1_VIP_SPV" },
	{ 0x8070, "SS1_VIP_SR" },
	{ 0x8074, "SS1_VIP_ILR" },
	{ 0x8078, "SS1_VIP_IM" },
	{ 0x807C, "SS1_VIP_IC0" },
	{ 0x8080, "SS1_VIP_IC1" },
	{ 0x8084, "SS1_VIP_IC2" },
	{ 0x8088, "SS1_VIP_IS0" },
	{ 0x808C, "SS1_VIP_IS1" },
	{ 0x8090, "SS1_VIP_IS2" },
	{ 0x8094, "SS1_VIP_LE0" },
	{ 0x8098, "SS1_VIP_LE1" },
	{ 0x809C, "SS1_VIP_LE2" },
	{ 0x9000, "SS1_CC_REQ_TO_VIP0" },
	{ 0x9004, "SS1_CC_REQ_TO_VIP1" },
	{ 0x9010, "SS1_INT_CC_REQ_MASK0" },
	{ 0x9014, "SS1_INT_CC_REQ_MASK1" },
	{ 0x9020, "SS1_CC_REQ_MASK0" },
	{ 0x9024, "SS1_CC_REQ_MASK1" },
	{ 0x9040, "SS1_VIP_DONE0" },
	{ 0x9044, "SS1_VIP_DONE1" },
	{ 0x9050, "SS1_INT_VIP_DONE_CC_MASK0" },
	{ 0x9054, "SS1_INT_VIP_DONE_CC_MASK1" },
	{ 0x9058, "SS1_INT_VIP_DONE_HOST_MASK0" },
	{ 0x905C, "SS1_INT_VIP_DONE_HOST_MASK1" },
	{ 0xC028, "SS1_SDMA_ERR_CODES" },
	{ 0xC500, "SS1_SDMA_STATUS_SE" },
	{ 0xC504, "SS1_SDMA_STATUS_INT_SOURCE" },
	{ 0xC508, "SS1_SDMA_STATUS_VC_INFO" },
};

static const struct vipx_reg regs_ss2[] = {
	{ 0x0000, "SS2_QCHANNEL" },
	{ 0x0004, "SS2_GLOBAL_CTRL" },
	{ 0x8048, "SS2_VIP_STATE" },
	{ 0x8058, "SS2_VIP_PWR_DWN" },
	{ 0x8060, "SS2_VIP_PC" },
	{ 0x8064, "SS2_VIP_LR" },
	{ 0x8068, "SS2_VIP_SP" },
	{ 0x806C, "SS2_VIP_SPV" },
	{ 0x8070, "SS2_VIP_SR" },
	{ 0x8074, "SS2_VIP_ILR" },
	{ 0x8078, "SS2_VIP_IM" },
	{ 0x807C, "SS2_VIP_IC0" },
	{ 0x8080, "SS2_VIP_IC1" },
	{ 0x8084, "SS2_VIP_IC2" },
	{ 0x8088, "SS2_VIP_IS0" },
	{ 0x808C, "SS2_VIP_IS1" },
	{ 0x8090, "SS2_VIP_IS2" },
	{ 0x8094, "SS2_VIP_LE0" },
	{ 0x8098, "SS2_VIP_LE1" },
	{ 0x809C, "SS2_VIP_LE2" },
	{ 0x9000, "SS2_CC_REQ_TO_VIP0" },
	{ 0x9004, "SS2_CC_REQ_TO_VIP1" },
	{ 0x9010, "SS2_INT_CC_REQ_MASK0" },
	{ 0x9014, "SS2_INT_CC_REQ_MASK1" },
	{ 0x9020, "SS2_CC_REQ_MASK0" },
	{ 0x9024, "SS2_CC_REQ_MASK1" },
	{ 0x9040, "SS2_VIP_DONE0" },
	{ 0x9044, "SS2_VIP_DONE1" },
	{ 0x9050, "SS2_INT_VIP_DONE_CC_MASK0" },
	{ 0x9054, "SS2_INT_VIP_DONE_CC_MASK1" },
	{ 0x9058, "SS2_INT_VIP_DONE_HOST_MASK0" },
	{ 0x905C, "SS2_INT_VIP_DONE_HOST_MASK1" },
	{ 0xC028, "SS2_SDMA_ERR_CODES" },
	{ 0xC500, "SS2_SDMA_STATUS_SE" },
	{ 0xC504, "SS2_SDMA_STATUS_INT_SOURCE" },
	{ 0xC508, "SS2_SDMA_STATUS_VC_INFO" },
};

static const struct vipx_reg regs_sysreg1[] = {
	{ 0x0100, "SYSREG1_SFR_APB" },
	{ 0x0104, "SYSREG1_BUS_COMPONENT_DRCG_EN" },
	{ 0x0108, "SYSREG1_MEMCLK" },
	{ 0x0400, "SYSREG1_AxCACHE" },
	{ 0x0404, "SYSREG1_AxQOS" },
	{ 0x0408, "SYSREG1_AxPROT" },
	{ 0x040C, "SYSREG1_AxCNTL" },
	{ 0x0410, "SYSREG1_CM7_STATUS0" },
	{ 0x0414, "SYSREG1_CM7_STATUS1" },
	{ 0x0418, "SYSREG1_CM7_STATUS2" },
	{ 0x041C, "SYSREG1_CM7_STATUS3" },
	{ 0x0420, "SYSREG1_SHARABILITY_CTRL" },
	{ 0x0424, "SYSREG1_VIPX1_DBG_MODE" },
	{ 0x0428, "SYSREG1_VIPX1_QCH_MON" },
	{ 0x0430, "SYSREG1_XIU_D_VIPX_S0" },
	{ 0x0434, "SYSREG1_XIU_D_VIPX1_S1" },
	{ 0x0438, "SYSREG1_XIU_D_VIPX_M0" },
};

static const struct vipx_reg regs_sysreg2[] = {
	{ 0x0100, "SYSREG2_SFR_APB" },
	{ 0x0104, "SYSREG2_BUS_COMPONENT_DRCG_EN" },
	{ 0x0108, "SYSREG2_MEMCLK" },
	{ 0x0400, "SYSREG2_AxCACHE" },
	{ 0x0404, "SYSREG2_AxQOS" },
	{ 0x0408, "SYSREG2_AxPROT" },
	{ 0x040C, "SYSREG2_AxCNTL" },
	{ 0x0420, "SYSREG2_SHARABILITY_CTRL" },
	{ 0x0424, "SYSREG2_VIPX1_DBG_MODE" },
	{ 0x0428, "SYSREG2_VIPX1_QCH_MON" },
};

static int vipx_exynos9610_ctrl_reset(struct vipx_system *sys)
{
	void __iomem *ss1, *ss2;
	unsigned int val;

	vipx_enter();
	ss1 = sys->reg_ss[REG_SS1];
	ss2 = sys->reg_ss[REG_SS2];

	/* TODO: check delay */
	val = readl(ss1 + regs_ss1[REG_SS1_GLOBAL_CTRL].offset);
	writel(val | 0xF1, ss1 + regs_ss1[REG_SS1_GLOBAL_CTRL].offset);
	udelay(10);

	val = readl(ss2 + regs_ss2[REG_SS2_GLOBAL_CTRL].offset);
	writel(val | 0xF1, ss2 + regs_ss2[REG_SS2_GLOBAL_CTRL].offset);
	udelay(10);

	val = readl(ss1 + regs_ss1[REG_SS1_CPU_CTRL].offset);
	writel(val | 0x1, ss1 + regs_ss1[REG_SS1_CPU_CTRL].offset);
	udelay(10);

	val = readl(ss1 + regs_ss1[REG_SS1_CORTEX_CONTROL].offset);
	writel(val | 0x1, ss1 + regs_ss1[REG_SS1_CORTEX_CONTROL].offset);
	udelay(10);

	writel(0x0, ss1 + regs_ss1[REG_SS1_CPU_CTRL].offset);
	udelay(10);

	val = readl(ss1 + regs_ss1[REG_SS1_QCHANNEL].offset);
	writel(val | 0x1, ss1 + regs_ss1[REG_SS1_QCHANNEL].offset);
	udelay(10);

	val = readl(ss2 + regs_ss2[REG_SS2_QCHANNEL].offset);
	writel(val | 0x1, ss2 + regs_ss2[REG_SS2_QCHANNEL].offset);
	udelay(10);

	vipx_leave();
	return 0;
}

static int vipx_exynos9610_ctrl_start(struct vipx_system *sys)
{
	vipx_enter();
	writel(0x0, sys->reg_ss[REG_SS1] +
			regs_ss1[REG_SS1_CORTEX_CONTROL].offset);
	vipx_leave();
	return 0;
}

static int vipx_exynos9610_ctrl_get_irq(struct vipx_system *sys, int direction)
{
	unsigned int offset;
	int val;

	vipx_enter();
	if (direction == IRQ0_TO_DEVICE) {
		offset = regs_ss1[REG_SS1_IRQ0_TO_DEVICE].offset;
		val = readl(sys->reg_ss[REG_SS1] + offset);
	} else if (direction == IRQ1_TO_DEVICE) {
		offset = regs_ss1[REG_SS1_IRQ1_TO_DEVICE].offset;
		val = readl(sys->reg_ss[REG_SS1] + offset);
	} else if (direction == IRQ_FROM_DEVICE) {
		offset = regs_ss1[REG_SS1_IRQ_FROM_DEVICE].offset;
		val = readl(sys->reg_ss[REG_SS1] + offset);
	} else {
		val = -EINVAL;
		vipx_err("Failed to get irq due to invalid direction (%d)\n",
				direction);
		goto p_err;
	}

	vipx_leave();
	return val;
p_err:
	return val;
}

static int vipx_exynos9610_ctrl_set_irq(struct vipx_system *sys, int direction,
		int val)
{
	int ret;
	unsigned int offset;

	vipx_enter();
	if (direction == IRQ0_TO_DEVICE) {
		offset = regs_ss1[REG_SS1_IRQ0_TO_DEVICE].offset;
		writel(val, sys->reg_ss[REG_SS1] + offset);
	} else if (direction == IRQ1_TO_DEVICE) {
		offset = regs_ss1[REG_SS1_IRQ1_TO_DEVICE].offset;
		writel(val, sys->reg_ss[REG_SS1] + offset);
	} else if (direction == IRQ_FROM_DEVICE) {
		ret = -EINVAL;
		vipx_err("Host can't set irq from device (%d)\n", direction);
		goto p_err;
	} else {
		ret = -EINVAL;
		vipx_err("Failed to set irq due to invalid direction (%d)\n",
				direction);
		goto p_err;
	}

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static int vipx_exynos9610_ctrl_clear_irq(struct vipx_system *sys,
		int direction, int val)
{
	int ret;
	unsigned int offset;

	vipx_enter();
	if (direction == IRQ0_TO_DEVICE || direction == IRQ1_TO_DEVICE) {
		ret = -EINVAL;
		vipx_err("Irq to device must be cleared at device (%d)\n",
				direction);
		goto p_err;
	} else if (direction == IRQ_FROM_DEVICE) {
		offset = regs_ss1[REG_SS1_IRQ_FROM_DEVICE].offset;
		writel(val, sys->reg_ss[REG_SS1] + offset);
	} else {
		ret = -EINVAL;
		vipx_err("direction of irq is invalid (%d)\n", direction);
		goto p_err;
	}

	vipx_leave();
	return 0;
p_err:
	return ret;
}

static int vipx_exynos9610_ctrl_debug_dump(struct vipx_system *sys)
{
	void __iomem *ss1, *ss2;
	int idx;
	unsigned int val;
	const char *name;

	vipx_enter();
	ss1 = sys->reg_ss[REG_SS1];
	ss2 = sys->reg_ss[REG_SS2];

	vipx_info("SS1 SFR Dump (count:%d)\n", REG_SS1_MAX);
	for (idx = 0; idx < REG_SS1_MAX; ++idx) {
		val = readl(ss1 + regs_ss1[idx].offset);
		name = regs_ss1[idx].name;
		vipx_info("[%2d][%32s] 0x%08x\n", idx, name, val);
	}

	vipx_info("SS2 SFR Dump (count:%d)\n", REG_SS2_MAX);
	for (idx = 0; idx < REG_SS2_MAX; ++idx) {
		val = readl(ss2 + regs_ss2[idx].offset);
		name = regs_ss2[idx].name;
		vipx_info("[%2d][%32s] 0x%08x\n", idx, name, val);
	}

	vipx_leave();
	return 0;
}

static int vipx_exynos9610_ctrl_hex_dump(struct vipx_system *sys)
{
	unsigned int temp[64] = { 0, };
	unsigned int temp_size;
	int idx;
	void __iomem *reg_base;
	const struct vipx_reg *reg_offset;

	vipx_enter();
	temp_size = ARRAY_SIZE(temp);
	if (temp_size < REG_SS1_MAX || temp_size < REG_SS2_MAX ||
			temp_size < REG_SYSREG1_MAX ||
			temp_size < REG_SYSREG2_MAX) {
		vipx_err("temp size for dump is too small(%u/%u/%u/%u/%u)\n",
				temp_size, REG_SS1_MAX, REG_SS2_MAX,
				REG_SYSREG1_MAX, REG_SYSREG2_MAX);
		return -EINVAL;
	}

	reg_base = sys->reg_ss[REG_SS1];
	reg_offset = regs_ss1;
	for (idx = 0; idx < REG_SS1_MAX; ++idx)
		temp[idx] = readl(reg_base + reg_offset[idx].offset);

	for (idx = 0; idx < REG_SS1_MAX; idx += 8)
		vipx_info("[%2d] %8x %8x %8x %8x %8x %8x %8x %8x\n",
				idx, temp[idx], temp[idx + 1], temp[idx + 2],
				temp[idx + 3], temp[idx + 4], temp[idx + 5],
				temp[idx + 6], temp[idx + 7]);

	reg_base = sys->reg_ss[REG_SS2];
	reg_offset = regs_ss2;
	for (idx = 0; idx < REG_SS2_MAX; ++idx)
		temp[idx] = readl(reg_base + reg_offset[idx].offset);

	for (idx = 0; idx < REG_SS2_MAX; idx += 8)
		vipx_info("[%2d] %8x %8x %8x %8x %8x %8x %8x %8x\n",
				idx, temp[idx], temp[idx + 1], temp[idx + 2],
				temp[idx + 3], temp[idx + 4], temp[idx + 5],
				temp[idx + 6], temp[idx + 7]);

	reg_base = sys->sysreg1;
	reg_offset = regs_sysreg1;
	for (idx = 0; idx < REG_SYSREG1_MAX; ++idx)
		temp[idx] = readl(reg_base + reg_offset[idx].offset);

	for (idx = 0; idx < REG_SYSREG1_MAX; idx += 8)
		vipx_info("[%2d] %8x %8x %8x %8x %8x %8x %8x %8x\n",
				idx, temp[idx], temp[idx + 1], temp[idx + 2],
				temp[idx + 3], temp[idx + 4], temp[idx + 5],
				temp[idx + 6], temp[idx + 7]);

	reg_base = sys->sysreg2;
	reg_offset = regs_sysreg2;
	for (idx = 0; idx < REG_SYSREG2_MAX; ++idx)
		temp[idx] = readl(reg_base + reg_offset[idx].offset);

	for (idx = 0; idx < REG_SYSREG2_MAX; idx += 8)
		vipx_info("[%2d] %8x %8x %8x %8x %8x %8x %8x %8x\n",
				idx, temp[idx], temp[idx + 1], temp[idx + 2],
				temp[idx + 3], temp[idx + 4], temp[idx + 5],
				temp[idx + 6], temp[idx + 7]);

	vipx_leave();
	return 0;
}

const struct vipx_ctrl_ops vipx_ctrl_ops = {
	.reset		= vipx_exynos9610_ctrl_reset,
	.start		= vipx_exynos9610_ctrl_start,
	.get_irq	= vipx_exynos9610_ctrl_get_irq,
	.set_irq	= vipx_exynos9610_ctrl_set_irq,
	.clear_irq	= vipx_exynos9610_ctrl_clear_irq,
	.debug_dump	= vipx_exynos9610_ctrl_debug_dump,
	.hex_dump	= vipx_exynos9610_ctrl_hex_dump,
};
