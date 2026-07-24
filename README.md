# MemoryLAN - A Local Area Content Replication Mesh

This repository contains working implementations of a "memory switch"
(in analogy to an "Ethernet switch") for forming a configuration-less
MemoryLAN that assists in replicating content within a confined
region.

A memory switch manages a cache of recently seen memory pages and
replicates its cache to the other switches while striving for
network-wide cache coherence.

![Three end nodes attached to a (multi-hop) MemoryLAN](docs/model.jpg)

The most prominent MemoryLAN use case is a wireless mesh network where
the in-network cache mitigates packet loss. LoRa and long range
Bluetooth Low Energy are two possible technologies on top of which the
MemoryLAN logic can easily operate.

See the [MemoryLAN whitepaper](docs/memoryLAN-whitepaper-20260723.pdf)
for an extended abstract.

## MemoryLAN API

The MemoryLAN software running on end nodes exposes a very simple
API. Implementations working directly at the OS level will roughly
follow this structure:

```
begin(medium_specific_parameters);
add(buffer, len);
get_update(buffer, &len);  // either polling or blocking
```

Alternatively, using a callback style:
```
void update_callback(buffer,len) {...}
begin(medium_specific_parameters, update_callback);
add(buffer,len);
```

Note that a MemoryLAN has no notion of addresses: Content injected via
the ```add()``` method will trigger an ```update``` event on all
attached end nodes.

There is no guarantee of ordering nor duplicate supression and
reliability: Applications running on the end nodes have to filter and
potentially reorder incoming updates themselves, as well as to
implement some form of ARQ in order to catch the cases where added
content did not make it to all attached end nodes. The major service
provided by a MemoryLAN is the extension of raw broadcast to multi-hop
settings combined with best-effort cache coherence.

Historically, this project first aimed at supporting [tinySSB](),
hence has a MTU of 128 Bytes. Larger MTUs up to Ethernet's default
size (1500 Bytes) are possible but become cumbersome for low-end
wireless technologies like LoRa and, to some extend, also BLE.


## ESP32-based Implementation

The code in the ```esp``` directory implements the memory switch for
ESP32 CPUs that have BLE and LoRa transmission capabilities (only
using BLE is fine, too).

See [BleBC](https://github.com/tschudin/BleBC) for turning the BLE
stack into a raw broadcast medium.


## Python Implementation (UDP multicast)


## Kotlin Implementation (BLE and UDP multicast)


## UNIX User Space Implementations (raw Ethernet/WiFi)

Most UNIX environments require special permission for promiscous LAN access.

---