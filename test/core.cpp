#include <Arduino.h>
#include <cassert>
#include <set>
#include <vector>

unsigned long test_millis = 1000;
uint32_t test_random = 0;
SerialStub Serial;
void set_led(bool) {}

static std::set<void*> live;
static void* allocate(size_t n)
{
    void* p = std::malloc(n);
    assert(p && live.insert(p).second);
    return p;
}
static void release(void* p)
{
    if (!p) return;
    assert(live.erase(p) == 1);
    std::free(p);
}

#define malloc allocate
#define free release
#include "memoryLAN.cpp"
#undef free
#undef malloc

using Packet = std::vector<uint8_t>;
static std::vector<Packet> updates;
static void collect(uint8_t* pkt)
{
    updates.emplace_back(pkt, pkt + *pkt + 1);
    release(pkt);
}

void IO_device::brcast_send(const uint8_t*, char*) {}
uint8_t* IO_device::brcast_recv() { return nullptr; }

struct Device : IO_device {
    uint8_t* incoming = nullptr;
    std::vector<Packet> sent;
    Device()
    {
        name = const_cast<char*>("test");
        rx_cnt = tx_cnt = 0;
        memset(neigh_tab, 0, sizeof(neigh_tab));
    }
    void brcast_send(const uint8_t* pkt, char*) override
    {
        sent.emplace_back(pkt, pkt + *pkt + 1);
    }
    uint8_t* brcast_recv() override
    {
        uint8_t* pkt = incoming;
        incoming = nullptr;
        return pkt;
    }
};

static uint8_t local_id[6] = {1, 2, 3, 4, 5, 6};
struct Fixture {
    MemoryLAN lan;
    Device radio;
    Fixture() : lan(local_id, collect)
    {
        assert(live.empty());
        updates.clear();
        test_millis = 1000;
        test_random = 0;
        lan.attach_if(&radio);
        radio.next_beacon = UINT64_MAX;
    }
    void receive(const Packet& pkt)
    {
        assert(pkt.size() == size_t(pkt[0]) + 1);
        radio.incoming = static_cast<uint8_t*>(allocate(pkt.size() + 1));
        memcpy(radio.incoming, pkt.data(), pkt.size());
        radio.incoming[pkt.size()] = uint8_t(-50);
        assert(lan.loop());
        assert(live.empty());
    }
};

static Packet page(uint8_t seq = 1, uint8_t len = 7)
{
    Packet pkt(size_t(len) + 1, 0x55);
    pkt[0] = len;
    if (len) pkt[1] = seq;
    return pkt;
}
static Packet beacon(uint8_t len = 48)
{
    Packet pkt(size_t(len) + 1, 0);
    pkt[0] = len;
    if (len >= 7) memset(pkt.data() + 1, 0xa5, 7);
    if (len >= 10) { pkt[8] = 3; pkt[9] = 4; pkt[10] = 1; }
    if (len >= 12) { pkt[11] = 144; pkt[12] = 5; }
    return pkt;
}

static void bloom()
{
    uint8_t vect[18];
    uint8_t item[] = "PAYLOAD";
    bf_init(vect, 144);
    assert(bf_absent(vect, 144, 5, 0, item, 7));
    int p = murmur3_32(item, 7, 0) % 144;
    vect[p / 8] |= 1 << (p % 8);
    assert(bf_absent(vect, 144, 5, 0, item, 7));
    bf_add(vect, 144, 5, 0, item, 7);
    assert(!bf_absent(vect, 144, 5, 0, item, 7));
    memset(vect, 0xff, sizeof(vect));
    assert(!bf_absent(vect, 144, 5, 0, item, 7));
}

static void receive_and_repair()
{
    Fixture f;
    f.receive(beacon());
    assert(f.radio.sent.empty() && f.lan.cache_cnt == 0);
    assert(f.radio.get_neigh_cnt() == 1);
    f.receive(page());
    f.receive(page());
    assert(f.lan.cache_cnt == 1 && f.lan.hash_cnt == 1);
    assert(updates.size() == 2 && f.radio.sent.size() == 1);
    f.receive(beacon());
    assert(f.radio.sent.size() == 2 && f.radio.sent.back() == page());
    assert(f.lan.cache_cnt == 1);
    Packet full = beacon();
    memset(full.data() + 15, 0xff, 18);
    f.receive(full);
    assert(f.radio.sent.size() == 2);
}

static void suppression()
{
    Fixture f;
    assert(f.lan.add(page().data()));
    uint8_t id[] = {5, 6};
    f.radio.neigh_add(id, 1, -60);
    test_random = 1;
    f.receive(beacon());
    assert(f.radio.get_neigh_cnt() == 2 && f.radio.sent.size() == 1);
    test_random = 0;
    f.receive(beacon());
    assert(f.radio.sent.size() == 2);
    test_millis += IODEV_NEIGH_TIMEOUT;
    test_random = 1;
    f.receive(beacon());
    assert(f.radio.get_neigh_cnt() == 1 && f.radio.sent.size() == 3);
}

static void invalid_packets()
{
    Fixture f;
    for (uint8_t n : {0, 1, 6, 121, 255}) {
        Packet pkt = page(1, n);
        assert(!f.lan.add(pkt.data()));
        f.receive(pkt);
    }
    assert(!f.lan.add(nullptr));
    for (uint8_t n : {7, 10, 47, 48, 49}) {
        Packet pkt = beacon(n);
        assert(!f.lan.add(pkt.data()));
        if (n == 48) pkt[11] = 143;
        f.receive(pkt);
    }
    Packet pkt = beacon();
    pkt[12] = 0;
    f.receive(pkt);
    assert(f.lan.cache_cnt == 0 && updates.empty());
    assert(f.radio.sent.empty() && f.radio.get_neigh_cnt() == 0);
}

static void local_content()
{
    Fixture f;
    const Packet pkt = page(10, 120);
    assert(f.lan.add(pkt.data()));
    assert(f.lan.add(pkt.data()));
    assert(f.radio.sent.size() == 1 && f.radio.sent[0] == pkt);
    assert(updates.size() == 2 && updates[0] == pkt);
    assert(f.lan.cache_cnt == 1 && f.lan.hash_cnt == 1 && live.empty());
    for (int i = 0; i < MEMLAN_CACHE_DEPTH + 1; i++)
        assert(f.lan.add(page(uint8_t(i)).data()));
    assert(f.lan.cache_cnt == MEMLAN_CACHE_DEPTH);
    assert(f.lan.cache[1] == 1);
    assert(f.lan.add(page(1).data()));
    assert(f.lan.cache[1] == 2);
    assert(f.lan.cache[(MEMLAN_CACHE_DEPTH - 1) * MEMLAN_CLSZ + 1] == 1);
    size_t calls = updates.size();
    f.lan.set_update_cb(nullptr);
    assert(f.lan.add(page(99).data()));
    assert(updates.size() == calls && live.empty());
}

static void emitted_beacon()
{
    Fixture f;
    f.radio.next_beacon = 0;
    assert(f.lan.loop() && f.radio.sent.size() == 1);
    const Packet& pkt = f.radio.sent[0];
    assert(pkt.size() == 49 && pkt[0] == 48);
    assert(pkt[11] == 144 && pkt[12] == 5);
    for (size_t i = 33; i < pkt.size(); i++) assert(pkt[i] == 0);
}

int main()
{
    bloom();
    receive_and_repair();
    suppression();
    invalid_packets();
    local_content();
    emitted_beacon();
    assert(live.empty());
    assert(MLAN_DEBUG ? Serial.writes > 0 : Serial.writes == 0);
    puts("core: all tests passed");
}
