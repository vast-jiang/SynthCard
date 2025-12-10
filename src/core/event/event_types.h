#pragma once
#include <stdint.h>
#include "platform/platform.h" // 需要引用 platform.h 中的 KeyEvent 定义

enum class EventType : uint16_t
{
    KEY_EVENT,

    // 播放控制
    PLAY_REQUEST,
    PAUSE_REQUEST,
    // 预留给未来扩展
    RESUME_REQUEST,
    STOP_REQUEST,
    NEXT_TRACK,
    PREV_TRACK,

    // 系统
    CONFIG_CHANGED,
};

struct Event
{
    EventType type;
    uint32_t ts_ms;

    union
    {
        KeyEvent key;
        struct
        {
            int dummy;
        } _placeholder; // 占位符，防止 union 为空
    } data;
};