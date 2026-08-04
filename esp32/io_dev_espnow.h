// io_dev_espnow.h

#include <cstddef>

#include "io_dev.h"
#include "io_buf.h"

class IO_dev_espnow: public IO_device {
public:
    IO_dev_espnow(uint8_t *my_id);
    void brcast_send(const uint8_t *pkt, char *comment = NULL); // broadcast
    // pkt points to [N][byte1][byte2]..[byteN]
    uint8_t* brcast_recv();
    // NULL, or ptr to buffer as above, to be freed by consumer

    PktQueue *from_brcast; // for storing data from broadcast medium
};

// eof
