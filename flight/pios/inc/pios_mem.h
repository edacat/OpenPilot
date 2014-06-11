/**
 ******************************************************************************
 *
 * @file       pios_mem.h
 * @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2014.
 * @addtogroup PiOS
 * @{
 * @addtogroup PiOS
 * @{
 * @brief PiOS memory allocation API
 *****************************************************************************/
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef PIOS_MEM_H
#define PIOS_MEM_H
#ifdef PIOS_TARGET_PROVIDES_FAST_HEAP
// relies on pios_general_malloc to perform the allocation (i.e. pios_msheap.c)
extern void *pios_general_malloc(size_t size, bool fastheap);

inline void *pios_fastheapmalloc(size_t size)
{
	return pios_general_malloc(size, true);
}


inline void *pios_malloc(size_t size)
{
	return pios_general_malloc(size, false);
}

inline void *pios_free(void *p)
{
	vPortFree(p);
}

#else
// demand to pvPortMalloc implementation
inline void *pios_fastheapmalloc(size_t size)
{
	return pvPortMalloc(size);
}


inline void *pios_malloc(size_t size)
{
	return pvPortMalloc(size);
}

inline void *pios_free(void *p)
{
	vPortFree(p);
}
#endif
#endif /* PIOS_MEM_H */