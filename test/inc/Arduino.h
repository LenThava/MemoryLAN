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
    explicit operator bool() const { return false; }
    template<class... Args> void printf(const char*, Args...) {}
};
extern SerialStub Serial;
