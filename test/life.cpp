#include <atomic>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <string>
#include <vector>
#include "trial_data.h"

#define MLAN_REV host_test
#define COMPILE_DATE "host test"
#define IODEV_NEIGH_TIMEOUT 20000
#define WAVESHARE_S3ZERO
#define RGB_BUILTIN 21
static unsigned led_r, led_g, led_b;
static void rgbLedWrite(unsigned, unsigned r, unsigned g, unsigned b)
{ led_r = r; led_g = g; led_b = b; }
static uint32_t now_ms = 1000;
static uint32_t millis() { return now_ms; }
static uint8_t my_id[8] = {0x12, 0x34};

struct TestSerial {
    std::string output, input;
    explicit operator bool() const { return true; }
    template<class... A> void printf(const char* fmt, A... a)
    {
        char buf[2048];
        int n = snprintf(buf, sizeof(buf), fmt, a...);
        assert(n >= 0 && size_t(n) < sizeof(buf));
        output += buf;
    }
    void print(const char* s) { output += s; }
    void println(const char* s) { output += s; output += '\n'; }
    int available() const { return !input.empty(); }
    char read() { char c = input[0]; input.erase(0, 1); return c; }
} Serial;

struct TestQueue {
    std::deque<uint8_t*> packets;
    uint8_t* dequeue()
    {
        if (packets.empty()) return nullptr;
        auto* p = packets.front(); packets.pop_front(); return p;
    }
};
struct TestBle {
    struct Peer { uint8_t id[2]; uint32_t last_heard; int16_t rssi; } neigh_tab[16] = {};
    uint32_t rx_cnt = 0;
    std::atomic<int> tx_cnt{0};
    std::atomic<uint32_t> rx_drop{0}, rx_bad{0}, tx_fail{0};
    TestQueue queue;
    TestQueue* from_brcast = &queue;
} radio;
static TestBle* ble_if = &radio;

struct TestLAN {
    unsigned cache_cnt = 0, hash_cnt = 0;
    void (*callback)(uint8_t*) = nullptr;
    std::vector<std::vector<uint8_t>> sent;
    bool accept = true;
    void set_update_cb(void (*cb)(uint8_t*)) { callback = cb; }
    bool add(const uint8_t* pkt)
    {
        if (!accept) return false;
        sent.emplace_back(pkt, pkt + pkt[0] + 1);
        if (callback) {
            auto* copy = static_cast<uint8_t*>(malloc(pkt[0] + 1));
            assert(copy); memcpy(copy, pkt, pkt[0] + 1); callback(copy);
        }
        return true;
    }
} lan;
static TestLAN* mlan = &lan;

#include "trial.h"

static void reset()
{
    Preferences::reset();
    memset(&trial, 0, sizeof(trial));
    trial_storage_ok = trial_store_open = false;
    now_ms = 1000;
    lan.callback = nullptr; lan.sent.clear(); lan.accept = true;
    lan.cache_cnt = lan.hash_cnt = 0;
    while (auto* p = radio.queue.dequeue()) free(p);
    radio.rx_cnt = radio.tx_cnt = 0;
    radio.rx_drop = radio.rx_bad = radio.tx_fail = 0;
    memset(radio.neigh_tab, 0, sizeof(radio.neigh_tab));
    Serial.input.clear(); Serial.output.clear();
}

static TrialRecord saved()
{
    TrialRecord r = {};
    assert(Preferences::bytes.size() == sizeof(r));
    memcpy(&r, Preferences::bytes.data(), sizeof(r));
    assert(trial_valid(r));
    return r;
}

static void prepare_and_finish()
{
    reset(); trial_setup();
    assert(trial_storage_ok && lan.callback);
    trial_dump();
    assert(Serial.output.find("\"node\":\"1234\"") != std::string::npos);
    trial_command("prep 7 1234 3 1000 10");
    assert(trial.state == READY && saved().state == READY);
    trial_command("prep 8 1234 3 1000 10");
    assert(trial.run == 7);
    trial_begin();
    assert(trial.state == RUNNING && saved().state == RUNNING);
    trial_loop(); assert(trial.sent == 1);
    now_ms = 1999; trial_loop(); assert(trial.sent == 1);
    now_ms = 2000; trial_loop(); assert(trial.sent == 2);
    now_ms = 3000; trial_loop(); assert(trial.sent == 3);
    assert(lan.sent.size() == 3);
    for (unsigned i = 0; i < 3; i++) assert(trial_seen(trial, i));
    radio.rx_cnt = 10; radio.tx_cnt = 12;
    radio.rx_drop = 2; radio.rx_bad = 1; radio.tx_fail = 3;
    now_ms = 11000; trial_loop();
    assert(trial.state == DONE && trial.stop_reason == 2 && trial.elapsed_ms == 10000);
    assert(saved().state == DONE && trial.rx_drop == 2 && trial.tx_fail == 3);
    TrialRecord before = trial;
    lan.add(lan.sent[0].data());
    trial_loop(); trial_dump();
    assert(memcmp(&before, &trial, sizeof(trial)) == 0);
    trial_begin(); assert(trial.state == DONE);
    memset(&trial, 0, sizeof(trial)); trial_setup();
    assert(trial.state == DONE && trial.sent == 3 && trial_seen(trial, 2));
    trial_command("clear");
    assert(trial.state == EMPTY && Preferences::bytes.empty());
}

