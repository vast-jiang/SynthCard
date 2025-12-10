#include "core/event/event_bus.h"
#include "log.h"
#include <vector>

#define MAX_QUEUE_SIZE 32
#define MAX_HANDLERS 16

struct Subscription
{
    EventType type;
    EventHandler handler;
};

static std::vector<Subscription> g_subs;
static Event g_queue[MAX_QUEUE_SIZE];
static int g_head = 0;
static int g_tail = 0;

void eventBusInit()
{
    g_subs.reserve(MAX_HANDLERS);
    g_head = g_tail = 0;
    LOG_CORE("EventBus initialized");
}

bool eventBusSubscribe(EventType type, EventHandler handler)
{
    if (g_subs.size() >= MAX_HANDLERS)
        return false;
    g_subs.push_back({type, handler});
    return true;
}

bool eventBusPublish(const Event &ev)
{
    int next = (g_head + 1) % MAX_QUEUE_SIZE;
    if (next == g_tail)
    {
        LOG_CORE("EventBus queue full!");
        return false;
    }
    g_queue[g_head] = ev;
    g_head = next;
    return true;
}

void eventBusPoll()
{
    while (g_head != g_tail)
    {
        Event &ev = g_queue[g_tail];
        for (const auto &sub : g_subs)
        {
            if (sub.type == ev.type)
            {
                sub.handler(ev);
            }
        }
        g_tail = (g_tail + 1) % MAX_QUEUE_SIZE;
    }
}