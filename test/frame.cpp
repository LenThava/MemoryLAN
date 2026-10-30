#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include "ble_frame.h"

int main()
{
    BleMeta sent = {{0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff},
                    2026110201, 0xffffffffu, 12345};
    std::array<uint8_t, BLE_FRAME_MAX> wire;
    for (unsigned size : {7u, 32u, 120u, 128u}) {
        std::vector<uint8_t> page(size + 1);
        page[0] = size;
        for (unsigned i = 1; i <= size; i++) page[i] = i;
        auto original = page;
        size_t n = ble_encode_frame(wire.data(), wire.size(), page.data(), sent);
        assert(n == BLE_FRAME_HEADER + size && page == original);
        BleMeta received;
        const uint8_t *body;
        size_t length;
        assert(ble_decode_frame(wire.data(), n, received, body, length));
        assert(length == size && !memcmp(body, page.data() + 1, size));
        assert(!memcmp(received.sender, sent.sender, 6));
        assert(received.run == sent.run && received.sequence == sent.sequence);
        assert(received.received_ms == 0);
        received.received_ms = 0x87654321;
        uint8_t storage[BLE_META_BYTES + 1];
        ble_store_meta(storage + 1, received); // Deliberately unaligned.
        BleMeta copy = ble_load_meta(storage + 1);
        assert(copy.received_ms == received.received_ms && copy.run == received.run);
        assert(copy.sequence == received.sequence && !memcmp(copy.sender, received.sender, 6));

        for (size_t cut = 0; cut < n; cut++)
            assert(!ble_decode_frame(wire.data(), cut, received, body, length));
        auto first = wire;
        BleMeta relay = sent;
        relay.sender[5] ^= 1; relay.sequence = 0; relay.run++;
        assert(ble_encode_frame(wire.data(), wire.size(), page.data(), relay) == n);
        assert(memcmp(wire.data(), first.data(), BLE_FRAME_HEADER) != 0);
        assert(!memcmp(wire.data() + BLE_FRAME_HEADER, first.data() + BLE_FRAME_HEADER, size));
        for (unsigned offset : {0u, 1u, 2u, 3u, 4u}) {
            auto invalid = wire;
            invalid[offset] ^= 1;
            assert(!ble_decode_frame(invalid.data(), n, received, body, length));
        }
        assert(!ble_encode_frame(wire.data(), n - 1, page.data(), sent));
    }
    BleMeta received;
    const uint8_t *body;
    size_t length;
    assert(ble_run_matches(sent, sent.run));
    assert(ble_run_matches(sent, 0));
    assert(!ble_run_matches(sent, sent.run + 1));
    uint8_t old[9] = {8, 0x31};
    assert(valid_ble_frame(old, sizeof(old)));
    assert(!ble_decode_frame(old, sizeof(old), received, body, length));
    assert(!ble_decode_frame(nullptr, 0, received, body, length));
    assert(!ble_encode_frame(wire.data(), wire.size(), nullptr, sent));
    uint8_t too_short[] = {6, 0};
    uint8_t too_long[] = {129, 0};
    assert(!ble_encode_frame(wire.data(), wire.size(), too_short, sent));
    assert(!ble_encode_frame(wire.data(), wire.size(), too_long, sent));
    puts("BLE MAC envelope: all tests passed");
}
