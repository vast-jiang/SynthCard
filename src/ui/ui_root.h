#pragma once
#include "core/state/app_state.h"

// 初始化 UI (分配内存、设置字体)
void uiInit(AppState *app);

// 渲染一帧画面
void uiRender();