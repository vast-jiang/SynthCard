#pragma once
#include <Arduino.h>

// 简单的日志宏，支持 TAG 和 格式化
#define LOG_TAG(tag, fmt, ...)                               \
    do                                                       \
    {                                                        \
        Serial.printf("[%s] " fmt "\n", tag, ##__VA_ARGS__); \
    } while (0)

#define LOG_PLATFORM(fmt, ...) LOG_TAG("PLAT", fmt, ##__VA_ARGS__)
#define LOG_AUDIO(fmt, ...) LOG_TAG("AUDIO", fmt, ##__VA_ARGS__)
#define LOG_UI(fmt, ...) LOG_TAG("UI", fmt, ##__VA_ARGS__)
#define LOG_CFG(fmt, ...) LOG_TAG("CFG", fmt, ##__VA_ARGS__)
#define LOG_CORE(fmt, ...) LOG_TAG("CORE", fmt, ##__VA_ARGS__)