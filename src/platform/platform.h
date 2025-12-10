#pragma once
#include <Arduino.h>
#include <vector>

enum class PlatformModel
{
    UNKNOWN,
    CARDPUTER_V1,
    CARDPUTER_ADV
};

enum class KeyCode
{
    NONE,
    OK,
    BACK,
    LIST,
    PLAY_PAUSE,
    UP,
    DOWN,
    LEFT,
    RIGHT,
    VOL_INC,
    VOL_DEC,
    MODE_SWITCH, // Tab
    MUTE_TOGGLE, // Ctrl
    REFRESH,     // R
    NEXT,
    PREV
};

struct KeyEvent
{
    KeyCode code;
    bool pressed;
};

enum class AppEvent
{
    NONE,
    PLAY,
    PAUSE,
    NEXT,
    PREV,
    VOL_UP,
    VOL_DOWN,
    SELECT_SONG,
    STOP,
    REFRESH
};

struct BatteryStatus
{
    float voltage;
    int32_t level;
    bool charging;
};

// API
void platformInit();
PlatformModel platformGetModel();
void platformUpdate();
void platformGfxFillScreen(uint32_t c);
void platformGfxSetBrightness(uint8_t level);
void platformGfxDrawText(int16_t x, int16_t y, const char *text, uint32_t rgb888, uint8_t size);

bool platformAudioInit(uint32_t sampleRate);
void platformAudioSetVolume(uint8_t vol);
void *platformGetAudioOutputPtr();

bool platformPollKeyEvent(KeyEvent &ev);
BatteryStatus platformGetBattery();
