#include "background_renderer.h"
#include <math.h>

// ======================
// 基本配置
// ======================
#define NUM_STARS 40
#define NUM_METEORS 3

// 深空底色
static const uint16_t SPACE_BG = 0x0002;

// 如果头文件里没有定义这三个颜色，就在这里给一份
#ifndef NEBULA_VIOLET
#define NEBULA_VIOLET 0x780F // 霓虹紫
#endif
#ifndef NEBULA_ORANGE
#define NEBULA_ORANGE 0xFD20 // 霓虹橙
#endif
#ifndef NEBULA_BLUE
#define NEBULA_BLUE 0x051F // 霓虹蓝
#endif

// ======================
// 全局数据
// ======================
static std::vector<Star> stars;
static std::vector<Meteor> meteors;

// 标记每颗流星是否在星云“后面”
static std::vector<bool> meteorsBehindNebula;

// 小像素“星系”
struct MicroGalaxy
{
    float x, y;
    float phase;
    float speed;
};

#define NUM_MICRO_GALAXIES 5
static MicroGalaxy microGalaxies[NUM_MICRO_GALAXIES];

// 按你给的参数初始化 NebulaWave
// 注意：NebulaWave 和 NUM_NEBULA_WAVES 在 background_renderer.h 里已经定义/声明
static NebulaWave nebulaWaves[NUM_NEBULA_WAVES] = {
    {25.0f, 0.015f, 1.0f, 80.0f, 0.20f, NEBULA_VIOLET},
    {20.0f, 0.020f, 3.5f, 95.0f, 0.35f, NEBULA_ORANGE},
    {15.0f, 0.025f, 0.0f, 110.0f, 0.50f, NEBULA_BLUE}};

// 星云滚动 & 时间
static float g_nebulaScrollX = 0.0f;
static float g_nebulaTime = 0.0f;

// ======================
// 颜色混合：fg 覆盖 bg
// alpha: 0.0 ~ 1.0
// ======================
static inline uint16_t blendColor(uint16_t fg, uint16_t bg, float alpha)
{
    if (alpha <= 0.0f)
        return bg;
    if (alpha >= 1.0f)
        return fg;

    uint8_t fr = (fg >> 11) & 0x1F;
    uint8_t fg_g = (fg >> 5) & 0x3F;
    uint8_t fb = fg & 0x1F;

    uint8_t br = (bg >> 11) & 0x1F;
    uint8_t bg_g = (bg >> 5) & 0x3F;
    uint8_t bb = bg & 0x1F;

    uint8_t r = br + (int)((fr - br) * alpha);
    uint8_t g = bg_g + (int)((fg_g - bg_g) * alpha);
    uint8_t b = bb + (int)((fb - bb) * alpha);

    return (r << 11) | (g << 5) | b;
}

// ======================
// 画星云（星云 ONLY 改动的核心）
//
// 对每条 NebulaWave：
//   - 用正弦算出一条波线 waveY
//   - 从 waveY 一直画到屏幕底部（不是上下对称的管状）
//   - 使用 fadeFactor 控制整体透明度
// ======================
static void drawNebulaInternal(M5Canvas *sprite,
                               float currentOffsetX,
                               float currentOffsetY,
                               float fadeFactor)
{
    if (fadeFactor <= 0.0f)
        return;

    const int screenH = 135;

    for (int w = 0; w < NUM_NEBULA_WAVES; ++w)
    {
        NebulaWave &wave = nebulaWaves[w];

        // 每一层的基础透明度，越靠下的层稍微亮一点
        float baseAlpha = 0.40f + 0.10f * w; // 0.40 / 0.50 / 0.60
        float layerAlpha = baseAlpha * fadeFactor;
        if (layerAlpha > 1.0f)
            layerAlpha = 1.0f;

        for (int x = 0; x < 240; ++x)
        {
            float worldX = (float)x + currentOffsetX * wave.parallaxFactor;

            float phase = worldX * wave.frequency + wave.phase + g_nebulaTime * 0.35f; // 轻微流动

            // 一条波线：决定“星云从哪里开始”
            float waveY = wave.verticalOffset + sinf(phase) * wave.amplitude + currentOffsetY;

            if (waveY >= screenH)
                continue;
            if (waveY < 0.0f)
                waveY = 0.0f;

            int yStart = (int)waveY;
            int h = screenH - yStart;
            if (h <= 0)
                continue;

            // 从 waveY 一直画到屏幕底部
            uint16_t col = blendColor(wave.color, SPACE_BG, layerAlpha);
            sprite->drawFastVLine(x, yStart, h, col);
        }
    }
}

// ======================
// 初始化
// ======================
void bgInit()
{
    // 星星
    stars.clear();
    stars.reserve(NUM_STARS);
    for (int i = 0; i < NUM_STARS; i++)
    {
        Star s;
        s.x = random(0, 240);
        s.y = random(0, 135);
        s.speed = (random(5, 30) / 100.0f); // 0.05 ~ 0.30
        s.brightness = random(50, 200);
        stars.push_back(s);
    }

    // 流星 + 层级
    meteors.clear();
    meteorsBehindNebula.clear();
    meteors.reserve(NUM_METEORS);
    meteorsBehindNebula.reserve(NUM_METEORS);

    for (int i = 0; i < NUM_METEORS; i++)
    {
        Meteor m;
        m.x = m.y = 0.0f;
        m.speedX = m.speedY = 0.0f;
        m.active = false;
        meteors.push_back(m);
        meteorsBehindNebula.push_back(false);
    }

    // 小像素星系
    for (int i = 0; i < NUM_MICRO_GALAXIES; ++i)
    {
        microGalaxies[i].x = random(10, 230);
        microGalaxies[i].y = random(15, 110);
        microGalaxies[i].phase = random(0, 628) / 100.0f;        // 0 ~ 2π
        microGalaxies[i].speed = 0.20f + random(0, 20) / 100.0f; // 0.20 ~ 0.40
    }

    g_nebulaScrollX = 0.0f;
    g_nebulaTime = 0.0f;
}

