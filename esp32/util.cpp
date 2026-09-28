// util.cpp

#include <cstring>
#include <cstdio>
#include <stdint.h>
#include <arpa/inet.h>

#include "util.h"


// ---------------------------------------------------------------------------

uint32_t crc32_ieee(uint8_t *pkt, int len) { // Ethernet/ZIP polynomial
  uint32_t crc = 0xffffffffu;
  while (len-- > 0) {
    crc ^= *pkt++;
    for (int i = 0; i < 8; i++)
      crc = crc & 1 ? (crc >> 1) ^ 0xEDB88320u : crc >> 1;
  }
  return htonl(crc ^ 0xffffffffu);
}

int crc_check(uint8_t *pkt, int len) // returns 0 if OK
{
  uint32_t crc = crc32_ieee(pkt, len-sizeof(crc));
  return memcmp(pkt+len-sizeof(crc), (void*)&crc, sizeof(crc));
}

// ---------------------------------------------------------------------------

static inline uint32_t murmur_32_scramble(uint32_t k) {
    k *= 0xcc9e2d51;
    k = (k << 15) | (k >> 17);
    k *= 0x1b873593;
    return k;
}

uint32_t murmur3_32(const uint8_t* key, size_t len, uint32_t seed)
{
    uint32_t h = seed;
    uint32_t k;

    for (size_t i = len >> 2; i; i--) {
        memcpy(&k, key, sizeof(uint32_t));
        key += sizeof(uint32_t);
        h ^= murmur_32_scramble(k);
        h = (h << 13) | (h >> 19);
        h = h * 5 + 0xe6546b64;
    }

    k = 0;
    for (size_t i = len & 3; i; i--) {
        k <<= 8;
        k |= key[i - 1];
    }
    h ^= murmur_32_scramble(k);

    h ^= len;
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}

// ---------------------------------------------------------------------------

void bf_init(uint8_t *vect, int bit_cnt)
{
    memset(vect, 0, (bit_cnt + 7) / 8);
}

void bf_add(uint8_t *vect, int bit_cnt, int num_hash, int seed,
            uint8_t *item, int item_len)
{
    for (int i = 0; i < num_hash; i++) {
        uint32_t pos = murmur3_32(item, item_len, seed + i) % bit_cnt;
        vect[pos / 8] |= 1 << (pos % 8);
    }
}

bool bf_absent(uint8_t *vect, int bit_cnt, int num_hash, int seed,
               uint8_t *item, int item_len)
{
    for (int i = 0; i < num_hash; i++) {
        uint32_t pos = murmur3_32(item, item_len, seed + i) % bit_cnt;
        if ( !((vect[pos / 8] >> (pos % 8)) & 0x01) )
            return true;
    }
    return false;
}

// ---------------------------------------------------------------------------

char* to_hex(uint8_t *ptr, int len)
{
    static char buf[100];
    char *cp = buf;
    *cp = '\0';

    for (int i = 0; i < len && i < sizeof(buf)/2; i++) {
        sprintf(cp, "%02x", *ptr++);
        cp += 2;
    }
    return buf;
}

// eof
