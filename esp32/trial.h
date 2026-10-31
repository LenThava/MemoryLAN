#pragma once
#include <Preferences.h>
#include "trial_data.h"
#include "trial_topology.h"
#ifdef MEMLAN_PKT_LEN
static_assert(TRIAL_MAX_BYTES <= MEMLAN_PKT_LEN, "Trial page exceeds the cache line");
#endif

static Preferences trial_store;
static TrialRecord trial;
static bool trial_storage_ok;
static bool trial_store_open;
static Preferences trial_config;
static bool trial_config_open;
static uint8_t trial_label;
static bool trial_result_saved, trial_topology_saved;
static uint32_t trial_led_shown = UINT32_MAX;
static uint32_t trial_start_ms, trial_next_ms, trial_peer_ms;
static uint32_t trial_rx0, trial_tx0, trial_drop0, trial_bad0, trial_fail0;
static TrialTopology trial_topology;

static void trial_observe(const uint8_t *pkt, const BleMeta &meta, int8_t rssi)
{
    if (trial.state != RUNNING || int32_t(meta.received_ms - trial_start_ms) < 0) return;
    if (meta.run != trial.run) { trial_topology.ignored_run++; return; }
    if (trial_topology.count == TRIAL_EVENTS) { trial_topology.overflow++; return; }
    auto &event = trial_topology.events[trial_topology.count++];
    memcpy(event.sender, meta.sender, 6);
    event.rssi = rssi; event.sequence = meta.sequence;
    event.ms = meta.received_ms - trial_start_ms;
    event.type = mlan->is_beacon(pkt) ? 1 : 0;
    event.page = -1;
    unsigned bytes = trial_bytes(trial);
    if (pkt && trial_size_valid(bytes) && pkt[0] == bytes && !memcmp(pkt + 1, "MLT1", 4)) {
        uint16_t seq = pkt[11] | (uint16_t(pkt[12]) << 8);
        uint8_t expected[TRIAL_MAX_BYTES + 1];
        trial_page(expected, trial, seq);
        if (seq < trial.total && !memcmp(expected, pkt, bytes + 1)) {
            event.type = 2; event.page = seq;
        }
    }
}

static void trial_led()
{
#if defined(WAVESHARE_S3ZERO)
    int state = trial_storage_ok ? trial.state : INTERRUPTED;
    const uint8_t colors[6][3] = {{0,0,0}, {0,12,0}, {12,12,0},
                                 {0,0,12}, {12,0,0}, {8,8,8}};
    unsigned r = 0, g = 0, b = 0;
    bool warning = state == INTERRUPTED || (state == DONE && !(trial_result_saved && trial_topology_saved));
    bool on = true;
    if (warning) {
        r = b = 12;
        on = millis() % 250 < 125;
    } else if (trial_label) {
        r = colors[trial_label][0]; g = colors[trial_label][1]; b = colors[trial_label][2];
        if (state == READY) on = millis() % 1000 < 500;
        if (state == RUNNING) on = millis() % 250 < 125;
    } else {
        switch (state) {
        case READY: r = 12; g = 6; break;
        case RUNNING: b = 12; break;
        case DONE: g = 12; break;
        }
    }
    if (!on) r = g = b = 0;
    uint32_t shown = (r << 16) | (g << 8) | b;
    if (shown == trial_led_shown) return;
    trial_led_shown = shown;
    rgbLedWrite(RGB_BUILTIN, r, g, b);
#endif
}

static void trial_reply(bool ok, const char *message)
{
    if (Serial) Serial.printf("@MLAN {\"ok\":%s,\"message\":\"%s\"}\n", ok ? "true" : "false", message);
}

#include "trial_log.h"

