#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

struct BleMeta {
    uint8_t sender[6];
    uint32_t run, sequence, received_ms;
};

const size_t BLE_META_BYTES = 18;

inline bool ble_run_matches(const BleMeta &meta, uint32_t local_run)
{
    return !local_run || meta.run == local_run;
}

inline uint32_t ble_read_u32(const uint8_t *p)
{
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
           (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

inline void ble_write_u32(uint8_t *p, uint32_t value)
{
    for (unsigned i = 0; i < 4; i++) p[i] = value >> (8 * i);
}

inline void ble_store_meta(uint8_t *p, const BleMeta &meta)
{
    memcpy(p, meta.sender, 6);
    ble_write_u32(p + 6, meta.run);
    ble_write_u32(p + 10, meta.sequence);
    ble_write_u32(p + 14, meta.received_ms);
}

inline BleMeta ble_load_meta(const uint8_t *p)
{
    BleMeta meta;
    memcpy(meta.sender, p, 6);
    meta.run = ble_read_u32(p + 6);
    meta.sequence = ble_read_u32(p + 10);
    meta.received_ms = ble_read_u32(p + 14);
    return meta;
}
