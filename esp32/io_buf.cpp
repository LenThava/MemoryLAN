// io_buf.cpp

// (c) 2026-08-01 <christian.tschudin@unibas.ch> - MIT license

#include <stdint.h>
#include <string.h>

#include "io_buf.h"

PktQueue::PktQueue(int capacity)
{
    queue = (uint8_t**) malloc(capacity * sizeof(uint8_t*));
    max_cnt = capacity;
    cnt = offs = 0;
}

PktQueue::~PktQueue()
{
    while (cnt > 0) {
        free(queue[offs]);
        offs = (offs + 1) % max_cnt;
        cnt--;
    }
    free(queue);
    queue = NULL;
}

bool PktQueue::is_empty() { return cnt <= 0; }

bool PktQueue::is_full()  { return cnt >= max_cnt; }

void PktQueue::enqueue(const uint8_t* pkt)
{
    if (is_full())
        return;
    int pos = (offs + cnt) % max_cnt;
    queue[pos] = (uint8_t*) pkt;
    cnt++;
}

void PktQueue::enqueue(const uint8_t* data, int len)
{
    if (is_full())
        return;
    uint8_t *pkt = (uint8_t*) malloc(len+1);
    if (pkt) {
        pkt[0] = len;
        memcpy(pkt+1, data, len);
        enqueue(pkt);
    }
}

uint8_t* PktQueue::dequeue()
{
    if (is_empty())
        return NULL;
    uint8_t *pkt = queue[offs];
    queue[offs] = NULL;
    offs = (offs + 1) % max_cnt;
    cnt--;
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
