#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

enum TrialState { EMPTY, READY, RUNNING, DONE, INTERRUPTED };
const unsigned TRIAL_MAX = 256;
const unsigned TRIAL_BYTES = 32;

inline bool trial_parse(const char *line, uint32_t fields[5])
{
    if (strncmp(line, "prep ", 5)) return false;
    const char *p = line + 5;
    for (unsigned i = 0; i < 5; i++) {
        while (*p == ' ') p++;
        if (!(*p >= '0' && *p <= '9') &&
            !(i == 1 && ((*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F'))))
            return false;
        char *end;
        errno = 0;
        unsigned long value = strtoul(p, &end, i == 1 ? 16 : 10);
        if (errno || value > UINT32_MAX || end == p || (*end && *end != ' ')) return false;
        fields[i] = value;
        p = end;
    }
    while (*p == ' ') p++;
    return !*p;
}

struct TrialPeer {
    uint16_t id, samples;
    int16_t rssi;
    uint16_t reserved;
    uint32_t last_ms;
};

struct TrialRecord {
    uint32_t version, run;
    uint16_t node, source, total, state;
    uint32_t gap_ms, limit_ms, elapsed_ms, sent, duplicates, invalid;
    uint32_t rx, tx, rx_drop, rx_bad, tx_fail, stop_reason;
    uint8_t seen[TRIAL_MAX / 8];
    TrialPeer peers[16];
    char build[64];
    uint32_t checksum;
};

inline uint32_t trial_checksum(const TrialRecord &r)
{
    uint32_t h = 2166136261u;
    const uint8_t *p = (const uint8_t*)&r;
    for (size_t i = 0; i < offsetof(TrialRecord, checksum); i++)
        h = (h ^ p[i]) * 16777619u;
    return h;
}

inline bool trial_valid(const TrialRecord &r)
{
    return r.version == 1 && r.run && r.total >= 1 && r.total <= TRIAL_MAX &&
        r.state >= READY && r.state <= INTERRUPTED && r.sent <= r.total &&
        r.gap_ms >= 100 && r.gap_ms <= 60000 &&
        r.limit_ms >= 1000 && r.limit_ms <= 3600000 &&
        memchr(r.build, 0, sizeof(r.build)) && r.checksum == trial_checksum(r);
}

inline bool trial_seen(const TrialRecord &r, unsigned seq)
{
    return seq < r.total && (r.seen[seq / 8] & (1u << (seq % 8)));
}

inline void trial_page(uint8_t *pkt, const TrialRecord &r, uint16_t seq)
{
    pkt[0] = TRIAL_BYTES;
    memcpy(pkt + 1, "MLT1", 4);
    for (unsigned i = 0; i < 4; i++) pkt[5+i] = r.run >> (8*i);
    pkt[9] = r.source; pkt[10] = r.source >> 8;
    pkt[11] = seq; pkt[12] = seq >> 8;
    pkt[13] = r.total; pkt[14] = r.total >> 8;
    for (unsigned i = 15; i <= TRIAL_BYTES; i++) pkt[i] = (uint8_t)(seq + i);
}

// Return 1 for a new page, 0 for a duplicate/another run, -1 for bad test data.
inline int trial_receive(TrialRecord &r, const uint8_t *pkt)
{
    if (r.state != RUNNING || !pkt || pkt[0] < 4 || memcmp(pkt+1, "MLT1", 4)) return 0;
    if (pkt[0] != TRIAL_BYTES) { r.invalid++; return -1; }
    uint32_t run = 0;
    for (unsigned i = 0; i < 4; i++) run |= uint32_t(pkt[5+i]) << (8*i);
    uint16_t source = pkt[9] | (uint16_t(pkt[10]) << 8);
    if (run != r.run || source != r.source) return 0;
    uint16_t seq = pkt[11] | (uint16_t(pkt[12]) << 8);
    uint8_t expected[TRIAL_BYTES + 1];
    trial_page(expected, r, seq);
    if (seq >= r.total || memcmp(pkt, expected, sizeof(expected))) {
        r.invalid++; return -1;
    }
    if (trial_seen(r, seq)) { r.duplicates++; return 0; }
    r.seen[seq / 8] |= 1u << (seq % 8);
    return 1;
}
