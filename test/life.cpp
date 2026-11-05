#include <atomic>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <string>
#include <vector>
#include "trial_data.h"
#include "ble_meta.h"

#define MLAN_REV host_test
#define COMPILE_DATE "host test"
#define IODEV_NEIGH_TIMEOUT 20000
#define WAVESHARE_S3ZERO
#define RGB_BUILTIN 21
static unsigned led_r, led_g, led_b;
static unsigned led_writes;
static void rgbLedWrite(unsigned, unsigned r, unsigned g, unsigned b)
{ led_r = r; led_g = g; led_b = b; led_writes++; }
static uint32_t now_ms = 1000;
static uint32_t millis() { return now_ms; }
static void delay(unsigned long ms) { now_ms += ms; }
static bool boot_pressed;
constexpr int LOW = 0, INPUT = 0, INPUT_PULLUP = 1;
static void pinMode(int, int) {}
static int digitalRead(int) { return boot_pressed ? LOW : 1; }
using std::min;
#include "libs/HotButton-0.1.1/src/HotButton.h"
static HotButton boot_button(0, true, LOW);
static uint8_t my_id[8] = {0x12, 0x34};
static uint8_t hardware_mac[6] = {0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc};

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
    uint32_t rx_cnt = 0, run_id = 0;
    uint8_t hardware_mac[6] = {};
    void (*observe)(const uint8_t*, const BleMeta&, int8_t) = nullptr;
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
    std::vector<std::vector<uint8_t>> pinned;
    unsigned next_pinned = 0;
    bool accept = true;
    bool is_beacon(const uint8_t* pkt) const
    { return pkt && pkt[0] == 48 && pkt[1] == 'B'; }
    void set_update_cb(void (*cb)(uint8_t*)) { callback = cb; }
    void clear_cache()
    { cache_cnt = hash_cnt = next_pinned = 0; pinned.clear(); }
    bool add(const uint8_t* pkt, bool pin = false)
    {
        if (!accept) return false;
        sent.emplace_back(pkt, pkt + pkt[0] + 1);
        if (pin) pinned.emplace_back(pkt, pkt + pkt[0] + 1);
        if (callback) {
            auto* copy = static_cast<uint8_t*>(malloc(pkt[0] + 1));
            assert(copy); memcpy(copy, pkt, pkt[0] + 1); callback(copy);
        }
        return true;
    }
    bool repeat_pinned()
    {
        if (pinned.empty()) return false;
        sent.push_back(pinned[next_pinned++ % pinned.size()]);
        return true;
    }
} lan;
static TestLAN* mlan = &lan;

#include "trial.h"

static void reset()
{
    Preferences::reset();
    LittleFS.reset();
    memset(&trial, 0, sizeof(trial));
    memset(&trial_mode, 0, sizeof(trial_mode));
    memset(&trial_topology, 0, sizeof(trial_topology));
    trial_storage_ok = trial_store_open = false;
    trial_config_open = false; trial_label = 0;
    trial_result_saved = trial_topology_saved = false;
    trial_led_shown = UINT32_MAX;
    led_r = led_g = led_b = led_writes = 0;
    trial_files_ok = false;
    now_ms = 1000;
    lan.callback = nullptr; lan.sent.clear(); lan.accept = true;
    lan.clear_cache();
    while (auto* p = radio.queue.dequeue()) free(p);
    radio.rx_cnt = radio.tx_cnt = radio.run_id = 0;
    radio.observe = nullptr;
    memcpy(radio.hardware_mac, hardware_mac, 6);
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
    assert(led_r && !led_g && led_b);
    trial_storage_ok = true; trial.state = INTERRUPTED; trial_led();
    assert(led_r && !led_g && led_b);
}

static void labels_and_led_patterns()
{
    reset(); trial_setup();
    const unsigned colors[5][3] = {{0,12,0}, {12,12,0}, {0,0,12}, {12,0,0}, {8,8,8}};
    for (unsigned label = 1; label <= 5; label++) {
        char command[16]; snprintf(command, sizeof(command), "label %u", label);
        trial_command(command); trial_led();
        assert(trial_label == label && Preferences::bytes.empty());
        assert(led_r == colors[label-1][0] && led_g == colors[label-1][1] && led_b == colors[label-1][2]);
    }
    unsigned writes = led_writes;
    trial_led(); trial_led();
    assert(led_writes == writes);
    for (const char *bad : {"label 0", "label 6", "label -1", "label 2 extra", "label 01", "label "}) {
        Serial.output.clear(); trial_command(bad);
        assert(trial_label == 5 && Serial.output.find("\"ok\":false") != std::string::npos);
    }
    trial_command("prep 15 1234 1 1000 10");
    auto before = Preferences::bytes;
    trial_command("label 4");
    assert(Preferences::bytes == before && trial_label == 4);
    now_ms = 1000; trial_led(); assert(led_r && !led_g && !led_b);
    now_ms = 1600; trial_led(); assert(!led_r && !led_g && !led_b);
    now_ms = 2000; trial_led(); assert(led_r && !led_g && !led_b);
    trial_begin();
    before = Preferences::bytes;
    Serial.output.clear(); trial_command("label 1");
    assert(trial_label == 4 && Preferences::bytes == before);
    assert(Serial.output.find("stop before changing label") != std::string::npos);
    now_ms = 2125; trial_led(); assert(!led_r && !led_g && !led_b);
    now_ms = 2250; trial_led(); assert(led_r && !led_g && !led_b);
    trial_stop(1); trial_led(); assert(led_r && !led_g && !led_b);
    now_ms = 2375; trial_led(); assert(led_r && !led_g && !led_b);
    assert(trial_result_saved && trial_topology_saved);
    before = Preferences::bytes;
    trial_setup(); trial_led();
    assert(trial_label == 4 && Preferences::bytes == before);
    assert(trial_result_saved && trial_topology_saved && led_r && !led_b);
    trial_command("clear");
    assert(trial_label == 4 && Preferences::config_values.at("label") == 4);
    trial_setup();
    assert(trial_label == 4 && trial.state == EMPTY);

    reset(); Preferences::fail_config_begin = true; trial_setup();
    assert(trial_storage_ok && !trial_config_open);
    trial_command("label 1");
    assert(trial_label == 0 && Preferences::bytes.empty());
    assert(Serial.output.find("label storage unavailable") != std::string::npos);
    reset(); trial_setup();
    trial_command("label 2");
    Preferences::fail_write = true; trial_command("label 3");
    assert(trial_label == 2);
    Preferences::fail_write = false; Preferences::fail_read = true;
    trial_command("label 4");
    assert(trial_label == 2);
}

