// io_dev_ble.cpp

// (c) 2026-08-01 <christian.tschudin@unibas.ch> - MIT license

#include "Arduino.h"

#include "board_config.h"
#include "util.h"

#if defined(USE_BLE) || defined(USE_BLE_CO)

#if defined(USE_BLE_CO)
  #include <NimBLEServer.h>
#endif

#include "io_buf.h"
#include "ble_frame.h"

#define MAX_PKT_LENGTH  128

#define LEGACY_ADV_INST   0
#define EXTENDED_ADV_INST 1 // must have setInstanceData() in this order!


#if defined(USE_BLE_CO)
// ---------------------------------------------------------------------------
// BLE connection support (for memoryLAN _clients_)

// tinySSB-specific:
#define BLE_SERVICE_UUID           "6e400001-7646-4b5b-9a50-71becce51558"
#define BLE_CHARACTERISTIC_UUID_RX "6e400002-7646-4b5b-9a50-71becce51558"
#define BLE_CHARACTERISTIC_UUID_TX "6e400003-7646-4b5b-9a50-71becce51558"
#define BLE_CHARACTERISTIC_UUID_ST "6e400004-7646-4b5b-9a50-71becce51558"

NimBLECharacteristic *rxChar = nullptr; // receive
NimBLECharacteristic *txChar = nullptr; // transmit (notify)
NimBLECharacteristic *stChar = nullptr; // statistics

NimBLEServer *ble_server;
PktQueue *from_client; // for storing data from connected BT client
#endif
// ----------------------------------------------------------------------
// extended advertisements for long range (LR) broadcast

static IO_dev_ble *ble_interface;

// forward declarations
void _lr_recv_enqueue(const uint8_t *buf, size_t len, int rssi);

NimBLEScan           *lr_receiver; // "nimBLE scanner"
NimBLEExtAdvertising *lr_sender;   // "nimBLE advertiser"

NimBLEExtAdvertisement lgc_adv(BLE_HCI_LE_PHY_1M, BLE_HCI_LE_PHY_1M); // 1st!
NimBLEExtAdvertisement ext_adv(BLE_HCI_LE_PHY_CODED, BLE_HCI_LE_PHY_CODED);
// in BLE 5.4, we should be able to pick S8 instead of S2?

class BLE_ScanCallbacks : public NimBLEScanCallbacks {

    void onDiscovered(const NimBLEAdvertisedDevice* adv) override {
        /*
        if (Serial) {
            Serial.printf("  (BLE adv from %s (%d) ",
                          adv->getAddress().toString().c_str(),
                          adv->getAdvLength());
            Serial.printf("lgcy=%d rssi=%d advT=%d ",
                          adv->isLegacyAdvertisement(),
                          adv->getRSSI(), adv->getAdvType());
            Serial.printf("pPhy=%d sPhy=%d datS=%d setI=%d)\r\n",
                          adv->getPrimaryPhy(), adv->getSecondaryPhy(),
                          adv->getDataStatus(), adv->getSetId());
        }
        */
        /*
        if (Serial) Serial.printf("   got adv LGCY=%d: setID=%d, %dB\r\n",
                                  adv->isLegacyAdvertisement(),
                                  adv->getSetId(), adv->getAdvLength());
        */
        if (!adv->isLegacyAdvertisement() && adv->getDataStatus() == 0) {
            _lr_recv_enqueue( adv->getPayload().data(), adv->getAdvLength(),
                              adv->getRSSI() );
            // if (ble_interface)
            //     ble_interface->rx_cnt++; // one LR broadcast was received
        }
    }

    void onScanEnd(const NimBLEScanResults& results, int reason) override {
        if (MLAN_DEBUG && Serial)
            Serial.printf("Scan ended, reason=%d\r\n", reason);
    }
};

// ---------------------------------------------------------------------------
#if defined(USE_BLE_CO)

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
        if (MLAN_DEBUG && Serial) Serial.printf("** BLE device connected\r\n");
        // Serial.println(connInfo.toString().c_str());
        if (ble_interface)
            ble_interface->client_cnt += 1;
    };
    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reasno) {
        if (MLAN_DEBUG && Serial) Serial.printf("** BLE device disconnected\r\n");
        if (ble_interface)
            ble_interface->client_cnt -= 1;
        // theUI->refresh();

        // resume advertising when peer disconnects
        NimBLEDevice::startAdvertising(LEGACY_ADV_INST,0,0);
    }
};

class RxCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *pChar,
                 NimBLEConnInfo &connInfo) {
        int len = pChar->getValue().length();
        // if (Serial) Serial.printf("   rxCallback %dB\r\n", len);
        if (len > 0 && from_client)
            from_client->enqueue(pChar->getValue().data(), len);
  }
};

