#pragma once
#include "core/state/app_state.h"

void configInit();
void configLoad(AppState *app);
void configSave(const AppState *app);