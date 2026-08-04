// memoryLAN.h

#define MEMLAN_MAX_IF        3

#define MEMLAN_PKT_LEN     120
#define MEMLAN_CACHE_DEPTH  10
#define MEMLAN_HHIST_DEPTH  20
#define MEMLAN_CLSZ (1 + MEMLAN_PKT_LEN) // cache line size

#define MEMLAN_NEXT_BEACON    5000 // msec

#define MEMLAN_HASH_TIMEOUT   8000 // msec

#include "io_dev.h"

// ---------------------------------------------------------------------------

typedef void (callback_fct)(uint8_t* ptr);

// ---------------------------------------------------------------------------

class MemoryLAN { // implement the MemoryLAN switch logic

public:
    MemoryLAN(uint8_t* mac, callback_fct* update_cb = NULL);
    void attach_if(IO_device *d);
    bool loop(); // serve MemoryLAN protocol on all interfaces, true if changes

    // in case we have applications on top of this MemoryLAN switch:
    void add(uint8_t *mempg); // mempg points to [N][byte1][byte2]..[byteN]
    uint8_t* set_update_cb(callback_fct* update_cb);
    // uint8_t* get_update();    // NULL or ptr to buffer as above, to be freed

    // --------------------------------------------------

    int get_BFwidth();   // in bits
    int get_BFhashes();

    // --------------------------------------------------

    int hash_cnt = 0;
    uint8_t cache[MEMLAN_CACHE_DEPTH * MEMLAN_CLSZ]; // list, newest at end
    int cache_cnt = 0;
    IO_device* if_table[MEMLAN_MAX_IF];
    uint8_t if_cnt = 0;

    uint32_t      hashfifo[MEMLAN_HHIST_DEPTH]; // list, newest is at end
    unsigned long timefifo[MEMLAN_HHIST_DEPTH]; // list, time when added
    
private:
    uint8_t my_id[6];
    void on_recv(IO_device *iod, uint8_t *pkt);
    uint8_t* on_beacon(IO_device *iod, uint8_t *pkt);
    uint8_t* on_mempage(IO_device *iod, uint8_t *pkt);
    void emit_beacon(IO_device *iod);
    void broadcast(IO_device *iod, uint8_t *pkt, bool do_backoff, char *cmnt);

    uint8_t dmx_hdr[7+2];
    callback_fct *update = NULL;
    uint64_t next_beacon = millis();
};

// eof
