// util.h

// crc
extern uint32_t crc32_ieee(uint8_t *pkt, int len);
extern int crc_check(uint8_t *pkt, int len);

// murmur3
extern uint32_t murmur3_32(const uint8_t* key, size_t len, uint32_t seed);

// bloom filter
extern void bf_init(uint8_t *vect, int bit_cnt);
extern void bf_add(uint8_t *vect, int bit_cnt, int num_hash, int seed,
            uint8_t *item, int item_len);
extern bool bf_absent(uint8_t *vect, int bit_cnt, int num_hash, int seed,
               uint8_t *item, int item_len);

// misc
extern char* to_hex(uint8_t *ptr, int len);

// eof
