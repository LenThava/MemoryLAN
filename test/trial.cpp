#include <cassert>
#include <cstdio>
#include <initializer_list>
#include "trial_data.h"

static TrialRecord record(unsigned total = 256)
{
    TrialRecord r = {};
    r.version = 1; r.run = 0x12345678;
    r.node = 0x5678; r.source = 0x1234;
    r.total = total; r.state = RUNNING;
    r.gap_ms = 1000; r.limit_ms = 300000;
    strcpy(r.build, "test");
    r.checksum = trial_checksum(r);
    return r;
}

static void validity()
{
    TrialRecord r = record();
    assert(trial_valid(r));
    r.total = 0; r.checksum = trial_checksum(r);
    assert(!trial_valid(r));
    r = record(); r.total = 257; r.checksum = trial_checksum(r);
    assert(!trial_valid(r));
    r = record(); r.version++; r.checksum = trial_checksum(r);
    assert(!trial_valid(r));
    r = record(); r.run = 0; r.checksum = trial_checksum(r);
    assert(!trial_valid(r));
    for (unsigned state : {0u, 5u, 65535u}) {
        r = record(); r.state = state; r.checksum = trial_checksum(r);
        assert(!trial_valid(r));
    }
    for (unsigned gap : {0u, 99u, 60001u}) {
        r = record(); r.gap_ms = gap; r.checksum = trial_checksum(r);
        assert(!trial_valid(r));
    }
    for (unsigned duration : {0u, 999u, 3600001u}) {
        r = record(); r.limit_ms = duration; r.checksum = trial_checksum(r);
        assert(!trial_valid(r));
    }
    r = record(); r.sent = 257; r.checksum = trial_checksum(r);
    assert(!trial_valid(r));
    r = record(); memset(r.build, 'x', sizeof(r.build)); r.checksum = trial_checksum(r);
    assert(!trial_valid(r));
    r = record(); r.seen[17] ^= 8;
    assert(!trial_valid(r));
}

static void all_sequences(unsigned bytes = 32, unsigned version = 1)
{
    TrialRecord r = record();
    r.version = version;
    r.payload_bytes = version == 1 ? 0 : bytes;
    uint8_t pkt[TRIAL_MAX_BYTES + 1], previous[TRIAL_MAX_BYTES + 1] = {};
    for (unsigned i = 0; i < 256; i++) {
        trial_page(pkt, r, i);
        assert(memcmp(previous, pkt, bytes + 1) != 0);
        assert(pkt[0] == bytes);
        assert(trial_receive(r, pkt) == 1 && trial_seen(r, i));
        assert(trial_receive(r, pkt) == 0);
        memcpy(previous, pkt, bytes + 1);
    }
    for (auto byte : r.seen) assert(byte == 255);
    assert(r.duplicates == 256 && r.invalid == 0);
    assert(!trial_seen(r, 256));
    trial_page(pkt, r, 256);
    assert(trial_receive(r, pkt) == -1 && r.invalid == 1);
    r.checksum = trial_checksum(r);
    assert(trial_valid(r));
}

static void filtering_and_corruption(unsigned bytes = 32, unsigned version = 1)
{
    TrialRecord r = record(3);
    r.version = version;
    r.payload_bytes = version == 1 ? 0 : bytes;
    TrialRecord other = r;
    uint8_t pkt[TRIAL_MAX_BYTES + 1];
    other.run++;
    trial_page(pkt, other, 0);
    assert(trial_receive(r, pkt) == 0 && !trial_seen(r, 0));
    other = r; other.source++;
    trial_page(pkt, other, 0);
    assert(trial_receive(r, pkt) == 0 && r.invalid == 0);
    assert(trial_receive(r, nullptr) == 0);
    trial_page(pkt, r, 0); pkt[1] = 'X';
    assert(trial_receive(r, pkt) == 0);
    trial_page(pkt, r, 0); pkt[0] = 3;
    assert(trial_receive(r, pkt) == 0);
    trial_page(pkt, r, 0); pkt[0]--;
    assert(trial_receive(r, pkt) == -1);
    unsigned bad = 1;
    for (unsigned i = 11; i <= bytes; i++) {
        trial_page(pkt, r, 0); pkt[i] ^= 0x80;
        assert(trial_receive(r, pkt) == -1);
        bad++;
    }
    assert(r.invalid == bad && !trial_seen(r, 0));
    const uint8_t short_page[] = {4, 'M', 'L', 'T', '1'};
    assert(trial_receive(r, short_page) == -1 && r.invalid == ++bad);
    uint8_t truncated[15] = {14, 'M', 'L', 'T', '1'};
    assert(trial_receive(r, truncated) == -1 && r.invalid == ++bad);
    other = r; other.version = 2; other.payload_bytes = bytes == 64 ? 32 : 64;
    trial_page(pkt, other, 0);
    assert(trial_receive(r, pkt) == -1 && r.invalid == ++bad);
    trial_page(pkt, r, 2);
    assert(trial_receive(r, pkt) == 1 && trial_seen(r, 2));
    for (unsigned state : {EMPTY, READY, DONE, INTERRUPTED}) {
        r.state = state;
        TrialRecord before = r;
        assert(trial_receive(r, pkt) == 0);
        assert(memcmp(&before, &r, sizeof(r)) == 0);
    }
}

