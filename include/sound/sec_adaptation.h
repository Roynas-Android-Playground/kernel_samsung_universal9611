/*
 * ALSA SoC - Samsung Adaptation driver
 *
 * Copyright (c) 2016 Samsung Electronics Co. Ltd.
  *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SEC_ADAPTATION_H
#define __SEC_ADAPTATION_H

#include <sound/smart_amp.h>
#include <sound/samsung/abox.h>


//--------------------------------------------------
#ifdef SMART_AMP
int ti_smartpa_write(void *data, int offset, int size);
int ti_smartpa_read(void *data, int offset, int size);
#else
static inline int ti_smartpa_write(void *data, int offset, int size)
{
	return -ENOSYS;
}

static inline int ti_smartpa_read(void *data, int offset, int size)
{
	return -ENOSYS;
}
#endif

#endif /* __SEC_ADAPTATION_H */

