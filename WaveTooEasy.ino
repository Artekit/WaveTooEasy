/***************************************************************************
 * Artekit Wavetooeasy
 * https://www.artekit.eu/products/devboards/wavetooeasy
 *
   Written by Ivan Meleca
 * Copyright (c) 2021 Artekit Labs
 * https://www.artekit.eu

### WaveTooEasy.ino

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

#include <Arduino.h>
#include "Debug.h"
#include "IoPin.h"
#include "PropConfig.h"
#include "lm49450.h"
#include "Led.h"
#include "SerialProtocol.h"
#include "Player.h"
#include "version.h"
#include "TimeCounter.h"
#include <strings.h>
#include <ctype.h>

#define IO_PINS_COUNT		16
#define MODE_IO				1
#define MODE_LATCHED		2
#define MODE_SERIAL			3

#define GPIOA_MASK			0x19FF
#define GPIOB_MASK			0x600
#define GPIOC_MASK 			0xE000

// Latch
static volatile bool latched = false;
static volatile uint32_t latched_num;
static Player* latch_player = NULL;
static void latchInterrupt();

// Configuration
static PropConfig config;
static uint32_t sample_rate;
static float speakers_volume_db;
static float headphone_volume_db;
static uint8_t mode;
static uint32_t baudrate;
static bool disable_leds = false;
static bool latch_in_active_low = false;
static bool latch_io_active_low = false;
static uint32_t low_power_timeout;

// Initialization
static bool initialized = false;

// Serial protocol
static SerialProtocol serial_protocol = SerialProtocol::getInstance();

// LEDs
static BoardLed led1(LED1);
static BoardLed led2(LED2);

// Channels
static IoPin* io_pins[IO_PINS_COUNT];

// Players
PlayersPool players = PlayersPool::getInstance();

// Timers
TimeCounter low_power_timer;

bool readConfig()
{
	// Set some defaults
	sample_rate = 44100;
	speakers_volume_db = 0;
	headphone_volume_db = 0;
	mode = MODE_IO;
	baudrate = 115200;
	disable_leds = false;

	// Fixed configuration name
	if (!config.begin("config.ini", false))
		return false;

	char szMode[32];
	uint32_t szModeLen = sizeof(szMode);
	if (config.readValue("settings", "mode", szMode, &szModeLen))
	{
		if (strncasecmp("serial", szMode, szModeLen) == 0)
			mode = MODE_SERIAL;
		else if (strncasecmp("io", szMode, szModeLen) == 0)
			mode = MODE_IO;
		else if (strncasecmp("latch", szMode, szModeLen) == 0)
			mode = MODE_LATCHED;
	}

	config.readValue("settings", "sample_rate", &sample_rate);
	config.readValue("settings", "speakers_volume", &speakers_volume_db);
	config.readValue("settings", "headphone_volume", &headphone_volume_db);
	config.readValue("settings", "baudrate", &baudrate);
	config.readValue("settings", "disable_leds", &disable_leds);
	config.readValue("settings", "low_power_timeout", &low_power_timeout);
	low_power_timeout *= 1000;
	return true;
}

void cleanupIoMode()
{
	for (uint8_t i = 0; i < IO_PINS_COUNT; i++)
	{
		if (io_pins[i])
		{
			io_pins[i]->end();
			delete io_pins[i];
		}
	}
}

static void strtolower(char* str)
{
	char *p = str;

	while (*p)
	{
		*p = tolower(*p);
		p++;
	}
}

bool initializeIoMode()
{
	char io_name[32];
	char tmp[256];
	uint32_t str_len;
	PinPolarity polarity;
	PinTriggerType trigger;
	PlayMode playback;
	DeassertMode deassert;
	uint32_t debounce = 20;
	float volume;

    // Initialize players list
    players.initialize(true);

	// Initialize every pin
	for (uint8_t i = 0; i < IO_PINS_COUNT; i++)
	{
		// Set some defaults
		playback = PlayModeNormal;
		polarity = PinActiveHigh;
		trigger = LevelTrigger;
		deassert = DeassertRestart;
		volume = 1.0f;
		debounce = 5;
		memset(tmp, 0, sizeof(tmp));

		sprintf(io_name, "pin%i_mode", i + 1);
		str_len = sizeof(tmp);
		if (config.readValue("io", io_name, tmp, &str_len))
		{
			strtolower(io_name);
			if (strstr(tmp, "loop") != NULL)
				playback = PlayModeLoop;

			if (strstr(tmp, "low") != NULL)
				polarity = PinActiveLow;

			if (strstr(tmp, "edge") != NULL)
				trigger = EdgeTrigger;

			if (strstr(tmp, "pause") != NULL)
				deassert = DeassertPause;
			else if (strstr(tmp, "stop") != NULL)
				// This should have sense only when trigger = EdgeTrigger
				deassert = DeassertStop;
		} else continue;

		// Read file name (if any)
		sprintf(io_name, "pin%i_file", i + 1);
		str_len = sizeof(tmp);
		if (!config.readValue("io", io_name, tmp, &str_len) || !strlen(tmp))
			sprintf(tmp, "%i.wav", i + 1);

		// Read volume
		sprintf(io_name, "pin%i_volume", i + 1);
		if (!config.readValue("io", io_name, &volume))
			volume = 1.0f;

		// Read debouncer settings
		sprintf(io_name, "pin%i_debounce", i + 1);
		config.readValue("io", io_name, &debounce);

		io_pins[i] = new IoPin(i, tmp, polarity, trigger, playback, volume, deassert, debounce);

		if (!io_pins[i])
		{
			cleanupIoMode();
			return false;
		}

		io_pins[i]->begin();
	}

	return true;
}

static bool initializeLatchedMode()
{
    // Initialize players list
    players.initialize(false);

	latch_player = players.get(0);

	latch_in_active_low = false;
	latch_io_active_low = false;

	config.readValue("latch", "latch_polarity", &latch_in_active_low);
	config.readValue("latch", "input_polarity", &latch_io_active_low);

	// Initialize channels pins
	for (uint8_t i = 0; i < IO_PINS_COUNT; i++)
		pinMode(i, (latch_io_active_low) ? INPUT_PULLUP : INPUT_PULLDOWN);

	// Initialize latch pin
	pinMode(LATCH, (latch_in_active_low) ? INPUT_PULLUP : INPUT_PULLDOWN);

	attachInterrupt(LATCH, latchInterrupt, (latch_in_active_low) ? FALLING : RISING);
	return true;
}

static bool initializeSerialMode()
{
    // Initialize players list
    players.initialize(false);
	serial_protocol.begin(Serial, baudrate);
	return true;
}

static void pollSerialMode()
{
	bool activity = serial_protocol.poll();
	if (activity && low_power_timer.active())
		low_power_timer.startTimeoutCounter(low_power_timeout);
}

static void pollIoMode()
{
	bool activity = false;

	for (uint8_t i = 0; i < IO_PINS_COUNT; i++)
		activity |= io_pins[i]->poll();

	if (activity && low_power_timer.active())
		low_power_timer.startTimeoutCounter(low_power_timeout);
}

static void pollLatchedMode()
{
	uint16_t num;

	// Max. file name is 65535.wav
	char file[10];

	if (latched)
	{
		__disable_irq();
		num = latched_num;
		latched = false;
		__enable_irq();

		if (latch_io_active_low)
			num = ~num;

		debugMsg(DebugInfo, "Latch detected, num = %i", num);

		sprintf(file, "%i.wav", num);
		if (latch_player->play(file))
			debugMsg(DebugInfo, "Latched mode - playing %s", file);
		else
			debugMsg(DebugError, "Latched mode - error playing %s", file);

		if (low_power_timer.active())
			low_power_timer.startTimeoutCounter(low_power_timeout);
	}
}

static void pollAudioActivityLED()
{
	static bool playing = false;

	if (disable_leds)
		return;

	if (Audio.isPlaying() && !playing)
	{
		playing = true;
		led1.stopBlink();
		led1.blink(250, 125);
	} else if (!Audio.isPlaying() && playing)
	{
		playing = false;
		led1.stopBlink();
		led1.blink(1000, 500);
	}
}

static void lowPowerMode()
{
	// Stop audio
	Audio.end();

	if (!disable_leds)
	{
		led1.setOff();
		led2.setOff();
		led1.end();
		led2.end();
	}

	if (mode == MODE_SERIAL)
		Serial.end();

	// Deinitialize every pin
	for (uint8_t i = 0; i < IO_PINS_COUNT; i++)
	{
		if (io_pins[i])
			io_pins[i]->end();
	}

	enterLowPowerMode();
}

void setup()
{
#if ENABLE_DEBUG
	initDebug();
#endif

	debugMsg(DebugInfo, "Artekit wavetooeasy version %i.%i.%i", VERSION_MAJOR, VERSION_MINOR, VERSION_FIX);

	// Configure LEDs
	led1.initialize();
	led2.initialize();

	if (!readConfig())
	{
		// Cannot read configuration file, blink both LEDs and wait for reset
		led1.blink(500, 250);
		led2.blink(500, 250);
		while(true);
	}

	if (!disable_leds)
	{
		led2.setOn();
		led1.blink(1000, 500);
	}

	Audio.begin(sample_rate);
	Audio.setSpeakersVolume(speakers_volume_db);
	Audio.setHeadphoneVolume(headphone_volume_db);

	delay(500);

	switch (mode)
	{
		case MODE_IO:
			initialized = initializeIoMode();
			break;

		case MODE_LATCHED:
			initialized = initializeLatchedMode();
			break;

		case MODE_SERIAL:
			initialized = initializeSerialMode();
	}

	initialized = true;
	if (initialized && low_power_timeout)
		low_power_timer.startTimeoutCounter(low_power_timeout);
}

void loop()
{
	if (!initialized)
		return;

	pollAudioActivityLED();

	players.poll();

	switch (mode)
	{
		case MODE_IO:
			pollIoMode();
			break;

		case MODE_SERIAL:
			pollSerialMode();
			break;

		case MODE_LATCHED:
			pollLatchedMode();
			break;
	}

	// Check if it's time to enter low power mode
	if (low_power_timer.active() && low_power_timer.timeout())
	{
		// Check if we have players playing
		if (!players.playing())
			lowPowerMode();
		else
			low_power_timer.startTimeoutCounter(low_power_timeout);
	}
}

void latchInterrupt()
{
	// Collect IO pin states into a 16 bit number
	latched_num = GPIOA->IDR & GPIOA_MASK;
	latched_num |= GPIOB->IDR & GPIOB_MASK;
	latched_num |= GPIOC->IDR & GPIOC_MASK;
	latched = true;
}
