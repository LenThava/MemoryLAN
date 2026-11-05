#pragma once
#include "trial_data.h"
#include "mac_id.h"

template<class Output>
static void trial_json(Output &out, const TrialRecord &r, bool storage_ok, uint16_t node,
                       const uint8_t *mac = nullptr, const TrialMode *mode = nullptr)
{
    const char *states[] = {"empty", "ready", "running", "done", "interrupted"};
    out.print("{");
    if (mac) {
        char address[13]; mac_text(mac, address);
        out.printf("\"mac\":\"%s\",", address);
    }
    out.printf("\"schema\":1,\"kind\":\"mlan-run\",\"state\":\"%s\",\"storage_ok\":%s,",
        states[r.state], storage_ok ? "true" : "false");
    out.printf("\"node\":\"%04x\",\"source\":\"%04x\",\"run\":%u,\"total\":%u,\"sent\":%u,",
        node, r.source, r.run, r.total, r.sent);
    if (r.version >= 2) out.printf("\"payload_bytes\":%u,", r.payload_bytes);
    if (mode) out.printf("\"source_pinned\":%s,\"repeat_ms\":%u,\"repeat_attempts\":%u,",
        mode->repeat_ms ? "true" : "false", mode->repeat_ms, mode->repeat_attempts);
    out.printf("\"gap_ms\":%u,\"limit_ms\":%u,\"elapsed_ms\":%u,\"stop_reason\":%u,\"cutoff\":\"local\",",
        r.gap_ms, r.limit_ms, r.elapsed_ms, r.stop_reason);
    out.printf("\"build\":\"%s\",\"duplicates\":%u,\"invalid\":%u,\"rx\":%u,\"tx\":%u,",
        r.build, r.duplicates, r.invalid, r.rx, r.tx);
    out.printf("\"rx_drop\":%u,\"rx_bad\":%u,\"tx_fail\":%u,\"received\":[",
        r.rx_drop, r.rx_bad, r.tx_fail);
    bool comma = false;
    for (unsigned i = 0; i < r.total; i++) if (trial_seen(r, i)) {
        out.printf("%s%u", comma ? "," : "", i); comma = true;
    }
    out.print("],\"peers\":["); comma = false;
    for (const auto &p : r.peers) if (p.id) {
        out.printf("%s{\"id\":\"%04x\",\"rssi\":%d,\"samples\":%u,\"last_ms\":%u}",
            comma ? "," : "", p.id, p.rssi, p.samples, p.last_ms);
        comma = true;
    }
    out.print("]}\n");
}
