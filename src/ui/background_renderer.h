#pragma once
#include <Arduino.h>
#include <M5Cardputer.h>
#include <vector>

struct Star
{
    float x, y;
    float speed;
    uint8_t brightness;
};

struct Meteor
{
    float x, y;
    float speedX, speedY;
    bool active;
};

// 你需要的 NebulaWave 结构，放在头文件里，只定义一次
struct NebulaWave
{
    float amplitude;
    float frequency;
    float phase;
    float verticalOffset;
    float parallaxFactor;
    uint16_t color;
};

// 星云条数常量（只声明一次）
constexpr int NUM_NEBULA_WAVES = 3;

void bgInit();
void bgUpdate(float dt = 0.1f);
void bgDraw(M5Canvas *sprite);
