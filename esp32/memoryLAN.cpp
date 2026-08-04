// memoryLAN.cpp

// (c) 2026-08-01 <christian.tschudin@unibas.ch> - MIT license

#include <Arduino.h>
#include <sodium/crypto_hash_sha256.h>

#include "memoryLAN.h"
#include "util.h"

extern void set_led(bool a);

// Bloom filter parameters, for cache size 32:
// #define BLOOM_BIT_COUNT   34*8 // 34 Bytes
// #define BLOOM_HASH_COUNT  6    // optimal for 272/32*ln(2) (32 elements)
#define BLOOM_BIT_COUNT   18*8 // 18 Bytes
#define BLOOM_HASH_COUNT  5    // optimal for 144/20*ln(2) (20 elements)

char *MEMLAN_DMX_STRING = "memoryLAN-v.01";

// ---------------------------------------------------------------------------

MemoryLAN::MemoryLAN(uint8_t *mac, callback_fct *update_cb)
{
    memcpy(my_id, mac, sizeof(my_id));

    update = update_cb;
    
    unsigned char h[crypto_hash_sha256_BYTES];
    crypto_hash_sha256(h, (uint8_t*)MEMLAN_DMX_STRING, strlen(MEMLAN_DMX_STRING));
    memcpy(dmx_hdr, h, 7);
    memcpy(dmx_hdr + 7, my_id, 2);
}

void MemoryLAN::attach_if(IO_device *d)
{
    if (if_cnt >= MEMLAN_MAX_IF) {
        if (Serial) Serial.printf("max # of interfaces exhausted for %s\r\n",
                                  d->name);
        return;
    }
    if_table[if_cnt++] = d;

    if (if_cnt == 1)
        d->next_beacon = millis();
    else { // spread the beacon time (especially BLE and ESPNOW)
        uint64_t base = if_table[0]->next_beacon;
        for (int i = 1; i < if_cnt; i++)
            if_table[i]->next_beacon = base + MEMLAN_NEXT_BEACON / if_cnt;
    }

    if (Serial) Serial.printf("mlan: attached %s interface\r\n", d->name);
}

bool MemoryLAN::loop(void) // return true if some counters changed
{
    bool chg = false;

    for (int i = 0; i < if_cnt; i++) {
        IO_device *d = if_table[i];
        uint8_t *pkt = d->brcast_recv();
        if (pkt) {
            on_recv(d, pkt);
            chg = true;
        }
    }

    uint64_t now = millis();
    for (int i = 0; i < if_cnt; i++)
        if (now > if_table[i]->next_beacon) {
            if_table[i]->next_beacon = now + MEMLAN_NEXT_BEACON - 250 + \
                                                      (esp_random() % 500);
            emit_beacon(if_table[i]);
            chg = true;
        }

    return chg;
}

uint8_t* MemoryLAN::set_update_cb(callback_fct* update_cb)
{
    update = update_cb;
}

int MemoryLAN::get_BFwidth()   // in bits
{
    return BLOOM_BIT_COUNT;
}

int MemoryLAN::get_BFhashes()
{
    return BLOOM_HASH_COUNT;
}

// ---------------------------------------------------------------------------

uint8_t* MemoryLAN::on_mempage(IO_device *iod, uint8_t *pkt)
{

    int len = *pkt;
    if (len > 120) return NULL;

    // add to cache, or move to front if already in cache
    int i;
    for (i = 0; i < cache_cnt; i++) {
        if (cache[i*MEMLAN_CLSZ] != len || memcmp(cache + i*MEMLAN_CLSZ + 1, pkt+1, len))
            continue; // content mismatch
        if (i == (cache_cnt-1)) break;                      // already at front
        memmove(cache + i*MEMLAN_CLSZ, cache + (i+1)*MEMLAN_CLSZ, (cache_cnt - 1 - i) * MEMLAN_CLSZ);
        memcpy(cache + (cache_cnt-1)*MEMLAN_CLSZ, pkt, len+1);
        break;
    }
    if (i == cache_cnt) { // need to add it
        if (cache_cnt >= MEMLAN_CACHE_DEPTH) { // evict oldest entry
            memmove(cache, cache + MEMLAN_CLSZ, (cache_cnt - 1) * MEMLAN_CLSZ);
            cache_cnt--;
        }
        memcpy(cache + cache_cnt*MEMLAN_CLSZ, pkt, len+1);
        cache_cnt++;
    }

    // delivery the new memorypage locally
    if (update) {
        uint8_t *pkt2 = (uint8_t*) malloc(*pkt + 1);
        if (pkt2) {
            memcpy(pkt2, pkt, *pkt + 1);
            update(pkt2);
        }
    }

    // add hash to history, move to front if already in list
    uint32_t h = murmur3_32(pkt+1, len, 9876);
    for (i = 0; i < hash_cnt; i++) {
        if (hashfifo[i] == h) { // known hash val
            unsigned long old_time = timefifo[i];
            // move to front
            for (int j = i; j < (hash_cnt - 1); j++) {
                hashfifo[j] = hashfifo[j+1];
                timefifo[j] = timefifo[j+1];
            }
            // too old: remove it (pretend to not know it), we add it just below
            if (millis() - old_time > MEMLAN_HASH_TIMEOUT) {
                if (Serial) Serial.printf("   retiring hash,");
                hash_cnt--;
                break;
            }
            hashfifo[hash_cnt-1] = h;
            timefifo[hash_cnt-1] = millis();
            if (Serial) Serial.printf("   hash %08x known, not flooding\r\n", h);
            return NULL;
        }
    }
    // unknown hash val: put into fifo, then continue flooding the packet
    if (hash_cnt >= MEMLAN_HHIST_DEPTH) {
        for (int j = 0; j < (hash_cnt - 1); j++) {
            hashfifo[j] = hashfifo[j+1];
            timefifo[j] = timefifo[j+1];
        }
        hashfifo[hash_cnt-1] = h;
        timefifo[hash_cnt-1] = millis();
    } else {
        hashfifo[hash_cnt] = h;
        timefifo[hash_cnt] = millis();
        hash_cnt++;
    }

    if (Serial) Serial.printf("   new cache entry h=%08x, flood it\r\n", h);
    return pkt;
}

