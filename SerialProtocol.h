#ifndef __SERIAL_H__
#define __SERIAL_H__

#include <Arduino.h>
#include "Player.h"
#include "WaveTooEasy_Protocol.h"

class SerialProtocol
{
public:
	static inline SerialProtocol& getInstance()
    {
        static SerialProtocol serial_protocol;
        return serial_protocol;
    }

	static uint32_t cbMillis()
	{
		return millis();
	}

	static uint8_t cbReceive(uint8_t* c, void* param)
	{
		SerialProtocol* p = (SerialProtocol*) param;

		if (p->serial->available())
		{
			*c = p->serial->read();
			return 1;
		}

		return 0;
	}

	static void cbSend(uint8_t* data, size_t len, void* param)
	{
		SerialProtocol* p = (SerialProtocol*) param;

		p->serial->write(data, len);
	}

	void begin(UARTClass& uart, uint32_t baudrate = 115200)
	{
		serial = &uart;
		serial->begin(baudrate);
		wteInit(cbMillis, cbReceive, cbSend, this);
	}

	void end()
	{
		serial->end();
	}

	bool pullPacket(wtePacket* packet)
	{
		return wtePullPacket(packet, 2);
	}

	void sendPacket(wtePacket* packet)
	{
		wtePushPacket(packet);
	}

	void sendErrorCode(uint8_t code)
	{
		wteSendErrorCode(code);
	}

    bool poll();

private:
    SerialProtocol() : serial(NULL) {}
    Player* verify(wtePacket* packet);
    void onPlayFile(wtePacket* packet);
    void onPlayChannel(wtePacket* packet);
    void onStopAll(wtePacket* packet);
    void onStop(wtePacket* packet);
    void onPause(wtePacket* packet);
    void onPauseAll(wtePacket* packet);
    void onResume(wtePacket* packet);
    void onResumeAll(wtePacket* packet);
    void onChannelsStatus(wtePacket* packet);
    void onChannelStatus(wtePacket* packet);
    void onGetChannelVolume(wtePacket* packet);
    void onSetChannelVolume(wtePacket* packet);
    void onSetSpeakersVolume(wtePacket* packet);
    void onSetHeadphoneVolume(wtePacket* packet);
    void onGetSpeakersVolume(wtePacket* packet);
    void onGetHeadphoneVolume(wtePacket* packet);

	UARTClass* serial;
	wtePacket packet;
};

#endif /* __SERIAL_H__ */
