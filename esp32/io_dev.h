// io_dev.h

#pragma once

#include <stdint.h>

// #include "io_buf.h"

#define IODEV_MAX_NEIGH         16
#define IODEV_NEIGH_TIMEOUT  20000 // msec

struct neigh_s {
    uint32_t last_heard;
    uint8_t id[2];
    uint8_t neigh_cnt;
    short rssi;
};

// ---------------------------------------------------------------------------

class IO_device {
public:
    virtual void     brcast_send(const uint8_t *pkt, char *comment);
    // pkt points to [N][byte1][byte2]..[byteN]
    virtual uint8_t* brcast_recv();
    // NULL, or ptr to buf as above, to be freed by consumer
    int rx_cnt, tx_cnt;
    char *name;
    uint64_t next_beacon;

    int get_neigh_cnt();
    struct neigh_s neigh_tab[IODEV_MAX_NEIGH];
    void neigh_add(uint8_t id[2], int neigh_cnt, short rssi);
};

// eof