static void archive_warning_led()
{
    reset(); trial_setup(); trial_command("label 1");
    trial_command("prep 16 1234 1 1000 10"); trial_begin(); trial_loop();
    LittleFS.fail_rename = true;
    trial_stop(1); trial_led();
    assert(trial_storage_ok && !trial_result_saved && !trial_topology_saved);
    assert(led_r && !led_g && led_b);
    now_ms += 125; trial_led(); assert(!led_r && !led_g && !led_b);
    LittleFS.fail_rename = false;
    memset(&trial, 0, sizeof(trial)); trial_setup();
    assert(trial_result_saved && !trial_topology_saved);
    now_ms = 2000; trial_led(); assert(led_r && !led_g && led_b);
    LittleFS.put("/123456789abc-16.top", "unrelated readable file\n");
    trial_setup();
    assert(!trial_topology_saved);
    trial_led(); assert(led_r && !led_g && led_b);
    trial.state = INTERRUPTED;
    trial_led(); assert(led_r && !led_g && led_b);

    reset(); trial_setup(); trial_command("label 1");
    trial_command("prep 16 1234 1 1000 10"); trial_begin(); trial_loop(); trial_stop(1);
    auto before = Preferences::bytes;
    std::string wrong = LittleFS.contents("/123456789abc-16.log");
    auto run_at = wrong.find("\"run\":16,");
    assert(run_at != std::string::npos);
    wrong.replace(run_at, 9, "\"run\":17,");
    LittleFS.put("/123456789abc-16.log", wrong);
    trial_setup(); now_ms = 2000; trial_led();
    assert(!trial_result_saved && trial_topology_saved);
    assert(Preferences::bytes == before && LittleFS.contents("/123456789abc-16.log") == wrong);
    assert(led_r && !led_g && led_b);
}

static void button_step(bool pressed, uint32_t elapsed)
{
    now_ms += elapsed;
    boot_pressed = pressed;
    boot_button.update();
    trial_start_button(boot_button.pressedFor(50));
}

static void start_only_button()
{
    reset(); trial_setup();
    trial_command("prep 12 1234 1 1000 10");
    button_step(false, 30);
    button_step(true, 30);
    assert(trial.state == READY);
    button_step(true, 60);
    assert(trial.state == RUNNING);
    uint32_t started = trial_start_ms;
    button_step(true, 1000);
    button_step(false, 30);
    for (uint32_t pause : {30u, 500u}) {
        button_step(true, pause);
        button_step(true, 100);
        button_step(false, 30);
        assert(trial.state == RUNNING && trial_start_ms == started);
    }
    now_ms = started + trial.limit_ms;
    trial_loop();
    assert(trial.state == DONE && saved().stop_reason == 2);
    button_step(true, 30);
    button_step(true, 60);
    assert(trial.state == DONE);
    button_step(false, 30);
}

static void finish_for_archive()
{
    trial_command("prep 7 1234 3 1000 10");
    assert(trial.state == READY);
    trial_begin();
    trial_loop();
    now_ms += 1000; trial_loop();
    now_ms += 1000; trial_loop();
    trial_stop(1);
    assert(trial.state == DONE && saved().state == DONE);
}

static std::string raw_json()
{
    Serial.output.clear(); trial_dump();
    assert(Serial.output.substr(0, 6) == "@MLAN ");
    return Serial.output.substr(6);
}

