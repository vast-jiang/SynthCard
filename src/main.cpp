#include <Arduino.h>
#include "platform/platform.h"
#include "core/state/app_state.h"
#include "core/config/config_store.h"
#include "ui/ui_root.h"

#include <AudioFileSourceSD.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>
#include <AudioOutputBuffer.h>

#define MAX_FILES 200
#define MAX_PATH_LEN 100

static AppState gAppState;

static char g_playlist[MAX_FILES][MAX_PATH_LEN];
static int g_totalTracks = 0;

static bool g_isMuted = false;

AudioGeneratorMP3 *mp3 = nullptr;
AudioFileSourceSD *file = nullptr;
AudioOutputBuffer *buff = nullptr;
AudioOutputI2S *out = nullptr;

volatile AppEvent g_pendingEvent = AppEvent::NONE;
volatile int g_seekDir = 0;
TaskHandle_t TaskHandle_Audio;

extern void uiShowBootAnim();

// --- 辅助 ---
const char *getPathByIndex(int index)
{
  if (index < 0 || index >= g_totalTracks)
    return "";
  return g_playlist[index];
}

const char *getFileNameFromPath(const char *path)
{
  const char *slash = strrchr(path, '/');
  return slash ? slash + 1 : path;
}

const char *getSafeTitle()
{
  if (g_totalTracks == 0)
    return "NO FILES";
  if (gAppState.currentTrackIdx < 0 || gAppState.currentTrackIdx >= g_totalTracks)
    return "IDX ERR";
  return getFileNameFromPath(g_playlist[gAppState.currentTrackIdx]);
}

// --- UI 接口 ---
bool audioEngineIsPlaying() { return gAppState.isPlaying; }
int audioEngineGetTotalTracks() { return g_totalTracks; }
int audioEngineGetCurrentIndex() { return gAppState.currentTrackIdx; }
const char *audioEngineGetCurrentTitle() { return gAppState.currentTitle; }
bool audioEngineIsMuted() { return g_isMuted; }

String audioEngineGetListItem(int index)
{
  if (index >= 0 && index < g_totalTracks)
  {
    return String(getFileNameFromPath(g_playlist[index]));
  }
  return "";
}

const std::vector<String> &audioEngineGetPlaylist()
{
  static std::vector<String> d;
  return d;
}

// --- 文件扫描 ---
void scanDir(File dir)
{
  while (true)
  {
    if (g_totalTracks >= MAX_FILES)
      break;

    File entry = dir.openNextFile();
    if (!entry)
      break;

    platformUpdate();

    if (entry.isDirectory())
    {
      scanDir(entry);
    }
    else
    {
      String name = entry.name();
      if (name.startsWith("."))
        continue;

      if (name.endsWith(".mp3") || name.endsWith(".MP3"))
      {
        String path = entry.path();
        if (path == "")
          path = "/" + name;

        if (path.length() < MAX_PATH_LEN)
        {
          strncpy(g_playlist[g_totalTracks], path.c_str(), MAX_PATH_LEN - 1);
          g_playlist[g_totalTracks][MAX_PATH_LEN - 1] = '\0';
          g_totalTracks++;
        }
      }
    }
    entry.close();
  }
}

