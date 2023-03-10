/*
 * Copyright (C) 2016 Samsung Electronics. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include "et7xx.h"

int et7xx_io_read_register(struct et7xx_data *etspi, u8 *addr, u8 *buf)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval = 0;
	struct spi_message m;
	int read_len = 1;

	u8 val, addrval;

	struct spi_transfer xfer = {
		.tx_buf = etspi->buf,
		.rx_buf = etspi->buf,
		.len = 3,
	};

	memset(etspi->buf, 0, xfer.len);
	*etspi->buf = OP_REG_R;

	if (copy_from_user(&addrval, (const u8 __user *) (uintptr_t) addr
		, read_len)) {
		pr_err("buffer copy_from_user fail\n");
		return -EFAULT;
	}

	*(etspi->buf + 1) = addrval;

	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	retval = spi_sync(etspi->spi, &m);
	if (retval < 0) {
		pr_err("read data error retval = %d\n", retval);
		return retval;
	}

	val = *(etspi->buf + 2);

	pr_debug("len = %d addr = %x val = %x\n", read_len, addrval, val);

	if (copy_to_user((u8 __user *) (uintptr_t) buf, &val, read_len)) {
		pr_err("buffer copy_to_user fail retval\n");
		return -EFAULT;
	}

	return retval;
#endif
}

int et7xx_io_burst_read_register(struct et7xx_data *etspi,
									struct egis_ioc_transfer *ioc)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval = 0;
	struct spi_message m;
	struct spi_transfer xfer = {
		.tx_buf = etspi->buf,
		.rx_buf = etspi->buf,
		.len = ioc->len + 2,
	};

	if (ioc->len <= 0 || ioc->len + 2 > etspi->bufsiz) {
		retval = -ENOMEM;
		pr_err("error retval = %d\n", retval);
		goto end;
	}

	memset(etspi->buf, 0, xfer.len);
	*etspi->buf = OP_REG_R_S;
	if (copy_from_user(etspi->buf + 1,
			(const u8 __user *) (uintptr_t) ioc->tx_buf, 1)) {
		pr_err("buffer copy_from_user fail\n");
		retval = -EFAULT;
		goto end;
	}
	pr_debug("tx_buf = %p op = %x reg = %x, len = %d\n",
			ioc->tx_buf, *etspi->buf, *(etspi->buf + 1), xfer.len);
	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	retval = spi_sync(etspi->spi, &m);

	if (retval < 0) {
		retval = -ENOMEM;
		pr_err("error retval = %d\n", retval);
		goto end;
	}

	if (copy_to_user((u8 __user *) (uintptr_t)ioc->rx_buf, etspi->buf + 2,
				ioc->len)) {
		retval = -EFAULT;
		pr_err("buffer copy_to_user fail retval\n");
		goto end;
	}
end:
	return retval;
#endif
}

int et7xx_io_burst_read_register_backward(struct et7xx_data *etspi,
											struct egis_ioc_transfer *ioc)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval = 0;
	struct spi_message m;
	struct spi_transfer xfer = {
		.tx_buf = etspi->buf,
		.rx_buf = etspi->buf,
		.len = ioc->len + 2,
	};

	if (ioc->len <= 0 || ioc->len + 2 > etspi->bufsiz) {
		retval = -ENOMEM;
		pr_err("error retval = %d\n", retval);
		goto end;
	}

	memset(etspi->buf, 0, xfer.len);
	*etspi->buf = OP_REG_R_S_BW;
	if (copy_from_user(etspi->buf + 1,
			(const u8 __user *) (uintptr_t)ioc->tx_buf, 1)) {
		pr_err("buffer copy_from_user fail\n");
		retval = -EFAULT;
		goto end;
	}
	pr_debug("tx_buf = %p op = %x reg = %x, len = %d\n",
			ioc->tx_buf, *etspi->buf, *(etspi->buf + 1), xfer.len);
	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	retval = spi_sync(etspi->spi, &m);

	if (retval < 0) {
		retval = -ENOMEM;
		pr_err("error retval = %d\n", retval);
		goto end;
	}

	if (copy_to_user((u8 __user *) (uintptr_t)ioc->rx_buf, etspi->buf + 2,
			ioc->len)) {
		retval = -EFAULT;
		pr_err("buffer copy_to_user fail retval\n");
		goto end;
	}
end:
	return retval;
#endif
}

int et7xx_io_write_register(struct et7xx_data *etspi, u8 *buf)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval = 0;
	int write_len = 2;
	struct spi_message m;

	u8 val[3];

	struct spi_transfer xfer = {
		.tx_buf = etspi->buf,
		.len = 3,
	};

	memset(etspi->buf, 0, xfer.len);
	*etspi->buf = OP_REG_W;

	if (copy_from_user(val, (const u8 __user *) (uintptr_t) buf,
			write_len)) {
		pr_err("buffer copy_from_user fail\n");
		return -EFAULT;
	}

	pr_debug("write_len = %d addr = %x data = %x\n",
			write_len, val[0], val[1]);

	*(etspi->buf + 1) = val[0];
	*(etspi->buf + 2) = val[1];

	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	retval = spi_sync(etspi->spi, &m);
	if (retval < 0) {
		pr_err("read data error retval = %d\n", retval);
		return retval;
	}

	return retval;
#endif
}

int et7xx_io_burst_write_register(struct et7xx_data *etspi,
									struct egis_ioc_transfer *ioc)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval = 0;
	struct spi_message m;
	struct spi_transfer xfer = {
		.tx_buf = etspi->buf,
		.len = ioc->len + 1,
	};

	if (ioc->len <= 0 || ioc->len + 2 > etspi->bufsiz) {
		retval = -ENOMEM;
		pr_err("error retval = %d\n", retval);
		goto end;
	}

	memset(etspi->buf, 0, ioc->len + 1);
	*etspi->buf = OP_REG_W_S;
	if (copy_from_user(etspi->buf + 1,
			(const u8 __user *) (uintptr_t) ioc->tx_buf,
			ioc->len)) {
		pr_err("buffer copy_from_user fail\n");
		retval = -EFAULT;
		goto end;
	}
	pr_debug("tx_buf = %p op = %x reg = %x, len = %d\n",
			ioc->tx_buf, *etspi->buf, *(etspi->buf + 1), xfer.len);
	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	retval = spi_sync(etspi->spi, &m);

	if (retval < 0) {
		pr_err("error retval = %d\n", retval);
		goto end;
	}
end:
	return retval;
#endif
}

int et7xx_io_burst_write_register_backward(struct et7xx_data *etspi,
											struct egis_ioc_transfer *ioc)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval = 0;
	struct spi_message m;
	struct spi_transfer xfer = {
		.tx_buf = etspi->buf,
		.len = ioc->len + 1,
	};

	if (ioc->len <= 0 || ioc->len + 2 > etspi->bufsiz) {
		retval = -ENOMEM;
		pr_err("error retval = %d\n", retval);
		goto end;
	}

	memset(etspi->buf, 0, ioc->len + 1);
	*etspi->buf = OP_REG_W_S_BW;
	if (copy_from_user(etspi->buf + 1,
		(const u8 __user *) (uintptr_t)ioc->tx_buf, ioc->len)) {
		pr_err("buffer copy_from_user fail\n");
		retval = -EFAULT;
		goto end;
	}
	pr_debug("tx_buf = %p op = %x reg = %x, len = %d\n",
		ioc->tx_buf, *etspi->buf, *(etspi->buf + 1), xfer.len);
	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	retval = spi_sync(etspi->spi, &m);

	if (retval < 0) {
		pr_err("error retval = %d\n", retval);
		goto end;
	}
end:
	return retval;
#endif
}

int et7xx_io_read_efuse(struct et7xx_data *etspi, struct egis_ioc_transfer *ioc)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval;
	struct spi_message m;
	u8 *buf = NULL;

	struct spi_transfer xfer = {
		.tx_buf = NULL,
		.rx_buf = NULL,
		.len = ioc->len + 1,
	};

	if (xfer.len >= LARGE_SPI_TRANSFER_BUFFER) {
		if ((xfer.len) % DIVISION_OF_IMAGE != 0)
			xfer.len = xfer.len + (DIVISION_OF_IMAGE -
					(xfer.len % DIVISION_OF_IMAGE));
	}

	buf = kzalloc(xfer.len, GFP_KERNEL);

	if (buf == NULL)
		return -ENOMEM;

	xfer.tx_buf = xfer.rx_buf = buf;
	buf[0] = OP_EF_R;

	pr_debug("len = %d, xfer.len = %d, buf = %p, rx_buf = %p\n",
			ioc->len, xfer.len, buf, ioc->rx_buf);

	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	retval = spi_sync(etspi->spi, &m);
	if (retval < 0) {
		pr_err("read data error retval = %d\n", retval);
		goto end;
	}

	if (copy_to_user((u8 __user *) (uintptr_t) ioc->rx_buf, buf + 1,
			ioc->len)) {
		pr_err("buffer copy_to_user fail retval\n");
		retval = -EFAULT;
	}
end:
	kfree(buf);
	return retval;
#endif
}

int et7xx_io_write_efuse(struct et7xx_data *etspi,
							struct egis_ioc_transfer *ioc)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval;
	struct spi_message m;
	u8 *buf = NULL;

	struct spi_transfer xfer = {
		.tx_buf = NULL,
		.rx_buf = NULL,
		.len = ioc->len + 1,
	};

	if (xfer.len >= LARGE_SPI_TRANSFER_BUFFER) {
		if ((xfer.len) % DIVISION_OF_IMAGE != 0)
			xfer.len = xfer.len + (DIVISION_OF_IMAGE -
					(xfer.len % DIVISION_OF_IMAGE));
	}

	buf = kzalloc(xfer.len, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	if (copy_from_user((u8 __user *) (uintptr_t) buf + 1, ioc->tx_buf,
			ioc->len)) {
		pr_err("buffer copy_from_user fail retval\n");
		retval = -EFAULT;
		goto end;
	}

	xfer.tx_buf = xfer.rx_buf = buf;
	buf[0] = OP_EF_W;

	pr_debug("len = %d, xfer.len = %d, buf = %p, tx_buf = %p\n",
			 ioc->len, xfer.len, buf, ioc->tx_buf);

	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	retval = spi_sync(etspi->spi, &m);

	if (retval < 0)
		pr_err("write data error retval = %d\n", retval);
end:
	kfree(buf);
	return retval;
#endif
}

int et7xx_io_get_frame(struct et7xx_data *etspi, u8 *fr, u32 size)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval;
	struct spi_message m;
	u8 *buf = NULL;

	struct spi_transfer xfer = {
		.tx_buf = NULL,
		.rx_buf = NULL,
		.len = size + 1,
	};

	if (xfer.len >= LARGE_SPI_TRANSFER_BUFFER) {
		if ((xfer.len) % DIVISION_OF_IMAGE != 0)
			xfer.len = xfer.len + (DIVISION_OF_IMAGE -
					(xfer.len % DIVISION_OF_IMAGE));
	}

	buf = kzalloc(xfer.len, GFP_KERNEL);

	if (buf == NULL)
		return -ENOMEM;

	xfer.tx_buf = xfer.rx_buf = buf;
	buf[0] = OP_FB_R;

	pr_debug("size = %d, xfer.len = %d, buf = %p, fr = %p\n",
		size, xfer.len, buf, fr);

	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	retval = spi_sync(etspi->spi, &m);
	if (retval < 0) {
		pr_err("read data error retval = %d\n", retval);
		goto end;
	}

	if (copy_to_user((u8 __user *) (uintptr_t) fr, buf + 1, size)) {
		pr_err("buffer copy_to_user fail retval\n");
		retval = -EFAULT;
	}
end:
	kfree(buf);
	return retval;
#endif
}

int et7xx_io_write_frame(struct et7xx_data *etspi, u8 *fr, u32 size)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval;
	struct spi_message m;
	u8 *buf = NULL;

	struct spi_transfer xfer = {
	    .tx_buf = NULL,
	    .rx_buf = NULL,
	    .len = size + 1,
	};

	if (xfer.len >= LARGE_SPI_TRANSFER_BUFFER) {
		if ((xfer.len) % DIVISION_OF_IMAGE != 0)
			xfer.len = xfer.len + (DIVISION_OF_IMAGE -
					(xfer.len % DIVISION_OF_IMAGE));
	}

	buf = kzalloc(xfer.len, GFP_KERNEL);

	if (buf == NULL)
		return -ENOMEM;

	if (copy_from_user((u8 __user *)(uintptr_t)buf + 1, fr, size)) {
		pr_err("buffer copy_from_user fail retval\n");
		retval = -EFAULT;
		goto end;
	}

	xfer.tx_buf = xfer.rx_buf = buf;
	buf[0] = OP_FB_W;

	pr_debug("size = %d, xfer.len = %d, buf = %p, fr = %p\n",
		size, xfer.len, buf, fr);

	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	retval = spi_sync(etspi->spi, &m);

	if (retval < 0)
		pr_err("write data error retval = %d\n", retval);

end:
	kfree(buf);
	return retval;
#endif
}

int et7xx_io_get_zone_average(struct et7xx_data *etspi, u8 *fr, u32 size)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval;
	struct spi_message m;
	u8 *buf = NULL;

	struct spi_transfer xfer = {
		.tx_buf = NULL,
		.rx_buf = NULL,
		.len = size + 1,
	};

	if (xfer.len >= LARGE_SPI_TRANSFER_BUFFER) {
		if ((xfer.len) % DIVISION_OF_IMAGE != 0)
			xfer.len = xfer.len + (DIVISION_OF_IMAGE -
					(xfer.len % DIVISION_OF_IMAGE));
	}

	buf = kzalloc(xfer.len, GFP_KERNEL);

	if (buf == NULL)
		return -ENOMEM;

	xfer.tx_buf = xfer.rx_buf = buf;
	buf[0] = OP_ZAVG_R;

	pr_debug("size = %d, xfer.len = %d, buf = %p, fr = %p\n",
		size, xfer.len, buf, fr);

	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	retval = spi_sync(etspi->spi, &m);
	if (retval < 0) {
		pr_err("read data error retval = %d\n", retval);
		goto end;
	}

	if (copy_to_user((u8 __user *) (uintptr_t) fr, buf + 1, size)) {
		pr_err("buffer copy_to_user fail retval\n");
		retval = -EFAULT;
	}
end:
	kfree(buf);
	return retval;
#endif
}

int et7xx_io_get_histogram(struct et7xx_data *etspi, u8 *fr, u32 size)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval;
	struct spi_message m;
	u8 *buf = NULL;

	struct spi_transfer xfer = {
		.tx_buf = NULL,
		.rx_buf = NULL,
		.len = size + 1,
	};

	if (xfer.len >= LARGE_SPI_TRANSFER_BUFFER) {
		if ((xfer.len) % DIVISION_OF_IMAGE != 0)
			xfer.len = xfer.len + (DIVISION_OF_IMAGE -
					(xfer.len % DIVISION_OF_IMAGE));
	}

	buf = kzalloc(xfer.len, GFP_KERNEL);

	if (buf == NULL)
		return -ENOMEM;

	xfer.tx_buf = xfer.rx_buf = buf;
	buf[0] = OP_HSTG_R;

	pr_debug("size = %d, xfer.len = %d, buf = %p, fr = %p\n",
		size, xfer.len, buf, fr);

	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	retval = spi_sync(etspi->spi, &m);
	if (retval < 0) {
		pr_err("read data error retval = %d\n", retval);
		goto end;
	}

	if (copy_to_user((u8 __user *) (uintptr_t) fr, buf + 1, size)) {
		pr_err("buffer copy_to_user fail retval\n");
		retval = -EFAULT;
	}
end:
	kfree(buf);
	return retval;
#endif
}

int et7xx_io_read_cis_register(struct et7xx_data *etspi, u8 *addr, u8 *buf)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
#define CIS_READ_TIMEOUT 1000
	int retval = 0, read_len = 2, try_time = 0;
	struct spi_message m;

	u8 tr[] = { OP_CIS_ADDR_R, 0x24, 0x00, 0x00, 0x00, 0x00};
	u8 val;

	struct spi_transfer xfer_addr = {
		.tx_buf = tr,
		.len = 6,
	};
	struct spi_transfer xfer_data = {
		.rx_buf = tr,
		.len = 1,
	};

	if (copy_from_user(&tr[2], (const u8 __user *) (uintptr_t) addr
		, read_len)) {
		pr_err("buffer copy_from_user fail. addr(%p)\n", addr);
		return -EFAULT;
	}
	tr[5] = tr[0] + tr[1] + tr[2] + tr[3] + tr[4];
	pr_info("len(%d) addr(%p) i2c(%x) addrH(%x) addrL(%x) crc(%x) buf(%p)\n",
		read_len, addr, tr[1], tr[2], tr[3], tr[5], buf);
	spi_message_init(&m);
	spi_message_add_tail(&xfer_addr, &m);
	retval = spi_sync(etspi->spi, &m);
	if (retval < 0) {
		pr_err("read data (0x71) error retval = %d\n", retval);
		return retval;
	}

	while (try_time < CIS_READ_TIMEOUT) {
		tr[0] = 0x00;
		spi_message_init(&m);
		spi_message_add_tail(&xfer_data, &m);
		retval = spi_sync(etspi->spi, &m);
		if (retval < 0) {
			pr_err("read data error retval = %d\n", retval);
			return retval;
		}
		if (tr[0] == 0xAA) {
			pr_info("tr[0] = %x, try_time(%d)\n", tr[0], try_time);
			break;
		}
		usleep_range(10, 20);
		try_time++;
	}
	if (try_time >= CIS_READ_TIMEOUT)
		pr_err("TIMEOUT!! try_time >= CIS_READ_TIMEOUT(1000)\n");

	tr[0] = 0x81;
	tr[1] = 0x24;
	tr[2] = 0x00;
	tr[3] = 0x00;
	tr[4] = 0x00;
	tr[5] = tr[0] + tr[1] + tr[2] + tr[3] + tr[4];
	pr_info("op(%d) i2c(%x) crc(%d)\n", tr[0], tr[1], tr[5]);
	spi_message_init(&m);
	spi_message_add_tail(&xfer_addr, &m);
	retval = spi_sync(etspi->spi, &m);
	if (retval < 0) {
		pr_err("read data (0x81) error retval = %d\n", retval);
		return retval;
	}

	tr[0] = 0x00;
	pr_info("get data(0x81) tr[0] = %x\n", tr[0]);
	spi_message_init(&m);
	spi_message_add_tail(&xfer_data, &m);
	retval = spi_sync(etspi->spi, &m);
	if (retval < 0) {
		pr_err("read data error retval = %d\n", retval);
		return retval;
	}

	val = tr[0];
	pr_info("val = %x\n", val);
	if (copy_to_user((u8 __user *) (uintptr_t) buf, &val, 1)) {
		pr_err("buffer copy_to_user fail retval\n");
		return -EFAULT;
	}

	if (try_time >= CIS_READ_TIMEOUT)
		return -ETIME;
	else
		return retval;
#endif
}

int et7xx_io_write_cis_register(struct et7xx_data *etspi, u8 *buf)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval = 0, write_len = 3, try_time = 0;
	struct spi_message m;
	u8 tx[] = {OP_CIS_REG_W, 0x24, 0x00, 0x00, 0x00, 0x00}, val[3];

	struct spi_transfer xfer = {
		.tx_buf = tx,
		.len = 6,
	};

	struct spi_transfer xfer_data = {
		.rx_buf = tx,
		.len = 1,
	};

	if (copy_from_user(val, (const u8 __user *) (uintptr_t) buf
		, write_len)) {
		pr_err("buffer copy_from_user fail. buf(%p)\n", buf);
		return -EFAULT;
	}
	pr_info("write_len = %d addrH = %x addrL = %x data = %x buf = %p\n",
		write_len, val[0], val[1], val[2], buf);

	tx[2] = val[0];
	tx[3] = val[1];
	tx[4] = val[2];
	tx[5] = tx[0] + tx[1] + tx[2] + tx[3] + tx[4];

	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	retval = spi_sync(etspi->spi, &m);
	if (retval < 0) {
		pr_err("write data (0x70) error retval = %d\n", retval);
		return retval;
	}

	while (try_time < 2000) {
		tx[0] = 0x00;
		spi_message_init(&m);
		spi_message_add_tail(&xfer_data, &m);
		retval = spi_sync(etspi->spi, &m);
		if (retval < 0) {
			pr_err("read data error retval = %d\n", retval);
			return retval;
		}
		if (tx[0] == 0xAA) {
			pr_info("tx[0] == 0xAA, try_time = %d\n", try_time);
			break;
		}
		usleep_range(10, 20);
		try_time++;
	}
	if (try_time >= 2000)
		pr_err("-------------    try_time >= 2000    ---------------\n");

	return retval;
#endif
}

int et7xx_io_pre_capture(struct et7xx_data *etspi)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval = 0;
	struct spi_message m;
	u8 tx[] = {OP_PRE_CAPTURE, 0x00, 0x00, 0x00, 0x00, 0x00};
	int try_time = 0;

	struct spi_transfer xfer = {
		.tx_buf = tx,
		.len = 6,
	};
	struct spi_transfer xfer_data = {
		.rx_buf = tx,
		.len = 1,
	};

	pr_info("tx[0] = %d\n", tx[0]);

	tx[5] = tx[0] + tx[1] + tx[2] + tx[3] + tx[4];

	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	retval = spi_sync(etspi->spi, &m);
	if (retval < 0) {
		pr_err("set pre capture error retval = %d\n", retval);
		return retval;
	}

	while (try_time < 8000) {
		tx[0] = 0x00;
		spi_message_init(&m);
		spi_message_add_tail(&xfer_data, &m);
		retval = spi_sync(etspi->spi, &m);
		if (retval < 0) {
			pr_err("read data error retval = %d\n", retval);
			return retval;
		}
		if (tx[0] == 0xAA) {
			pr_info("tx[0] == 0xAA, try_time = %d\n", try_time);
			break;
		}
		usleep_range(10, 20);
		try_time++;
	}
	if (try_time >= 8000) {
		pr_err("-------------    try_time >= 8000    ---------------\n");
		return -ETIME;
	}

	return retval;
#endif
}

int et7xx_io_get_cis_frame(struct et7xx_data *etspi, u8 *fr, u32 size)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval;
	struct spi_message m;
	u8 *buf = NULL;
	u8 tx[] = { OP_GET_FRAME, 0x00, 0x00, 0x00, 0x00, 0x00};
	u32 size_et736 = 0;

	struct spi_transfer xfer_op = {
		.tx_buf = tx,
		.len = 6,
	};
	struct spi_transfer xfer_data = {
		.rx_buf = NULL,
	};

	size_et736 = ((324 * 324) / 1024) * 1024 + 1024;

	buf = kzalloc(size_et736, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	pr_info("size = %d,  buf = %p, fr = %p\n", size, buf, fr);
	/*read sector 0.*/
	xfer_data.rx_buf = buf;
	xfer_data.len = 60*1024;

	tx[4] = 60;
	tx[5] = tx[0] + tx[1] + tx[2] + tx[3] + tx[4];
	pr_info("(0) xfer_data.len = %d, rx_buf = %p, tx[2](%x), tx[3](%x), tx[4](%x)\n",
		xfer_data.len, xfer_data.rx_buf, tx[2], tx[3], tx[4]);

	spi_message_init(&m);
	spi_message_add_tail(&xfer_op, &m);
	retval = spi_sync(etspi->spi, &m);
	if (retval < 0) {
		pr_err("set sector(0) error retval = %d\n", retval);
		goto end;
	}

	/*read sector 0.*/
	spi_message_init(&m);
	spi_message_add_tail(&xfer_data, &m);
	retval = spi_sync(etspi->spi, &m);
	if (retval < 0) {
		pr_err("read data(0) error retval = %d\n", retval);
		goto end;
	}

	tx[2] = 60;
	tx[4] = 43;
	tx[5] = tx[0] + tx[1] + tx[2] + tx[3] + tx[4];
	pr_info("(1) xfer_data.len = %d, rx_buf = %p, tx[2](%x), tx[3](%x), tx[4](%x)\n",
		size_et736 - 60 * 1024, buf + 60 * 1024, tx[2], tx[3], tx[4]);
	usleep_range(10 * 1000, 12 * 1000);
	spi_message_init(&m);
	spi_message_add_tail(&xfer_op, &m);
	retval = spi_sync(etspi->spi, &m);
	if (retval < 0) {
		pr_err("set sector(0) error retval = %d\n", retval);
		goto end;
	}

	/* read sector 1 */
	xfer_data.rx_buf = buf + 60 * 1024;
	xfer_data.len = size_et736 - 60 * 1024;

	spi_message_init(&m);
	spi_message_add_tail(&xfer_data, &m);
	retval = spi_sync(etspi->spi, &m);
	if (retval < 0) {
		pr_err("read data(0) error retval = %d\n", retval);
		goto end;
	}

	pr_info("(1) xfer_data.len = %d, rx_buf = %p, tx[1](%x), tx[2](%x), tx[3](%x)\n",
			xfer_data.len, xfer_data.rx_buf, tx[1], tx[2], tx[3]);

	if (copy_to_user((u8 __user *) (uintptr_t) fr, buf, size)) {
		pr_err("buffer copy_to_user fail.\n");
		retval = -EFAULT;
	}
