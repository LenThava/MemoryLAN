#pragma once
#include <cstring>
#include <vector>
#include <map>
#include <string>

class Preferences {
    bool config = false, opened = false;
    std::vector<unsigned char>& value(const char* key)
    { return !strcmp(key, "mode") ? mode_bytes : bytes; }
public:
    inline static std::vector<unsigned char> bytes;
    inline static std::vector<unsigned char> mode_bytes;
    inline static bool fail_begin = false, fail_write = false, fail_read = false;
    inline static bool fail_config_begin = false;
    inline static std::map<std::string, unsigned char> config_values;
    bool begin(const char* name, bool)
    {
        config = !strcmp(name, "mlan_cfg");
        opened = !fail_begin && !(config && fail_config_begin);
        return opened;
    }
    bool isKey(const char* key) { return config ? config_values.count(key) : !value(key).empty(); }
    uint8_t getUChar(const char* key, uint8_t fallback = 0)
    {
        if (!opened || !config || fail_read || !config_values.count(key)) return fallback;
        return config_values.at(key);
    }
    size_t putUChar(const char* key, uint8_t value)
    {
        if (!opened || !config || fail_write) return 0;
        config_values[key] = value;
        return 1;
    }
    size_t getBytesLength(const char* key) { return value(key).size(); }
    size_t putBytes(const char* key, const void* data, size_t size)
    {
        if (fail_write) return 0;
        const auto* p = static_cast<const unsigned char*>(data);
        value(key).assign(p, p + size);
        return size;
    }
    size_t getBytes(const char* key, void* data, size_t size)
    {
        const auto& data_bytes = value(key);
        if (fail_read || data_bytes.size() > size) return 0;
        if (!data_bytes.empty()) memcpy(data, data_bytes.data(), data_bytes.size());
        return data_bytes.size();
    }
    bool remove(const char* key)
    {
        if (config) config_values.erase(key);
        else value(key).clear();
        return true;
    }
    static void reset()
    {
        bytes.clear();
        mode_bytes.clear();
        config_values.clear();
        fail_begin = fail_write = fail_read = false;
        fail_config_begin = false;
    }
};
