// board_config.h

// (c) 2026-08-01 <christian.tschudin@unibas.ch> - MIT license

// ---------------------------------------------------------------------------
// board-specific selection of features/availability, where:
//   BLE    = Bluetooth Long Range - extended advertisements / PYH_CODED
//   BLE_CO = BLE Connection-Oriented - old tinySSB style, attach a device
//   ESPNOW = Espressif's peer-to-peer WiFi stack, not impressed
//   LORA   = LoRa
//   U8G2   = OLED

// By default, all available functionalities of a board are enabled
// except ESPNOW. Uncomment those USE_XXX which are not desirable

#define SW_RELEASE_DATE "2026-08-01"

// WARNING: do not enable BLE/BLE_CO and ESPNOW: both stacks use the
// the same radio HW and performance of ESPNOW is close to 0 ...

#if defined(HELTEC_V2) // this device can't do extended BT advert./PHY_CODED
  #define BOARD_NAME "Heltec-v2"
  #define USE_BLE_CO
  // #define USE_ESPNOW
  #define USE_LORA
  #define USE_U8G2
#elif defined(HELTEC_V3)
  #define BOARD_NAME "Heltec-v3"
  #define USE_BLE
  #define USE_BLE_CO
  // #define USE_ESPNOW
  #define USE_LORA
  #define USE_U8G2
#elif defined(HELTEC_V4)
  #define BOARD_NAME "Heltec-v4"
  #define USE_BLE
  #define USE_BLE_CO
  // #define USE_ESPNOW
  #define USE_LORA
  #define USE_U8G2
#elif defined(C3MINI)
  #define BOARD_NAME "C3supermini"
  #define USE_BLE
  #define USE_BLE_CO
  // #define USE_ESPNOW
  #define USE_U8G2
#elif defined(LILYGO_TBEAMS3) // SX1862
  #define BOARD_NAME "TBeam-Supr."
  #define USE_BLE
  #define USE_BLE_CO
  // #define USE_ESPNOW
  #define USE_LORA
  #define USE_U8G2
#elif defined(LILYGO_T3S3V12) // LR1121, Lora 2.4Ghz
  #define BOARD_NAME "T3S3-v1.2"
  #define USE_BLE
  #define USE_BLE_CO
  // #define USE_ESPNOW
  #define USE_LORA
  #define USE_U8G2
#elif defined(LILYGO_T3S3V13) // SX1280, Lora 2.4Ghz
  #define BOARD_NAME "T3S3-v1.3"
  #define USE_BLE
  #define USE_BLE_CO
  // #define USE_ESPNOW
  #define USE_LORA
  #define USE_U8G2
#endif // boards

// ---------------------------------------------------------------------------
// feature-specific includes and default values

#if defined(USE_BLE) || defined(USE_BLE_CO)
  #include <NimBLEDevice.h>
  #include <NimBLEExtAdvertising.h>

  #if !MYNEWT_VAL(BLE_EXT_ADV)
    #error Must enable extended advertising, see nimconfig.h file.
  #endif

  #include "io_dev_ble.h"
#endif // USE_BLE_*

#if defined(USE_ESPNOW)
  #include "io_dev_espnow.h"
#endif

#if defined(USE_LORA)
  #include "RadioLib.h"
  #include "RadioLib_convenience.h"

  #if defined(LILYGO_T3S3V13) // SX1280 2.4GHz Ranging + PA
    #define FREQ     2440.00 // in MHz
    #define BW       (2*203.125)  // 203.125 // (4*203.125)
    #define SF       7
  #elif defined(LILYGO_T3S3V12) // LR1121, 2.4GHz
    #define FREQ     2440.00 // in MHz
    #define BW       500.
    #define SF       7
  #else
    #define FREQ     867.67 // 902.4 // in MHz
    #define BW       500.0 // 7.8, 10.4, 15.6, 20.8, 31.25, 41.7, 62.5, 125.0, 250.0 and 500.0 kHz
    #define SF       7
  #endif

  extern int8_t xmitpow;

  #include "io_dev_lora.h"
#endif // USE_LORA

#if defined(USE_U8G2)
  #include "U8g2lib.h"
#endif

// ---------------------------------------------------------------------------
// board-specific globals

#if defined(HELTEC_V2)
  extern SX1276 radio;
  extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
#elif defined(HELTEC_V3)
  extern SX1262 radio;
  extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
#elif defined(HELTEC_V4)
  extern SX1262 radio;
  // extern u8g2
#elif defined(C3MINI)
  extern U8G2_SSD1306_72X40_ER_F_SW_I2C u8g2;
#elif defined(LILYGO_TBEAMS3) //
  extern SX1262 radio;
  extern U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2;
#elif defined(LILYGO_T3S3V12) // LR1121
  extern LR1121 radio;
  extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
#elif defined(LILYGO_T3S3V13) // Lora 2.4Ghz
  extern SX1280 radio;
  extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;
#endif // boards

// eof
