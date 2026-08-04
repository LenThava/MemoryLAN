// io_dev_espnow.cpp

// (c) 2026-08-01 <christian.tschudin@unibas.ch> - MIT license

#include "board_config.h"

#if defined(USE_ESPNOW)

#include <WiFi.h>
#include <esp_now.h>

static IO_dev_espnow *espnow_if;

void onDataRecv(const uint8_t *mac, const uint8_t *data, int len)
{
    if (!espnow_if || espnow_if->from_brcast->is_full()) // tail drop
        return;
    uint8_t *pkt = (uint8_t*) malloc(len+2);
    if (pkt) { // [N][byte1][byte[2]..[byteN][rssi]
        *pkt = len;
        memcpy(pkt+1, data, len);
        *(int8_t*) (pkt+len+1) = 0; // fake rssi
        espnow_if->from_brcast->enqueue(pkt);
    }
}

IO_dev_espnow::IO_dev_espnow(uint8_t *my_id)
{
    name = "ESP-NOW";
    rx_cnt = tx_cnt = 0;
    memset(neigh_tab, 0, sizeof(neigh_tab));

    espnow_if = this;

    WiFi.mode(WIFI_STA); // required, else crash
    WiFi.disconnect();
    if (esp_now_init() != ESP_OK) {
        if (Serial) Serial.printf("espnow: initialization error\r\n");
        return;
    }

    from_brcast = new PktQueue(3);
    esp_now_register_recv_cb(esp_now_recv_cb_t(onDataRecv));

    static esp_now_peer_info_t peerInfo = {};
    memset(peerInfo.peer_addr, 0xff, 6);
    if (esp_now_add_peer(&peerInfo) != ESP_OK)
        if (Serial) Serial.printf("espnow: failed to add peer\r\n");
}

uint8_t* IO_dev_espnow::brcast_recv()
{
    if (!from_brcast) return NULL;

    uint8_t* pkt = from_brcast->dequeue();
    if (pkt) {
        rx_cnt++;
        if (Serial) Serial.printf("e> %dB\r\n", *pkt);
    }
    return pkt;
}

void IO_dev_espnow::brcast_send(const uint8_t *pkt, char *comment)
{
    static const uint8_t brcast_addr[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    int len = *pkt;

    esp_now_send(brcast_addr, pkt+1, len);
    tx_cnt++;

    if (Serial) Serial.printf(comment ? "e< %dB (%s)\r\n" : "l< %dB\r\n",
                              len, comment);
}

#endif //USE_ESPNOW

// eof
