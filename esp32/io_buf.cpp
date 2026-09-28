// io_buf.cpp

// (c) 2026-08-01 <christian.tschudin@unibas.ch> - MIT license

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "io_buf.h"

PktQueue::PktQueue(int capacity)
{
    queue = capacity > 0 ? xQueueCreate(capacity, sizeof(uint8_t*)) : NULL;
}

PktQueue::~PktQueue()
{
    while (uint8_t *pkt = dequeue()) free(pkt);
    if (queue) vQueueDelete(queue);
}

bool PktQueue::is_empty() { return get_len() == 0; }

bool PktQueue::is_full() { return !queue || uxQueueSpacesAvailable(queue) == 0; }
int PktQueue::get_len() { return queue ? uxQueueMessagesWaiting(queue) : 0; }

bool PktQueue::enqueue(const uint8_t* pkt)
{
    return pkt && queue && xQueueSend(queue, &pkt, 0) == pdTRUE;
}

bool PktQueue::enqueue(const uint8_t* data, int len)
{
    if (!data || len < 1 || len > 255) return false;
    uint8_t *pkt = (uint8_t*) malloc(len+1);
    if (!pkt) return false;
    pkt[0] = len;
    memcpy(pkt+1, data, len);
    if (enqueue(pkt)) return true;
    free(pkt);
    return false;
}

uint8_t* PktQueue::dequeue()
{
    uint8_t *pkt = NULL;
    if (queue) xQueueReceive(queue, &pkt, 0);
    return pkt;
}

/*
RingBuffer::RingBuffer(short capacity, short len)
{
  buf = (uint8_t*) malloc(capacity * (len+1));
  max_cnt = capacity;
  item_len = len;
  cnt = offs = 0;
}

bool RingBuffer::is_empty() { return cnt <= 0; }

bool RingBuffer::is_full()  { return cnt >= max_cnt; }

void RingBuffer::add(const uint8_t* pkt, short len)
{
  if (is_full())
    return;
  if (len > item_len)
    len = item_len;
  uint8_t *cp = buf + offs * (item_len+1);
  *cp = len;
  memcpy(cp+1, pkt, len);
  offs = (offs + 1) % max_cnt;
  cnt++;
}

unsigned char* RingBuffer::out()
{
  if (is_empty())
    return NULL;
  unsigned char *cp = (unsigned char*) buf +
                 ((offs + max_cnt - cnt) % max_cnt) * (item_len+1);
  cnt--;
  return cp;
}
*/

// eof
