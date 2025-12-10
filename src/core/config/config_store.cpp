#include "core/config/config_store.h"
#include <Preferences.h>
#include <Arduino.h>

static Preferences prefs;

void configInit()
{
    // NVS 初始化
}

void configLoad(AppState *app)
{
    if (!app)
        return;

    if (!prefs.begin("mp3_cfg", true))
    { // read-only = true
        // 默认值
        app->volume = 60;
        app->currentTrackIdx = 0;
        app->playMode = PlayMode::SEQUENCE;
        return;
    }

    app->volume = prefs.getInt("vol", 60);
    app->currentTrackIdx = prefs.getInt("idx", 0);
    // [修复] 读取模式
    app->playMode = (PlayMode)prefs.getInt("mode", (int)PlayMode::SEQUENCE);

    prefs.end();
}

void configSave(const AppState *app)
{
    if (!app)
        return;

    if (!prefs.begin("mp3_cfg", false))
        return; // read-write

    prefs.putInt("vol", app->volume);
    prefs.putInt("idx", app->currentTrackIdx);
    // [修复] 保存模式
    prefs.putInt("mode", (int)app->playMode);

    prefs.end();
}