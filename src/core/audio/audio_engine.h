#pragma once
#include <vector>
#include <Arduino.h>
#include "../state/app_state.h"

void audioEngineInit();
void audioEngineUpdate(); // 在 FreeRTOS 模式下留空即可

void audioEnginePlay(int trackIndex);
void audioEnginePause();
void audioEngineResume();
void audioEngineStop();

void audioEngineNext();
void audioEnginePrev();
void audioEngineSeek(int offsetSec);

void audioEngineSetMode(PlayMode mode);
void audioEngineSetVolume(int volume);

bool audioEngineIsPlaying();
int audioEngineGetTotalTracks();
const char *audioEngineGetCurrentTitle();
int audioEngineGetCurrentIndex();
const std::vector<String> &audioEngineGetPlaylist();