// --- Audio Task ---
void Task_Audio_Loop(void *pvParameters)
{
  vTaskDelay(500);

  platformAudioInit(44100);
  buff = (AudioOutputBuffer *)platformGetAudioOutputPtr();
  mp3 = new AudioGeneratorMP3();

  if (!g_isMuted)
    platformAudioSetVolume(gAppState.volume);

  while (true)
  {
    if (g_pendingEvent != AppEvent::NONE)
    {
      AppEvent evt = g_pendingEvent;
      g_pendingEvent = AppEvent::NONE;

      if (evt == AppEvent::SELECT_SONG || evt == AppEvent::NEXT || evt == AppEvent::PREV || evt == AppEvent::PLAY || evt == AppEvent::REFRESH)
      {
        platformAudioSetVolume(0);

        if (mp3->isRunning())
          mp3->stop();
        if (file)
        {
          delete file;
          file = nullptr;
        }

        if (g_totalTracks > 0 && gAppState.currentTrackIdx < g_totalTracks)
        {
          const char *path = g_playlist[gAppState.currentTrackIdx];
          Serial.printf("[AUDIO] Play: %s\n", path);

          file = new AudioFileSourceSD(path);
          if (file && file->isOpen())
          {
            mp3->begin(file, buff);
            gAppState.isPlaying = true;
            strncpy(gAppState.currentTitle, getSafeTitle(), 63);
          }
          else
          {
            Serial.println("[AUDIO] Open Failed");
          }
        }
        if (!g_isMuted)
          platformAudioSetVolume(gAppState.volume);
      }
      else if (evt == AppEvent::STOP)
      {
        if (mp3->isRunning())
          mp3->stop();
        gAppState.isPlaying = false;
      }
      else if (evt == AppEvent::PAUSE)
      {
        gAppState.isPlaying = false;
      }
      else if (evt == AppEvent::VOL_UP || evt == AppEvent::VOL_DOWN)
      {
        if (!g_isMuted)
          platformAudioSetVolume(gAppState.volume);
      }
    }

    if (g_seekDir != 0 && file && mp3->isRunning())
    {
      platformAudioSetVolume(0);
      int current = file->getPos();
      int target = current + (g_seekDir * 32000);
      if (target < 0)
        target = 0;
      if (target > file->getSize())
        target = file->getSize() - 1024;
      file->seek(target, SEEK_SET);
      g_seekDir = 0;
      if (!g_isMuted)
        platformAudioSetVolume(gAppState.volume);
    }

    if (gAppState.isPlaying && mp3->isRunning())
    {
      if (!mp3->loop())
      {
        mp3->stop();
        if (gAppState.playMode == PlayMode::SHUFFLE)
        {
          gAppState.currentTrackIdx = random(0, g_totalTracks);
        }
        else if (gAppState.playMode == PlayMode::REPEAT)
        {
        }
        else
        {
          gAppState.currentTrackIdx++;
        }

        if (gAppState.currentTrackIdx >= g_totalTracks)
          gAppState.currentTrackIdx = 0;
        g_pendingEvent = AppEvent::SELECT_SONG;
      }
    }
    else
    {
      vTaskDelay(10);
    }
    vTaskDelay(1);
  }
}