end:
	kfree(buf);
	return retval;
#endif
}

int et7xx_io_transfer_command(struct et7xx_data *etspi, u8 *tx, u8 *rx,
								u32 size)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval = 0;
	struct spi_message m;
	u8 *tr;

	struct spi_transfer xfer = {
	    .len = size,
	};

	pr_info("tx(%p), rx(%p), size(%d)\n", tx, rx, size);
	tr = kzalloc(size, GFP_KERNEL);
	if (tr == NULL)
		return -ENOMEM;

	xfer.tx_buf = xfer.rx_buf = tr;

	if (copy_from_user(tr, (const u8 __user *)(uintptr_t)tx, size)) {
		pr_err("buffer copy_from_user fail. tr(%p), tx(%p)\n", tr,
		       tx);
		retval = -EFAULT;
		goto out;
	}

	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	retval = spi_sync(etspi->spi, &m);
	if (retval < 0) {
		pr_err("retval = %d\n", retval);
		goto out;
	}

	if (copy_to_user(rx, (const u8 __user *)(uintptr_t)tr, size)) {
		pr_err("buffer copy_to_user fail. tr(%p) rx(%p)\n", tr,
		       rx);
		retval = -EFAULT;
		goto out;
	}

out:
	kfree(tr);
	return retval;
