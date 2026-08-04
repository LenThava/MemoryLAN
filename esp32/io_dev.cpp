// io_dev.cpp

// (c) 2026-08-01 <christian.tschudin@unibas.ch> - MIT license

#include <Arduino.h>
#include <stdint.h>

#include "io_dev.h"

int IO_device::get_neigh_cnt() // return # of neigh, heard from in last 20 sec
{
    int cnt = 0;
    for (int i = 0; i < IODEV_MAX_NEIGH; i++)
        if ( (neigh_tab[i].id[0] || neigh_tab[i].id[1]) &&
             millis() - neigh_tab[i].last_heard < IODEV_NEIGH_TIMEOUT )
            cnt++;
    return cnt;
}

void IO_device::neigh_add(uint8_t id[2], int neigh_cnt, short rssi)
{
    uint32_t age = 0, pos = 255, empty = 255;
    uint32_t now = millis();
    for (int i = 0; i < IODEV_MAX_NEIGH; i++) {
        if (neigh_tab[i].id[0] == id[0] && neigh_tab[i].id[1] == id[1]) {
            pos = i;
            break;
        }
        if (neigh_tab[i].id[0] == 0 && neigh_tab[i].id[1] == 0) {
            empty = i;
        }
        if (now - neigh_tab[i].last_heard > age) {
            age = now - neigh_tab[i].last_heard;
            pos = i;
        }
    }
    if (pos == 255 && empty != 255)
        pos = empty;
    if (pos != 255) {
        neigh_tab[pos].id[0] = id[0], neigh_tab[pos].id[1] = id[1];
        neigh_tab[pos].last_heard = now;
        neigh_tab[pos].neigh_cnt = neigh_cnt;
        neigh_tab[pos].rssi = rssi;
    }
    return;

}

// eof
