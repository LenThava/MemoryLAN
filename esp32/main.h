// MemoryLAN/esp32/main.h

// (c) 2026-08-01 <christian.tschudin@unibas.ch> - MIT license

#include <Arduino.h>
#include <Wire.h>

#include <sodium/crypto_hash_sha256.h>

#include "compile_date.h"
#include "util.h"

unsigned char my_id[sizeof(uint64_t)];

#include "board_config.h"

#include "memoryLAN.h"
MemoryLAN *mlan;

#include "board_setup.h"


// ---------------------------------------------------------------------------

#define SCREEN_SAVER 30000 // turn screen dark after msec

bool dark_mode; // whether to drive OLED and LED or not
uint64_t next_dark_mode;

#include "HotButton.h"
HotButton user_button(BUTTON_PIN, true, LOW);

#if defined(USE_BLE) || defined(USE_BLE_CO)
  IO_dev_ble *ble_if;
#endif
#if defined(USE_ESPNOW)
  IO_dev_espnow *espnow_if;
#endif
#if defined(USE_LORA)
  IO_dev_lora *lora_if;
#endif

// ---------------------------------------------------------------------------

void acquire_id()
{
    // compute our ID by hashing the hardware MAC address
    *(uint64_t*)my_id = ESP.getEfuseMac();
    if (Serial) {
        Serial.printf("MAC: ");
        for (int i = 0; i < 6; i++)
            Serial.printf(i == 0 ? "%02x" : ":%02x", my_id[i]);
        Serial.printf("\r\n");
    }

    uint8_t h[crypto_hash_sha256_BYTES];
    crypto_hash_sha256(h, my_id, sizeof(my_id));
    memcpy(my_id, h, sizeof(my_id));

    if (Serial) {
        Serial.printf("MAC': ");
        for (int i = 0; i < 6; i++)
            Serial.printf(i == 0 ? "%02x" : ":%02x", my_id[i]);
        Serial.printf("\r\n");
    }
}

// ---------------------------------------------------------------------------

void display_stats()
{
    static uint64_t next_stats;
    char stats[100];
    stats[0] = '\0';

    #if defined(USE_BLE_CO)
    sprintf(stats + strlen(stats), "BLE(c=%d,a=%d,u=%d)", ble_if->client_cnt,
              ble_if->co_add_cnt, ble_if->co_upd_cnt);
    #endif
 
    #if defined(USE_BLE)
      sprintf(stats + strlen(stats), "BLR(n=%d,r=%d,t=%d)",
              ble_if->get_neigh_cnt(), ble_if->rx_cnt, ble_if->tx_cnt);
    #endif

    #if defined(USE_LORA)
      sprintf(stats + strlen(stats), "LORA(n=%d,r=%d,t=%d)",
              lora_if->get_neigh_cnt(), lora_if->rx_cnt, lora_if->tx_cnt);
    #endif

    #if defined(USE_ESPNOW)
    sprintf(stats + strlen(stats), "ESPNOW(n=%d,r=%d,t=%d)",
            espnow_if->get_neigh_cnt(), espnow_if->rx_cnt, espnow_if->tx_cnt);
    #endif
 
    sprintf(stats + strlen(stats), "MLAN(h=%d,c=%d)", mlan->hash_cnt,
            mlan->cache_cnt);

    #if defined(USE_BLE_CO)
      ble_if->client_stats(stats); // notify all attached BLT clients
    #endif

    if (millis() > next_stats) {
        if (Serial) Serial.printf("  *%s\r\n", stats);
        next_stats = millis() + 10000; // refresh only every 10 seconds
    }
}

// ---------------------------------------------------------------------------
// Arduino environment

void setup() {
    if (coming_back_from_deep_sleep()) {
        Serial.begin(115200);
        if (Serial)
            Serial.println("\r\nwoke up but going deep sleep again\r\n");
        pinMode(LED_PIN, OUTPUT);
        go_deep_sleep_now();
    }

    Serial.begin(115200);
    delay(500);
    if (Serial) Serial.printf("\r\n\nStarting MemoryLAN on ESP32\r\n\n");

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);

    acquire_id();
    static char ssid[30];
    sprintf(ssid, "mlan%02X%02X", my_id[0], my_id[1]);
    if (Serial) Serial.printf("SSID: %s\r\n\n", ssid);
    
    board_setup();

    next_dark_mode = millis() + SCREEN_SAVER;

    mlan = new MemoryLAN(my_id, NULL);

#if defined(USE_BLE) || defined(USE_BLE_CO)
    ble_if = new IO_dev_ble(my_id, ssid);
    mlan->attach_if(ble_if);
#endif

#if defined(USE_ESPNOW)
    espnow_if = new IO_dev_espnow(my_id);
    mlan->attach_if(espnow_if);
#endif

#if defined(USE_LORA)
    lora_if = new IO_dev_lora();
    mlan->attach_if(lora_if);
#endif

    if (Serial) Serial.print("setup() done\r\n\n");
}

void loop() {
    user_button.update();
    
    if (user_button.isSingleClick()) {
        next_dark_mode = millis() + SCREEN_SAVER;
        if (dark_mode) {
            #if defined(USE_U8G2)
              u8g2.setPowerSave(false);
            #endif
            dark_mode = false;
        } else
            current_screen = (current_screen + 1) % 4;
        display_redraw();
    } else if (current_screen == 0 && user_button.isDoubleClick())
        go_deep_sleep_now();

    if ( mlan->loop() ) {
        display_stats();
        display_redraw();
    }

    if (!dark_mode && millis() > next_dark_mode) {
        #if defined(USE_U8G2)
          u8g2.setPowerSave(true);
        #endif
        dark_mode = true;
    }
}

// eof
