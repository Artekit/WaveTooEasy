#ifndef __PLAYER_H__
#define __PLAYER_H__

#include <Arduino.h>

#define MAX_PLAYERS     10

typedef enum
{
	playerStopped,
	playerPlaying,
	playerPaused,
	playerPausing,
	playerStopping,
} playerStatus;

class PlayersPool;

class Player
{
    friend class PlayersPool;

public:
    bool play(const char* filename, PlayMode mode = PlayModeNormal)
    {
        if (status == playerPausing ||
            status == playerStopping)
        {
            wav.stop();
            status = playerStopped;
        }

        if (status == playerStopped ||
            status == playerPaused)
        {
            wav.setVolume(base_volume);
        }

        if (wav.play(filename, mode))
        {
            status = playerPlaying;
            return true;
        }

        return false;
    }

    void stop(bool ramp_volume = false)
    {
        if (status == playerStopped)
            return;

        if (ramp_volume && status != playerPaused)
        {
            if (status == playerPlaying)
                wav.setVolume(0);

            status = playerStopping;
        } else {
            wav.stop();
            status = playerStopped;
        }
    }

    int32_t getStatus()
    {
        switch (status)
        {
            case playerStopping:
            case playerStopped:
            default:
                return playerStopped;

            case playerPlaying:
                return playerPlaying;

            case playerPausing:
            case playerPaused:
                return playerPaused;
        }
    }

    void pause(bool ramp_volume = false)
    {
        if (status == playerPaused  ||
            status == playerStopped ||
            status == playerStopping)
            return;

        if (ramp_volume)
        {
            if (status == playerPlaying)
            {
                wav.setVolume(0);
				status = playerPausing;
            }
        } else {
            wav.pause();
			status = playerPaused;
        }
    }

    void resume()
    {
        if (status == playerStopping ||
            status == playerStopped ||
            status == playerPlaying)
            return;

        if (status == playerPausing)
            wav.pause();

        wav.setVolume(base_volume);
        wav.resume();
        status = playerPlaying;
    }

    float getVolume()
    {
        return base_volume;
    }

    void setVolume(float volume)
    {
        base_volume = volume;
        wav.setVolume(volume);
    }

protected:
    Player() : status(playerStopped), busy(false), base_volume(1.0f) {}
    void poll()
    {
        if (status == playerStopping && wav.getVolume() == 0)
        {
            wav.stop();
        } else if (status == playerPausing && wav.getVolume() == 0)
        {
            wav.pause();
            status = playerPaused;
        }

        if (wav.getStatus() == AudioSourceStopped)
            status = playerStopped;
    }

    playerStatus status;
    bool busy;
    float base_volume;
    WavPlayer wav;
};

class PlayersPool
{
    /*
     * This class represents a list of players and 'synchronized'
     * here means that the list is accessed through the
     * acquire() and release() methods, used by the io_mode in
     * which there are 10 players/channels shared between 16 inputs.
     *
     * In serial_mode and latched_mode, the list is accessed by an
     * index using the get() function.
    */

private:
    PlayersPool() : initialized(false), synchronized(true) {}
    Player players[MAX_PLAYERS];

    bool initialized;
    bool synchronized;

public:
    void initialize(bool synchronized = true)
    {
        this->synchronized = synchronized;
        initialized = true;
    }

    static PlayersPool& getInstance()
    {
        static PlayersPool pool;
        return pool;
    }

    Player* acquire()
    {
        if (!synchronized || !initialized)
            return NULL;

        for (uint8_t i = 0; i < MAX_PLAYERS; i++)
        {
            if (!players[i].busy)
            {
                players[i].busy = true;
                return &players[i];
            }
        }

        return NULL;
    }

    void release(Player* player)
    {
        if (!synchronized || !initialized || !player)
            return;

        // Ensure stopped state
        player->stop();
        player->busy = false;
    }

    Player* get(uint8_t num)
    {
        if (synchronized || !initialized)
            return NULL;

        if (num > MAX_PLAYERS)
            return NULL;

        players[num].busy = true;
        return &players[num];
    }

    void poll()
    {
        if (!initialized)
            return;

        for (uint8_t i = 0; i < MAX_PLAYERS; i++)
        {
            if (players[i].busy)
                players[i].poll();
        }
    }

    void stopAll(bool ramp_volume = false)
    {
        if (!initialized)
            return;

        for (uint8_t i = 0; i < MAX_PLAYERS; i++)
        {
            if (players[i].busy)
                players[i].stop(ramp_volume);
        }
    }

    void pauseAll(bool ramp_volume = false)
    {
        if (!initialized)
            return;

        for (uint8_t i = 0; i < MAX_PLAYERS; i++)
        {
            if (players[i].busy)
                players[i].pause(ramp_volume);
        }
    }

    void resumeAll()
    {
        if (!initialized)
            return;

        for (uint8_t i = 0; i < MAX_PLAYERS; i++)
        {
            if (players[i].busy)
                players[i].resume();
        }
    }

    void releaseAll()
    {
        if (!initialized || !synchronized)
            return;

        for (uint8_t i = 0; i < MAX_PLAYERS; i++)
            release(&players[i]);
    }

    bool playing()
    {
    	AudioSourceStatus status;

    	for (uint8_t i = 0; i < MAX_PLAYERS; i++)
        {
    			status = players[i].wav.getStatus();
            if (status == AudioSourcePlaying || status == AudioSourcePaused)
                return true;
        }

        return false;
    }

    inline uint8_t getMaxPlayers() { return MAX_PLAYERS; }
};

#endif // __PLAYER_H__
