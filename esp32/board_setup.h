// board_setup.h

// (c) 2026-08-01 <christian.tschudin@unibas.ch> - MIT license

// ---------------------------------------------------------------------------
// board-specific setups

#if defined(C3MINI)

  #define LED_PIN     GPIO_NUM_8
  #define BUTTON_PIN  GPIO_NUM_9

  #define SDA_OLED    GPIO_NUM_5
  #define SCL_OLED    GPIO_NUM_6
  U8G2_SSD1306_72X40_ER_F_SW_I2C u8g2(U8G2_R0, SCL_OLED, SDA_OLED, U8X8_PIN_NONE);

#elif defined(HELTEC_V2)

  #define LED_PIN   GPIO_NUM_25
  #define BUTTON_PIN  GPIO_NUM_0
  #define VEXT      Vext // GPIO_NUM_36
  // SPI pins
  #define SS        GPIO_NUM_18
  #define MOSI      GPIO_NUM_27
  #define MISO      GPIO_NUM_19
  #define SCK       GPIO_NUM_5
  // Radio pins
  #define DIO0      GPIO_NUM_26
  #define DIO1      GPIO_NUM_35
  #define DIO5      GPIO_NUM_35
  #define DIO6      GPIO_NUM_35
  #define RST_LoRa  GPIO_NUM_14
  #define BUSY_LoRa GPIO_NUM_5
  // OLED
  #define SDA_OLED  GPIO_NUM_4
  #define SCL_OLED  GPIO_NUM_15
  #define RST_OLED  GPIO_NUM_16

  SX1276 radio = new Module(SS, DIO0, RST_LoRa); // , BUSY_LoRa);
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, RST_OLED, SCL_OLED, SDA_OLED);

#elif defined(HELTEC_V3)

  #define LED_PIN   GPIO_NUM_35
  #define BUTTON_PIN  GPIO_NUM_0
  #define VEXT      GPIO_NUM_36
  // Battery voltage measurement
  #define VBAT_CTRL GPIO_NUM_37
  #define VBAT_ADC  GPIO_NUM_1
  // SPI pins
  #define SS        GPIO_NUM_8
  #define MOSI      GPIO_NUM_10
  #define MISO      GPIO_NUM_11
  #define SCK       GPIO_NUM_9
  // Radio pins
  #define DIO1      GPIO_NUM_14
  #define RST_LoRa  GPIO_NUM_12
  #define BUSY_LoRa GPIO_NUM_13
  // Display pins
  #define SDA_OLED  GPIO_NUM_17
  #define SCL_OLED  GPIO_NUM_18
  #define RST_OLED  GPIO_NUM_21

  SX1262 radio = new Module(SS, DIO1, RST_LoRa, BUSY_LoRa);
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, RST_OLED, SCL_OLED, SDA_OLED);

#elif defined(HELTEC_V4)

  #define LED_PIN   GPIO_NUM_35
  #define BUTTON_PIN  GPIO_NUM_0
  #define VEXT      GPIO_NUM_36
  // Battery voltage measurement
  #define VBAT_CTRL GPIO_NUM_37 // ?
  #define VBAT_ADC  GPIO_NUM_1 // ?
  // SPI pins
  #define SS        GPIO_NUM_8
  #define MOSI      GPIO_NUM_10
  #define MISO      GPIO_NUM_11
  #define SCK       GPIO_NUM_9
  // Radio pins
  #define DIO1      GPIO_NUM_14
  #define RST_LoRa  GPIO_NUM_12
  #define BUSY_LoRa GPIO_NUM_13
  // Display pins
  #define SDA_OLED  GPIO_NUM_17
  #define SCL_OLED  GPIO_NUM_18
  #define RST_OLED  GPIO_NUM_21

  SX1262 radio = new Module(SS, DIO1, RST_LoRa, BUSY_LoRa);
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, RST_OLED, SCL_OLED, SDA_OLED);