static bool trial_saved_file(bool topology)
{
    if (!trial_files_ok || trial.state != DONE) return false;
    char path[40]; trial_log_path(path, sizeof(path), trial.node, trial.run, topology);
    bool legacy = !topology && !LittleFS.exists(path);
    if (legacy) trial_log_path(path, sizeof(path), trial.node, trial.run, false, true);
    File file = LittleFS.open(path, "r");
    size_t bytes; uint32_t crc;
    if (!trial_file_info(file, bytes, crc)) return false;
    char prefix[192], address[13]; mac_text(hardware_mac, address);
    if (topology)
        snprintf(prefix, sizeof(prefix), "{\"mac\":\"%s\",\"node\":\"%04x\",\"run\":%u,\"schema\":1,\"kind\":\"mlan-topology\",",
            address, trial.node, trial.run);
    else if (legacy)
        snprintf(prefix, sizeof(prefix), "{\"schema\":1,\"kind\":\"mlan-run\",\"state\":\"done\",\"storage_ok\":true,\"node\":\"%04x\",\"source\":\"%04x\",\"run\":%u,", trial.node, trial.source, trial.run);
    else
        snprintf(prefix, sizeof(prefix), "{\"mac\":\"%s\",\"schema\":1,\"kind\":\"mlan-run\",\"state\":\"done\",\"storage_ok\":true,\"node\":\"%04x\",\"source\":\"%04x\",\"run\":%u,", address, trial.node, trial.source, trial.run);
    size_t length = strlen(prefix);
    uint8_t actual[sizeof(prefix)];
    return bytes >= length && file.read(actual, length) == length && !memcmp(actual, prefix, length);
}

static bool trial_save()
{
    trial.checksum = trial_checksum(trial);
    TrialRecord check = {};
    bool ok = trial_store.putBytes("run", &trial, sizeof(trial)) == sizeof(trial) &&
        trial_store.getBytesLength("run") == sizeof(check) &&
        trial_store.getBytes("run", &check, sizeof(check)) == sizeof(check) &&
        memcmp(&check, &trial, sizeof(check)) == 0;
    trial_storage_ok = trial_storage_ok && ok;
    return ok;
}

static void trial_update(uint8_t *pkt)
{
    trial_receive(trial, pkt);
    free(pkt);
}

static void trial_stats()
{
    trial.elapsed_ms = millis() - trial_start_ms;
    trial.rx = uint32_t(ble_if->rx_cnt) - trial_rx0;
    trial.tx = uint32_t(ble_if->tx_cnt) - trial_tx0;
    trial.rx_drop = ble_if->rx_drop.load() - trial_drop0;
    trial.rx_bad = ble_if->rx_bad.load() - trial_bad0;
    trial.tx_fail = ble_if->tx_fail.load() - trial_fail0;
}

static void trial_peers()
{
    for (const auto &n : ble_if->neigh_tab) {
        uint16_t id = (uint16_t(n.id[0]) << 8) | n.id[1];
        if (!id || millis() - n.last_heard >= IODEV_NEIGH_TIMEOUT) continue;
        for (auto &p : trial.peers) {
            if (p.id && p.id != id) continue;
            uint32_t heard = n.last_heard - trial_start_ms;
            if (!p.id || heard != p.last_ms) {
                p.id = id; p.rssi = n.rssi; p.last_ms = heard;
                if (p.samples < 65535) p.samples++;
            }
            break;
        }
    }
}

static void trial_stop(uint32_t reason)
{
    if (trial.state != RUNNING) { trial_reply(false, "not running"); return; }
    trial_stats(); trial_peers();
    trial.state = DONE;
    trial.stop_reason = reason;
    bool ok = trial_save();
    trial_result_saved = ok && trial_archive(trial);
    trial_topology_saved = ok && trial_archive_topology(trial);
    trial_reply(ok, !ok ? "save failed; keep power and dump" : trial_result_saved ?
        (trial_topology_saved ? "saved to NVS, log and topology; safe to unplug" :
         "saved to NVS and log; topology unavailable; keep power and retry topo RUN") :
        "saved to NVS; log unavailable; use dump");
}

static void trial_begin()
{
    if (!trial_storage_ok || trial.state != READY) { trial_reply(false, "prepare a new run first"); return; }
    trial.state = RUNNING;
    if (!trial_save()) { trial.state = INTERRUPTED; trial_reply(false, "start save failed"); return; }
    mlan->cache_cnt = mlan->hash_cnt = 0;
    while (uint8_t *pkt = ble_if->from_brcast->dequeue()) free(pkt);
    memset(ble_if->neigh_tab, 0, sizeof(ble_if->neigh_tab));
    trial_rx0 = ble_if->rx_cnt; trial_tx0 = ble_if->tx_cnt;
    trial_drop0 = ble_if->rx_drop.load(); trial_bad0 = ble_if->rx_bad.load();
    trial_fail0 = ble_if->tx_fail.load();
    trial_start_ms = trial_next_ms = trial_peer_ms = millis();
    memset(&trial_topology, 0, sizeof(trial_topology));
    trial_topology.run = trial.run;
    trial_result_saved = trial_topology_saved = false;
    ble_if->run_id = trial.run;
    trial_reply(true, "running");
}