static void retained_layout_and_archive()
{
    static_assert(sizeof(TrialRecord) == 356, "Retained NVS layout changed");
    static_assert(offsetof(TrialRecord, checksum) == 352, "Retained checksum offset changed");
    reset(); trial_setup(); finish_for_archive();
    assert(LittleFS.exists("/123456789abc-7.log"));
    std::string original = LittleFS.contents("/123456789abc-7.log");
    assert(original == raw_json());
    assert(!LittleFS.exists("/123456789abc-7.log.tmp"));
    assert(LittleFS.format_calls == 0 && LittleFS.begin_arguments == std::vector<bool>{false});
    trial_command("clear");
    assert(Preferences::bytes.empty() && LittleFS.contents("/123456789abc-7.log") == original);
    trial_command("prep 7 1234 3 1000 10");
    assert(trial.state == EMPTY && Preferences::bytes.empty());
    assert(Serial.output.find("run number already archived") != std::string::npos);
    trial_command("prep 8 1234 3 1000 10");
    assert(trial.state == READY && saved().run == 8);
}

static void archive_reboot_and_conflict()
{
    reset(); trial_setup();
    LittleFS.fail_rename = true;
    finish_for_archive();
    assert(!LittleFS.exists("/123456789abc-7.log") && LittleFS.exists("/123456789abc-7.log.tmp"));
    auto nvs = Preferences::bytes;
    std::string original = raw_json();
    LittleFS.fail_rename = false;
    memset(&trial, 0, sizeof(trial)); trial_setup();
    assert(LittleFS.contents("/123456789abc-7.log") == original && Preferences::bytes == nvs);
    unsigned writes = LittleFS.write_calls, renames = LittleFS.rename_calls;
    memset(&trial, 0, sizeof(trial)); trial_setup();
    assert(LittleFS.write_calls == writes && LittleFS.rename_calls == renames);

    reset(); trial_setup();
    trial_command("prep 7 1234 3 1000 10"); trial_begin(); trial_loop();
    LittleFS.put("/123456789abc-7.log", "earlier conflicting result\n");
    trial_stop(1);
    assert(LittleFS.contents("/123456789abc-7.log") == "earlier conflicting result\n");
    assert(saved().state == DONE && trial_storage_ok);
    assert(Serial.output.find("saved to NVS; log unavailable") != std::string::npos);
    original = raw_json();
    memset(&trial, 0, sizeof(trial)); trial_setup();
    assert(raw_json() == original);
    assert(LittleFS.contents("/123456789abc-7.log") == "earlier conflicting result\n");
}

static void archive_failures()
{
    for (unsigned failure = 0; failure < 6; failure++) {
        reset();
        if (failure == 0) LittleFS.fail_begin = true;
        trial_setup();
        if (failure == 1) LittleFS.fail_write = true;
        if (failure == 2) LittleFS.write_limit = 13;
        if (failure == 3) LittleFS.fail_rename = true;
        if (failure == 4) LittleFS.fail_read = true;
        if (failure == 5) LittleFS.fail_seek = true;
        finish_for_archive();
        assert(trial_storage_ok && saved().state == DONE);
        assert(!LittleFS.exists("/123456789abc-7.log"));
        assert(raw_json().find("\"state\":\"done\"") != std::string::npos);
        assert(LittleFS.format_calls == 0);
        assert(std::all_of(LittleFS.begin_arguments.begin(), LittleFS.begin_arguments.end(), [](bool a) { return !a; }));
    }
}

static void filesystem_initialisation()
{
    reset(); LittleFS.fail_begin = true;
    LittleFS.put("/old.log", "preserve while mount fails");
    trial_setup();
    assert(!trial_files_ok && LittleFS.format_calls == 0);
    trial_command("prep 7 1234 3 1000 10");
    trial_command("fsinit");
    assert(LittleFS.format_calls == 0 && LittleFS.files.count("/old.log"));
    trial_begin(); trial_command("fsinit");
    assert(LittleFS.format_calls == 0 && trial.state == RUNNING);
    trial_stop(1); trial_command("clear");
    LittleFS.fail_begin = false;
    trial_command("fsinit");
    assert(trial_files_ok && LittleFS.format_calls == 1 && LittleFS.files.empty());
    trial_command("fsinit");
    assert(LittleFS.format_calls == 1);

    reset(); LittleFS.fail_begin = true; trial_setup();
    LittleFS.fail_format = true; trial_command("fsinit");
    assert(!trial_files_ok && LittleFS.format_calls == 1);

    reset(); LittleFS.fail_begin = true;
    Preferences::bytes = {1, 2, 3}; trial_setup();
    assert(trial.state == EMPTY && !trial_storage_ok);
    trial_command("fsinit");
    assert(LittleFS.format_calls == 0 && Preferences::bytes.size() == 3);

    reset(); LittleFS.fail_begin = true; Preferences::fail_begin = true;
    trial_setup(); trial_command("fsinit");
    assert(LittleFS.format_calls == 0 && !trial_store_open);
}

static void largest_archive()
{
    reset(); trial_setup();
    trial_command("prep 4294967295 1234 256 100 3600");
    assert(trial.state == READY);
    trial.state = DONE; trial.stop_reason = 2; trial.sent = 256;
    trial.elapsed_ms = trial.duplicates = trial.invalid = UINT32_MAX;
    trial.rx = trial.tx = trial.rx_drop = trial.rx_bad = trial.tx_fail = UINT32_MAX;
    memset(trial.seen, 0xff, sizeof(trial.seen));
    memset(trial.build, 'x', sizeof(trial.build) - 1); trial.build[sizeof(trial.build) - 1] = 0;
    for (unsigned i = 0; i < 16; i++) {
        trial.peers[i].id = 0xff00 + i;
        trial.peers[i].samples = 65535;
        trial.peers[i].rssi = -32768;
        trial.peers[i].last_ms = UINT32_MAX;
    }
    assert(trial_save() && trial_archive(trial));
    std::string json = LittleFS.contents("/123456789abc-4294967295.log");
    assert(json.size() < 4096 && json == raw_json());
    assert(json.find("254,255]") != std::string::npos);
    assert(json.find("\"id\":\"ff0f\"") != std::string::npos);
    printf("largest JSON log: %zu bytes (limit 4096)\n", json.size());
}

