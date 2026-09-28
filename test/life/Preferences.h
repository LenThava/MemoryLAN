#pragma once
#include <cstring>
#include <vector>

class Preferences {
public:
    inline static std::vector<unsigned char> bytes;
    inline static bool fail_begin = false, fail_write = false, fail_read = false;
    bool begin(const char*, bool) { return !fail_begin; }
    bool isKey(const char*) { return !bytes.empty(); }
    size_t getBytesLength(const char*) { return bytes.size(); }
    size_t putBytes(const char*, const void* data, size_t size)
    {
        if (fail_write) return 0;
        const auto* p = static_cast<const unsigned char*>(data);
        bytes.assign(p, p + size);
        return size;
    }
    size_t getBytes(const char*, void* data, size_t size)
    {
        if (fail_read || bytes.size() > size) return 0;
        memcpy(data, bytes.data(), bytes.size());
        return bytes.size();
    }
    bool remove(const char*) { bytes.clear(); return true; }
    static void reset()
    {
        bytes.clear();
        fail_begin = fail_write = fail_read = false;
    }
};
