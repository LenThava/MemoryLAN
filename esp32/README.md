# An ESP32-based MemoryLAN Implementation

The code in this directory implements multi-hop wireless broadcast of
opaque frames (memory pages) of up to 120B with reliability support
based on limited in-network memory.

Supported media:
- BLE extended advertisements (PHY_CODED for long range connectivity)
- BLE connections (for attaching end devices)
- ESP-NOW (WiFi-based, but doubtful performance in presence of BLE use)
- LoRa

Supported ESP32 devices:
- Heltec LoRa: v2, v3, v4
- Lilygo TBeam: Supreme
- Lilygo T3S3: v1.2, v1.3
- noname "C3 supermini", w/ and w/o 0.46" OLED


## Configuration

- the file ```board_config.h``` has per-board configuration switches as well as media-specific parameters (LoRa frequency etc)

- board-specific initialization is in ```board_setup.h```


## Creating the Firmware

We assume that the Arduino command-line utility is installed.

Use ```make``` to compile and upload:

```
% make BOARD=C3mini compile
% make BOARD=C3mini upload
```

Without explicit target, the ```Makefile``` will do the ```compile```
and ```upload``` steps in one go. Upload will work for all devices
currently attached to the computer, permitting efficient reflashing of
multiple ESP devices (of the same sort).


## Monitoring the Internal Status

When ```BLE_CO``` (Bluetooth connections) is enabled, one can connect
to one of the "mlanXXXX" devices using a BLE scanner app and read its
```6e400004``` characteristic of the ```6e400001``` service. This
returns an ASCII string with per interface statistics as well as the
number of entries in the MemoryLAN history and cache:

```
BLE(c=0,a=0,u=0)BLR(n=2,r=7,t=12)ESPNOW(n=0,r=0,t=11)LORA(n=2,r=8,t=11)MLAN(h=0,c=0)
```

Using the serial line, the following log is shown:
```
e< 48B (beacon)
b< 48B (beacon)
l< 48B (beacon)
b> 48B rssi=-43
   on_beacon  from <69:b2>: n=2
b> 48B rssi=-56
   on_beacon  from <08:38>: n=2
e< 48B (beacon)
b< 48B (beacon)
b> 48B rssi=-55
l> 48B rssi=-12
   on_beacon  from <08:38>: n=2
```

where the first character identifies the interface (b=BT long range,
e=ESP-NOW, l=LoRa), followed by the direction of the exchanged frame:
```>``` for incoming, ```<``` for outgoing.


## Display and Deep Sleep

All devices show (when an OLED is available) four screens:
- general info and stats
- top history and cache entries
- neighborhood list
- compile-time config

With a single-click on the boot button one can cycle through the four
screens.

The screen goes dark after 30 seconds and can be reactivated by a
single-click on the boot button.

All devices except the C3 supermini can be put into deep-sleep mode by
double-clicking on the boot button while on the first screen.  Later,
a click on the boot button wakes up the device.

---
