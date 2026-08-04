// io_dev_lora.cpp

// (c) 2026-08-01 <christian.tschudin@unibas.ch> - MIT license

#include "board_config.h"

#if defined(USE_LORA)

#include "util.h"

int8_t xmitpow = 30; // will be clipped

volatile bool lora_fetching = false;
volatile bool lora_transmitting = false;
volatile bool lora_new_pkt = false;

void lora_rx_intr() { // interrupt handler
    if (lora_transmitting)
        return;
    if (lora_fetching)
        return;
    lora_new_pkt = true;
}

IO_dev_lora::IO_dev_lora()
{
    name = "LoRa";
    rx_cnt = tx_cnt = 0;
    memset(neigh_tab, 0, sizeof(neigh_tab));

    RADIOLIB_OR_HALT(radio.begin());

    radio.standby();
    RADIOLIB_OR_HALT(radio.setFrequency(FREQ));
    RADIOLIB_OR_HALT(radio.setBandwidth(BW));
    RADIOLIB_OR_HALT(radio.setSpreadingFactor(SF));
    // radio.checkOutputPower(30, &xmitpow);
    radio.checkOutputPower(30, &xmitpow);
    Serial.printf("   lora: clipped TX power, from 30 -> %d\r\n", xmitpow);
    RADIOLIB_OR_HALT(radio.setOutputPower(xmitpow));
#ifdef LILYGO_T3S3V12 // LR1121
    static const uint32_t rfswitch_dio_pins[] = {
        RADIOLIB_LR11X0_DIO5, RADIOLIB_LR11X0_DIO6,
        RADIOLIB_NC, RADIOLIB_NC, RADIOLIB_NC
    };
    static const Module::RfSwitchMode_t rfswitch_table[] = {
        // mode                  DIO5  DIO6
        { LR11x0::MODE_STBY,   { LOW,  LOW  } },
        { LR11x0::MODE_RX,     { HIGH, LOW  } },
        { LR11x0::MODE_TX,     { LOW,  HIGH } },
        { LR11x0::MODE_TX_HP,  { LOW,  HIGH } },
        { LR11x0::MODE_TX_HF,  { LOW,  LOW  } },
        { LR11x0::MODE_GNSS,   { LOW,  LOW  } },
        { LR11x0::MODE_WIFI,   { LOW,  LOW  } },
        END_OF_MODE_TABLE,
    };
    radio.setRfSwitchTable(rfswitch_dio_pins, rfswitch_table);
    // LR1121 TCXO Voltage 2.85~3.15V
    RADIOLIB_OR_HALT(radio.setTCXO(3.0));
#endif
#ifdef LILYGO_T3S3V13
    #define LORA_RX_EN GPIO_NUM_21
    #define LORA_TX_EN GPIO_NUM_10
    radio.setRfSwitchPins(LORA_RX_EN, LORA_TX_EN);
#endif

    int toa = radio.getTimeOnAir(120);
    Serial.printf("   lora: time on air (120B): %d usec\r\n", toa);

    radio.setPacketReceivedAction(lora_rx_intr);
    radio.startReceive();
}

uint8_t* IO_dev_lora::brcast_recv()
{
    if (!lora_new_pkt)
        return NULL;
    static uint8_t rxdata[128];

    lora_fetching = true;
    int len = radio.getPacketLength();
    if (len > sizeof(rxdata))
        len = sizeof(rxdata);
    if (_radiolib_status == RADIOLIB_ERR_NONE && len > 0)
        radio.readData(rxdata, len);
    else
        len = -1;
    lora_new_pkt = false;
    lora_fetching = false;
    radio.startReceive(); // RADIOLIB_SX126X_RX_TIMEOUT_INF);
    if (len < 0)
        return NULL;

    rx_cnt++;
    int rssi = radio.getRSSI();
    if (rssi < -128) rssi = -128;
    if (Serial) Serial.printf("l> %dB rx=%d rssi=%d\r\n", len, rx_cnt, rssi);

    if (crc_check(rxdata, len)) {
        if (Serial) Serial.printf("   bad CRC\r\n");
        return NULL;
    }
    len -= sizeof(uint32_t);

    uint8_t *pkt = (uint8_t*) malloc(len+2);
    if (!pkt)
        return NULL;
    pkt[0] = len;
    memcpy(pkt+1, rxdata, len);
    *(int8_t*) (pkt+len+1) = rssi;

    return pkt;
}

void IO_dev_lora::brcast_send(const uint8_t *pkt, char *comment)
{
    // LoRa: add CRC
    int len = *pkt;
    if (len > 120)
        return;
    static uint8_t txbuf[124];
    memcpy(txbuf, pkt+1, len);
    uint32_t crc = crc32_ieee(txbuf, len);
    memcpy(txbuf+len, &crc, sizeof(crc));
    len += sizeof(crc);

    // if (!dark_mode) set_led(20);
    lora_transmitting = true;
    RADIOLIB(radio.transmit(txbuf, len));
    lora_transmitting = false;
    // set_led(0);
    radio.startReceive();
    tx_cnt++;

    if (Serial) Serial.printf(comment ? "l< %dB (%s)\r\n" : "l< %dB\r\n",
                              len, comment);
}

#endif //USE_LORA

// eof