#endif
}

int et7xx_write_register(struct et7xx_data *etspi, u8 addr, u8 buf)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval;
	struct spi_message m;

	u8 tx[] = {OP_REG_W, addr, buf};

	struct spi_transfer xfer = {
		.tx_buf = tx,
		.rx_buf	= NULL,
		.len = 3,
	};

	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	retval = spi_sync(etspi->spi, &m);

	if (retval == 0) {
		pr_info("address = %x\n", addr);
	} else {
		pr_err("read data error retval = %d\n", retval);
	}

	return retval;
#endif
}

int et7xx_read_register(struct et7xx_data *etspi, u8 addr, u8 *buf)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval;
	struct spi_message m;

	u8 read_value[] = {OP_REG_R, addr, 0x00};
	u8 result[] = {0xFF, 0xFF, 0xFF};

	struct spi_transfer xfer = {
		.tx_buf = read_value,
		.rx_buf	= result,
		.len = 3,
	};

	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	retval = spi_sync(etspi->spi, &m);

	if (retval == 0) {
		*buf = result[2];
		pr_info("address = %x result = %x %x\n", addr, result[1], result[2]);
	} else {
		pr_err("read data error retval = %d\n", retval);
	}

	return retval;
#endif
}

/* For microchip sst */
/* microchip sst communication opcode */
#define MICROCHIP_SST_PAGE_PROGRAM 0x02 /* To program up to 256 Bytes */
#define MICROCHIP_SST_READ 0x03 /* Read Memory */
#define MICROCHIP_SST_WRDI 0x04 /* Write-Disable */
#define MICROCHIP_SST_RDSR 0x05 /* Read-Status-Register */
#define MICROCHIP_SST_WREN 0x06 /* Write-Enable */
#define MICROCHIP_SST_CHIP_ERASE 0x60 /* Erase Full Memory Array */
#define MICROCHIP_SST_SECTOR_ERASE 0xD7 /* Erase 4 KByle sector */
#define MICROCHIP_SST_BLOCK_ERASE 0xD8 /* Erase 64 KByle block */

