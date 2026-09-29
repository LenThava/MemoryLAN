# MemoryLAN - A Local Area Content Replication Mesh

## ESP32 trial branch

This branch adds Waveshare ESP32-S3-Zero support, core correctness fixes and an
optional single-source measurement mode. See [trial instructions](test/README.md)
for building, testing and retrieving results. The trial measures application-page
delivery; it does not establish a working five-node radio path by itself.

---

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
the ```add()``` method will trigger an ```update()``` event on all
attached end nodes.

There is no guarantee of ordering nor duplicate supression and
reliability: Applications running on the end nodes have to filter and
potentially reorder incoming updates themselves, as well as to
implement some form of ARQ in order to catch the cases where added
content did not make it to all attached end nodes. The major service
provided by a MemoryLAN is the extension of raw broadcast to multi-hop
settings combined with best-effort cache coherence.

Historically, this project first aimed at supporting
[tinySSB](https://github.com/ssbc/tinySSB), hence has a MTU of 128
Bytes. Larger MTUs up to Ethernet's default size (1500 Bytes) are
possible but become cumbersome for low-end wireless technologies like
LoRa and, to some extend, also BLE.


## A Simple Demo Application ("Shared Shopping List")

In order to demonstrate the use of a MemoryLAN, we introduce a shared
whiteboard (think "shopping list") where each peer is allowed to
insert arbitrarily many new entries, delete items and re-add them
afterwards. The MemoryLAN is configured for only 5 cache lines,
wherefore the app instances periodically and randomly pick a local
list entry and propagate it as an udpate through the memory-constraint
MemoryLAN.  Nevertheless and eventually, all end nodes will share the
same list. In other words, the Shared Shopping List app uses a
randomized synchronization protocol.

Implementation detail: List entries are solely identified by their
text content and are sorted alphabetically for presentation
purposes. To each entry we attach a timestamp: Positive means that
this is an active list entry, negative means that the entry has been
deleted. Updates are merged based on these timestamp and the most
recent one wins (it's a "last-writer-wins" data structure).

Note that (except for the ESP32 case) the end nodes run the app as
well as the memory switch logic _in the same process_. This
process-internal MemoryLAN switch is analogous to the Network
Interface Card that Ethernet requires for attaching end nodes to the
network.


## ESP32-based Implementation (BLE, LoRa)

The code in the ```esp``` directory implements the memory switch for
ESP32 CPUs that have BLE and LoRa transmission capabilities (only
using BLE is fine, too).

Note that the ESP32 implementation does not contain the Shared
Shopping List app due to lack if suitable input/output.

We use the [BleBC](https://github.com/tschudin/BleBC) library for
turning the BLE stack into a raw broadcast medium.


## Python Implementation (UDP multicast)

See the [./py](./py) directory for a Python-based implementation of
the MemoryLAN switch (120 LoC), and the "Shared Shopping List" demo
app.


## Kotlin Implementation (BLE, UDP multicast)

pending ...


## UNIX User Space Implementation (raw Ethernet/WiFi)

Most UNIX environments require special permission for promiscous LAN access.

pending ...

---
