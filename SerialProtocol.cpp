#include "SerialProtocol.h"
#include "Player.h"
#include "version.h"

extern PlayersPool players;

// Checks for channel number.
// If everything is OK it will return a pointer
// to a player, otherwise it will return null, while
// sending the appropriate error through UART.
Player* SerialProtocol::verify(wtePacket* packet)
{
	if (packet->data_len < 1)
		return NULL;

    uint8_t channel = packet->data[0];
	uint8_t num = channel - 1;
	if (!channel || num > players.getMaxPlayers())
	{
		sendErrorCode(ERROR_INVALID_CHANNEL);
		return NULL;
	}

    Player* player = players.get(num);
    if (!player)
        sendErrorCode(ERROR_INTERNAL);

    return player;
}

void SerialProtocol::onPlayFile(wtePacket* packet)
{
	if (packet->data_len < 3)
	{
		sendErrorCode(ERROR_INVALID_LENGTH);
		return;
	}

	uint16_t path_len = packet->data_len - 2;

	if (!path_len || path_len > 254)
	{
		sendErrorCode(ERROR_INVALID_FILE_LENGTH);
		return;
	}

	char* path = (char*) packet->data + 2;

	// Set null termination
	path[path_len] = 0;

  	uint8_t mode = packet->data[1];
	if (mode > 1)
	{
		sendErrorCode(ERROR_INVALID_MODE);
	}

    PlayMode play_mode = mode ? PlayModeLoop : PlayModeNormal;

    Player* player = verify(packet);
	if (!player)
		return;

	if (!player->play(path, play_mode))
	{
		sendErrorCode(ERROR_PLAYING);
		return;
	}

	packet->data_len = 1;
	sendPacket(packet);
}

void SerialProtocol::onPlayChannel(wtePacket* packet)
{
	if (packet->data_len < 2)
	{
		sendErrorCode(ERROR_INVALID_LENGTH);
		return;
	}

	uint8_t mode = packet->data[1];
	if (mode > 1)
	{
		sendErrorCode(ERROR_INVALID_MODE);
		return;
	}

    PlayMode play_mode = mode ? PlayModeLoop : PlayModeNormal;

    Player* player = verify(packet);
	if (!player)
		return;

	// Max. file can be 16.wav
	char path[8];
	snprintf(path, 8, "%i.wav", packet->data[0]);

	if (!player->play(path, play_mode))
	{
		sendErrorCode(ERROR_PLAYING);
		return;
	}

	packet->data_len = 1;
	sendPacket(packet);
}

void SerialProtocol::onStopAll(wtePacket* packet)
{
    players.stopAll(true);
	packet->data_len = 0;
	sendPacket(packet);
}

void SerialProtocol::onStop(wtePacket* packet)
{
	if (packet->data_len != 1)
	{
		sendErrorCode(ERROR_INVALID_LENGTH);
		return;
	}

    Player* player = verify(packet);
    if (!player)
        return;

    player->stop(true);

	packet->data_len = 1;
	sendPacket(packet);
}

void SerialProtocol::onPause(wtePacket* packet)
{
	if (packet->data_len != 1)
	{
		sendErrorCode(ERROR_INVALID_LENGTH);
		return;
	}

    Player* player = verify(packet);
	if (!player)
		return;

	packet->data_len = 1;

	if (player->getStatus() == playerStopped)
	{
		packet->data[0] = 0;
	} else {
        player->pause(true);
		packet->data[0] = 1;
	}

	sendPacket(packet);
}

void SerialProtocol::onPauseAll(wtePacket* packet)
{
    players.pauseAll(true);
    packet->data_len = 0;
	sendPacket(packet);
}

void SerialProtocol::onResume(wtePacket* packet)
{
	if (packet->data_len != 1)
	{
		sendErrorCode(ERROR_INVALID_LENGTH);
		return;
	}

    Player* player = verify(packet);
	if (!player)
		return;

	packet->data_len = 1;

	if (player->getStatus() != playerPaused)
	{
		packet->data[0] = 0;
	} else {
        player->resume();
		packet->data[0] = 1;
	}

	sendPacket(packet);
}

void SerialProtocol::onResumeAll(wtePacket* packet)
{
    players.resumeAll();
	packet->data_len = 0;
	sendPacket(packet);
}

void SerialProtocol::onChannelsStatus(wtePacket* packet)
{
	for (uint8_t i = 0; i < players.getMaxPlayers(); i++)
    {
        packet->data[i] = 0;

        Player* player = players.get(i);
        if (player)
		    packet->data[i] = (uint8_t) player->getStatus();
    }

	packet->data_len = players.getMaxPlayers();
	sendPacket(packet);
}

void SerialProtocol::onChannelStatus(wtePacket* packet)
{
    Player* player = verify(packet);
	if (!player)
		return;

	packet->data[0] = (uint8_t) player->getStatus();
	packet->data_len = 1;
	sendPacket(packet);
}

