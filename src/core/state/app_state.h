#pragma once
#include <stdint.h>
#include <Arduino.h>

enum class UiMode
{
    PLAYER,
    BROWSER
};

enum class PlayMode
{
    SEQUENCE,
    REPEAT,
    SHUFFLE
};

struct AppState
{
    int32_t volume;
    int32_t brightness;

    // UI 状态
    UiMode uiMode;
    int32_t browserCursor;
    int32_t browserScrollTop;
    bool inBrowser; // 辅助标志

    // 播放状态
    bool isPlaying;
    int32_t currentTrackIdx;
    char currentTitle[64];
    PlayMode playMode;
    int32_t totalTracks;
};