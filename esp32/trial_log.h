#pragma once
#include <LittleFS.h>
#include <stdarg.h>
#include "trial_json.h"

static bool trial_files_ok;
static const unsigned TRIAL_LOG_BYTES = 4096;

static uint32_t trial_crc32(const uint8_t *data, size_t len, uint32_t crc = 0xffffffffu)
{
    while (len--) {
        crc ^= *data++;
        for (unsigned i = 0; i < 8; i++) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1)));
    }
    return crc;
}

struct TrialText {
    char data[TRIAL_LOG_BYTES];
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

static bool trial_file_info(File &file, size_t &bytes, uint32_t &crc)
{
    bytes = file.size();
    if (!file || !bytes || bytes >= TRIAL_LOG_BYTES || !file.seek(0)) return false;
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

static void trial_log_path(char *path, size_t capacity, uint16_t node, uint32_t run)
{
    snprintf(path, capacity, "/%04x-%lu.log", node, (unsigned long)run);
}

static bool trial_archive(const TrialRecord &r)
{
    if (!trial_files_ok || !trial_valid(r) || (r.state != DONE && r.state != INTERRUPTED)) return false;
    static TrialText text;
    text.used = 0; text.ok = true;
    trial_json(text, r, true, r.node);
    if (!text.ok) return false;
    uint32_t expected = trial_crc32((const uint8_t*)text.data, text.used) ^ 0xffffffffu;
    char path[40], temp[44];
    trial_log_path(path, sizeof(path), r.node, r.run);
    if (LittleFS.exists(path)) {
        File existing = LittleFS.open(path, "r");
        size_t bytes; uint32_t crc;
        return trial_file_info(existing, bytes, crc) && bytes == text.used && crc == expected;
    }
    snprintf(temp, sizeof(temp), "%s.tmp", path);
    File file = LittleFS.open(temp, "w");
    if (!file || file.write((const uint8_t*)text.data, text.used) != text.used) return false;
    file.flush(); file.close();
    file = LittleFS.open(temp, "r");
    size_t bytes; uint32_t crc;
    bool ok = trial_file_info(file, bytes, crc) && bytes == text.used && crc == expected;
    file.close();
    return ok && LittleFS.rename(temp, path);
}

static void trial_files_list(uint16_t node)
{
    if (!trial_files_ok) { trial_reply(false, "LittleFS unavailable; export NVS with dump first"); return; }
    File root = LittleFS.open("/");
    if (!root) { trial_reply(false, "cannot list LittleFS"); return; }
    Serial.printf("@MLAN {\"ok\":true,\"kind\":\"mlan-files\",\"node\":\"%04x\",\"files\":[", node);
    bool comma = false;
    for (File file = root.openNextFile(); file; file = root.openNextFile()) {
        unsigned id; unsigned long run; int used = 0;
        const char *name = file.name();
        if (*name == '/') name++;
        if (sscanf(name, "%4x-%lu.log%n", &id, &run, &used) != 2 || name[used] || id != node || !run || run > UINT32_MAX)
            continue;
        size_t bytes; uint32_t crc;
        if (!trial_file_info(file, bytes, crc)) continue;
        Serial.printf("%s{\"run\":%lu,\"name\":\"%s\",\"bytes\":%u,\"crc32\":\"%08lx\"}",
            comma ? "," : "", run, name, unsigned(bytes), (unsigned long)crc);
        comma = true;
    }
    Serial.print("]}\n");
}

static char trial_uu_char(unsigned n) { n &= 63; return n ? char(n + 32) : '`'; }

static void trial_file_dump(uint16_t node, uint32_t run)
{
    if (!trial_files_ok) { trial_reply(false, "LittleFS unavailable"); return; }
    char path[40]; trial_log_path(path, sizeof(path), node, run);
    File file = LittleFS.open(path, "r");
    size_t bytes; uint32_t crc;
    if (!trial_file_info(file, bytes, crc)) { trial_reply(false, "log missing or unreadable"); return; }
    Serial.printf("@MLAN {\"ok\":true,\"kind\":\"mlan-file\",\"node\":\"%04x\",\"run\":%lu,\"name\":\"%s\",\"bytes\":%u,\"crc32\":\"%08lx\"}\n",
        node, (unsigned long)run, path + 1, unsigned(bytes), (unsigned long)crc);
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