#define MICROCHIP_SST_ADDRESS_SIZE 3
#define MICROCHIP_SST_RW_OFFSET 4 /* OP code 8 bits and address 32bits */
/* The programmed data must be between 1 to 256 Bytes and in whole byte
 * increments; sending less than a full byte will cause the partial byte to
 * be ignored.
 */
#define MICROCHIP_SST_PAGE_PROGRAM_LIMITATION 256
#define MICROCHIP_SST_HIGH_SPEED_READ_DUMMY_LEN 1
/* Wait to write/erase finish */
#define MICROCHIP_SST_STATUS_MAX_RETRY_COUNT 100

int et7xx_eeprom_rdsr(struct et7xx_data *etspi, u8 *eeprom_status)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval;
	struct spi_message m;
	u8 buf[2];

	struct spi_transfer xfer = {
		.tx_buf = NULL,
		.rx_buf = NULL,
		.len = 2,
	};

	xfer.tx_buf = xfer.rx_buf = buf;
	buf[0] = MICROCHIP_SST_RDSR;

	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	retval = spi_sync(etspi->spi, &m);
	if (retval < 0) {
		pr_err("spi_sync error retval = %d\n", retval);
	} else {
		if (copy_to_user((u8 __user *) (uintptr_t) eeprom_status,
				buf + 1, 1)) {
			pr_err("buffer copy_to_user fail\n");
			retval = -EFAULT;
		}
		pr_info("eeprom_status  = %d\n", buf[1]);
	}

	return retval;
