// io_dev_ble.h

#include "io_dev.h"
#include "io_buf.h"
#include <atomic>

// ---------------------------------------------------------------------------

class IO_dev_ble: public IO_device {
public:
    IO_dev_ble(uint8_t *my_id, char *ssid = NULL);
    // (leave ssid NULL for broadcast-only functionality)

    // broadcast:
    void     brcast_send(const uint8_t *pkt, char *comment = NULL); // broadcast
    uint8_t* brcast_recv();                    // retrieve pkt from medium
    // where pkt -> [N][byte0][byte1]..[byteN][rssi]

    PktQueue *from_brcast; // for storing data from broadcast medium
    std::atomic<uint32_t> rx_drop{0}, rx_bad{0}, tx_fail{0};
    
#if defined(USE_BLE_CO)
    // client:
    void     client_send(const uint8_t *pkt);  // update to remote client
    uint8_t* client_recv();                    // remote client had data, get it
    // where pkt -> [N][byte0][byte1]..[byteN]
    void     client_stats(const char *str); // notify remote client about stats
    volatile int client_cnt;
    volatile int co_add_cnt, co_upd_cnt; // BLE client: # of add() and update() events
#endif
};

// eof
