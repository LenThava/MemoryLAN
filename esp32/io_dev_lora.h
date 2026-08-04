// io_dev_lora.h

#include "io_dev.h"

class IO_dev_lora: public IO_device {
public:
    IO_dev_lora();
    void brcast_send(const uint8_t *pkt, char *comment = NULL); // broadcast
    // pkt points to [N][byte1][byte2]..[byteN]
    uint8_t* brcast_recv();
    // NULL, or ptr to buffer as above, to be freed by consumer
};

// eof