static void file_commands()
{
    reset(); trial_setup(); finish_for_archive();
    std::string json = LittleFS.contents("/123456789abc-7.log");
    uint32_t crc = trial_crc32((const uint8_t*)json.data(), json.size()) ^ 0xffffffffu;
    char checksum[32]; snprintf(checksum, sizeof(checksum), "\"crc32\":\"%08lx\"", (unsigned long)crc);
    Serial.output.clear(); trial_command("id");
    assert(Serial.output == "@MLAN {\"mac\":\"123456789abc\",\"ok\":true,\"node\":\"1234\",\"files_ok\":true,\"label\":0}\n");
    LittleFS.put("/123456789abc-7.log.tmp", "partial");
    LittleFS.put("/abcd-8.log", "other node");
    LittleFS.put("/123456789abc-9.log.bad", "not a log");
    Serial.output.clear(); trial_command("files");
    assert(Serial.output.find("\"kind\":\"mlan-files\"") != std::string::npos);
    assert(Serial.output.find("\"name\":\"123456789abc-7.log\"") != std::string::npos);
    assert(Serial.output.find(checksum) != std::string::npos);
    assert(Serial.output.find("abcd") == std::string::npos && Serial.output.find(".tmp") == std::string::npos);
    Serial.output.clear(); trial_command("dump 7");
    assert(Serial.output.find("\"kind\":\"mlan-file\"") != std::string::npos);
    assert(Serial.output.find(checksum) != std::string::npos);
    assert(Serial.output.find("\nbegin 644 123456789abc-7.log\n") != std::string::npos);
    assert(Serial.output.substr(Serial.output.size() - 6) == "`\nend\n");

    LittleFS.read_calls = 0;
    LittleFS.fail_read_after = (json.size() + 127) / 128 + 1;
    Serial.output.clear(); trial_command("dump 7");
    assert(Serial.output.find("\nbegin 644 123456789abc-7.log\n") != std::string::npos);
    assert(Serial.output.find("\nend\n") == std::string::npos);
    assert(saved().state == DONE && LittleFS.contents("/123456789abc-7.log") == json);
    LittleFS.fail_read_after = std::numeric_limits<size_t>::max();

    LittleFS.put("/123456789abc-8.log", "Cat");
    Serial.output.clear(); trial_command("dump 8");
    assert(Serial.output.find("\nbegin 644 123456789abc-8.log\n#0V%T\n`\nend\n") != std::string::npos);
    const uint8_t check[] = "123456789";
    assert((trial_crc32(check, 9) ^ 0xffffffffu) == 0xcbf43926u);
    for (const char* command : {"dump 0", "dump -1", "dump 7 extra", "dump 4294967296", "dump 99"}) {
        Serial.output.clear(); trial_command(command);
        assert(Serial.output.find("\"ok\":false") != std::string::npos);
    }
    trial_command("clear"); trial_command("prep 9 1234 3 1000 10"); trial_begin();
    for (const char* command : {"files", "dump 7", "fsinit"}) {
        Serial.output.clear(); trial_command(command);
        assert(Serial.output == "@MLAN {\"ok\":false,\"message\":\"stop before file access\"}\n");
        assert(trial.state == RUNNING);
    }
    assert(raw_json().find("\"state\":\"running\"") != std::string::npos);
}

static BleMeta peer_frame(uint32_t run, uint32_t sequence, uint32_t time)
{
    return {{0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff}, run, sequence, time};
}

