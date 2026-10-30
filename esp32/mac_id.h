#pragma once
#include <stdint.h>
#include <stdio.h>

inline void mac_text(const uint8_t mac[6], char text[13])
{
    snprintf(text, 13, "%02x%02x%02x%02x%02x%02x",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}