// ======================
// 更新
// ======================
void bgUpdate(float dt)
{
    // 星星移动
    for (auto &s : stars)
    {
        s.x -= s.speed;
        if (s.x < 0)
        {
            s.x = 240;
            s.y = random(0, 135);
        }
    }

    // 流星（更少、更随机、直线）
    for (size_t i = 0; i < meteors.size(); ++i)
    {
        auto &m = meteors[i];

        if (m.active)
        {
            m.x += m.speedX;
            m.y += m.speedY;

            if (m.x < -40 || m.y > 160)
            {
                m.active = false;
            }
        }
        else
        {
            // 出现概率 7 / 1000
            if (random(0, 1000) < 7)
            {
                m.active = true;

                // 从右上区域随机飞进
                m.x = random(240, 290);
                m.y = random(-20, 60);

                float base = 2.0f + random(0, 20) / 10.0f;    // 2.0 ~ 4.0
                float ratioX = 0.7f + random(0, 30) / 100.0f; // 0.7 ~ 1.0
                float ratioY = 0.3f + random(0, 30) / 100.0f; // 0.3 ~ 0.6

                m.speedX = -base * ratioX; // 左
                m.speedY = base * ratioY;  // 下

                // 随机有一部分在星云后面
                meteorsBehindNebula[i] = (random(0, 100) < 45);
            }
        }
    }

    // 小星系闪烁 / 漂移
    for (int i = 0; i < NUM_MICRO_GALAXIES; ++i)
    {
        microGalaxies[i].phase += microGalaxies[i].speed * dt;
        if (microGalaxies[i].phase > 6.28318f)
            microGalaxies[i].phase -= 6.28318f;

        microGalaxies[i].x += 0.05f * sinf(microGalaxies[i].phase * 0.5f);
        if (microGalaxies[i].x < 5)
            microGalaxies[i].x = 235;
        if (microGalaxies[i].x > 235)
            microGalaxies[i].x = 5;
    }

    // 星云滚动
    g_nebulaScrollX += 10.0f * dt;
    if (g_nebulaScrollX > 10000.0f)
        g_nebulaScrollX -= 10000.0f;

    g_nebulaTime += dt;
}

// ======================
// 绘制
// ======================
void bgDraw(M5Canvas *sprite)
{
    // 深空背景
    sprite->fillScreen(SPACE_BG);

    // 1) 星星
    for (auto &s : stars)
    {
        uint8_t b = s.brightness;
        if (random(0, 100) < 2)
            b = 255; // 偶尔闪一下

        uint16_t color = sprite->color565(b, b, b);
        sprite->drawPixel((int)s.x, (int)s.y, color);
    }

    // 2) 小像素星系
    for (int i = 0; i < NUM_MICRO_GALAXIES; ++i)
    {
        float tw = (sinf(microGalaxies[i].phase) * 0.5f + 0.5f); // 0~1
        uint8_t base = 120 + (uint8_t)(tw * 135);                // 120~255

        uint16_t col = sprite->color565(
            (uint8_t)(base * 0.6f),
            (uint8_t)(base * 0.8f),
            base);

        int px = (int)microGalaxies[i].x;
        int py = (int)microGalaxies[i].y;

        sprite->drawPixel(px, py, col);

        if (tw > 0.8f)
        {
            sprite->drawPixel(px + 1, py, col);
            sprite->drawPixel(px, py + 1, col);
        }
    }

    // 3) 先画“在星云后面”的流星（等会被星云覆盖一部分）
    for (size_t i = 0; i < meteors.size(); ++i)
    {
        const auto &m = meteors[i];
        if (!m.active)
            continue;
        if (!meteorsBehindNebula[i])
            continue;

        sprite->drawLine(
            (int)m.x,
            (int)m.y,
            (int)(m.x - m.speedX * 3.0f),
            (int)(m.y - m.speedY * 3.0f),
            0xFFFF);
    }

    // 4) 星云（按你的 NebulaWave，从波线往下铺到底）
    drawNebulaInternal(sprite,
                       g_nebulaScrollX, // currentOffsetX
                       0.0f,            // currentOffsetY
                       1.0f);           // fadeFactor（目前常驻 1）

    // 5) 再画“在星云前面”的流星
    for (size_t i = 0; i < meteors.size(); ++i)
    {
        const auto &m = meteors[i];
        if (!m.active)
            continue;
        if (meteorsBehindNebula[i])
            continue;

        sprite->drawLine(
            (int)m.x,
            (int)m.y,
            (int)(m.x - m.speedX * 3.0f),
            (int)(m.y - m.speedY * 3.0f),
            0xFFFF);
    }

    // ✅ 不画行星圆圈 / 地平线 / 额外条纹
}
