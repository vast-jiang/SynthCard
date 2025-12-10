#pragma once
#include "event_types.h"

typedef void (*EventHandler)(const Event &);

void eventBusInit();
bool eventBusSubscribe(EventType type, EventHandler handler);
bool eventBusPublish(const Event &ev);
void eventBusPoll();