#pragma once
#include <stddef.h>
#include <stdint.h>

inline bool valid_ble_frame(const uint8_t *buf, size_t len)
{
    return buf && len >= 9 && len <= 130 &&
           buf[0] == len - 1 && buf[1] == 0x31;
}