uint8_t* MemoryLAN::on_beacon(IO_device *iod, uint8_t *pkt)
{   // return the cache line as a response to the received beacon

    // beacon format:  DMX(7B) | STAT(3B) | filter | SIGN(16B) where
    // - stat is: ID(2B) | #NEIGHB(1B)
    // - filter is: BC|HC|SEED|VECT // bit count (1B), hash count (1B), seed (2B), VECT Bytes
    // - SIGN is HMAC-MD5

    if (Serial) Serial.printf("   on_beacon");

    if (pkt[1+7] || pkt[1+8]) {
        int8_t rssi = *(pkt + 1 + *pkt);
        if (Serial) Serial.printf("  from <%02x:%02x>: n=%d", //  rssi=%hhd",
                                  pkt[8], pkt[9], pkt[10]);   //, rssi);
        iod->neigh_add(pkt+1+7, pkt[1+9], rssi);
    }

    if (cache_cnt == 0) {
        if (Serial) Serial.printf("\r\n");
        return NULL;
    }

    int BC = pkt[1+10], HC=pkt[1+11], VL = *pkt - 14 - 16;
    // FIXME: do sanity checks of BC, HC, VL
    int seed = *(uint16_t*)(pkt+1+12);
    uint8_t *bfilter = pkt + 1 + 14;
    // Serial.printf("%s ..", to_hex(bfilter, 10));

    // pick randomly one of the cache entries which are NOT in the filter
    int offs = esp_random() % cache_cnt;
    for (int i = 0; i < cache_cnt; i++) {
        int ndx = (i + offs) % cache_cnt;
        uint8_t *e = cache + ndx * MEMLAN_CLSZ; // cache entry
        if (bf_absent(bfilter, BLOOM_BIT_COUNT, BLOOM_HASH_COUNT, seed, e+1, *e)) {
            if (Serial) Serial.printf(" --> send entry %d with h=0x", i+offs);
            uint32_t h = murmur3_32(e+1, *e, 9876);
            if (Serial) Serial.printf("%08x\r\n", h);

            return e; //flood(e, *e + 1);
        }
    }
    if (Serial) Serial.printf(" --> no Bloom filter deficit\r\n");
    return NULL;
}

void MemoryLAN::emit_beacon(IO_device *iod)
{   // pkt format: see above

    uint16_t seed = esp_random();
    uint8_t buf[1 + 7 + 3 + 1 + 1 + 2 + (BLOOM_BIT_COUNT + 7) / 8 + 16];
    buf[0] = sizeof(buf) - 1;
    memcpy(buf + 1, dmx_hdr, sizeof(dmx_hdr)); // DMX and id[:2]

    buf[1+9] = iod->get_neigh_cnt();
    buf[1+10] = BLOOM_BIT_COUNT;
    buf[1+11] = BLOOM_HASH_COUNT;
    *(uint16_t*) (buf+1+12) = seed;
    uint8_t *vect = buf + 1 + 14;

    bf_init(vect, BLOOM_BIT_COUNT);
    for (int i = 0; i < cache_cnt; i++) {
        uint8_t *e = cache + i * MEMLAN_CLSZ; // entry
        bf_add(vect, BLOOM_BIT_COUNT, BLOOM_HASH_COUNT, seed, e+1, *e);
    }
    
    broadcast(iod, buf, false, "beacon"); // always send, no backoff
}

void MemoryLAN::on_recv(IO_device *iod, uint8_t *pkt)
{   // pkt format:  points at [N][byte1][byte2]...[byteN][rssi], N<=120
    if (*pkt >= 7) {
        if (!memcmp(dmx_hdr, pkt+1, 7)) {
            pkt = on_beacon(iod, pkt);
            if (pkt)
                for (int i = 0; i < if_cnt; i++)  // backoff for response to BF
                    broadcast(if_table[i], pkt, true, "BF response");
        } else {
            pkt = on_mempage(iod, pkt);
            if (pkt)
                for (int i = 0; i < if_cnt; i++)
                    broadcast(if_table[i], pkt, false, "mempage");
        }
    }
    free(pkt);
}


void MemoryLAN::broadcast(IO_device *iod, uint8_t *pkt, bool backoff,
                          char *comment = NULL)
{
    int dice = 0;
    if (backoff) { // depending on # of neighbors, do NOT send a missing entry
        int nc = iod->get_neigh_cnt();
        if (nc > 1) {
            // tune probability such that chance of nobody sending is approx 1/2
            int dice = esp_random() % int(1.5 * (nc+1));
        }
    }
    if (dice > 0) {
        if (Serial) Serial.printf("   voluntarily not sending %s pkt\r\n",
                                  iod->name);
        pkt = 0;
    }

    if (pkt) {
        set_led(true);
        iod->brcast_send(pkt, comment);
        delay(20); // FIXME: should do this asynchronously instead of blocking
        set_led(false);
    }
}

// eof
