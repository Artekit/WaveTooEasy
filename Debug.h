/***************************************************************************
 * Artekit Wavetooeasy
 * https://www.artekit.eu/products/devboards/wavetooeasy
 *
   Written by Ivan Meleca
 * Copyright (c) 2021 Artekit Labs
 * https://www.artekit.eu

### Debug.h

#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 3 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.

***************************************************************************/

#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <Arduino.h>

/* Debug macros */
#ifndef ENABLE_DEBUG
#define ENABLE_DEBUG 0
#endif

#if ENABLE_DEBUG

#define DEBUG_CONTEXT(x)	x

#ifndef DEBUG_SERIAL
#define DEBUG_SERIAL	Serial
#endif

#ifndef DEBUG_LEVEL_MASK
#define DEBUG_LEVEL_MASK	DebugInfoAll
#endif

void initDebug();
void debugPrint(uint32_t level, const char* fmt, ...);
void debugOut(char* str, bool crlf);

#define debugMsg(level, fmt, ...)						\
do {													\
	if (level && (level & DEBUG_LEVEL_MASK))	\
	{													\
		debugPrint(level, fmt, ##__VA_ARGS__);			\
	}													\
} while(0)

#else
#define DEBUG_CONTEXT(x)
#define initDebug()
#define debugPrint(x, y, ...) (void)(0)
#define debugOut(x, y) (void)(0)
#define debugMsg(level, fmt, ...) (void)(0)
#endif /* ENABLE_DEBUG */

#define DebugError		0x01
#define DebugWarning	0x02
#define DebugInfo		0x04
#define DebugInfoAll	(DebugError | DebugWarning | DebugInfo)

#endif /* __DEBUG_H__ */
