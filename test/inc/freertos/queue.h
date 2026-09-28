#pragma once
#include "FreeRTOS.h"
#include <cassert>
#include <cstring>
#include <deque>

struct HostQueue { unsigned capacity; std::deque<const void*> items; };
using QueueHandle_t = HostQueue*;
inline QueueHandle_t xQueueCreate(unsigned n, unsigned size)
{
    assert(size == sizeof(void*));
    return new HostQueue{n, {}};
}
inline void vQueueDelete(QueueHandle_t q) { delete q; }
inline unsigned uxQueueMessagesWaiting(QueueHandle_t q) { return q->items.size(); }
inline unsigned uxQueueSpacesAvailable(QueueHandle_t q) { return q->capacity - q->items.size(); }
inline int xQueueSend(QueueHandle_t q, const void *value, unsigned)
{
    if (!uxQueueSpacesAvailable(q)) return pdFALSE;
    const void *ptr;
    memcpy(&ptr, value, sizeof(ptr));
    q->items.push_back(ptr);
    return pdTRUE;
}
inline int xQueueReceive(QueueHandle_t q, void *value, unsigned)
{
    if (q->items.empty()) return pdFALSE;
    const void *ptr = q->items.front();
    memcpy(value, &ptr, sizeof(ptr));
    q->items.pop_front();
    return pdTRUE;
}
