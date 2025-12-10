#pragma once
#include <stdint.h>

enum class ActionId : uint8_t
{
    NONE,

    // 播放控制
    PLAY_TOGGLE,  // 播放/暂停
    STOP_PLAYING, // 停止

    // 导航
    NAV_UP,     // 向上 (切歌/选行)
    NAV_DOWN,   // 向下 (切歌/选行)
    NAV_SELECT, // 确定 (进入列表/播放选中)
    NAV_BACK,   // 返回 (退出列表/停止)
    ENTER_LIST, // 直接进入列表

    // 进度与音量
    SEEK_FORWARD, // 快进
    SEEK_REWIND,  // 快退
    VOLUME_UP,    // 音量+
    VOLUME_DOWN,  // 音量-

    // 系统
    TOGGLE_MODE, // 切换循环/随机模式
};