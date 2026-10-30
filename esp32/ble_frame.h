#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "ble_meta.h"

const size_t BLE_FRAME_HEADER = 19;
const size_t BLE_CONTENT_MAX = 128;
const size_t BLE_FRAME_MAX = BLE_FRAME_HEADER + BLE_CONTENT_MAX;

inline bool valid_ble_frame(const uint8_t *buf, size_t len)
{
    return buf && len >= 9 && len <= 130 &&
           buf[0] == len - 1 && buf[1] == 0x31;
}

// Versioned envelope: AD length/type, ML, version, sender MAC, run, sequence.
inline size_t ble_encode_frame(uint8_t *buf, size_t capacity,
                               const uint8_t *pkt, const BleMeta &meta)
{
    if (!buf || !pkt || *pkt < 7 || *pkt > BLE_CONTENT_MAX ||
        capacity < BLE_FRAME_HEADER + *pkt) return 0;
    size_t len = BLE_FRAME_HEADER + *pkt;
    buf[0] = len - 1;
    buf[1] = 0x31;
    buf[2] = 'M'; buf[3] = 'L'; buf[4] = 1;
    memcpy(buf + 5, meta.sender, 6);
    ble_write_u32(buf + 11, meta.run);
    ble_write_u32(buf + 15, meta.sequence);
    memcpy(buf + BLE_FRAME_HEADER, pkt + 1, *pkt);
    return len;
}

inline bool ble_decode_frame(const uint8_t *buf, size_t len, BleMeta &meta,
                              const uint8_t *&content, size_t &content_len)
{
    content = nullptr; content_len = 0;
    if (!buf || len < BLE_FRAME_HEADER + 7 || len > BLE_FRAME_MAX ||
        buf[0] != len - 1 || buf[1] != 0x31 ||
        buf[2] != 'M' || buf[3] != 'L' || buf[4] != 1) return false;
    memcpy(meta.sender, buf + 5, 6);
    meta.run = ble_read_u32(buf + 11);
    meta.sequence = ble_read_u32(buf + 15);
    meta.received_ms = 0;
    content = buf + BLE_FRAME_HEADER;
    content_len = len - BLE_FRAME_HEADER;
    return true;
}
