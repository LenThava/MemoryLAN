#pragma once
#include "ble_meta.h"
#include "mac_id.h"
#include "trial_data.h"

const unsigned TRIAL_EVENTS = 256;

struct TrialEvent {
    uint8_t sender[6];
    int8_t rssi;
    uint8_t type;
    uint32_t sequence, ms;
    int16_t page;
};

struct TrialTopology {
    uint32_t run, count, overflow, ignored_run;
    TrialEvent events[TRIAL_EVENTS];
};

template<class Output>
static void trial_topology_json(Output &out, const TrialRecord &r,
                                const TrialTopology &topology, const uint8_t mac[6])
{
    char address[13]; mac_text(mac, address);
    out.printf("{\"mac\":\"%s\",\"node\":\"%04x\",\"run\":%u,\"schema\":1,\"kind\":\"mlan-topology\",",
        address, r.node, r.run);
    out.printf("\"build\":\"%s\",\"clock\":\"local_ms_since_start\",\"elapsed_ms\":%u,",
        r.build, r.elapsed_ms);
    if (r.version == 2) out.printf("\"payload_bytes\":%u,", r.payload_bytes);
    out.printf("\"capacity\":%u,\"overflow\":%u,\"ignored_run\":%u,\"events\":[",
        TRIAL_EVENTS, topology.overflow, topology.ignored_run);
    const char *types[] = {"other", "beacon", "page"};
    for (unsigned i = 0; i < topology.count; i++) {
        const auto &event = topology.events[i];
        mac_text(event.sender, address);
        out.printf("%s{\"sender\":\"%s\",\"run\":%u,\"seq\":%u,\"ms\":%u,\"rssi\":%d,\"type\":\"%s\",\"page\":",
            i ? "," : "", address, r.run, event.sequence, event.ms, event.rssi, types[event.type]);
        if (event.page < 0) out.print("null");
        else out.printf("%d", event.page);
        out.print("}");
    }
    out.print("]}\n");
}
