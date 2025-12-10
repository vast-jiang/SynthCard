#include "platform/platform.h"
#include <M5Cardputer.h>
#include <SPI.h>
#include <FS.h>
#include <SD.h>
#include <AudioOutputI2S.h>
#include <AudioOutputBuffer.h>
#include <math.h>
#include <esp_task_wdt.h>

#define USE_CARDPUTER_ADV 1

#define SD_SPI_SCK 40
#define SD_SPI_MISO 39
#define SD_SPI_MOSI 14
#define SD_SPI_CS 12

static bool g_isInitialized = false;
static AudioOutputI2S *g_baseOut = nullptr;
static AudioOutputBuffer *g_buffOut = nullptr;

static uint32_t g_lastVolRepeatTime = 0;
static uint32_t g_lastCtrlTime = 0;

void platformInit()
{
    if (g_isInitialized)
        return;

    auto cfg = M5.config();
    cfg.output_power = true;
    M5Cardputer.begin(cfg, true);

    M5.Speaker.begin();
    M5.Speaker.setVolume(255);

    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.fillScreen(BLACK);
    M5Cardputer.Display.setTextSize(1);

    SPI.begin(SD_SPI_SCK, SD_SPI_MISO, SD_SPI_MOSI, SD_SPI_CS);
    // 降速到 16MHz 提升大文件读取稳定性
    if (!SD.begin(SD_SPI_CS, SPI, 16000000))
        Serial.println("SD Fail");

    // [核心修复] 延长看门狗到 60秒，防止读取大文件时重启
    esp_task_wdt_init(60, true);
    esp_task_wdt_add(NULL); // 将主线程加入监控

    g_isInitialized = true;
}

PlatformModel platformGetModel() { return PlatformModel::CARDPUTER_ADV; }

void platformUpdate()
{
    M5Cardputer.update();
    // 主线程喂狗
    esp_task_wdt_reset();
}

void platformGfxFillScreen(uint32_t c) { M5Cardputer.Display.fillScreen(c); }
void platformGfxSetBrightness(uint8_t level) { M5Cardputer.Display.setBrightness(level); }
void platformGfxDrawText(int16_t x, int16_t y, const char *text, uint32_t rgb888, uint8_t size)
{
    M5Cardputer.Display.setTextColor(rgb888);
    M5Cardputer.Display.setTextSize(size);
    M5Cardputer.Display.setCursor(x, y);
    M5Cardputer.Display.print(text);
}

static uint8_t g_lastVol = 0;

bool platformAudioInit(uint32_t sampleRate)
{
    if (g_baseOut)
        return true;

    g_baseOut = new AudioOutputI2S(1, AudioOutputI2S::EXTERNAL_I2S);
    g_baseOut->SetPinout(41, 43, 46);
    g_baseOut->SetOutputModeMono(false);

    // [优化] 减小 Buffer 到 12KB，给解码器留出更多堆内存防止崩盘
    g_buffOut = new AudioOutputBuffer(1024 * 12, g_baseOut);
    g_baseOut->SetGain(0.05);
    g_lastVol = 5;
    return true;
}

void platformAudioSetVolume(uint8_t vol)
{
    if (g_baseOut)
    {
        float v = (float)vol / 100.0f;
        float gain = pow(v, 3.0f);
        g_baseOut->SetGain(gain);
        g_lastVol = vol;
    }
}

void *platformGetAudioOutputPtr() { return (void *)g_buffOut; }
size_t platformAudioWrite(const int16_t *interleavedStereo, size_t samples) { return samples; }
void platformAudioStop()
{
    if (g_buffOut)
        g_buffOut->stop();
}
bool platformIsHeadphonePlugged() { return false; }

bool platformPollKeyEvent(KeyEvent &ev)
{
    // 1. Ctrl (Mute)
    if (M5Cardputer.Keyboard.isKeyPressed(KEY_LEFT_CTRL))
    {
        if (millis() - g_lastCtrlTime > 300)
        {
            g_lastCtrlTime = millis();
            ev.code = KeyCode::MUTE_TOGGLE;
            ev.pressed = true;
            return true;
        }
        return false;
    }

    // 2. 音量
    if (M5Cardputer.Keyboard.isPressed())
    {
        if (!M5Cardputer.Keyboard.isKeyPressed(KEY_LEFT_CTRL) && !M5Cardputer.Keyboard.isKeyPressed(KEY_TAB))
        {
            if (millis() - g_lastVolRepeatTime > 100)
            {
                KeyCode vk = KeyCode::NONE;
                if (M5Cardputer.Keyboard.isKeyPressed('=') || M5Cardputer.Keyboard.isKeyPressed('+'))
                    vk = KeyCode::VOL_INC;
                else if (M5Cardputer.Keyboard.isKeyPressed('-') || M5Cardputer.Keyboard.isKeyPressed('_'))
                    vk = KeyCode::VOL_DEC;
                if (vk != KeyCode::NONE)
                {
                    g_lastVolRepeatTime = millis();
                    ev.code = vk;
                    ev.pressed = true;
                    return true;
                }
            }
        }
    }

    // 3. 单发键
    if (M5Cardputer.Keyboard.isChange())
    {
        if (M5Cardputer.Keyboard.isPressed())
        {
            KeyCode k = KeyCode::NONE;
            if (M5Cardputer.Keyboard.isKeyPressed(KEY_TAB))
                k = KeyCode::MODE_SWITCH;
            else if (M5Cardputer.Keyboard.isKeyPressed(';'))
                k = KeyCode::UP;
            else if (M5Cardputer.Keyboard.isKeyPressed('.'))
                k = KeyCode::DOWN;
            else if (M5Cardputer.Keyboard.isKeyPressed(','))
                k = KeyCode::LEFT;
            else if (M5Cardputer.Keyboard.isKeyPressed('/'))
                k = KeyCode::RIGHT;
            else if (M5Cardputer.Keyboard.isKeyPressed(KEY_ENTER))
                k = KeyCode::OK;
            else if (M5Cardputer.Keyboard.isKeyPressed(KEY_BACKSPACE))
                k = KeyCode::LIST;
            else if (M5Cardputer.Keyboard.isKeyPressed(27) || M5Cardputer.Keyboard.isKeyPressed('`'))
                k = KeyCode::BACK;
            else if (M5Cardputer.Keyboard.isKeyPressed(' '))
                k = KeyCode::PLAY_PAUSE;
            else if (M5Cardputer.Keyboard.isKeyPressed('r') || M5Cardputer.Keyboard.isKeyPressed('R'))
                k = KeyCode::REFRESH;

            if (M5Cardputer.Keyboard.isKeyPressed('=') || M5Cardputer.Keyboard.isKeyPressed('-') ||
                M5Cardputer.Keyboard.isKeyPressed(KEY_LEFT_CTRL))
                return false;

            if (k != KeyCode::NONE)
            {
                ev.code = k;
                ev.pressed = true;
                return true;
            }
        }
    }
    return false;
}

BatteryStatus platformGetBattery()
{
    BatteryStatus st;
    st.level = M5Cardputer.Power.getBatteryLevel();
    st.voltage = M5Cardputer.Power.getBatteryVoltage() / 1000.0f;
    st.charging = M5Cardputer.Power.isCharging();
    return st;
}
