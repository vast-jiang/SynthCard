#include "ui/ui_root.h"
#include "platform/platform.h"
#include "core/audio/audio_engine.h"
#include "background_renderer.h"
#include <M5Cardputer.h>
#include <math.h>

static AppState *g_app = nullptr;
static M5Canvas *g_sprite = nullptr;
static int g_scrollOffset = 0;
static int g_listScroll = 0;
static uint32_t g_titleScrollTime = 0;
static uint32_t g_listScrollTime = 0;

// 外部函数声明
bool audioEngineIsMuted();
String audioEngineGetListItem(int index);
int audioEngineGetTotalTracks();

// ==========================================
// 颜色定义
// ==========================================
#define C_BLACK 0x0000
#define C_WHITE 0xFFFF
#define C_CYAN 0x07FF
#define C_MAGENTA 0xF81F
#define C_GREEN 0x07E0
#define C_RED 0xF800
#define C_DARK 0x1082
#define C_DIM 0x0821
#define C_MASK 0x0000 // 遮罩色(黑色)

// 初始化 UI
void uiInit(AppState *app)
{
    g_app = app;
    g_sprite = new M5Canvas(&M5Cardputer.Display);
    g_sprite->createSprite(240, 135);
    // 使用内置中文支持
    g_sprite->setFont(&fonts::efontCN_16);
    g_sprite->setTextSize(1);

    // 初始化星空 / 星云背景
    bgInit();
}

// 开机动画：赛博宇宙背景 + VAST_JIANG Player 霓虹字闪烁+轻微晃动
void uiShowBootAnim()
{
    if (!g_sprite)
        return;

    uint32_t start = millis();
    while (millis() - start < 2000) // ~2s 动画
    {
        float t = (millis() - start) * 0.004f; // 时间相位

        // 背景
        bgUpdate(0.06f);
        bgDraw(g_sprite);

        const char *title1 = "VAST_JIANG";
        const char *title2 = "Player";

        // 霓虹闪烁：亮度在 0.7~1.0 之间轻微起伏
        float flick = 0.7f + 0.3f * sinf(t * 3.0f);

        // 霓虹基色：紫 + 青，使用 0~255 的 RGB，真正亮起来
        int r1 = (int)(255 * flick);
        int g1 = (int)(80 * flick);
        int b1 = (int)(255 * flick);

        int r2 = (int)(80 * flick);
        int g2 = (int)(255 * flick);
        int b2 = (int)(255 * flick);

        // 简单夹紧
        if (r1 > 255)
            r1 = 255;
        if (g1 > 255)
            g1 = 255;
        if (b1 > 255)
            b1 = 255;
        if (r2 > 255)
            r2 = 255;
        if (g2 > 255)
            g2 = 255;
        if (b2 > 255)
            b2 = 255;

        uint16_t col1 = g_sprite->color565(r1, g1, b1);
        uint16_t col2 = g_sprite->color565(r2, g2, b2);

        // 轻微晃动（抖动 1~2 像素）
        int jitterX = (int)(sinf(t * 1.7f) * 2.0f);
        int jitterY1 = (int)(cosf(t * 1.3f) * 1.5f);
        int jitterY2 = (int)(sinf(t * 1.1f) * 1.5f);

        int cx1 = 120 + jitterX;
        int cx2 = 120 + jitterX;
        int y1 = 46 + jitterY1;
        int y2 = 70 + jitterY2;

        // 外描边阴影（稍微偏移 1 像素）
        g_sprite->setTextColor(C_BLACK);
        g_sprite->drawCenterString(title1, cx1 + 1, y1 + 1);
        g_sprite->drawCenterString(title2, cx2 + 1, y2 + 1);

        // 霓虹主体
        g_sprite->setTextColor(col1);
        g_sprite->drawCenterString(title1, cx1, y1);
        g_sprite->setTextColor(col2);
        g_sprite->drawCenterString(title2, cx2, y2);

        g_sprite->pushSprite(0, 0);
        delay(20);
    }
}

// ==========================
// 辅助：绘制状态文字（完全透明背景）
// ==========================
void drawMaskedText(const String &str, int x, int y, uint32_t color)
{
    // 不再画任何遮罩或底色，直接透明叠加在背景上
    g_sprite->setTextColor(color);
    g_sprite->drawString(str, x, y);
}

