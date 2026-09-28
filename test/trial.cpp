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

static void all_sequences()
{
    TrialRecord r = record();
    uint8_t pkt[TRIAL_BYTES + 1], previous[TRIAL_BYTES + 1] = {};
    for (unsigned i = 0; i < 256; i++) {
        trial_page(pkt, r, i);
        assert(memcmp(previous, pkt, sizeof(pkt)) != 0);
        assert(pkt[0] == TRIAL_BYTES);
        assert(trial_receive(r, pkt) == 1 && trial_seen(r, i));
        assert(trial_receive(r, pkt) == 0);
        memcpy(previous, pkt, sizeof(pkt));
    }
    for (auto byte : r.seen) assert(byte == 255);
    assert(r.duplicates == 256 && r.invalid == 0);
    assert(!trial_seen(r, 256));
    trial_page(pkt, r, 256);
    assert(trial_receive(r, pkt) == -1 && r.invalid == 1);
    r.checksum = trial_checksum(r);
    assert(trial_valid(r));
}

static void filtering_and_corruption()
{
    TrialRecord r = record(3), other = r;
    uint8_t pkt[TRIAL_BYTES + 1];
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
    for (unsigned i = 11; i <= TRIAL_BYTES; i++) {
        trial_page(pkt, r, 0); pkt[i] ^= 0x80;
        assert(trial_receive(r, pkt) == -1);
        bad++;
    }
    assert(r.invalid == bad && !trial_seen(r, 0));
    trial_page(pkt, r, 2);
    assert(trial_receive(r, pkt) == 1 && trial_seen(r, 2));
    for (unsigned state : {EMPTY, READY, DONE, INTERRUPTED}) {
        r.state = state;
        TrialRecord before = r;
        assert(trial_receive(r, pkt) == 0);
        assert(memcmp(&before, &r, sizeof(r)) == 0);
    }
}

int main()
{
    uint32_t fields[5];
    assert(trial_parse("prep 1 abcd 100 1000 600", fields) && fields[1] == 0xabcd);
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
    puts("trial data: all tests passed");
}