#endif
}

int et7xx_eeprom_read_status_internal(struct et7xx_data *etspi, u8 *eeprom_status)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval;
	struct spi_message m;
	u8 buf[2];

	struct spi_transfer xfer = {
		.tx_buf = NULL,
		.rx_buf = NULL,
		.len = 2,
	};

	xfer.tx_buf = xfer.rx_buf = buf;
	buf[0] = MICROCHIP_SST_RDSR;

	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	retval = spi_sync(etspi->spi, &m);
	if (retval < 0) {
		pr_err("spi_sync error retval = %d\n", retval);
	} else {
		*eeprom_status = buf[1];
		pr_info("eeprom_status  = %d\n", *eeprom_status);
	}

	return retval;
#endif
}

int et7xx_eeprom_chip_erase(struct et7xx_data *etspi)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval;
	struct spi_message m;
	u8 buf[1];

	struct spi_transfer xfer = {
		.tx_buf = NULL,
		.rx_buf = NULL,
		.len = 1,
	};

	xfer.tx_buf = buf;
	buf[0] = MICROCHIP_SST_CHIP_ERASE;

	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	retval = spi_sync(etspi->spi, &m);
	if (retval < 0)
		pr_err("spi_sync error retval = %d\n", retval);

	return retval;
#endif
}

