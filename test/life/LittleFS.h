#pragma once
#include <algorithm>
#include <cstring>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <vector>

struct LittleFSState {
    using Bytes = std::vector<uint8_t>;
    inline static std::map<std::string, std::shared_ptr<Bytes>> files;
    inline static bool mounted, fail_begin, fail_format, fail_open, fail_write;
    inline static bool fail_read, fail_seek, fail_rename;
    inline static size_t write_limit = std::numeric_limits<size_t>::max();
    inline static size_t read_calls, fail_read_after = std::numeric_limits<size_t>::max();
    inline static unsigned format_calls, write_calls, rename_calls;
    inline static std::vector<bool> begin_arguments;
};

class File {
    std::shared_ptr<LittleFSState::Bytes> bytes;
    std::string path;
    std::vector<std::string> children;
    size_t position = 0, child = 0;
    bool directory = false, opened = false, writable = false;
public:
    File() = default;
    File(const std::string &name, std::shared_ptr<LittleFSState::Bytes> data, bool write = false)
        : bytes(data), path(name), opened(true), writable(write) {}
    explicit File(const std::vector<std::string> &names)
        : path("/"), children(names), directory(true), opened(true) {}
    explicit operator bool() const { return opened; }
    size_t size() const { return opened && bytes ? bytes->size() : 0; }
    const char *name() const { return path.c_str(); }
    bool seek(size_t offset)
    {
        if (!opened || !bytes || LittleFSState::fail_seek || offset > bytes->size()) return false;
        position = offset;
        return true;
    }
    size_t read(uint8_t *out, size_t count)
    {
        if (!opened || !bytes || LittleFSState::fail_read ||
                LittleFSState::read_calls++ >= LittleFSState::fail_read_after) return 0;
        count = std::min(count, bytes->size() - position);
        if (count) memcpy(out, bytes->data() + position, count);
        position += count;
        return count;
    }
    size_t write(const uint8_t *data, size_t count)
    {
        LittleFSState::write_calls++;
        if (!opened || !bytes || !writable || LittleFSState::fail_write) return 0;
        count = std::min(count, LittleFSState::write_limit);
        if (position + count > bytes->size()) bytes->resize(position + count);
        if (count) memcpy(bytes->data() + position, data, count);
        position += count;
        return count;
    }
    int available() const { return opened && bytes ? int(bytes->size() - position) : 0; }
    void flush() {}
    void close() { opened = false; }
    File openNextFile()
    {
        if (!opened || !directory) return {};
        while (child < children.size()) {
            std::string name = children[child++];
            auto found = LittleFSState::files.find(name);
            if (found != LittleFSState::files.end()) return File(name, found->second);
        }
        return {};
    }
};

class LittleFSClass : public LittleFSState {
public:
    bool begin(bool format_on_fail)
    {
        begin_arguments.push_back(format_on_fail);
        mounted = !fail_begin;
        return mounted;
    }
    bool format()
    {
        format_calls++;
        if (fail_format) return false;
        files.clear();
        return true;
    }
    File open(const char *name, const char *mode = "r")
    {
        if (!mounted || fail_open) return {};
        std::string path(name);
        if (path == "/") {
            std::vector<std::string> names;
            for (const auto &file : files) names.push_back(file.first);
            return File(names);
        }
        if (!strcmp(mode, "w")) {
            auto bytes = std::make_shared<Bytes>();
            files[path] = bytes;
            return File(path, bytes, true);
        }
        auto found = files.find(path);
        return found == files.end() ? File() : File(path, found->second);
    }
    bool exists(const char *path) const { return mounted && files.count(path); }
    bool rename(const char *from, const char *to)
    {
        rename_calls++;
        if (!mounted || fail_rename || files.count(to)) return false;
        auto found = files.find(from);
        if (found == files.end()) return false;
        files[to] = found->second;
        files.erase(found);
        return true;
    }
    static void put(const std::string &name, const std::string &value)
    {
        files[name] = std::make_shared<Bytes>(value.begin(), value.end());
    }
    static std::string contents(const std::string &name)
    {
        const auto &bytes = *files.at(name);
        return std::string(bytes.begin(), bytes.end());
    }
    static void reset()
    {
        files.clear();
        mounted = fail_begin = fail_format = fail_open = fail_write = false;
        fail_read = fail_seek = fail_rename = false;
        write_limit = std::numeric_limits<size_t>::max();
        read_calls = 0; fail_read_after = std::numeric_limits<size_t>::max();
        format_calls = write_calls = rename_calls = 0;
        begin_arguments.clear();
    }
};

inline LittleFSClass LittleFS;