static void trial_start_button(bool pressed)
{
    if (pressed && trial.state == READY) trial_begin();
}

static void trial_dump()
{
    if (!Serial) return;
    if (trial.state == RUNNING) { trial_stats(); trial_peers(); }
    Serial.print("@MLAN ");
    trial_json(Serial, trial, trial_storage_ok, (uint16_t(my_id[0]) << 8) | my_id[1], hardware_mac);
}

static void trial_command(const char *line)
{
    uint16_t node = (uint16_t(my_id[0]) << 8) | my_id[1];
    if (!strcmp(line, "id")) {
        char address[13]; mac_text(hardware_mac, address);
        Serial.printf("@MLAN {\"mac\":\"%s\",\"ok\":true,\"node\":\"%04x\",\"files_ok\":%s,\"label\":%u}\n",
            address, node, trial_files_ok ? "true" : "false", trial_label);
        return;
    }
    if (!strncmp(line, "label ", 6)) {
        if (trial.state == RUNNING) { trial_reply(false, "stop before changing label"); return; }
        if (!trial_config_open) { trial_reply(false, "label storage unavailable"); return; }
        if (line[6] < '1' || line[6] > '5' || line[7]) { trial_reply(false, "use label 1 through 5"); return; }
        uint8_t label = line[6] - '0';
        bool ok = trial_config.putUChar("label", label) == 1 && trial_config.getUChar("label", 0) == label;
        if (ok) trial_label = label;
        trial_reply(ok, ok ? "label saved" : "label save failed");
        return;
    }
    if (!strcmp(line, "files") || !strncmp(line, "dump ", 5) || !strncmp(line, "topo ", 5) || !strcmp(line, "fsinit")) {
        if (trial.state == RUNNING) { trial_reply(false, "stop before file access"); return; }
        if (!strcmp(line, "files")) { trial_files_list(node); return; }
        if (!strcmp(line, "fsinit")) {
            if (!trial_store_open || !trial_storage_ok || trial_store.isKey("run") ||
                trial.state != EMPTY || trial_files_ok) {
                trial_reply(false, "requires healthy empty NVS and unmounted LittleFS"); return;
            }
            trial_files_ok = LittleFS.format() && LittleFS.begin(false);
            trial_reply(trial_files_ok, trial_files_ok ? "LittleFS initialised" : "LittleFS initialisation failed");
            return;
        }
        const char *p = line + 5; char *end;
        errno = 0;
        unsigned long run = strtoul(p, &end, 10);
        if (*p < '0' || *p > '9' || errno || !run || run > UINT32_MAX || *end) {
            trial_reply(false, "use dump RUN or topo RUN with a positive decimal run number"); return;
        }
        bool topology = !strncmp(line, "topo ", 5);
        if (topology && run == trial.run) {
            trial_topology_saved = trial_archive_topology(trial) || trial_saved_file(true);
            trial_result_saved = trial_saved_file(false);
        }
        trial_file_dump(node, uint32_t(run), topology); return;
    }
    if (!strcmp(line, "start")) { trial_begin(); return; }
    if (!strcmp(line, "stop")) { trial_stop(1); return; }
    if (!strcmp(line, "dump")) { trial_dump(); return; }
    if (!strcmp(line, "clear")) {
        if (trial.state == RUNNING) { trial_reply(false, "stop and export first"); return; }
        if (!trial_store_open) { trial_reply(false, "NVS unavailable"); return; }
        bool ok = !trial_store.isKey("run") || trial_store.remove("run");
        if (ok) {
            memset(&trial, 0, sizeof(trial));
            memset(&trial_topology, 0, sizeof(trial_topology));
            trial_result_saved = trial_topology_saved = false;
            ble_if->run_id = 0; trial_storage_ok = true;
        }
        trial_reply(ok, ok ? "cleared" : "clear failed"); return;
    }
    uint32_t fields[6];
    if (trial_parse(line, fields)) {
        uint32_t run = fields[0], source = fields[1], total = fields[2], gap = fields[3], seconds = fields[4];
        if (!trial_storage_ok || trial.state != EMPTY) { trial_reply(false, "export then clear the existing record"); return; }
        if (!run || source > 65535 || !total || total > TRIAL_MAX || gap < 100 || gap > 60000 ||
            seconds < 1 || seconds > 3600 || !trial_size_valid(fields[5]) ||
            uint64_t(total) * gap >= uint64_t(seconds) * 1000) {
            trial_reply(false, "invalid run, source, count, interval or duration"); return;
        }
        char path[40]; trial_log_path(path, sizeof(path), node, run);
        if (trial_files_ok && trial_run_exists(node, run)) { trial_reply(false, "run number already archived; choose a new one"); return; }
        memset(&trial, 0, sizeof(trial));
        trial.version = 2; trial.payload_bytes = fields[5];
        trial.run = run; trial.source = source; trial.total = total;
        trial.node = (uint16_t(my_id[0]) << 8) | my_id[1];
        trial.gap_ms = gap; trial.limit_ms = seconds * 1000; trial.state = READY;
        #define MLAN_STR_(x) #x
        #define MLAN_STR(x) MLAN_STR_(x)
        snprintf(trial.build, sizeof(trial.build), "%s %s", MLAN_STR(MLAN_REV), COMPILE_DATE);
        bool ok = trial_save();
        if (ok) ble_if->run_id = trial.run;
        trial_reply(ok, ok ? "prepared; move to final power, then hold BOOT briefly" : "prepare save failed"); return;
    }
    trial_reply(false, "commands: prep RUN SOURCE_HEX COUNT GAP_MS LIMIT_SEC [BYTES]; start; stop; dump [RUN]; topo RUN; label N; clear; id; files; fsinit");
}