#endif
// ---------------------------------------------------------------------------
// joint advertisement callbacks (inst 0=BLE, inst 1=EXT

class AdvCallbacks : public NimBLEExtAdvertisingCallbacks {

    void onStopped(NimBLEExtAdvertising* pAdv, int reason, uint8_t instId) override {
        // if (Serial) Serial.printf("   adv instance %u stopped, reason %d\r\n", instId, reason);
        if (instId == EXTENDED_ADV_INST) { // timeout is normal (only one TX)
            if (ble_interface)
                ble_interface->tx_cnt++; // one LR broadcast was sent
            if (lr_receiver)
                lr_receiver->start(0);
        } else
            if (reason == BLE_HS_ETIMEOUT)
                if (MLAN_DEBUG && Serial) Serial.printf("*  AdvCallback: timeout\r\n");
    }
};

// ---------------------------------------------------------------------------

void _lr_recv_enqueue(const uint8_t *buf, size_t len, int rssi)
{
    if (!ble_interface || !ble_interface->from_brcast) return;
    if (!valid_ble_frame(buf, len)) {
        ble_interface->rx_bad++;
        return;
    }
    if (ble_interface->from_brcast->is_full()) {
        ble_interface->rx_drop++;
        return;
    }
    len -= 2; // remove BLE AD  length and payload type bytes
    if (len > MAX_PKT_LENGTH) // ignore
        return;
    uint8_t *pkt = (uint8_t*) malloc(len+2);
    if (pkt) { // [N][byte1][byte[2]..[byteN][rssi]
        *pkt = len;
        memcpy(pkt+1, buf + 2, len);
        if (rssi < -128) rssi = -128;
        *(int8_t*) (pkt+len+1) = (int8_t) rssi;
        if (!ble_interface->from_brcast->enqueue(pkt)) {
            free(pkt);
            ble_interface->rx_drop++;
        }
    } else ble_interface->rx_drop++;
}

// ----------------------------------------------------------------------