int et7xx_eeprom_sector_erase(struct et7xx_data *etspi, struct egis_ioc_transfer *ioc)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	struct spi_message m;
	int retval;
	u8 *buf = NULL;
	struct spi_transfer xfer = {
		.tx_buf = NULL,
		.rx_buf = NULL,
		.len = 0,
	};

	if (ioc->len > MICROCHIP_SST_PAGE_PROGRAM_LIMITATION) {
		pr_err("len EINVAL\n");
		retval = -EINVAL;
	}

	buf = kzalloc(ioc->len + 1, GFP_KERNEL);
	if (buf == NULL) {
		pr_err("buf kzalloc fail\n");
		retval = -ENOMEM;
		goto end;
	}

	if (copy_from_user(buf + 1, (const u8 __user *) (uintptr_t) ioc->tx_buf
		, ioc->len)) {
		pr_err("buffer copy_from_user fail\n");
		retval = -EFAULT;
		goto end;
	}

	pr_info("Init ioc->len = %d, buf[1] = 0x%x buf[2] = 0x%x, buf[3] = 0x%x\n",
		ioc->len, buf[1], buf[2], buf[3]);

	xfer.len = ioc->len + 1; /* OP code */
	xfer.tx_buf = buf;
	xfer.rx_buf = NULL;
	buf[0] = MICROCHIP_SST_SECTOR_ERASE;

	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	retval = spi_sync(etspi->spi, &m);

end:

	if (buf)
		kfree(buf);

	return retval;

#endif
}

int et7xx_eeprom_block_erase(struct et7xx_data *etspi, struct egis_ioc_transfer *ioc)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	struct spi_message m;
	int retval;
	u8 *buf = NULL;
	struct spi_transfer xfer = {
		.tx_buf = NULL,
		.rx_buf = NULL,
		.len = 0,
	};

	if (ioc->len > MICROCHIP_SST_PAGE_PROGRAM_LIMITATION) {
		pr_err("len EINVAL\n");
		retval = -EINVAL;
	}

	buf = kzalloc(ioc->len + 1, GFP_KERNEL);
	if (buf == NULL) {
		pr_err("buf kzalloc fail\n");
		retval = -ENOMEM;
		goto end;
	}

	if (copy_from_user(buf + 1, (const u8 __user *) (uintptr_t) ioc->tx_buf
		, ioc->len)) {
		pr_err("buffer copy_from_user fail\n");
		retval = -EFAULT;
		goto end;
	}

	pr_info("Init ioc->len = %d, buf[1] = 0x%x buf[2] = 0x%x, buf[3] = 0x%x\n",
		ioc->len, buf[1], buf[2], buf[3]);

	xfer.len = ioc->len + 1; /* OP code */
	xfer.tx_buf = buf;
	xfer.rx_buf = NULL;
	buf[0] = MICROCHIP_SST_BLOCK_ERASE;

	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	retval = spi_sync(etspi->spi, &m);

end:
	if (buf)
		kfree(buf);

	return retval;

#endif
}

int et7xx_eeprom_write_controller(struct et7xx_data *etspi, int enable)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	int retval;
	struct spi_message m;
	u8 buf[1];
	struct spi_transfer xfer = {
		.tx_buf = NULL,
		.rx_buf = NULL,
		.len = 1,
	};

	xfer.tx_buf = xfer.rx_buf = buf;
	if (enable > 0)
		buf[0] = MICROCHIP_SST_WREN;
	else
		buf[0] = MICROCHIP_SST_WRDI;

	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	retval = spi_sync(etspi->spi, &m);
	if (retval < 0)
		pr_err("spi_sync error retval = %d\n", retval);

	return retval;
#endif
}