// --- 按键逻辑（完全保留） ---
void handleInput()
{
  platformUpdate();

  KeyEvent kev;
  static uint32_t lastKeyTime = 0;

  if (platformPollKeyEvent(kev))
  {
    if (!kev.pressed)
      return;
    if (millis() - lastKeyTime < 150)
      return;
    lastKeyTime = millis();

    bool updateUI = true;
    bool saveConfig = false;

    switch (kev.code)
    {
    case KeyCode::PLAY_PAUSE:
      if (gAppState.isPlaying)
      {
        gAppState.isPlaying = false;
        g_pendingEvent = AppEvent::PAUSE;
      }
      else
      {
        gAppState.isPlaying = true;
        if (!mp3 || !mp3->isRunning())
          g_pendingEvent = AppEvent::PLAY;
      }
      break;

    case KeyCode::REFRESH:
      g_pendingEvent = AppEvent::REFRESH;
      gAppState.isPlaying = true;
      break;

    case KeyCode::LIST:
      gAppState.uiMode = UiMode::BROWSER;
      gAppState.browserCursor = gAppState.currentTrackIdx;
      break;

    case KeyCode::OK:
      if (gAppState.uiMode == UiMode::BROWSER)
      {
        gAppState.currentTrackIdx = gAppState.browserCursor;
        gAppState.uiMode = UiMode::PLAYER;
        g_pendingEvent = AppEvent::SELECT_SONG;
        saveConfig = true;
      }
      else
      {
        gAppState.uiMode = UiMode::BROWSER;
        gAppState.browserCursor = gAppState.currentTrackIdx;
      }
      break;

    case KeyCode::BACK:
      if (gAppState.uiMode == UiMode::BROWSER)
      {
        gAppState.uiMode = UiMode::PLAYER;
      }
      else
      {
        gAppState.isPlaying = false;
        g_pendingEvent = AppEvent::STOP;
      }
      break;

    case KeyCode::UP:
      if (gAppState.uiMode == UiMode::BROWSER)
      {
        gAppState.browserCursor--;
        if (gAppState.browserCursor < 0)
          gAppState.browserCursor = g_totalTracks - 1;
      }
      else
      {
        gAppState.currentTrackIdx--;
        if (gAppState.currentTrackIdx < 0)
          gAppState.currentTrackIdx = g_totalTracks - 1;
        g_pendingEvent = AppEvent::PREV;
        saveConfig = true;
      }
      break;

    case KeyCode::DOWN:
      if (gAppState.uiMode == UiMode::BROWSER)
      {
        gAppState.browserCursor++;
        if (gAppState.browserCursor >= g_totalTracks)
          gAppState.browserCursor = 0;
      }
      else
      {
        gAppState.currentTrackIdx++;
        if (gAppState.currentTrackIdx >= g_totalTracks)
          gAppState.currentTrackIdx = 0;
        g_pendingEvent = AppEvent::NEXT;
        saveConfig = true;
      }
      break;

    case KeyCode::LEFT:
      if (gAppState.uiMode == UiMode::PLAYER)
        g_seekDir = -1;
      break;
    case KeyCode::RIGHT:
      if (gAppState.uiMode == UiMode::PLAYER)
        g_seekDir = 1;
      break;

    case KeyCode::VOL_INC:
      g_isMuted = false;
      gAppState.volume += 2;
      if (gAppState.volume > 100)
        gAppState.volume = 100;
      platformAudioSetVolume(gAppState.volume);
      saveConfig = true;
      lastKeyTime = millis() - 100;
      break;
    case KeyCode::VOL_DEC:
      g_isMuted = false;
      gAppState.volume -= 2;
      if (gAppState.volume < 0)
        gAppState.volume = 0;
      platformAudioSetVolume(gAppState.volume);
      saveConfig = true;
      lastKeyTime = millis() - 100;
      break;

    case KeyCode::MODE_SWITCH:
      if (gAppState.playMode == PlayMode::SEQUENCE)
        gAppState.playMode = PlayMode::REPEAT;
      else if (gAppState.playMode == PlayMode::REPEAT)
        gAppState.playMode = PlayMode::SHUFFLE;
      else
        gAppState.playMode = PlayMode::SEQUENCE;
      saveConfig = true;
      break;

    case KeyCode::MUTE_TOGGLE:
      g_isMuted = !g_isMuted;
      if (g_isMuted)
        platformAudioSetVolume(0);
      else
        platformAudioSetVolume(gAppState.volume);
      break;

    default:
      updateUI = false;
      break;
    }

    if (updateUI)
      uiRender();
    if (saveConfig)
      configSave(&gAppState);
  }
}

void setup()
{
  Serial.begin(115200);
  platformInit();

  uiInit(&gAppState);

  // 赛博开机动画
  uiShowBootAnim();

  configInit();
  AppState loaded;
  configLoad(&loaded);
  gAppState.volume = loaded.volume;
  gAppState.currentTrackIdx = loaded.currentTrackIdx;
  gAppState.inBrowser = false;
  gAppState.playMode = loaded.playMode;
  gAppState.isPlaying = false;

  if (SD.cardType() != CARD_NONE)
  {
    File root = SD.open("/");
    scanDir(root);
    root.close();
    Serial.printf("Loaded %d songs\n", g_totalTracks);

    if (g_totalTracks > 0)
    {
      strncpy(gAppState.currentTitle, getSafeTitle(), 63);
    }
    else
    {
      strncpy(gAppState.currentTitle, "No Files", 63);
    }
  }

  xTaskCreatePinnedToCore(Task_Audio_Loop, "Audio", 65536, NULL, 2, &TaskHandle_Audio, 0);
  platformAudioSetVolume(gAppState.volume);

  uiRender();
}

void loop()
{
  handleInput();

  static uint32_t lastDraw = 0;
  if (millis() - lastDraw > 40)
  {
    if (!gAppState.inBrowser)
    {
      const char *title = getSafeTitle();
      if (strncmp(gAppState.currentTitle, title, 63) != 0)
      {
        strncpy(gAppState.currentTitle, title, 63);
        gAppState.currentTitle[63] = '\0';
      }
    }
    uiRender();
    lastDraw = millis();
  }

  static uint32_t lastSave = 0;
  static int lastSavedIdx = -1;
  if (millis() - lastSave > 5000)
  {
    if (gAppState.currentTrackIdx != lastSavedIdx)
    {
      configSave(&gAppState);
      lastSavedIdx = gAppState.currentTrackIdx;
    }
    lastSave = millis();
  }
}