// ==========================
// 音量进度条（颜色表示音量，高音量变红）
// ==========================
void drawVolBar(int x, int y, int w, int h, int vol, bool isMuted)
{
    // 外框
    g_sprite->drawRect(x, y, w, h, C_DARK);

    // 静音：只保留边框，内部透明，在正中间显示 MUTED
    if (isMuted)
    {
        int cx = x + w / 2;
        int textY = y + (h / 2 - 8); // 16 像素字高的一半

        // 不画任何方块，让背景透出来
        g_sprite->setTextColor(C_RED);
        g_sprite->drawCenterString("MUTED", cx, textY);
        return;
    }

    // 非静音：16 段彩色块填满整个内部宽度
    const int total = 16;
    int active = map(vol, 0, 100, 0, total);
    if (active < 0)
        active = 0;
    if (active > total)
        active = total;

    int innerW = w - 2; // 去掉左右边框后的内部宽度
    if (innerW < total)
        innerW = total;

    int bw = innerW / total; // 基础块宽
    if (bw < 1)
        bw = 1;

    for (int i = 0; i < total; i++)
    {
        // 颜色：低音量青色，中音量品红，高音量红色
        uint16_t c = C_DIM;
        if (i < active)
        {
            float level = (float)(i + 1) / (float)total;
            if (level > 0.8f)
                c = C_RED;
            else if (level > 0.5f)
                c = C_MAGENTA;
            else
                c = C_CYAN;
        }

        int bx = x + 1 + i * bw;
        // 最后一格吃掉所有剩余宽度，避免右侧空白
        int curW = (i == total - 1) ? (innerW - i * bw) : bw;
        if (curW < 1)
            curW = 1;

        g_sprite->fillRect(bx, y + 2, curW - 1, h - 4, c);
    }
}

// ==========================
// 播放界面
// ==========================
void renderPlayer()
{
    // 1. 背景：星空 + 流星 + 星云
    bgUpdate(0.05f);
    bgDraw(g_sprite);

    // 2. 顶部 Header
    g_sprite->fillRect(0, 0, 240, 18, C_MASK);
    g_sprite->drawFastHLine(0, 18, 240, C_CYAN);

    g_sprite->setTextColor(C_CYAN);
    g_sprite->drawString(" VAST_JIANG OS", 2, 2);

    BatteryStatus bat = platformGetBattery();
    char batS[16];
    sprintf(batS, "PWR:%d%%", bat.level);
    uint32_t batCol = bat.level > 20 ? C_GREEN : C_RED;
    g_sprite->setTextColor(batCol);
    g_sprite->drawRightString(batS, 236, 2);

    // 3. 歌名区域 (赛博框，内部透明，只画边框)
    int boxY = 30;
    g_sprite->drawRect(10, boxY, 220, 32, C_MAGENTA);
    // 装饰角
    g_sprite->fillRect(10, boxY, 4, 2, C_MAGENTA);
    g_sprite->fillRect(226, boxY + 30, 4, 2, C_MAGENTA);

    String title = g_app->currentTitle;
    g_sprite->setTextColor(C_WHITE);
    int tW = g_sprite->textWidth(title);

    // 播放栏标题滚动（带裁剪，不会穿出边框）
    int clipX = 12;
    int clipY = boxY + 4;
    int clipW = 216;
    int clipH = 24;

    if (tW > clipW)
    {
        if (millis() - g_titleScrollTime > 80) // 更慢一点
        {
            g_titleScrollTime = millis();
            g_scrollOffset -= 1;
            int minOffset = -(tW - clipW);
            if (g_scrollOffset < minOffset - 10)
                g_scrollOffset = 0; // 走完一轮后回到开头
        }

        g_sprite->setClipRect(clipX, clipY, clipW, clipH);
        g_sprite->drawString(title, clipX + g_scrollOffset, boxY + 8);
        g_sprite->clearClipRect();
    }
    else
    {
        g_scrollOffset = 0;
        g_sprite->drawCenterString(title, 120, boxY + 8);
    }

    // 4. 状态 & 伪频谱
    const char *st = g_app->isPlaying ? ">> RUNNING" : "|| PAUSED";
    uint32_t stCol = g_app->isPlaying ? C_GREEN : C_RED;
    drawMaskedText(st, 15, 75, stCol);

    // 频谱线（赛博霓虹色随机跳动）
    int specY = 95;
    g_sprite->drawFastHLine(15, specY, 140, C_MAGENTA);
    if (g_app->isPlaying)
    {
        for (int i = 0; i < 140; i += 6)
        {
            if (random(10) > 4)
            {
                int h = random(4, 18);
                // 根据 X 位置切换颜色，霓虹风
                uint16_t barColor;
                int mod = (i / 6) % 3;
                if (mod == 0)
                    barColor = C_CYAN;
                else if (mod == 1)
                    barColor = C_MAGENTA;
                else
                    barColor = g_sprite->color565(255, 128, 0); // 霓虹橙

                g_sprite->drawFastVLine(15 + i, specY - h / 2, h, barColor);
            }
        }
    }

    // 5. Mode 区域（仅显示模式文本，居中）
    int modeX = 170;
    int modeY = 70;
    int modeW = 60;
    int modeH = 28;

    // 只画边框，内部透明
    g_sprite->drawRect(modeX, modeY, modeW, modeH, C_CYAN);

    const char *modeStr = "SEQ";
    if (g_app->playMode == PlayMode::REPEAT)
        modeStr = "LOOP";
    else if (g_app->playMode == PlayMode::SHUFFLE)
        modeStr = "RND";

    // 手动水平 + 垂直居中（按 16 像素字体估算）
    g_sprite->setTextColor(C_WHITE);
    int modeTextW = g_sprite->textWidth(modeStr);
    int modeTextX = modeX + (modeW - modeTextW) / 2;
    int modeTextY = modeY + (modeH - 16) / 2; // 16 是字体高度
    g_sprite->drawString(modeStr, modeTextX, modeTextY);

    // 6. 底部：音量
    g_sprite->setTextColor(C_CYAN);
    g_sprite->drawString("VOL", 5, 113);
    drawVolBar(35, 113, 110, 12, g_app->volume, audioEngineIsMuted());
}