static void topology_lifecycle()
{
    reset(); trial_setup();
    assert(radio.observe == trial_observe);
    trial_command("prep 71 9999 3 1000 10");
    uint8_t beacon[49] = {48, 'B'};
    BleMeta meta = peer_frame(71, 1, now_ms);
    radio.observe(beacon, meta, -51);
    assert(trial_topology.count == 0);
    trial_begin();
    assert(radio.run_id == 71 && trial_topology.run == 71);
    meta.run = 72;
    radio.observe(beacon, meta, -51);
    assert(trial_topology.count == 0 && trial_topology.ignored_run == 1);
    meta.run = 71;
    meta.received_ms = trial_start_ms - 1;
    radio.observe(beacon, meta, -51);
    assert(trial_topology.count == 0);
    meta.received_ms = trial_start_ms;
    unsigned writes = LittleFS.write_calls;
    radio.observe(beacon, meta, -51);
    assert(trial_topology.count == 1 && LittleFS.write_calls == writes);
    uint8_t page[TRIAL_BYTES + 1];
    trial_page(page, trial, 2);
    meta.sequence++;
    meta.received_ms += 15;
    radio.observe(page, meta, -61);
    assert(trial_topology.count == 2 && LittleFS.write_calls == writes);
    now_ms = meta.received_ms;
    trial_stop(1);
    assert(LittleFS.exists("/123456789abc-71.top"));
    std::string top = LittleFS.contents("/123456789abc-71.top");
    assert(top.find("\"kind\":\"mlan-topology\"") != std::string::npos);
    assert(top.find("\"mac\":\"123456789abc\"") != std::string::npos);
    assert(top.find("aabbccddeeff") != std::string::npos);
    assert(top.find("\"run\":71") != std::string::npos);
    auto count = trial_topology.count;
    meta.sequence++;
    radio.observe(beacon, meta, -70);
    assert(trial_topology.count == count);
    Serial.output.clear(); trial_command("topo 71");
    assert(Serial.output.find("\nbegin 644 123456789abc-71.top\n") != std::string::npos);
    assert(Serial.output.find("\nend\n") != std::string::npos);
    trial_command("clear");
    assert(LittleFS.contents("/123456789abc-71.top") == top);
    trial_command("prep 72 9999 3 1000 10");
    now_ms = 0;
    trial_begin();
    meta = peer_frame(72, 1, 0);
    radio.observe(beacon, meta, -45);
    assert(trial_topology.run == 72 && trial_topology.count == 1);
    assert(trial_topology.overflow == 0 && trial_topology.ignored_run == 0);
    Serial.output.clear(); trial_command("topo 71");
    assert(Serial.output.find("stop before file access") != std::string::npos);
}

static void topology_overflow()
{
    reset(); trial_setup();
    trial_command("prep 73 9999 3 1000 10"); trial_begin();
    uint8_t beacon[49] = {48, 'B'};
    size_t capacity = sizeof(trial_topology.events) / sizeof(trial_topology.events[0]);
    unsigned writes = LittleFS.write_calls;
    for (unsigned i = 0; i < capacity + 3; i++) {
        BleMeta meta = peer_frame(73, i + 1, trial_start_ms + i);
        now_ms = meta.received_ms;
        radio.observe(beacon, meta, -60);
    }
    assert(trial_topology.count == capacity && trial_topology.overflow == 3);
    assert(LittleFS.write_calls == writes);
    trial_stop(1);
    std::string top = LittleFS.contents("/123456789abc-73.top");
    assert(top.find("\"overflow\":3") != std::string::npos);
    assert(top.size() < 65536);
}

static void topology_reboot_and_conflict()
{
    reset(); trial_setup();
    trial_command("prep 74 9999 3 1000 10"); trial_begin();
    uint8_t beacon[49] = {48, 'B'};
    BleMeta meta = peer_frame(74, 1, now_ms);
    radio.observe(beacon, meta, -50);
    LittleFS.fail_rename = true;
    trial_stop(1);
    assert(saved().state == DONE && !LittleFS.exists("/123456789abc-74.top"));
    auto nvs = Preferences::bytes;
    memset(&trial, 0, sizeof(trial));
    memset(&trial_topology, 0, sizeof(trial_topology));
    LittleFS.fail_rename = false;
    trial_setup();
    assert(trial.state == DONE && Preferences::bytes == nvs);
    assert(!LittleFS.exists("/123456789abc-74.top"));
    Serial.output.clear(); trial_command("topo 74");
    assert(Serial.output.find("\"ok\":false") != std::string::npos);

    reset(); trial_setup();
    trial_command("prep 75 9999 3 1000 10"); trial_begin();
    meta = peer_frame(75, 1, now_ms);
    radio.observe(beacon, meta, -50);
    LittleFS.put("/123456789abc-75.top", "earlier topology file\n");
    trial_stop(1);
    assert(saved().state == DONE && LittleFS.exists("/123456789abc-75.log"));
    assert(LittleFS.contents("/123456789abc-75.top") == "earlier topology file\n");
}

static void legacy_archive()
{
    reset(); trial_setup();
    const std::string legacy = "{\"kind\":\"mlan-run\",\"node\":\"1234\",\"run\":99}\n";
    LittleFS.put("/1234-99.log", legacy);
    Serial.output.clear(); trial_command("dump 99");
    assert(Serial.output.find("\nbegin 644 1234-99.log\n") != std::string::npos);
    assert(Serial.output.find("\nend\n") != std::string::npos);
    assert(LittleFS.contents("/1234-99.log") == legacy);
    Serial.output.clear(); trial_command("topo 99");
    assert(Serial.output.find("\"ok\":false") != std::string::npos);
    assert(!LittleFS.exists("/123456789abc-99.top"));
}