static void payload_versions()
{
    assert(sizeof(TrialRecord) == 356 && offsetof(TrialRecord, run) == 4);
    assert(offsetof(TrialRecord, checksum) == 352);
    TrialRecord r = record();
    assert(trial_bytes(r) == 32);
    r.payload_bytes = 32; r.checksum = trial_checksum(r);
    assert(!trial_valid(r));
    r.version = 2;
    for (unsigned size : {32u, 64u, 120u}) {
        r.payload_bytes = size; r.checksum = trial_checksum(r);
        assert(trial_valid(r) && trial_bytes(r) == size);
        all_sequences(size, 2);
        filtering_and_corruption(size, 2);
    }
    for (unsigned size : {0u, 14u, 31u, 33u, 63u, 119u, 121u, 128u, 65535u}) {
        r.payload_bytes = size; r.checksum = trial_checksum(r);
        assert(!trial_valid(r));
    }
    r.version = 3; r.payload_bytes = 32; r.checksum = trial_checksum(r);
    assert(trial_valid(r));
    r.version = 4; r.checksum = trial_checksum(r);
    assert(!trial_valid(r));
}

int main()
{
    uint32_t fields[7];
    assert(trial_parse("prep 1 abcd 100 1000 600", fields) && fields[1] == 0xabcd && fields[5] == 32);
    for (const char *line : {"prep 1 abcd 100 1000 600 64", "prep 1 abcd 100 1000 600 64  "})
        assert(trial_parse(line, fields) && fields[5] == 64);
    assert(trial_parse("prep 1 abcd 100 1000 600 120", fields) && fields[5] == 120);
    assert(!trial_parse("prep 1 abcd 100 1000 600 -32", fields));
    assert(!trial_parse("prep 1 abcd 100 1000 600 4294967296", fields));
    assert(trial_parse("prep 1 abcd 5 1000 600 64 5000", fields) && fields[6] == 5000);
    assert(trial_parse("prep 1 abcd 5 1000 600 64 0", fields) && fields[6] == 0);
    assert(!trial_parse("prep 1 abcd 5 1000 600 64 -1", fields));
    assert(!trial_parse("prep 1 abcd 5 1000 600 64 4294967296", fields));
    assert(!trial_parse("prep 1 abcd 5 1000 600 64 5000 extra", fields));
    assert(!trial_parse("prep 1 abcd 100 1000 600 64 extra", fields));
    assert(!trial_parse("prep -1 abcd 100 1000 600", fields));
    assert(!trial_parse("prep \t-1 abcd 100 1000 600", fields));
    assert(!trial_parse("prep 1 abcd \t+100 1000 600", fields));
    assert(!trial_parse("prep 4294967296 abcd 100 1000 600", fields));
    assert(!trial_parse("prep 99999999999999999999999999 abcd 100 1000 600", fields));
    assert(!trial_parse("prep 1 abcd 100 1000 600 extra", fields));
    assert(!trial_parse("prep 1 abcd 100 1000", fields));
    validity();
    all_sequences();
    filtering_and_corruption();
    payload_versions();
    puts("trial data: all tests passed");
}
