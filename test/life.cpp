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
static void delay(unsigned long ms) { now_ms += ms; }
static bool boot_pressed;
constexpr int LOW = 0, INPUT = 0, INPUT_PULLUP = 1;
static void pinMode(int, int) {}
static int digitalRead(int) { return boot_pressed ? LOW : 1; }
using std::min;
#include "libs/HotButton-0.1.1/src/HotButton.h"
static HotButton boot_button(0, true, LOW);
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
    LittleFS.reset();
    memset(&trial, 0, sizeof(trial));
    trial_storage_ok = trial_store_open = false;
    trial_files_ok = false;
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
    assert(LittleFS.exists("/1234-7.log"));
    std::string original = LittleFS.contents("/1234-7.log");
    assert(original == raw_json());
    assert(!LittleFS.exists("/1234-7.log.tmp"));
    assert(LittleFS.format_calls == 0 && LittleFS.begin_arguments == std::vector<bool>{false});
    trial_command("clear");
    assert(Preferences::bytes.empty() && LittleFS.contents("/1234-7.log") == original);
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
    assert(!LittleFS.exists("/1234-7.log") && LittleFS.exists("/1234-7.log.tmp"));
    auto nvs = Preferences::bytes;
    std::string original = raw_json();
    LittleFS.fail_rename = false;
    memset(&trial, 0, sizeof(trial)); trial_setup();
    assert(LittleFS.contents("/1234-7.log") == original && Preferences::bytes == nvs);
    unsigned writes = LittleFS.write_calls, renames = LittleFS.rename_calls;
    memset(&trial, 0, sizeof(trial)); trial_setup();
    assert(LittleFS.write_calls == writes && LittleFS.rename_calls == renames);

    reset(); trial_setup();
    trial_command("prep 7 1234 3 1000 10"); trial_begin(); trial_loop();
    LittleFS.put("/1234-7.log", "earlier conflicting result\n");
    trial_stop(1);
    assert(LittleFS.contents("/1234-7.log") == "earlier conflicting result\n");
    assert(saved().state == DONE && trial_storage_ok);
    assert(Serial.output.find("saved to NVS; log unavailable") != std::string::npos);
    original = raw_json();
    memset(&trial, 0, sizeof(trial)); trial_setup();
    assert(raw_json() == original);
    assert(LittleFS.contents("/1234-7.log") == "earlier conflicting result\n");
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
        assert(!LittleFS.exists("/1234-7.log"));
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
    std::string json = LittleFS.contents("/1234-4294967295.log");
    assert(json.size() < 4096 && json == raw_json());
    assert(json.find("254,255]") != std::string::npos);
    assert(json.find("\"id\":\"ff0f\"") != std::string::npos);
    printf("largest JSON log: %zu bytes (limit 4096)\n", json.size());
}

static void file_commands()
{
    reset(); trial_setup(); finish_for_archive();
    std::string json = LittleFS.contents("/1234-7.log");
    uint32_t crc = trial_crc32((const uint8_t*)json.data(), json.size()) ^ 0xffffffffu;
    char checksum[32]; snprintf(checksum, sizeof(checksum), "\"crc32\":\"%08lx\"", (unsigned long)crc);
    Serial.output.clear(); trial_command("id");
    assert(Serial.output == "@MLAN {\"ok\":true,\"node\":\"1234\",\"files_ok\":true}\n");
    LittleFS.put("/1234-7.log.tmp", "partial");
    LittleFS.put("/abcd-8.log", "other node");
    LittleFS.put("/1234-9.log.bad", "not a log");
    Serial.output.clear(); trial_command("files");
    assert(Serial.output.find("\"kind\":\"mlan-files\"") != std::string::npos);
    assert(Serial.output.find("\"name\":\"1234-7.log\"") != std::string::npos);
    assert(Serial.output.find(checksum) != std::string::npos);
    assert(Serial.output.find("abcd") == std::string::npos && Serial.output.find(".tmp") == std::string::npos);
    Serial.output.clear(); trial_command("dump 7");
    assert(Serial.output.find("\"kind\":\"mlan-file\"") != std::string::npos);
    assert(Serial.output.find(checksum) != std::string::npos);
    assert(Serial.output.find("\nbegin 644 1234-7.log\n") != std::string::npos);
    assert(Serial.output.substr(Serial.output.size() - 6) == "`\nend\n");

    LittleFS.read_calls = 0;
    LittleFS.fail_read_after = (json.size() + 127) / 128 + 1;
    Serial.output.clear(); trial_command("dump 7");
    assert(Serial.output.find("\nbegin 644 1234-7.log\n") != std::string::npos);
    assert(Serial.output.find("\nend\n") == std::string::npos);
    assert(saved().state == DONE && LittleFS.contents("/1234-7.log") == json);
    LittleFS.fail_read_after = std::numeric_limits<size_t>::max();

    LittleFS.put("/1234-8.log", "Cat");
    Serial.output.clear(); trial_command("dump 8");
    assert(Serial.output.find("\nbegin 644 1234-8.log\n#0V%T\n`\nend\n") != std::string::npos);
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

int main(int argc, char **argv)
{
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
    start_only_button();
    retained_layout_and_archive();
    archive_reboot_and_conflict();
    archive_failures();
    filesystem_initialisation();
    largest_archive();
    file_commands();
    puts("trial lifecycle: all tests passed");
}