#elif defined(LILYGO_TBEAMS3) // Sx1262

  #define LED_PIN     GPIO_NUM_17
  #define BUTTON_PIN  GPIO_NUM_0
  #define VBAT_ADC    GPIO_NUM_1
  #define VEXT        U8X8_PIN_NONE // disable

  #define I2C_SDA   GPIO_NUM_42
  #define I2C_SCL   GPIO_NUM_41
  // SPI pins
  #define SS	    GPIO_NUM_10
  #define SCK       GPIO_NUM_12
  #define MISO      GPIO_NUM_13
  #define MOSI      GPIO_NUM_11
  // Radio pins
  #define RST_LoRa  GPIO_NUM_5
  #define DIO1      GPIO_NUM_1
  // #define DIO9      GPIO_NUM_?? 36
  #define BUSY_LoRa GPIO_NUM_4
  // Display pins
  #define SDA_OLED  GPIO_NUM_17
  #define SCL_OLED  GPIO_NUM_18
  #define RST_OLED  U8X8_PIN_NONE // disable

  // SD CS	GPIO_NUM_13
  // SD MOSI	GPIO_NUM_11
  // SD MISO	GPIO_NUM_2
  // SD SCK	GPIO_NUM_14

  SX1262 radio = new Module(SS, DIO1, RST_LoRa, BUSY_LoRa);
  U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, SCL_OLED, SDA_OLED);

#elif defined(LILYGO_T3S3V12) // LR1121

  #define LED_PIN     GPIO_NUM_37
  #define BUTTON_PIN  GPIO_NUM_0
  #define VBAT_ADC    GPIO_NUM_1
  #define VEXT        U8X8_PIN_NONE // disable

  #define I2C_SDA   GPIO_NUM_18
  #define I2C_SCL   GPIO_NUM_17
  // SPI pins
  #define SS	    GPIO_NUM_7
  #define SCK       GPIO_NUM_5
  #define MISO      GPIO_NUM_3
  #define MOSI      GPIO_NUM_6
  // Radio pins
  #define RST_LoRa  GPIO_NUM_8
  #define DIO1      GPIO_NUM_9
  #define DIO9      GPIO_NUM_36
  #define BUSY_LoRa GPIO_NUM_34
  // Display pins
  #define SDA_OLED  GPIO_NUM_18
  #define SCL_OLED  GPIO_NUM_17
  #define RST_OLED  U8X8_PIN_NONE // disable

  // SD CS	GPIO_NUM_13
  // SD MOSI	GPIO_NUM_11
  // SD MISO	GPIO_NUM_2
  // SD SCK	GPIO_NUM_14

  LR1121 radio = new Module(SS, DIO9, RST_LoRa, BUSY_LoRa);
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, SCL_OLED, SDA_OLED);

#elif defined(LILYGO_T3S3V13) // Lora 2.4GHz (SX1280)

  #define LED_PIN     GPIO_NUM_37
  #define BUTTON_PIN  GPIO_NUM_0
  #define VBAT_ADC    GPIO_NUM_1
  #define VEXT        U8X8_PIN_NONE // disable

  #define I2C_SDA   GPIO_NUM_18
  #define I2C_SCL   GPIO_NUM_17
  // SPI pins
  #define SS	    GPIO_NUM_7
  #define SCK       GPIO_NUM_5
  #define MISO      GPIO_NUM_3
  #define MOSI      GPIO_NUM_6
  // Radio pins
  #define RST_LoRa   GPIO_NUM_8
  #define DIO1       GPIO_NUM_9
  // #define DIO9      GPIO_NUM_??
  #define BUSY_LoRa  GPIO_NUM_36
  // Display pins
  #define SDA_OLED  GPIO_NUM_18
  #define SCL_OLED  GPIO_NUM_17
  #define RST_OLED  U8X8_PIN_NONE // disable

  // SD CS	GPIO_NUM_13
  // SD MOSI	GPIO_NUM_11
  // SD MISO	GPIO_NUM_2
  // SD SCK	GPIO_NUM_14

  SX1280 radio = new Module(SS, DIO1, RST_LoRa, BUSY_LoRa);
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, SCL_OLED, SDA_OLED);

#elif defined(SEEED_XIAOS3)

  #define LED_PIN     GPIO_NUM_21
  #define BUTTON_PIN  GPIO_NUM_0
/*
  #define VBAT_ADC    GPIO_NUM_1
  #define VEXT        U8X8_PIN_NONE // disable
*/

  #define I2C_SDA   GPIO_NUM_5
  #define I2C_SCL   GPIO_NUM_6