void SerialProtocol::onGetChannelVolume(wtePacket* packet)
{
    Player* player = verify(packet);
	if (!player)
		return;

	float volume = player->getVolume();
	uint16_t packet_vol = (uint16_t) (volume * 100.0f);
	memcpy(packet->data, (uint8_t*) &packet_vol, 2);
	packet->data_len = 2;
	sendPacket(packet);
}

void SerialProtocol::onSetChannelVolume(wtePacket* packet)
{
	if (packet->data_len != 3)
	{
		sendErrorCode(ERROR_INVALID_LENGTH);
		return;
	}

    Player* player = verify(packet);
	if (!player)
		return;

	uint16_t packet_vol;

	memcpy((uint8_t*) &packet_vol, &packet->data[1], 2);
	if (packet_vol > 500)
		packet_vol = 500;

	float volume = (float) packet_vol / 100.0f;
    player->setVolume(volume);
	packet->data_len = 0;
	sendPacket(packet);
}

void SerialProtocol::onSetSpeakersVolume(wtePacket* packet)
{
	if (packet->data_len != 2)
	{
		sendErrorCode(ERROR_INVALID_LENGTH);
		return;
	}

    int16_t packet_vol;
    memcpy((uint8_t*) &packet_vol, packet->data, 2);
    float volume = packet_vol / 10.0f;
	if (!Audio.setSpeakersVolume(volume))
	{
		sendErrorCode(ERROR_INTERNAL);
		return;
	}

	packet->data_len = 0;
	sendPacket(packet);
}

void SerialProtocol::onSetHeadphoneVolume(wtePacket* packet)
{
	if (packet->data_len != 2)
	{
		sendErrorCode(ERROR_INVALID_LENGTH);
		return;
	}

    int16_t packet_vol;
    memcpy((uint8_t*) &packet_vol, packet->data, 2);
    float volume = packet_vol / 10.0f;
	if (!Audio.setHeadphoneVolume(volume))
	{
		sendErrorCode(ERROR_INTERNAL);
		return;
	}

	packet->data_len = 0;
	sendPacket(packet);
}

void SerialProtocol::onGetSpeakersVolume(wtePacket* packet)
{
	if (packet->data_len)
	{
		sendErrorCode(ERROR_INVALID_LENGTH);
		return;
	}

	float volume = Audio.getSpeakersVolume() * 10.0f;
    int16_t packet_vol = (int16_t) volume;

    memcpy(packet->data, (uint8_t*) &packet_vol, 2);
    packet->data_len = 2;
	sendPacket(packet);
}

void SerialProtocol::onGetHeadphoneVolume(wtePacket* packet)
{
	if (packet->data_len)
	{
		sendErrorCode(ERROR_INVALID_LENGTH);
		return;
	}

	float volume = Audio.getHeadphoneVolume() * 10.0f;
    int16_t packet_vol = (int16_t) volume;

    memcpy(packet->data, (uint8_t*) &packet_vol, 2);
    packet->data_len = 2;
	sendPacket(packet);
}

bool SerialProtocol::poll()
{
	if (!pullPacket(&packet))
		return false;

	switch (packet.cmd)
	{
		case CMD_HELLO:
			packet.data_len = 0;
			sendPacket(&packet);
			break;

		case CMD_VERSION:
			packet.data_len = 3;
			packet.data[0] = VERSION_MAJOR;
			packet.data[1] = VERSION_MINOR;
			packet.data[2] = VERSION_FIX;
			sendPacket(&packet);
			break;

		case CMD_PLAY_FILE:
			onPlayFile(&packet);
			break;

		case CMD_PLAY_CHANNEL:
			onPlayChannel(&packet);
			break;

		case CMD_STOP_ALL:
			onStopAll(&packet);
			break;

		case CMD_STOP:
			onStop(&packet);
			break;

		case CMD_PAUSE:
			onPause(&packet);
			break;

		case CMD_PAUSE_ALL:
			onPauseAll(&packet);
			break;

		case CMD_RESUME:
			onResume(&packet);
			break;

		case CMD_RESUME_ALL:
			onResumeAll(&packet);
			break;

		case CMD_CHANNELS_STATUS:
			onChannelsStatus(&packet);
			break;

		case CMD_CHANNEL_STATUS:
			onChannelStatus(&packet);
			break;

		case CMD_GET_CHANNEL_VOL:
			onGetChannelVolume(&packet);
			break;

		case CMD_SET_CHANNEL_VOL:
			onSetChannelVolume(&packet);
			break;

		case CMD_SET_SPEAKERS_VOL:
			onSetSpeakersVolume(&packet);
			break;

		case CMD_SET_HEADPHONE_VOL:
			onSetHeadphoneVolume(&packet);
			break;

		case CMD_GET_SPEAKERS_VOL:
			onGetSpeakersVolume(&packet);
			break;

		case CMD_GET_HEADPHONE_VOL:
			onGetHeadphoneVolume(&packet);
			break;

		default:
			return false;
	}

	return true;
}