static void interrupted_and_ready_reboot()
{
    reset(); trial_setup();
    trial_command("prep 9 9999 2 1000 10");
    memset(&trial, 0, sizeof(trial)); trial_setup();
    assert(trial.state == READY && lan.sent.empty());
    trial_begin(); now_ms += 2000; trial_loop();
    assert(trial.state == RUNNING && lan.sent.empty());
    trial_command("clear"); assert(trial.state == RUNNING);
    memset(&trial, 0, sizeof(trial)); trial_setup();
    assert(trial.state == INTERRUPTED && saved().state == INTERRUPTED);
    trial_begin(); assert(trial.state == INTERRUPTED);
}

static void storage_errors()
{
    reset(); Preferences::bytes = {1, 2, 3}; trial_setup();
    assert(!trial_storage_ok && lan.callback);
    assert(Preferences::bytes.size() == 3);
    trial_command("clear");
    trial_command("prep 1 1234 1 1000 5");
    trial_begin(); trial_loop();
    assert(trial_seen(trial, 0));

    reset(); Preferences::fail_begin = true; trial_setup();
    trial_command("clear");
    assert(!trial_storage_ok && !trial_store_open);

    reset(); trial_setup(); trial_command("prep 1 1234 1 1000 5");
    Preferences::fail_write = true; trial_begin();
    assert(trial.state == INTERRUPTED && !trial_storage_ok && lan.sent.empty());

    reset(); trial_setup(); trial_command("prep 1 1234 1 1000 5"); trial_begin();
    Preferences::fail_write = true; trial_stop(1);
    assert(trial.state == DONE && !trial_storage_ok && saved().state == RUNNING);
    assert(Serial.output.find("save failed; keep power and dump") != std::string::npos);
}

static void commands_and_peers()
{
    reset(); trial_setup();
    for (const char* cmd : {"prep 0 1234 1 1000 5", "prep 1 1234 0 1000 5",
        "prep 1 1234 257 1000 300", "prep 1 1234 1 99 5", "prep 1 1234 5 1000 5",
        "prep 1 10000 1 1000 5", "prep 1 1234 1 1000 5 extra"}) {
        trial_command(cmd); assert(trial.state == EMPTY);
    }
    Serial.input = std::string(120, 'x') + "\nprep 11 1234 1 1000 5\n";
    trial_loop(); assert(trial.state == READY);
    assert(Serial.output.find("command too long") != std::string::npos);
    trial_begin();
    auto& p = radio.neigh_tab[0];
    p.id[0] = 0xab; p.id[1] = 0xcd; p.last_heard = now_ms; p.rssi = -50;
    trial_peers(); trial_peers();
    assert(trial.peers[0].id == 0xabcd && trial.peers[0].samples == 1);
    now_ms += 1000; p.last_heard = now_ms; p.rssi = -60; trial_peers();
    assert(trial.peers[0].samples == 2 && trial.peers[0].rssi == -60);
    trial_stop(1); assert(trial.stop_reason == 1);
}

static void led_states()
{
    reset(); trial_setup(); trial_led();
    assert(!led_r && !led_g && !led_b);
    trial_command("prep 1 1234 1 1000 5"); trial_led();
    assert(led_r && led_g && !led_b);
    trial_begin(); trial_led();
    assert(!led_r && !led_g && led_b);
    trial_stop(1); trial_led();
    assert(!led_r && led_g && !led_b);
    trial_storage_ok = false; trial_led();
    assert(led_r && !led_g && !led_b);
    trial_storage_ok = true; trial.state = INTERRUPTED; trial_led();
    assert(led_r && !led_g && !led_b);
}

int main()
{
    prepare_and_finish();
    interrupted_and_ready_reboot();
    storage_errors();
    commands_and_peers();
    led_states();
    puts("trial lifecycle: all tests passed");
}
