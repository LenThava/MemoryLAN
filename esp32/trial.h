#pragma once
#include <Preferences.h>
#include "trial_data.h"

static Preferences trial_store;
static TrialRecord trial;
static bool trial_storage_ok;
static bool trial_store_open;
static uint32_t trial_start_ms, trial_next_ms, trial_peer_ms;
static uint32_t trial_rx0, trial_tx0, trial_drop0, trial_bad0, trial_fail0;

static void trial_led()
{
#if defined(WAVESHARE_S3ZERO)
    static int shown = -1;
    int state = trial_storage_ok ? trial.state : INTERRUPTED;
    if (state == shown) return;
    shown = state;
    switch (state) {
    case READY: rgbLedWrite(RGB_BUILTIN, 12, 6, 0); break;
    case RUNNING: rgbLedWrite(RGB_BUILTIN, 0, 0, 12); break;
    case DONE: rgbLedWrite(RGB_BUILTIN, 0, 12, 0); break;
    case INTERRUPTED: rgbLedWrite(RGB_BUILTIN, 12, 0, 0); break;
    default: rgbLedWrite(RGB_BUILTIN, 0, 0, 0);
    }
#endif
}

static void trial_reply(bool ok, const char *message)
{
    if (Serial) Serial.printf("@MLAN {\"ok\":%s,\"message\":\"%s\"}\n", ok ? "true" : "false", message);
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
    trial_reply(ok, ok ? "saved; safe to unplug" : "save failed; keep power and dump");
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
    const char *states[] = {"empty", "ready", "running", "done", "interrupted"};
    Serial.printf("@MLAN {\"schema\":1,\"kind\":\"mlan-run\",\"state\":\"%s\",\"storage_ok\":%s,",
        states[trial.state], trial_storage_ok ? "true" : "false");
    Serial.printf("\"node\":\"%04x\",\"source\":\"%04x\",\"run\":%u,\"total\":%u,\"sent\":%u,",
        (uint16_t(my_id[0]) << 8) | my_id[1], trial.source, trial.run, trial.total, trial.sent);
    Serial.printf("\"gap_ms\":%u,\"limit_ms\":%u,\"elapsed_ms\":%u,\"stop_reason\":%u,\"cutoff\":\"local\",",
        trial.gap_ms, trial.limit_ms, trial.elapsed_ms, trial.stop_reason);
    Serial.printf("\"build\":\"%s\",\"duplicates\":%u,\"invalid\":%u,\"rx\":%u,\"tx\":%u,",
        trial.build, trial.duplicates, trial.invalid, trial.rx, trial.tx);
    Serial.printf("\"rx_drop\":%u,\"rx_bad\":%u,\"tx_fail\":%u,\"received\":[",
        trial.rx_drop, trial.rx_bad, trial.tx_fail);
    bool comma = false;
    for (unsigned i = 0; i < trial.total; i++) if (trial_seen(trial, i)) {
        Serial.printf("%s%u", comma ? "," : "", i); comma = true;
    }
    Serial.print("],\"peers\":["); comma = false;
    for (const auto &p : trial.peers) if (p.id) {
        Serial.printf("%s{\"id\":\"%04x\",\"rssi\":%d,\"samples\":%u,\"last_ms\":%u}",
            comma ? "," : "", p.id, p.rssi, p.samples, p.last_ms);
        comma = true;
    }
    Serial.println("]}");
}

static void trial_command(const char *line)
{
    if (!strcmp(line, "start")) { trial_begin(); return; }
    if (!strcmp(line, "stop")) { trial_stop(1); return; }
    if (!strcmp(line, "dump")) { trial_dump(); return; }
    if (!strcmp(line, "clear")) {
        if (trial.state == RUNNING) { trial_reply(false, "stop and export first"); return; }
        if (!trial_store_open) { trial_reply(false, "NVS unavailable"); return; }
        bool ok = !trial_store.isKey("run") || trial_store.remove("run");
        if (ok) { memset(&trial, 0, sizeof(trial)); trial_storage_ok = true; }
        trial_reply(ok, ok ? "cleared" : "clear failed"); return;
    }
    uint32_t fields[5];
    if (trial_parse(line, fields)) {
        uint32_t run = fields[0], source = fields[1], total = fields[2], gap = fields[3], seconds = fields[4];
        if (!trial_storage_ok || trial.state != EMPTY) { trial_reply(false, "export then clear the existing record"); return; }
        if (!run || source > 65535 || !total || total > TRIAL_MAX || gap < 100 || gap > 60000 ||
            seconds < 1 || seconds > 3600 || uint64_t(total) * gap >= uint64_t(seconds) * 1000) {
            trial_reply(false, "invalid run, source, count, interval or duration"); return;
        }
        memset(&trial, 0, sizeof(trial));
        trial.version = 1; trial.run = run; trial.source = source; trial.total = total;
        trial.node = (uint16_t(my_id[0]) << 8) | my_id[1];
        trial.gap_ms = gap; trial.limit_ms = seconds * 1000; trial.state = READY;
        #define MLAN_STR_(x) #x
        #define MLAN_STR(x) MLAN_STR_(x)
        snprintf(trial.build, sizeof(trial.build), "%s %s", MLAN_STR(MLAN_REV), COMPILE_DATE);
        bool ok = trial_save();
        trial_reply(ok, ok ? "prepared; move to final power, then hold BOOT briefly" : "prepare save failed"); return;
    }
    trial_reply(false, "commands: prep RUN SOURCE_HEX COUNT GAP_MS LIMIT_SEC; start; stop; dump; clear");
}

static void trial_setup()
{
    mlan->set_update_cb(trial_update);
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
        if (trial.state == RUNNING) { trial.state = INTERRUPTED; trial_save(); }
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
        uint8_t pkt[TRIAL_BYTES + 1];
        trial_page(pkt, trial, trial.sent);
        if (mlan->add(pkt)) trial.sent++;
        trial_next_ms = millis() + trial.gap_ms;
    }
    if (now - trial_peer_ms >= 1000) { trial_peers(); trial_peer_ms = now; }
}