static void legacy_reboot_archive()
{
    for (auto state : {DONE, INTERRUPTED}) {
        reset();
        TrialRecord old = {};
        old.version = 1; old.run = state == DONE ? 80 : 81;
        old.node = 0x1234; old.source = 0x9999; old.total = 3; old.state = state;
        old.gap_ms = 1000; old.limit_ms = old.elapsed_ms = 10000;
        strcpy(old.build, "old firmware");
        old.checksum = trial_checksum(old);
        assert(trial_valid(old));
        const auto *bytes = reinterpret_cast<const unsigned char*>(&old);
        Preferences::bytes.assign(bytes, bytes + sizeof(old));
        auto original_nvs = Preferences::bytes;
        TrialText text;
        trial_json(text, old, true, old.node);
        std::string legacy(text.data, text.used);
        assert(text.ok && legacy.find("\"mac\"") == std::string::npos);
        char legacy_path[40], new_path[40], command[24];
        snprintf(legacy_path, sizeof(legacy_path), "/1234-%u.log", old.run);
        snprintf(new_path, sizeof(new_path), "/123456789abc-%u.log", old.run);
        snprintf(command, sizeof(command), "dump %u", old.run);
        LittleFS.put(legacy_path, legacy);
        trial_setup();
        assert(trial.state == state && Preferences::bytes == original_nvs);
        assert(!LittleFS.exists(new_path));
        assert(LittleFS.contents(legacy_path) == legacy);
        Serial.output.clear(); trial_command(command);
        std::string envelope = Serial.output.substr(0, Serial.output.find('\n'));
        assert(envelope.find("\"mac\"") == std::string::npos);
        assert(envelope.find(std::string("\"name\":\"") + (legacy_path + 1) + "\"") != std::string::npos);
        assert(Serial.output.find(std::string("\nbegin 644 ") + (legacy_path + 1) + "\n") != std::string::npos);
        assert(Serial.output.find("\nend\n") != std::string::npos);
        assert(raw_json().find("\"mac\":\"123456789abc\"") != std::string::npos);
        assert(LittleFS.contents(legacy_path) == legacy && !LittleFS.exists(new_path));
    }
}

static void captured_v1_record()
{
    // Node 1's saved 356-byte record, captured from flash before the upgrade.
    const char *hex =
        "0100000066b9c378605d605d05000300e8030000e09304003fc5040005000000"
        "03000000000000000d0000003900000011000000000000000000000002000000"
        "1f00000000000000000000000000000000000000000000000000000000000000"
        "c7180a00a4ff0000059e04000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000000000000"
        "6532313665653920323032362d30392d32382030353a34340000000000000000"
        "0000000000000000000000000000000000000000000000000000000000000000"
        "fe84ac16";
    std::vector<unsigned char> bytes;
    for (size_t i = 0; hex[i]; i += 2) {
        char pair[] = {hex[i], hex[i+1], 0};
        bytes.push_back(strtoul(pair, nullptr, 16));
    }
    assert(bytes.size() == 356);
    TrialRecord old = {};
    memcpy(&old, bytes.data(), sizeof(old));
    assert(old.version == 1 && old.payload_bytes == 0 && trial_valid(old));
    TrialText text;
    trial_json(text, old, true, old.node);
    const std::string legacy = R"({"schema":1,"kind":"mlan-run","state":"done","storage_ok":true,"node":"5d60","source":"5d60","run":2026092902,"total":5,"sent":5,"gap_ms":1000,"limit_ms":300000,"elapsed_ms":312639,"stop_reason":2,"cutoff":"local","build":"e216ee9 2026-09-28 05:44","duplicates":3,"invalid":0,"rx":13,"tx":57,"rx_drop":17,"rx_bad":0,"tx_fail":0,"received":[0,1,2,3,4],"peers":[{"id":"18c7","rssi":-92,"samples":10,"last_ms":302597}]})" "\n";
    assert(text.ok && std::string(text.data, text.used) == legacy);
    const std::string current = "{\"mac\":\"ac276eb0f9b4\"," + legacy.substr(1);
    uint8_t saved_id[2], saved_mac[6];
    memcpy(saved_id, my_id, 2); memcpy(saved_mac, hardware_mac, 6);
    my_id[0] = 0x5d; my_id[1] = 0x60;
    const uint8_t mac[] = {0xac,0x27,0x6e,0xb0,0xf9,0xb4};
    memcpy(hardware_mac, mac, 6);
    for (bool short_name : {true, false}) {
        reset(); Preferences::bytes = bytes;
        const char *path = short_name ? "/5d60-2026092902.log" : "/ac276eb0f9b4-2026092902.log";
        LittleFS.put(path, short_name ? legacy : current);
        trial_setup();
        assert(trial_storage_ok && trial.state == DONE && trial_bytes(trial) == 32);
        assert(Preferences::bytes == bytes);
        assert(LittleFS.contents(path) == (short_name ? legacy : current));
        assert(raw_json() == current);
        if (short_name) assert(!LittleFS.exists("/ac276eb0f9b4-2026092902.log"));
        trial_setup();
        assert(Preferences::bytes == bytes && raw_json() == current);
    }
    memcpy(my_id, saved_id, 2); memcpy(hardware_mac, saved_mac, 6);
}

