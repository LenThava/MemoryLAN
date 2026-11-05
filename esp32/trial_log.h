#pragma once
#include <LittleFS.h>
#include <stdarg.h>
#include "trial_json.h"

static bool trial_files_ok;
static const unsigned TRIAL_LOG_BYTES = 4096;
static const unsigned TRIAL_FILE_BYTES = 65536;

static uint32_t trial_crc32(const uint8_t *data, size_t len, uint32_t crc = 0xffffffffu)
{
    while (len--) {
        crc ^= *data++;
        for (unsigned i = 0; i < 8; i++) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1)));
    }
    return crc;
}

template<size_t Capacity>
struct TrialBuffer {
    char data[Capacity];
    size_t used = 0;
    bool ok = true;
    void printf(const char *fmt, ...)
    {
        if (!ok) return;
        va_list args;
        va_start(args, fmt);
        int n = vsnprintf(data + used, sizeof(data) - used, fmt, args);
        va_end(args);
        if (n < 0 || size_t(n) >= sizeof(data) - used) { ok = false; return; }
        used += n;
    }
    void print(const char *s) { printf("%s", s); }
};
using TrialText = TrialBuffer<TRIAL_LOG_BYTES>;

static bool trial_file_info(File &file, size_t &bytes, uint32_t &crc)
{
    bytes = file.size();
    if (!file || !bytes || bytes > TRIAL_FILE_BYTES || !file.seek(0)) return false;
    uint8_t chunk[128];
    crc = 0xffffffffu;
    size_t count = 0;
    while (count < bytes) {
        size_t n = file.read(chunk, sizeof(chunk));
        if (!n) return false;
        count += n;
        crc = trial_crc32(chunk, n, crc);
    }
    crc ^= 0xffffffffu;
    return count == bytes && file.seek(0);
}

static void trial_log_path(char *path, size_t capacity, uint16_t node, uint32_t run,
                           bool topology = false, bool legacy = false)
{
    if (legacy) snprintf(path, capacity, "/%04x-%lu.log", node, (unsigned long)run);
    else {
        char address[13]; mac_text(hardware_mac, address);
        snprintf(path, capacity, "/%s-%lu.%s", address, (unsigned long)run, topology ? "top" : "log");
    }
}

static bool trial_run_exists(uint16_t node, uint32_t run)
{
    char path[40];
    trial_log_path(path, sizeof(path), node, run);
    if (LittleFS.exists(path)) return true;
    trial_log_path(path, sizeof(path), node, run, true);
    if (LittleFS.exists(path)) return true;
    trial_log_path(path, sizeof(path), node, run, false, true);
    return LittleFS.exists(path);
}

static bool trial_write_archive(const char *path, const char *data, size_t length)
{
    uint32_t expected = trial_crc32((const uint8_t*)data, length) ^ 0xffffffffu;
    if (LittleFS.exists(path)) {
        File existing = LittleFS.open(path, "r");
        size_t bytes; uint32_t crc;
        return trial_file_info(existing, bytes, crc) && bytes == length && crc == expected;
    }
    char temp[44];
    snprintf(temp, sizeof(temp), "%s.tmp", path);
    File file = LittleFS.open(temp, "w");
    if (!file || file.write((const uint8_t*)data, length) != length) return false;
    file.flush(); file.close();
    file = LittleFS.open(temp, "r");
    size_t bytes; uint32_t crc;
    bool ok = trial_file_info(file, bytes, crc) && bytes == length && crc == expected;
    file.close();
    return ok && LittleFS.rename(temp, path);
}

static bool trial_archive(const TrialRecord &r)
{
    if (!trial_files_ok || !trial_valid(r) || (r.state != DONE && r.state != INTERRUPTED)) return false;
    static TrialText text;
    text.used = 0; text.ok = true;
    trial_json(text, r, true, r.node, hardware_mac, r.version >= 3 ? &trial_mode : nullptr);
    if (!text.ok) return false;
    char path[40]; trial_log_path(path, sizeof(path), r.node, r.run);
    return trial_write_archive(path, text.data, text.used);
}

static bool trial_archive_topology(const TrialRecord &r)
{
    if (!trial_files_ok || !trial_valid(r) || r.state != DONE || trial_topology.run != r.run) return false;
    static TrialBuffer<TRIAL_FILE_BYTES> text;
    text.used = 0; text.ok = true;
    trial_topology_json(text, r, trial_topology, hardware_mac, r.version >= 3 ? &trial_mode : nullptr);
    if (!text.ok) return false;
    char path[40]; trial_log_path(path, sizeof(path), r.node, r.run, true);
    return trial_write_archive(path, text.data, text.used);
}