int et7xx_eeprom_read(struct et7xx_data *etspi, struct egis_ioc_transfer *ioc)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	struct spi_message m;
	int retval;
	u8 *buf = NULL, addr[MICROCHIP_SST_ADDRESS_SIZE];
	struct spi_transfer xfer = {
		.tx_buf = NULL,
		.rx_buf = NULL,
		.len = 0,
	};

	pr_info("Version is 1128-3");

	if (copy_from_user(addr, (const u8 __user *) (uintptr_t) ioc->tx_buf
		, MICROCHIP_SST_ADDRESS_SIZE)) {
		pr_err("buffer copy_from_user fail\n");
		return -EFAULT;
	}

	xfer.len = ioc->len + MICROCHIP_SST_RW_OFFSET;
	if (xfer.len >= LARGE_SPI_TRANSFER_BUFFER) {
		if ((xfer.len) % DIVISION_OF_IMAGE != 0)
			xfer.len = xfer.len + (DIVISION_OF_IMAGE -
				(xfer.len % DIVISION_OF_IMAGE));
	}

	pr_info("xfer.len = %d, addr[0] = 0x%x addr[1] = 0x%x, addr[2] = 0x%x\n",
		xfer.len, addr[0], addr[1], addr[2]);

	buf = kzalloc(xfer.len, GFP_KERNEL);
	if (buf == NULL) {
		pr_err("buf kzalloc fail\n");
		return -ENOMEM;
	}
	memset(buf, 0x0, xfer.len);

	xfer.tx_buf = xfer.rx_buf = buf;
	buf[0] = MICROCHIP_SST_READ;
	memcpy(buf + 1, addr, MICROCHIP_SST_ADDRESS_SIZE);

	pr_info("ioc->len = %d, xfer.len = %d, buf = %p ", ioc->len, xfer.len, buf);

	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	retval = spi_sync(etspi->spi, &m);
	if (retval < 0) {
		pr_err("spi_sync error retval = %d\n", retval);
		goto end;
	}

	if (copy_to_user((u8 __user *) (uintptr_t) ioc->rx_buf,
			buf + MICROCHIP_SST_RW_OFFSET, ioc->len)) {
		pr_err("buffer copy_to_user fail\n");
		retval = -EFAULT;
	}

end:
	if (buf)
		kfree(buf);

	return retval;
#endif
}

int et7xx_eeprom_high_speed_read(struct et7xx_data *etspi, struct egis_ioc_transfer *ioc)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	struct spi_message m;
	int retval;
	u8 *buf = NULL, addr[MICROCHIP_SST_ADDRESS_SIZE];
	struct spi_transfer xfer = {
		.tx_buf = NULL,
		.rx_buf = NULL,
		.len = 0,
	};

	pr_info("Version is 1124-5");

	if (copy_from_user(addr, (const u8 __user *) (uintptr_t) ioc->tx_buf
		, MICROCHIP_SST_ADDRESS_SIZE)) {
		pr_err("buffer copy_from_user fail\n");
		return -EFAULT;
	}

	xfer.len = ioc->len + MICROCHIP_SST_RW_OFFSET;
	if (xfer.len >= LARGE_SPI_TRANSFER_BUFFER) {
		if ((xfer.len) % DIVISION_OF_IMAGE != 0)
			xfer.len = xfer.len + (DIVISION_OF_IMAGE -
				(xfer.len % DIVISION_OF_IMAGE));
	}

	pr_info("xfer.len = %d, addr[0] = 0x%x addr[1] = 0x%x, addr[2] = 0x%x\n",
		xfer.len, addr[0], addr[1], addr[2]);

	buf = kzalloc(xfer.len + MICROCHIP_SST_HIGH_SPEED_READ_DUMMY_LEN,
		GFP_KERNEL);
	if (buf == NULL) {
		pr_err("buf kzalloc fail\n");
		return -ENOMEM;
	}
	memset(buf, 0x0, xfer.len);

	xfer.tx_buf = xfer.rx_buf = buf;
	buf[0] = MICROCHIP_SST_READ;
	memcpy(buf + 1, addr, MICROCHIP_SST_ADDRESS_SIZE);

	pr_info("ioc->len = %d, xfer.len = %d, buf = %p ",
		ioc->len, xfer.len, buf);

	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	retval = spi_sync(etspi->spi, &m);
	if (retval < 0) {
		pr_err("spi_sync error retval = %d\n", retval);
		goto end;
	}

	if (copy_to_user((u8 __user *) (uintptr_t) ioc->rx_buf,
			buf + MICROCHIP_SST_RW_OFFSET +
			MICROCHIP_SST_HIGH_SPEED_READ_DUMMY_LEN, ioc->len)) {
		pr_err("buffer copy_to_user fail\n");
		retval = -EFAULT;
	}

end:
	if (buf)
		kfree(buf);

	return retval;
#endif
}

int et7xx_eeprom_write(struct et7xx_data *etspi, struct egis_ioc_transfer *ioc)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	struct spi_message m;
	int retval;
	u8 *buf = NULL;
	struct spi_transfer xfer = {
		.tx_buf = NULL,
		.rx_buf = NULL,
		.len = 0,
	};

	if (ioc->len > MICROCHIP_SST_PAGE_PROGRAM_LIMITATION) {
		pr_err("len EINVAL\n");
		retval = -EINVAL;
	}

	buf = kzalloc(ioc->len + 1, GFP_KERNEL);
	if (buf == NULL) {
		pr_err("buf kzalloc fail\n");
		retval = -ENOMEM;
		goto end;
	}

	if (copy_from_user(buf + 1, (const u8 __user *) (uintptr_t) ioc->tx_buf
		, ioc->len)) {
		pr_err("buffer copy_from_user fail\n");
		retval = -EFAULT;
		goto end;
	}

	pr_info("Init ioc->len = %d, buf[1] = 0x%x buf[2] = 0x%x, buf[3] = 0x%x\n",
		ioc->len, buf[1], buf[2], buf[3]);

	xfer.len = ioc->len + 1; /* OP code */
	xfer.tx_buf = buf;
	xfer.rx_buf = NULL;
	buf[0] = MICROCHIP_SST_PAGE_PROGRAM;

	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	retval = spi_sync(etspi->spi, &m);

end:
	if (buf)
		kfree(buf);

	return retval;

#endif
}

void et7xx_eeprom_finish_operation(struct et7xx_data *etspi)
{
	pr_info("Entry\n");
	gpio_set_value(etspi->sleepPin, 1);
	usleep_range(1050, 1100);
	gpio_set_value(etspi->sleepPin, 0);
}