static void trial_setup()
{
    trial_label = 0;
    trial_config_open = trial_config.begin("mlan_cfg", false);
    if (trial_config_open) {
        uint8_t label = trial_config.getUChar("label", 0);
        if (label >= 1 && label <= 5) trial_label = label;
    }
    trial_result_saved = trial_topology_saved = false;
    trial_led_shown = UINT32_MAX;
    mlan->set_update_cb(trial_update);
    ble_if->observe = trial_observe;
    ble_if->run_id = 0;
    memset(&trial_topology, 0, sizeof(trial_topology));
    trial_files_ok = LittleFS.begin(false);
    trial_store_open = trial_storage_ok = trial_store.begin("mlan_lab", false);
    if (!trial_storage_ok) { trial_reply(false, "NVS unavailable"); return; }
    if (trial_store.isKey("run")) {
        TrialRecord saved = {};
        if (trial_store.getBytesLength("run") != sizeof(saved) ||
            trial_store.getBytes("run", &saved, sizeof(saved)) != sizeof(saved) || !trial_valid(saved)) {
            trial_storage_ok = false;
            trial_reply(false, "invalid saved record; preserved until explicit clear"); return;
        }
        trial = saved;
        ble_if->run_id = trial.run;
        if (trial.state == RUNNING) { trial.state = INTERRUPTED; trial_save(); }
        if (trial_storage_ok && (trial.state == DONE || trial.state == INTERRUPTED)) {
            char legacy[40];
            trial_log_path(legacy, sizeof(legacy), trial.node, trial.run, false, true);
            if (!LittleFS.exists(legacy)) trial_archive(trial);
        }
        if (trial.state == DONE) {
            trial_result_saved = trial_saved_file(false);
            trial_topology_saved = trial_saved_file(true);
        }
    }
    trial_reply(true, "trial firmware; use dump for status");
}

static void trial_loop()
{
    static char line[96];
    static unsigned used;
    static bool overflow;
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\r') continue;
        if (c == '\n') {
            line[used] = 0;
            if (overflow) trial_reply(false, "command too long");
            else if (used) trial_command(line);
            used = 0; overflow = false;
        } else if (used < sizeof(line)-1) line[used++] = c;
        else overflow = true;
    }
    if (trial.state != RUNNING) return;
    uint32_t now = millis();
    if (now - trial_start_ms >= trial.limit_ms) { trial_stop(2); return; }
    if (trial.node == trial.source && trial.sent < trial.total && int32_t(now - trial_next_ms) >= 0) {
        uint8_t pkt[TRIAL_MAX_BYTES + 1];
        trial_page(pkt, trial, trial.sent);
        if (mlan->add(pkt)) trial.sent++;
        trial_next_ms = millis() + trial.gap_ms;
    }
    if (now - trial_peer_ms >= 1000) { trial_peers(); trial_peer_ms = now; }
}
