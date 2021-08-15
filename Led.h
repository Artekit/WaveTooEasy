/***************************************************************************
 * Artekit Wavetooeasy
 * https://www.artekit.eu/products/devboards/wavetooeasy
 *
   Written by Ivan Meleca
 * Copyright (c) 2021 Artekit Labs
 * https://www.artekit.eu

### Led.h

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

#ifndef __LED_H__
#define __LED_H__

#include <Arduino.h>
#include <ServiceTimer.h>

class BoardLed : public STObject
{
public:
	BoardLed(uint32_t led) :
		led(led), value(false), blink_counter(0), blink_cycle_ticks(0), blink_time_on(0) {}

	void initialize() { pinMode(led, OUTPUT); set(false); }
	void end() { pinMode(led, INPUT); }

	void toggle()
	{
		isOn() ? setOff() : setOn();
	}

	inline void set(bool on) { value = on; digitalWrite(led, value ? LOW : HIGH); }
	inline void setOn() { set(true); }
	inline void setOff() { set(false); }
	inline bool isOn() { return value; }

	void blink(uint32_t time_ms, uint32_t time_on_ms)
	{
		blink_cycle_ticks = (1000 / getFrequency()) * time_ms;
		blink_time_on = time_on_ms;
		blink_counter = 0;
		set(true);
		add();
	}

	void stopBlink()
	{
		remove();
		setOff();
	}

	void poll()
	{
		blink_counter++;
		if (isOn() && blink_counter > blink_time_on)
			setOff();

		if (!isOn() && blink_counter > blink_cycle_ticks)
		{
			blink_counter = 0;
			setOn();
		}
	}

private:
	uint32_t led;
	bool value;
	uint32_t blink_counter;
	uint32_t blink_cycle_ticks;
	uint32_t blink_time_on;
};

#endif /* __LED_H__ */