int et7xx_eeprom_write_in_non_tz(struct et7xx_data *etspi, struct egis_ioc_transfer *ioc)
{
#ifdef ENABLE_SENSORS_FPRINT_SECURE
	return 0;
#else
	struct spi_message m;
	int retval, page_count, index = 0, address_64bits, current_len,
		status_retry_count = 0;
	u8 *buf = NULL, *data = NULL, eeprom_status = 0x0;
	struct spi_transfer xfer = {
		.tx_buf = NULL,
		.rx_buf = NULL,
		.len = 0,
	};

	et7xx_eeprom_finish_operation(etspi);

	retval = et7xx_eeprom_write_controller(etspi, 1);
	if (retval < 0) {
		pr_err("Write enable fail retval = %d\n", retval);
		return retval;
	}

	et7xx_eeprom_finish_operation(etspi);

	retval = et7xx_eeprom_chip_erase(etspi);
	if (retval < 0) {
		pr_err("erase full fail retval = %d\n", retval);
		return retval;
	}

	et7xx_eeprom_finish_operation(etspi);

	do {
		usleep_range(500, 1000);
		retval = et7xx_eeprom_read_status_internal(etspi, &eeprom_status);
		et7xx_eeprom_finish_operation(etspi);
		if (retval < 0) {
			pr_err("get eeprom_status fail retval = %d\n", retval);
			goto end;
		}

		if (++status_retry_count > MICROCHIP_SST_STATUS_MAX_RETRY_COUNT) {
			pr_err("not finish erase eeprom\n");
			break;
		}
	} while ((eeprom_status & 0x01) != 0);

	retval = et7xx_eeprom_write_controller(etspi, 0);
	if (retval < 0) {
		pr_err("Write disable fail retval = %d\n", retval);
		return retval;
	}

	et7xx_eeprom_finish_operation(etspi);

	data = kzalloc(ioc->len, GFP_KERNEL);
	if (data == NULL) {
		pr_err("data kzalloc fail\n");
		return -ENOMEM;
	}

	buf = kzalloc(MICROCHIP_SST_RW_OFFSET +
		MICROCHIP_SST_PAGE_PROGRAM_LIMITATION, GFP_KERNEL);
	if (buf == NULL) {
		pr_err("buf kzalloc fail\n");
		retval = -ENOMEM;
		goto end;
	}

	if (copy_from_user(data, (const u8 __user *) (uintptr_t) ioc->tx_buf
		, ioc->len)) {
		pr_err("buffer copy_from_user fail\n");
		retval = -EFAULT;
		goto end;
	}

	address_64bits = (data[0] << 16) + (data[1] << 8) + data[2];
	page_count = (ioc->len - MICROCHIP_SST_ADDRESS_SIZE) /
		MICROCHIP_SST_PAGE_PROGRAM_LIMITATION;
	if ((ioc->len - MICROCHIP_SST_ADDRESS_SIZE) %
		MICROCHIP_SST_PAGE_PROGRAM_LIMITATION != 0)
		page_count++;

	pr_info("Init data : ioc->len = %d, page_count = %d\n",
		ioc->len, page_count);
	pr_info("Init ioc->len = %d, address = 0x%x address[1] = 0x%x address[2] = 0x%x, address[3] = 0x%x\n",
		ioc->len, address_64bits, data[0], data[1], data[2]);

write_eeprom:

	retval = et7xx_eeprom_write_controller(etspi, 1);
	if (retval < 0) {
		pr_err("Write enable fail retval = %d\n", retval);
		goto end;
	}

	et7xx_eeprom_finish_operation(etspi);

	if (index >= (page_count - 1)) {
		current_len = (ioc->len - MICROCHIP_SST_ADDRESS_SIZE) %
			MICROCHIP_SST_PAGE_PROGRAM_LIMITATION;
		if (current_len == 0)
			current_len = MICROCHIP_SST_PAGE_PROGRAM_LIMITATION;
	} else {
		current_len = MICROCHIP_SST_PAGE_PROGRAM_LIMITATION;
	}

	pr_info("Run data : ioc->len = %d, page_count = %d\n",
		ioc->len, page_count);

	xfer.len = MICROCHIP_SST_RW_OFFSET + current_len;
	xfer.tx_buf = buf;
	xfer.rx_buf = NULL;
	buf[0] = MICROCHIP_SST_PAGE_PROGRAM;
	buf[1] = (address_64bits & 0xff0000) >> 16;
	buf[2] = (address_64bits & 0x00ff00) >> 8;
	buf[3] = (address_64bits & 0x0000ff);
	memcpy(buf + MICROCHIP_SST_RW_OFFSET, data + MICROCHIP_SST_ADDRESS_SIZE +
		(index * MICROCHIP_SST_PAGE_PROGRAM_LIMITATION), current_len);

	pr_info("Run index = %d page_count = %d current_len = %d, xfer.len = %d\n",
		index, page_count, current_len, xfer.len);
	pr_info("Run address = 0x%x address[1] = 0x%x address[2] = 0x%x, address[3] = 0x%x\n",
		address_64bits, buf[1], buf[2], buf[3]);

	spi_message_init(&m);
	spi_message_add_tail(&xfer, &m);
	retval = spi_sync(etspi->spi, &m);
	et7xx_eeprom_finish_operation(etspi);
	if (retval < 0) {
		pr_err("spi_sync error retval = %d\n", retval);
		goto end;
	}

	status_retry_count = 0;
	eeprom_status = 0x0;
	do {
		usleep_range(500, 1000);
		retval = et7xx_eeprom_read_status_internal(etspi, &eeprom_status);
		et7xx_eeprom_finish_operation(etspi);
		if (retval < 0) {
			pr_err("get eeprom_status fail retval = %d\n", retval);
			goto end;
		}

		if (++status_retry_count > MICROCHIP_SST_STATUS_MAX_RETRY_COUNT) {
			pr_err("not finish writing eeprom\n");
			break;
		}
	} while ((eeprom_status & 0x01) != 0);

	retval = et7xx_eeprom_write_controller(etspi, 0);
	if (retval < 0) {
		pr_err("Write disable fail retval = %d\n", retval);
		goto end;
	}

	et7xx_eeprom_finish_operation(etspi);

	if (++index < page_count) {
		address_64bits += MICROCHIP_SST_PAGE_PROGRAM_LIMITATION;
		goto write_eeprom;
	}

end:
	gpio_set_value(etspi->sleepPin, 1);

	if (buf)
		kfree(buf);

	if (data)
		kfree(data);

	return retval;
#endif
}
