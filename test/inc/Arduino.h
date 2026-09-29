#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern unsigned long test_millis;
extern uint32_t test_random;
inline unsigned long millis() { return test_millis; }
inline uint32_t esp_random() { return test_random; }
inline void delay(unsigned long) {}
struct SerialStub {
    unsigned writes = 0;
    explicit operator bool() const { return true; }
    template<class... Args> void printf(const char*, Args...) { ++writes; }
};
extern SerialStub Serial;