// SPI pins
  #define SS	    GPIO_NUM_41
  #define SCK       GPIO_NUM_7
  #define MISO      GPIO_NUM_8
  #define MOSI      GPIO_NUM_9
  // Radio pins
  #define RST_LoRa   GPIO_NUM_42
  #define DIO1       GPIO_NUM_39
  #define BUSY_LoRa  GPIO_NUM_40
  // RF SWITCH 	GPIO_NUM_38

  SX1262 radio = new Module(SS, DIO1, RST_LoRa, BUSY_LoRa);

#elif defined(WAVESHARE_S3ZERO)

  // The exact Arduino board variant maps digitalWrite() on LED_BUILTIN to
  // the onboard addressable WS2812 on GPIO21.
  #define LED_PIN     LED_BUILTIN
  #define BUTTON_PIN  GPIO_NUM_0

#endif // boards

// ---------------------------------------------------------------------------

void set_led(bool a)
{
#if defined(MLAN_TRIAL) && defined(WAVESHARE_S3ZERO)
    return; // trial state owns the LED instead of packet activity
#endif
#if defined(C3MINI) || defined(SEEED_XIAOS3)
    digitalWrite(LED_PIN, a ? LOW : HIGH);
#else
    digitalWrite(LED_PIN, a ? HIGH : LOW);
#endif
    /*
https://github.com/FastLED/FastLED.git
#include <FastLED.h>
#define NUM_LEDS 60
CRGB leds[NUM_LEDS];

void setup() { 
  FastLED.addLeds<WS2812, 6>(leds, NUM_LEDS); 
}

void loop() {
  leds[0] = CRGB::Red; FastLED.show(); delay(500);
  leds[0] = CRGB::Blue; FastLED.show(); delay(500);
  }*/

    /*
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif
#define PIN        6
#define NUMPIXELS 16

Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
#define DELAYVAL 500

void setup() {
#if defined(__AVR_ATtiny85__) && (F_CPU == 16000000)
  clock_prescale_set(clock_div_1);
#endif

  pixels.begin();
}

void loop() {
  pixels.clear();

  for(int i=0; i<NUMPIXELS; i++) {

    pixels.setPixelColor(i, pixels.Color(0, 150, 0));
    pixels.show();
    delay(DELAYVAL);
  }
  }*/
}

void set_vext(bool b)
{
#if defined(VEXT)
    if (VEXT == U8X8_PIN_NONE)
        return;
    if (b) {
        pinMode(VEXT, OUTPUT);
        digitalWrite(VEXT, LOW);
    } else // pulled up, no need to drive it
        pinMode(VEXT, INPUT);
#endif
}

// ---------------------------------------------------------------------------
// deep sleep

#if defined(HELTEC_V2) || defined(HELTEC_V3) || defined(HELTEC_V4) || \
    defined(LILYGO_T3S3V12) || defined(LILYGO_T3S3V13) || defined(SEEED_XIAOS3)

void go_deep_sleep_now()
{
    if (Serial) Serial.printf("\r\ngoing deep sleep now\r\n\n");
    #if defined(USE_U8G2)
        u8g2.setPowerSave(false);
        u8g2.clearBuffer();
        u8g2.drawStr(0, 10,      "--> deep sleep mode");
        u8g2.updateDisplay();
        delay(1000);
        u8g2.setPowerSave(true);
    #endif

    // display.displayOff();
    #if defined(USE_LORA)
      // 'false' here is to not have a warm start, we re-init the after sleep.
      radio.sleep(); // false);
    #endif
    
    set_led(true); delay(500); set_led(false);
    set_vext(false);

    // Set all pins to input to save power
#if defined(HELTEC_V3) || defined(HELTEC_V4)
    pinMode(VBAT_CTRL, INPUT);
    pinMode(VBAT_ADC, INPUT);
    // digitalWrite(VBAT_CTRL, LOW);
#endif
#if defined(USE_LORA)
    pinMode(LED_PIN, INPUT);
    pinMode(DIO1, INPUT);
    pinMode(RST_LoRa, INPUT);
    pinMode(BUSY_LoRa, INPUT);
    pinMode(SS, INPUT);
    pinMode(MISO, INPUT);
    pinMode(MOSI, INPUT);
    pinMode(SCK, INPUT);
#endif
#if defined(USE_U8G2)
    pinMode(SDA_OLED, INPUT);
    pinMode(SCL_OLED, INPUT);
  #if defined(RST_OLED)
    pinMode(RST_OLED, INPUT);
  #endif
#endif

    // enable button wakeup
    esp_sleep_enable_ext0_wakeup(BUTTON_PIN, LOW);
    // wait button to be released, first
    while (digitalRead(BUTTON_PIN) == LOW) {}
    // start timer (one day) and then go to sleep
    esp_sleep_enable_timer_wakeup(1000000ULL * 24 * 60 * 60);
    // esp_sleep_enable_timer_wakeup(1000000ULL * 5); // test with 5sec sleep
    esp_deep_sleep_start();
}