IO_dev_ble::IO_dev_ble(uint8_t *my_id, char *ssid)
{
    name = "BleLR"; // long range
    from_brcast = NULL;
    rx_cnt = tx_cnt = 0;
    memset(neigh_tab, 0, sizeof(neigh_tab));

    ble_interface = this;
#if defined(USE_BLE_CO)
    client_cnt = 0;
    co_add_cnt = co_upd_cnt = 0;
#endif
    // if (Serial) Serial.println("BLE init 1");

    NimBLEDevice::init("");
    NimBLEDevice::setPower(22); /** +22db is max */
    // NimBLEDevice::setMTU(128); // ??

    lr_sender = NimBLEDevice::getAdvertising();
    lr_sender->setCallbacks(new AdvCallbacks());

#if defined(USE_BLE_CO)
    if (ssid) {
        from_client = new PktQueue(3);
        ble_server = BLEDevice::createServer();

        ble_server->setCallbacks(new ServerCallbacks());
        NimBLEService *ble_service = ble_server->createService(BLE_SERVICE_UUID);

        // if (Serial) Serial.println("BLE init 2");

        // Create our BLE Characteristics
        stChar = ble_service->createCharacteristic(BLE_CHARACTERISTIC_UUID_ST,
                                  NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

        txChar = ble_service->createCharacteristic(BLE_CHARACTERISTIC_UUID_TX,
                                  NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

        rxChar = ble_service->createCharacteristic(BLE_CHARACTERISTIC_UUID_RX,
                                  NIMBLE_PROPERTY::WRITE);
        rxChar->setCallbacks(new RxCallbacks());

        // if (Serial) Serial.println("BLE init 3");
        ble_service->start();
        // if (Serial) Serial.println("BLE init 4");

        // Start legacy advertising
        lgc_adv.setAddress(NimBLEAddress(my_id, BLE_ADDR_RANDOM));
        lgc_adv.setName(ssid); // must be small enough for all to fit into 37B
        lgc_adv.setCompleteServices({NimBLEUUID(BLE_SERVICE_UUID)});
        lgc_adv.setLegacyAdvertising(true);
        // lgc_adv.setScannable(true);
        lgc_adv.setConnectable(true);
        lgc_adv.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);

        // if (Serial) Serial.println("BLE init 5");
        if(!lr_sender->setInstanceData(LEGACY_ADV_INST, lgc_adv))
            if (MLAN_DEBUG && Serial) Serial.printf("lgcy set inst data\r\n");
        // if (Serial) Serial.println("BLE init 6");
    }
#endif
        
    // prepare extended advertising but do not start yet
    ext_adv.setAnonymous(true);
    ext_adv.setLegacyAdvertising(false);
    ext_adv.setFlags(0);
    ext_adv.setConnectable(false);
    ext_adv.setScannable(false);
    ext_adv.setMinInterval(80);
    ext_adv.setMaxInterval(160);
    // must set type to BLE_HCI_ADV_RPT_EVTYPE_NONCONN_IND, how?

    if (!lr_sender->setInstanceData(EXTENDED_ADV_INST, ext_adv))
        if (MLAN_DEBUG && Serial) Serial.printf("failed: extd set inst data\r\n");
    // if (Serial) Serial.println("BLE init 7");

#if defined(USE_BLE_CO)
    if (ssid) {
        if (!lr_sender->start(LEGACY_ADV_INST,0,0))
            if (MLAN_DEBUG && Serial) Serial.printf("lgcy adv start failed\r\n");
        // if (Serial) Serial.println("BLE init 8");
    }
#endif

    // BLR support --------------------------------------------------
    from_brcast = new PktQueue(3);

    lr_receiver = NimBLEDevice::getScan();
    lr_receiver->setScanCallbacks(new BLE_ScanCallbacks());
    /*
    lr_receiver->setInterval(45);
    lr_receiver->setWindow(45);
    */
    lr_receiver->setActiveScan(false);
    lr_receiver->setDuplicateFilter(false);
    lr_receiver->setMaxResults(1);
    lr_receiver->setPhy(NimBLEScan::Phy::SCAN_CODED); // force PHY_CODED
    if (!lr_receiver->start(0)) // must start AFTER starting legacy adverisment!
        if (MLAN_DEBUG && Serial) Serial.printf("cannot start scanner\r\n");

    // if (Serial) Serial.println("BLE init 9");
}

void IO_dev_ble::brcast_send(const uint8_t *pkt, char *comment)
// pkt -> [N][byte0][byte1]..[byteN]
{
    if (*pkt <= MAX_PKT_LENGTH) {
        if (MLAN_DEBUG && Serial) Serial.printf(comment ? "b< %dB (%s)\r\n" : "b< %dB\r\n",
                                  *pkt, comment);

        static uint8_t buf[2 + MAX_PKT_LENGTH];
        buf[0] = 1 + *pkt; // BLE payload len
        buf[1] = '\x31';   // BleBC-compatible tag; payload is not encrypted
        memcpy(buf + 2, pkt + 1, *pkt);
        if (!ext_adv.setData(buf, 2 + *pkt)) {
            tx_fail++;
            if (MLAN_DEBUG && Serial)
                Serial.printf("failed to set data for ext_adv\r\n");
            return;
        }

        if (!lr_sender)
            return;
        while (lr_sender->isActive(EXTENDED_ADV_INST))
            delay(1);
#if 0
        if (ble_server)
            ble_server->stopAdvertising(LEGACY_ADV_INST);
#endif
        if (!lr_sender->setInstanceData(EXTENDED_ADV_INST, ext_adv)) {
            tx_fail++;
            return;
        }
        if (lr_receiver)
            lr_receiver->stop(); // scanner
        if (!lr_sender->start(EXTENDED_ADV_INST, 0, 1)) { // (instID, dur, #evt)
                                    // this will also restart the receiver
            if (MLAN_DEBUG && Serial)
                Serial.printf("failed to start extd advertising\r\n");
            tx_fail++;
            if (lr_receiver) lr_receiver->start(0);
        }
    }
}

uint8_t* IO_dev_ble::brcast_recv()
{
    uint8_t* pkt = from_brcast->dequeue();
    
    if (pkt) {
        rx_cnt++;
        if (MLAN_DEBUG && Serial) Serial.printf("b> %dB rssi=%hhd\r\n",
                                  *pkt, *(pkt + *pkt + 1));
    }
    return pkt;
}

#if defined(USE_BLE_CO)

void IO_dev_ble::client_send(const uint8_t *pkt) // update external client
{
    if (client_cnt == 0) return;

    if (MLAN_DEBUG && Serial) Serial.printf("c< %dB\r\n", *pkt);
    txChar->setValue(pkt+1, *pkt);
    txChar->notify();
    co_upd_cnt++;
}

uint8_t* IO_dev_ble::client_recv() // client had data, get it
{
    if (!from_client)
        return NULL;

    uint8_t* pkt = from_client->dequeue();
    if (pkt) {
        co_add_cnt++;
        if (MLAN_DEBUG && Serial) Serial.printf("c> %dB\r\n", *pkt);
    }
    return pkt;
}

void IO_dev_ble::client_stats(const char *cp)
{
    if (client_cnt == 0) return;

    stChar->setValue((uint8_t*) cp, strlen(cp));
    stChar->notify();
    if (MLAN_DEBUG && Serial)
        Serial.printf("c< %dB stats <%s>\r\n", strlen(cp), cp);
}
#endif // USE_BLE_CO

#endif // USE_BLE

// eof
