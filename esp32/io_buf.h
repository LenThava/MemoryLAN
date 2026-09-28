// io_buf.h

#ifndef _INCLUDE_BUF_H
#define _INCLUDE_BUF_H

#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

class PktQueue { // FIFO

public:
    PktQueue(int capacity);
    ~PktQueue();
    bool is_empty();
    bool is_full();
    bool enqueue(const uint8_t *data, int len);
    bool enqueue(const uint8_t *pkt); // ownership transfers only on success
    uint8_t* dequeue();
    int get_len();

private:
    QueueHandle_t queue;
};


#endif