bool coming_back_from_deep_sleep() {
    return esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER;
}

#else // defined(C3MINI) || defined(LILYGO_TBEAMS3)

void go_deep_sleep_now()
{
    if (Serial) Serial.println("ignoring deep sleep request\r\n");
}
bool coming_back_from_deep_sleep() { return false; }

#endif

// ---------------------------------------------------------------------------
// for 128x64 pixel screens

int current_screen;

#if defined(USE_U8G2)

void display_screen0_stats()
{
    if (!mlan) return; // wait for stats until initialized

    char buf[50];
    int tx_cnt = 0;
    int rx_cnt = 0;
    uint32_t sum = 0;

    for (int i = 0; i < mlan->if_cnt; i++) tx_cnt += mlan->if_table[i]->tx_cnt;
    for (int i = 0; i < mlan->if_cnt; i++) rx_cnt += mlan->if_table[i]->rx_cnt;

    for (int i = 0; i < mlan->cache_cnt; i++) {
        uint8_t *pkt = mlan->cache + MEMLAN_CLSZ*(mlan->cache_cnt - 1 - i);
        uint32_t h = murmur3_32(pkt+1, pkt[0], 9876);
        sum ^= h;
    }
    
#if !defined(C3MINI)
    sprintf(buf, "HL %d", mlan->hash_cnt);
    u8g2.drawStr(0, 53, buf);

    sprintf(buf, "CL %d", mlan->cache_cnt);
    u8g2.drawStr(128 - u8g2.getStrWidth(buf), 53, buf);

    sprintf(buf, "TX %d", tx_cnt % 10000);
    u8g2.drawStr(0, 64, buf);

    sprintf(buf, "RX %d", rx_cnt % 10000);
    u8g2.drawStr(128 - u8g2.getStrWidth(buf), 64, buf);
#else
    sprintf(buf, "HL=%d", mlan->hash_cnt);
    u8g2.drawStr(0, 18, buf);

    sprintf(buf, "CL=%d", mlan->cache_cnt);
    u8g2.drawStr(72 - u8g2.getStrWidth(buf), 18, buf);

    sprintf(buf, "t=%d", tx_cnt % 1000);
    u8g2.drawStr(0, 28, buf);

    sprintf(buf, "r=%d", rx_cnt % 1000);
    u8g2.drawStr(72 - u8g2.getStrWidth(buf), 28, buf);
#endif // C3MINI

    int btco = 0, btlr = 0, lora = 0, espnow = 0;
    for (int i = 0; i < mlan->if_cnt; i++) {
        IO_device *iod = mlan->if_table[i];
        int d = iod->get_neigh_cnt();
        #if defined(USE_BLE)
          if (!strcmp(iod->name, "BleLR")) {
              btlr += d;
              #if defined(USE_BLE_CO)
              btco += ((IO_dev_ble*)iod)->client_cnt;
              #endif
          }
        #endif
        #if defined(USE_ESPNOW)
          if (!strcmp(iod->name, "ESP-NOW")) espnow += d;
        #endif
        #if defined(USE_LORA)
          if (!strcmp(iod->name, "LoRa")) lora += d;
        #endif
    }

#if !defined(C3MINI)
    sprintf(buf, "%d/%d/%d/", btco, btlr, lora);
    #if defined(USE_ESPNOW)
      sprintf(buf + strlen(buf), "%d/", espnow);
    #endif
    sprintf(buf + strlen(buf), "%08x", sum);
    buf[strlen(buf) - 5] = '\0';
    u8g2.drawStr(64 - u8g2.getStrWidth(buf)/2, 53, buf);
#else
    int a = sprintf(buf, "CBE: %d %d %d", btco, btlr, espnow);
    u8g2.drawStr(0, 38, buf);

    sprintf(buf, "%08x", sum);
    buf[3] = '\0';
    u8g2.drawStr(72 - u8g2.getStrWidth(buf), 8, buf);
#endif
}