static void packet_sizes()
{
    for (unsigned bytes : {32u, 64u, 120u}) {
        reset(); trial_setup(); trial_command("label 3");
        char command[64];
        snprintf(command, sizeof(command), "prep 90 1234 2 1000 10 %u", bytes);
        trial_command(command);
        assert(trial.state == READY && trial.version == 3 && trial_bytes(trial) == bytes);
        auto ready = Preferences::bytes;
        trial_setup();
        assert(trial.state == READY && trial_bytes(trial) == bytes && Preferences::bytes == ready);
        trial_begin(); trial_loop();
        now_ms += 1000; trial_loop();
        assert(lan.sent.size() == 2 && lan.sent[0].size() == bytes + 1);
        BleMeta meta = peer_frame(90, 1, now_ms);
        radio.observe(lan.sent[0].data(), meta, -50);
        assert(trial_topology.count == 1 && trial_topology.events[0].type == 2);
        assert(trial_topology.events[0].page == 0);
        std::vector<uint8_t> broken = lan.sent[1]; broken.back() ^= 1;
        radio.observe(broken.data(), meta, -50);
        assert(trial_topology.events[1].type == 0 && trial_topology.events[1].page == -1);
        const uint8_t short_page[] = {4, 'M', 'L', 'T', '1'};
        radio.observe(short_page, meta, -50);
        assert(trial_topology.events[2].type == 0);
        trial_stop(1);
        std::string field = "\"payload_bytes\":" + std::to_string(bytes);
        assert(raw_json().find(field) != std::string::npos);
        std::string result = LittleFS.contents("/123456789abc-90.log");
        std::string topology = LittleFS.contents("/123456789abc-90.top");
        assert(result.find(field) != std::string::npos && topology.find(field) != std::string::npos);
        auto done = Preferences::bytes;
        trial_setup();
        assert(trial.state == DONE && trial_bytes(trial) == bytes && Preferences::bytes == done);
        assert(trial_result_saved && trial_topology_saved && trial_label == 3);
        assert(LittleFS.contents("/123456789abc-90.log") == result);
        assert(LittleFS.contents("/123456789abc-90.top") == topology);
    }
    reset(); trial_setup();
    for (const char *size : {"0", "14", "31", "33", "63", "119", "121", "128", "65536", "-32", "32 extra"}) {
        std::string command = std::string("prep 90 1234 2 1000 10 ") + size;
        trial_command(command.c_str());
        assert(trial.state == EMPTY && Preferences::bytes.empty());
    }
    trial_command("prep 90 1234 2 1000 10");
    assert(trial.version == 3 && trial_bytes(trial) == 32);
}

static void source_mode()
{
    reset(); trial_setup();
    for (const char* command : {"prep 91 1234 11 1000 60 32 5000",
             "prep 91 1234 5 1000 60 32 999", "prep 91 1234 5 1000 60 32 60001"}) {
        trial_command(command);
        assert(trial.state == EMPTY && Preferences::bytes.empty() && Preferences::mode_bytes.empty());
    }
    trial_command("prep 91 1234 2 1000 60 32 5000");
    auto ready = Preferences::bytes, ready_mode = Preferences::mode_bytes;
    trial_setup();
    assert(trial.state == READY && trial_mode.repeat_ms == 5000 && trial_mode.repeat_attempts == 0);
    assert(Preferences::bytes == ready && Preferences::mode_bytes == ready_mode);
    trial_begin();
    trial_loop();
    assert(trial.sent == 1 && lan.pinned.size() == 1 && !trial_mode.repeat_attempts);
    now_ms += 1000; trial_loop();
    assert(trial.sent == 2 && lan.pinned.size() == 2 && lan.sent.size() == 2);
    const auto originals = lan.sent;
    now_ms += 4999; trial_loop();
    assert(lan.sent.size() == 2 && !trial_mode.repeat_attempts);
    now_ms++; trial_loop();
    assert(lan.sent.size() == 3 && lan.sent.back() == originals[0]);
    now_ms += 5000; trial_loop();
    assert(lan.sent.size() == 4 && lan.sent.back() == originals[1]);
    assert(trial_mode.repeat_attempts == 2 && trial.sent == 2 && trial.duplicates == 0);
    assert(trial_seen(trial, 0) && trial_seen(trial, 1));
    trial_stop(1);
    std::string result = LittleFS.contents("/123456789abc-91.log");
    std::string topology = LittleFS.contents("/123456789abc-91.top");
    for (const auto& json : {result, topology}) {
        assert(json.find("\"source_pinned\":true") != std::string::npos);
        assert(json.find("\"repeat_ms\":5000") != std::string::npos);
        assert(json.find("\"repeat_attempts\":2") != std::string::npos);
    }
    now_ms += 5000; trial_loop();
    assert(lan.sent.size() == 4);
    trial_setup();
    assert(trial.state == DONE && trial_mode.repeat_attempts == 2 && trial_mode.repeat_ms == 5000);
    assert(trial_result_saved && trial_topology_saved && raw_json() == result);
    trial_command("clear");
    assert(Preferences::bytes.empty() && Preferences::mode_bytes.empty());
    assert(LittleFS.contents("/123456789abc-91.log") == result);

    reset(); trial_setup(); trial_command("prep 92 9999 2 1000 60 32 5000");
    trial_begin(); now_ms += 20000; trial_loop();
    assert(trial_mode.repeat_ms == 5000 && !trial_mode.repeat_attempts);
    assert(lan.pinned.empty() && lan.sent.empty() && !trial.sent);

    reset(); trial_setup(); trial_command("prep 93 1234 2 1000 60 32 0");
    trial_begin(); trial_loop(); now_ms += 1000; trial_loop();
    now_ms += 20000; trial_loop();
    assert(trial.sent == 2 && lan.sent.size() == 2 && lan.pinned.empty());
    assert(!trial_mode.repeat_ms && !trial_mode.repeat_attempts);
}

