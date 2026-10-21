#include <Arduino.h>
#include <cassert>
#include <vector>

unsigned long test_millis = 1000;
uint32_t test_random = 0;
SerialStub Serial;
void set_led(bool) {}

#include "memoryLAN.cpp"
#include "trial_data.h"

void IO_device::brcast_send(const uint8_t*, char*) {}
uint8_t* IO_device::brcast_recv() { return nullptr; }

struct Device : IO_device {
    Device()
    {
        name = const_cast<char*>("host");
        rx_cnt = 0;
        memset(neigh_tab, 0, sizeof(neigh_tab));
    }
    void brcast_send(const uint8_t*, char*) override {}
    uint8_t* brcast_recv() override { return nullptr; }
};

static uint8_t local_id[6] = {1, 2, 3, 4, 5, 6};

struct Fixture {
    MemoryLAN lan;
    Device radio;
    TrialRecord run = {};

    Fixture() : lan(local_id)
    {
        test_millis = 1000;
        test_random = 0;
        run.run = 1;
        run.source = 0x0201;
        run.total = 20;
        lan.attach_if(&radio);
        radio.next_beacon = UINT64_MAX;
    }

    void add(unsigned seq)
    {
        uint8_t pkt[TRIAL_BYTES + 1];
        trial_page(pkt, run, seq);
        assert(lan.add(pkt));
    }

    std::vector<unsigned> resident() const
    {
        std::vector<unsigned> ids;
        for (int i = 0; i < lan.cache_cnt; i++) {
            const uint8_t* p = lan.cache + i * MEMLAN_CLSZ;
            assert(p[0] == TRIAL_BYTES && !memcmp(p + 1, "MLT1", 4));
            ids.push_back(unsigned(p[11]) | (unsigned(p[12]) << 8));
        }
        return ids;
    }
};

static bool has_zero(const std::vector<unsigned>& ids)
{
    for (unsigned id : ids) if (id == 0) return true;
    return false;
}

static void row(const char* name, unsigned unique, unsigned duplicates,
                unsigned long idle_ms, const std::vector<unsigned>& ids,
                bool comma)
{
    printf("%s{\"scenario\":\"%s\",\"cache_capacity\":%u,"
           "\"unique_injected\":%u,\"duplicate_injections\":%u,"
           "\"idle_ms\":%lu,\"resident_count\":%zu,"
           "\"page0_resident\":%s,\"resident_ids\":[",
           comma ? ",\n" : "", name, unsigned(MEMLAN_CACHE_DEPTH), unique,
           duplicates, idle_ms, ids.size(), has_zero(ids) ? "true" : "false");
    for (size_t i = 0; i < ids.size(); i++) printf("%s%u", i ? "," : "", ids[i]);
    printf("]}");
}

int main()
{
    static_assert(MEMLAN_CACHE_DEPTH == 10, "Update experiment expectations for another cache size");
    printf("[\n");
    bool comma = false;
    for (unsigned count : {5u, 10u, 20u}) {
        Fixture f;
        for (unsigned i = 0; i < count; i++) f.add(i);
        const auto ids = f.resident();
        assert(ids.size() == (count < 10 ? count : 10));
        assert(ids.front() == (count > 10 ? count - 10 : 0));
        char name[24];
        snprintf(name, sizeof(name), "cold_%u", count);
        row(name, count, 0, 0, ids, comma);
        comma = true;
    }
    {
        Fixture f;
        for (unsigned i = 0; i < 10; i++) f.add(i);
        for (unsigned i = 10; i < 20; i++) {
            f.add(0);
            f.add(i);
        }
        const auto ids = f.resident();
        assert(ids.size() == 10 && has_zero(ids));
        row("refreshed_20", 20, 10, 0, ids, true);
    }
    {
        Fixture f;
        for (unsigned i = 0; i < 5; i++) f.add(i);
        const auto before = f.resident();
        test_millis += 3600000;
        assert(!f.lan.loop());
        assert(before == f.resident());
        row("idle_5", 5, 0, 3600000, f.resident(), true);
    }
    {
        Fixture before;
        for (unsigned i = 0; i < 5; i++) before.add(i);
        assert(before.resident().size() == 5);
        Fixture after;
        assert(after.resident().empty());
        row("reset_5", 5, 0, 0, after.resident(), true);
    }
    printf("\n]\n");
}
