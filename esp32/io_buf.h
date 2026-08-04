// io_buf.h

#ifndef _INCLUDE_BUF_H
#define _INCLUDE_BUF_H

#include <stdint.h>

class PktQueue { // FIFO

public:
    PktQueue(int capacity);
    ~PktQueue();
    bool is_empty();
    bool is_full();
    void enqueue(const uint8_t *data, int len);
    void enqueue(const uint8_t *pkt); // -> [N][byte0][byte1]..[byteN]
    uint8_t* dequeue();
    int get_len() { return cnt; };

private:
    uint8_t **queue; // array of pkt pointers
    volatile short cnt, offs;
    short max_cnt;
};


#endif