static void source_mode_storage()
{
    reset(); trial_setup(); trial_command("prep 94 1234 5 1000 60 32 5000");
    auto run_bytes = Preferences::bytes, mode_bytes = Preferences::mode_bytes;
    for (unsigned bad = 0; bad < 5; bad++) {
        reset(); Preferences::bytes = run_bytes; Preferences::mode_bytes = mode_bytes;
        if (bad == 0) Preferences::mode_bytes.clear();
        else if (bad == 1) Preferences::mode_bytes.pop_back();
        else {
            TrialMode mode;
            memcpy(&mode, mode_bytes.data(), sizeof(mode));
            if (bad == 2) mode.run++;
            if (bad == 3) mode.repeat_ms = 999;
            mode.checksum = trial_mode_checksum(mode);
            if (bad == 4) mode.checksum++;
            memcpy(Preferences::mode_bytes.data(), &mode, sizeof(mode));
        }
        auto invalid = Preferences::mode_bytes;
        trial_setup(); trial_begin();
        assert(!trial_storage_ok && trial.state == INTERRUPTED);
        assert(Preferences::bytes == run_bytes && Preferences::mode_bytes == invalid);
        assert(LittleFS.files.empty() && lan.sent.empty());
        trial_command("clear");
        assert(Preferences::bytes.empty() && Preferences::mode_bytes.empty());
    }

    reset();
    TrialRecord old;
    memcpy(&old, run_bytes.data(), sizeof(old));
    old.version = 2; old.checksum = trial_checksum(old);
    Preferences::bytes.assign((uint8_t*)&old, (uint8_t*)&old + sizeof(old));
    trial_setup();
    assert(trial_storage_ok && trial.state == READY && !trial_mode.repeat_ms);
    assert(Preferences::mode_bytes.empty());

    reset(); trial_setup(); Preferences::fail_write = true;
    trial_command("prep 95 1234 5 1000 60 32 5000");
    assert(!trial_storage_ok && Preferences::bytes.empty());
    trial_begin(); assert(lan.sent.empty());
}

static void largest_topology()
{
    reset(); trial_setup(); trial_command("prep 4294967295 1234 10 1000 3600 120 1000");
    trial_begin();
    trial.state = DONE; trial.elapsed_ms = UINT32_MAX; trial.stop_reason = 1;
    memset(trial.build, 'x', sizeof(trial.build) - 1); trial.build[sizeof(trial.build) - 1] = 0;
    trial_mode.repeat_attempts = UINT32_MAX;
    trial_topology.count = TRIAL_EVENTS;
    trial_topology.overflow = trial_topology.ignored_run = UINT32_MAX;
    for (auto& event : trial_topology.events) {
        memset(event.sender, 0xff, sizeof(event.sender));
        event.sequence = event.ms = UINT32_MAX;
        event.rssi = -128; event.type = 1; event.page = -1;
    }
    assert(trial_save() && trial_archive_topology(trial));
    const auto json = LittleFS.contents("/123456789abc-4294967295.top");
    assert(json.size() < TRIAL_FILE_BYTES && trial_topology.count == 512);
    printf("largest topology: %zu bytes (limit %u)\n", json.size(), TRIAL_FILE_BYTES);
    Serial.output.clear(); trial_command("topo 4294967295");
    assert(Serial.output.find("begin 644 123456789abc-4294967295.top\n") != std::string::npos);
}

int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "--worst-topology-transfer")) {
        largest_topology();
        fwrite(Serial.output.data(), 1, Serial.output.size(), stdout);
        return 0;
    }
    if (argc == 2 && !strcmp(argv[1], "--topology-transfer")) {
        reset(); trial_setup();
        trial_command("prep 7 9999 3 1000 10"); trial_begin();
        uint8_t beacon[49] = {48, 'B'};
        BleMeta meta = peer_frame(7, 9, trial_start_ms + 123);
        radio.observe(beacon, meta, -51);
        uint8_t page[TRIAL_BYTES + 1]; trial_page(page, trial, 2);
        meta.sequence = 10; meta.received_ms = trial_start_ms + 200;
        radio.observe(page, meta, -61);
        now_ms = trial_start_ms + 200; trial_stop(1);
        Serial.output.clear(); trial_command("topo 7");
        fwrite(Serial.output.data(), 1, Serial.output.size(), stdout);
        return 0;
    }
    if (argc == 2 && !strcmp(argv[1], "--transfer")) {
        reset(); trial_setup(); finish_for_archive();
        Serial.output.clear(); trial_command("dump 7");
        fwrite(Serial.output.data(), 1, Serial.output.size(), stdout);
        return 0;
    }
    prepare_and_finish();
    interrupted_and_ready_reboot();
    storage_errors();
    commands_and_peers();
    led_states();
    labels_and_led_patterns();
    archive_warning_led();
    start_only_button();
    retained_layout_and_archive();
    archive_reboot_and_conflict();
    archive_failures();
    filesystem_initialisation();
    largest_archive();
    file_commands();
    topology_lifecycle();
    topology_overflow();
    topology_reboot_and_conflict();
    legacy_archive();
    legacy_reboot_archive();
    captured_v1_record();
    packet_sizes();
    source_mode();
    source_mode_storage();
    largest_topology();
    puts("trial lifecycle: all tests passed");
}
