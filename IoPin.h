/***************************************************************************
 * Artekit Wavetooeasy
 * https://www.artekit.eu/products/devboards/wavetooeasy
 *
   Written by Ivan Meleca
 * Copyright (c) 2021 Artekit Labs
 * https://www.artekit.eu

### IoPin.h

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

#ifndef __CHANNEL_H__
#define __CHANNEL_H__

#include <Arduino.h>
#include "TimeCounter.h"
#include "Player.h"

enum PinPolarity
{
	PinActiveLow,
	PinActiveHigh
};

enum PinTriggerType
{
	EdgeTrigger,
	LevelTrigger
};

enum PinState
{
	PinDeasserted,
	PinAsserted
};

enum DeassertMode
{
	DeassertRestart,
	DeassertPause,
	DeassertStop
};

class IoPin
{

public:
	IoPin(uint8_t num, char* file, PinPolarity polarity, PinTriggerType trigger,
			  PlayMode playback, float volume, DeassertMode deassert, uint32_t debounce);
	bool begin();
	void end();
	bool poll();

private:
	Player* player;
	uint8_t pin_num;
	bool enabled;
	volatile PinState state;
	PinState last_state;
	char file_path[256];
	bool error;
	DeassertMode deassert_mode;
	PinPolarity io_polarity;
	PinTriggerType trigger_type;
	PlayMode playback_mode;
	float volume;

	// Debouncing
	TimeCounter debouncer;
	uint32_t debounce;
	PinState debouncer_state;

	static void triggeredStub(void* param)
	{
		if (param)
			((IoPin*) param)->triggered();
	}

	void triggered();
	void processEdgeAsserted();
	void processLevelAsserted();
	void processLevelDeasserted();
};

#endif /* __CHANNEL_H__ */
