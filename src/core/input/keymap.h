#pragma once
#include "platform/platform.h"
#include "input_actions.h"

struct KeyBinding
{
    KeyCode key;
    ActionId action;
};

// --- 按键绑定配置 ---
static const KeyBinding GLOBAL_KEYMAP[] = {
    // 播放
    {KeyCode::PLAY_PAUSE, ActionId::PLAY_TOGGLE}, // Space

    // 导航 (确认/返回/列表)
    {KeyCode::OK, ActionId::NAV_SELECT},   // Enter
    {KeyCode::BACK, ActionId::NAV_BACK},   // Esc
    {KeyCode::LIST, ActionId::ENTER_LIST}, // Backspace

    // 方向键 (上下切歌，左右快进)
    {KeyCode::UP, ActionId::NAV_UP},          // ;
    {KeyCode::DOWN, ActionId::NAV_DOWN},      // .
    {KeyCode::RIGHT, ActionId::SEEK_FORWARD}, // /
    {KeyCode::LEFT, ActionId::SEEK_REWIND},   // ,

    // 快捷键 (兼容旧习惯)
    {KeyCode::NEXT, ActionId::NAV_DOWN}, // n
    {KeyCode::PREV, ActionId::NAV_UP},   // p

    // 音量
    {KeyCode::VOL_INC, ActionId::VOLUME_UP},   // +
    {KeyCode::VOL_DEC, ActionId::VOLUME_DOWN}, // -

    // 模式 (顶部按键)
    {KeyCode::MODE_SWITCH, ActionId::TOGGLE_MODE}, // BtnA
};

static const size_t KEYMAP_SIZE = sizeof(GLOBAL_KEYMAP) / sizeof(GLOBAL_KEYMAP[0]);

inline ActionId lookupAction(KeyCode key)
{
    for (size_t i = 0; i < KEYMAP_SIZE; ++i)
    {
        if (GLOBAL_KEYMAP[i].key == key)
        {
            return GLOBAL_KEYMAP[i].action;
        }
    }
    return ActionId::NONE;
}