void display_screen0()
{
    char buf[60];
    sprintf(buf, "<%02x:%02x>", my_id[0], my_id[1]);

    u8g2.clearBuffer();

#if defined(C3MINI)
    u8g2.drawStr(0, 8, buf); // my_id
#else
    u8g2.drawStr(64 - u8g2.getStrWidth(buf)/2, 64, buf); // my_id

    u8g2.drawStr(0, 10, ">> memoryLAN/ESP32 <<");
    u8g2.drawStr(0, 21, " (C) cft, " SW_RELEASE_DATE);

  #if defined(USE_LORA)
    char freq[10], bw[10];
    if (FREQ >= 1000) sprintf(freq, "%.3fG", FREQ/1000.);
    else              sprintf(freq, "%.2fM", (float) FREQ);
    if (BW >= 1000)   sprintf(bw, "%.3fM", BW/1000.);
    else              sprintf(bw, "%.0fk", (float) BW);
    sprintf(buf, "%s %s %i %idBm", freq, bw, SF, xmitpow);
    u8g2.drawStr(64 - u8g2.getStrWidth(buf)/2, 32, buf);
  #endif

    strcpy(buf, "BTco/BTlr/LoRa/");
    #if defined(USE_ESPNOW)
      strcat(buf, "espN/");
    #endif
    strcat(buf, "CHK");
    u8g2.setFont(u8g2_font_micro_mr);
    u8g2.drawStr(64 - u8g2.getStrWidth(buf)/2, 43, buf);
    u8g2.setFont(u8g2_font_6x10_mr);
#endif // C3MINI
    
    display_screen0_stats();

}

void display_screen1()
{
    char buf[10];
    u8g2.clearBuffer();
    u8g2.drawStr(0, 10, "H");
    for (int i = 0; i < mlan->hash_cnt && i < 5; i++) {
        sprintf(buf, "%08x", mlan->hashfifo[mlan->hash_cnt - 1 - i]);
        u8g2.drawStr(10, 10 + i*10, buf);
    }
    if (mlan->hash_cnt > 5)
        u8g2.drawStr(10, 10 + 50, "...");

    u8g2.drawStr(64, 10, "C");
    uint32_t sum = 0;
    char *cp;
    for (int i = 0; i < mlan->cache_cnt; i++) {
        uint8_t *pkt = mlan->cache + MEMLAN_CLSZ*(mlan->cache_cnt - 1 - i);
        uint32_t h = murmur3_32(pkt+1, pkt[0], 9876);
        sum ^= h;
        if (i < 6) {
            sprintf(buf, "%08x", h);
            u8g2.drawStr(74, 10 + i*10, buf);
        }
    }
    sprintf(buf, "%08x", sum);
    buf[3] = '\0';
    u8g2.setFontDirection(3);
    u8g2.drawStr(70, 63, buf);
    u8g2.setFontDirection(0);
}

void display_screen2()
{
    u8g2.clearBuffer();
    u8g2.drawStr(0, 10, "m id    #n  rssi  sec");

    int cnt = 0;
    for (int i = 0; i < mlan->if_cnt; i++) {
        IO_device *iod = mlan->if_table[i];
        for (int j = 0; j < IODEV_MAX_NEIGH && cnt < 5; j++) {
            struct neigh_s *t = iod->neigh_tab + j;
            if (t->id[0] || t->id[1]) {
                uint32_t age = millis() - t->last_heard;
                if (age < 20000) {
                    char buf[50];
                    sprintf(buf, "%c %02x:%02x %2d  %4hhd  %3d",
                            iod->name[0], t->id[0], t->id[1],
                            t->neigh_cnt, t->rssi, -(int)(age/1000));
                    u8g2.drawStr(0, 20 + cnt*10, buf);
                    cnt++;
                }
            }
        }
    }
}

