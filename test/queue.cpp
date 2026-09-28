#include <cassert>
#include <cstdio>
#include <cstdlib>
#include "io_buf.h"
#include "ble_frame.h"

int main()
{
    PktQueue q(2);
    uint8_t data[] = {1, 2, 3};
    assert(q.is_empty() && !q.is_full());
    assert(q.enqueue(data, 3) && q.enqueue(data, 3));
    assert(q.is_full() && !q.enqueue(data, 3));
    auto *owned = (uint8_t*)malloc(4);
    assert(!q.enqueue(owned));
    free(owned); // caller still owns rejected pointers
    uint8_t *p = q.dequeue();
    assert(p && p[0] == 3 && p[3] == 3);
    free(p);
    p = q.dequeue(); free(p);
    assert(!q.dequeue() && !q.enqueue(nullptr) && !q.enqueue(data, 256));
    assert(q.enqueue(data, 3)); // destructor owns remaining successful enqueue
    PktQueue failed(0);
    assert(failed.is_full() && !failed.enqueue(data, 3) && !failed.dequeue());
    uint8_t frame[130] = {8, 0x31};
    assert(valid_ble_frame(frame, 9));
    assert(!valid_ble_frame(nullptr, 9));
    assert(!valid_ble_frame(frame, 8) && !valid_ble_frame(frame, 10));
    frame[0] = 129;
    assert(valid_ble_frame(frame, 130) && !valid_ble_frame(frame, 131));
    frame[1] = 0;
    assert(!valid_ble_frame(frame, 130));
    puts("queue/frame: all tests passed (host FreeRTOS stub)");
}