static bool trial_file_name(const char *name, uint16_t node, uint32_t &run, bool &topology, bool &legacy)
{
    char address[13], alias[5]; mac_text(hardware_mac, address);
    snprintf(alias, sizeof(alias), "%04x", node);
    const char *dash = strchr(name, '-');
    if (!dash) return false;
    size_t prefix = dash - name;
    legacy = prefix == 4 && !strncmp(name, alias, 4);
    if (!legacy && !(prefix == 12 && !strncmp(name, address, 12))) return false;
    const char *p = dash + 1;
    if (*p < '1' || *p > '9') return false;
    char *end; errno = 0;
    unsigned long value = strtoul(p, &end, 10);
    if (errno || !value || value > UINT32_MAX) return false;
    topology = !strcmp(end, ".top");
    if (strcmp(end, ".log") && !(topology && !legacy)) return false;
    run = value;
    return true;
}

static void trial_files_list(uint16_t node)
{
    if (!trial_files_ok) { trial_reply(false, "LittleFS unavailable; export NVS with dump first"); return; }
    File root = LittleFS.open("/");
    if (!root) { trial_reply(false, "cannot list LittleFS"); return; }
    char address[13]; mac_text(hardware_mac, address);
    Serial.printf("@MLAN {\"mac\":\"%s\",\"ok\":true,\"kind\":\"mlan-files\",\"node\":\"%04x\",\"files\":[", address, node);
    bool comma = false;
    for (File file = root.openNextFile(); file; file = root.openNextFile()) {
        uint32_t run; bool topology, legacy;
        const char *name = file.name();
        if (*name == '/') name++;
        if (!trial_file_name(name, node, run, topology, legacy)) continue;
        size_t bytes; uint32_t crc;
        if (!trial_file_info(file, bytes, crc)) continue;
        Serial.printf("%s{", comma ? "," : "");
        if (!legacy) Serial.printf("\"mac\":\"%s\",", address);
        Serial.printf("\"run\":%lu,\"name\":\"%s\",\"type\":\"%s\",\"bytes\":%u,\"crc32\":\"%08lx\"}",
            (unsigned long)run, name, topology ? "topology" : "result", unsigned(bytes), (unsigned long)crc);
        comma = true;
    }
    Serial.print("]}\n");
}

static char trial_uu_char(unsigned n) { n &= 63; return n ? char(n + 32) : '`'; }

static void trial_file_dump(uint16_t node, uint32_t run, bool topology = false)
{
    if (!trial_files_ok) { trial_reply(false, "LittleFS unavailable"); return; }
    char path[40]; trial_log_path(path, sizeof(path), node, run, topology);
    bool legacy = !topology && !LittleFS.exists(path);
    if (legacy) trial_log_path(path, sizeof(path), node, run, false, true);
    File file = LittleFS.open(path, "r");
    size_t bytes; uint32_t crc;
    if (!trial_file_info(file, bytes, crc)) { trial_reply(false, "log missing or unreadable"); return; }
    Serial.print("@MLAN {");
    char address[13]; mac_text(hardware_mac, address);
    if (!legacy) Serial.printf("\"mac\":\"%s\",", address);
    Serial.printf("\"ok\":true,\"kind\":\"mlan-file\",\"node\":\"%04x\",\"run\":%lu,\"name\":\"%s\",\"type\":\"%s\",\"bytes\":%u,\"crc32\":\"%08lx\"}\n",
        node, (unsigned long)run, path + 1, topology ? "topology" : "result", unsigned(bytes), (unsigned long)crc);
    Serial.printf("begin 644 %s\n", path + 1);
    uint8_t chunk[45];
    while (file.available()) {
        size_t n = file.read(chunk, sizeof(chunk));
        if (!n) return;
        char line[63]; unsigned pos = 0;
        line[pos++] = trial_uu_char(n);
        for (size_t i = 0; i < n; i += 3) {
            unsigned a = chunk[i], b = i + 1 < n ? chunk[i+1] : 0, c = i + 2 < n ? chunk[i+2] : 0;
            line[pos++] = trial_uu_char(a >> 2);
            line[pos++] = trial_uu_char((a << 4) | (b >> 4));
            line[pos++] = trial_uu_char((b << 2) | (c >> 6));
            line[pos++] = trial_uu_char(c);
        }
        line[pos++] = '\n'; line[pos] = 0; Serial.print(line);
    }
    Serial.print("`\nend\n");
}