void display_screen3()
{
    u8g2.clearBuffer();
    u8g2.drawStr(0, 10, "Config");
    u8g2.drawStr(128 - u8g2.getStrWidth(BOARD_NAME), 10, BOARD_NAME);

    char buf[50];
/*    
#if defined(HELTEC_V3) || defined(HELTEC_V4)
    sprintf(buf, "bat=%.1f", get_vbat());
    u8g2.drawStr(87, 10, buf);
#endif
*/
    sprintf(buf, " #hash=%d #cache=%d", MEMLAN_HHIST_DEPTH, MEMLAN_CACHE_DEPTH);
    u8g2.drawStr(0, 23, buf);

    sprintf(buf, " BF %d/%d", mlan->get_BFwidth(), mlan->get_BFhashes());
    u8g2.drawStr(0, 34, buf);

#if defined(LILYGO_T3S3V12) || defined(LILYGO_T3S3V13)
    // https://github.com/Xinyuan-LilyGO/TTGO-T-Display/issues/35
    // digitalWrite(14, HIGH);
    pinMode(VBAT_ADC, INPUT);
    delay(1);
    float vbat = analogRead(VBAT_ADC) / 4095.0 * 7.26;
    // digitalWrite(14, LOW);    
    if (Serial) Serial.printf("   vbat=%f\r\n", vbat);

    sprintf(buf, "bat=%.1fV", vbat);
    u8g2.drawStr(128 - u8g2.getStrWidth(buf), 34, buf);
#endif
#if 0 // defined(HELTEC_V3) || defined(HELTEC_V4)
    // does not workl :-(
    pinMode(VBAT_ADC, INPUT);
analogSetClockDiv(1);
analogSetAttenuation(ADC_11db);
analogSetPinAttenuation(VBAT_ADC, ADC_11db);
adcAttachPin(VBAT_ADC);
    pinMode(VBAT_CTRL, OUTPUT);
    digitalWrite(VBAT_CTRL, LOW);
    delay(50);
// float get_vbat() {
    float vbat = analogRead(VBAT_ADC); //  * 5.42 * (3.3 / 1024.0); // / 238.7;
    if (Serial) Serial.printf("   vbat=%f\r\n", vbat);
    // pulled up, no need to drive it
    pinMode(VBAT_CTRL, INPUT);

    sprintf(buf, "bat=%.1fV", vbat);
    u8g2.drawStr(128 - u8g2.getStrWidth(buf), 34, buf);
#endif

    sprintf(buf, " beacon: ~%d msec", MEMLAN_NEXT_BEACON);
    u8g2.drawStr(0, 43, buf);

    sprintf(buf, " TO hash=%ds neigh=%ds",
            MEMLAN_HASH_TIMEOUT/1000, IODEV_NEIGH_TIMEOUT/1000);
    u8g2.drawStr(0, 53, buf);

    // sprintf(buf, " > %s %.5s", __DATE__, __TIME__);
    sprintf(buf, " > " COMPILE_DATE);
    u8g2.drawStr(0, 63, buf);
}

void display_redraw()
{
    switch(current_screen) {
    case 0:  display_screen0(); break;
    case 1:  display_screen1(); break;
    case 2:  display_screen2(); break;
    case 3:
    default: display_screen3(); break;
    }
    u8g2.updateDisplay();
}

#else // no screen

void display_redraw() {}

#endif // USE_U8G2

// ---------------------------------------------------------------------------

void board_setup()
{
#if defined(SCK)
    SPI.begin(SCK, MISO, MOSI, SS);
#endif
    set_vext(true);

    // pinMode(BUTTON_PIN, INPUT_PULLUP);

#if defined(RST_OLED)
    if (RST_OLED != U8X8_PIN_NONE) {
        pinMode(RST_OLED, OUTPUT);
        digitalWrite(RST_OLED, HIGH);
    }
#endif

#if defined(USE_U8G2)
    u8g2.setI2CAddress(2 * 0x3c);
    u8g2.begin();
    u8g2.setContrast(255);
    u8g2.setFont(u8g2_font_6x10_mr); // good size for Heltec devices
    display_redraw();
#endif

    if (Serial) Serial.printf("board_setup() done\r\n");
}

// eof