// ==========================
// 列表浏览界面
// ==========================
void renderBrowser()
{
    // 1. 背景
    bgUpdate(0.05f);
    bgDraw(g_sprite);

    // Header
    g_sprite->fillRect(0, 0, 240, 20, C_MASK);
    g_sprite->drawFastHLine(0, 20, 240, C_GREEN);
    g_sprite->setTextColor(C_GREEN);
    g_sprite->drawString(" > FILE EXPLORER", 5, 2);

    int total = audioEngineGetTotalTracks();
    if (total == 0)
    {
        g_sprite->setTextColor(C_RED);
        g_sprite->drawCenterString("NO FILES", 120, 60);
        return;
    }

    // 列表布局
    int maxLines = 5;
    int lh = 22;
    int startY = 25;

    if (g_app->browserCursor < g_app->browserScrollTop)
        g_app->browserScrollTop = g_app->browserCursor;
    if (g_app->browserCursor >= g_app->browserScrollTop + maxLines)
        g_app->browserScrollTop = g_app->browserCursor - maxLines + 1;

    for (int i = 0; i < maxLines; i++)
    {
        int idx = g_app->browserScrollTop + i;
        if (idx >= total)
            break;

        int y = startY + i * lh;
        bool sel = (idx == g_app->browserCursor);
        String name = audioEngineGetListItem(idx);

        if (sel)
        {
            // 选中项：白色条 + 黑字
            g_sprite->fillRect(0, y, 230, lh, C_WHITE);
            g_sprite->setTextColor(C_BLACK);

            int w = g_sprite->textWidth(name);
            int clipX = 2;
            int clipY = y + 2;
            int clipW = 226;
            int clipH = lh - 4;

            if (w > clipW)
            {
                if (millis() - g_listScrollTime > 80)
                {
                    g_listScrollTime = millis();
                    g_listScroll -= 1;
                    int minOffset = -(w - clipW);
                    if (g_listScroll < minOffset - 10)
                        g_listScroll = 0;
                }
                g_sprite->setClipRect(clipX, clipY, clipW, clipH);
                g_sprite->drawString(name, 5 + g_listScroll, y + 3);
                g_sprite->clearClipRect();
            }
            else
            {
                g_listScroll = 0;
                g_sprite->drawString("> " + name, 5, y + 3);
            }
        }
        else
        {
            // 普通项：背景透明，只画绿色文字
            g_sprite->setTextColor(C_GREEN);
            g_sprite->drawString("  " + name, 5, y + 3);
        }
    }

    // 滚动条
    int barH = (total > 0) ? (100 * maxLines / total) : 100;
    if (barH < 5)
        barH = 5;
    int barY = startY + (g_app->browserScrollTop * (100 - barH) / (total - maxLines > 0 ? total - maxLines : 1));
    g_sprite->drawRect(236, startY, 3, 100, C_DARK);
    g_sprite->fillRect(236, barY, 3, barH, C_CYAN);
}

// ==========================
// 主 UI 渲染入口
// ==========================
void uiRender()
{
    if (!g_sprite)
        return;

    if (g_app->uiMode == UiMode::PLAYER)
        renderPlayer();
    else
        renderBrowser();

    g_sprite->pushSprite(0, 0);
}
