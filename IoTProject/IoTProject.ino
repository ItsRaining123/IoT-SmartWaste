#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>

// MQ2 Sensor Pins
#define MQ2_ANALOG_PIN A0
#define MQ2_DIGITAL_PIN 9

// LoRaWAN credentials
static const u1_t PROGMEM APPEUI[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; // dummy credential
void os_getArtEui (u1_t* buf) { memcpy_P(buf, APPEUI, 8); }

static const u1_t PROGMEM DEVEUI[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; // dummy credential
void os_getDevEui (u1_t* buf) { memcpy_P(buf, DEVEUI, 8); }

static const u1_t PROGMEM APPKEY[16] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; // dummy credential
void os_getDevKey (u1_t* buf) { memcpy_P(buf, APPKEY, 16); }

// LoRaWAN job and timing
static osjob_t sendjob;
const unsigned TX_INTERVAL = 3;

// Pin mapping for LoRa
const lmic_pinmap lmic_pins = {
    .nss = 10,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = 7,
    .dio = {2, 5, 6},
};

// Latency & Packet Loss Variables
unsigned long lastSendTime = 0;
unsigned long latency = 0;
bool awaitingAck = false;
int packetsSent = 0;
int packetsAcked = 0;

// Hex print helper
void printHex2(unsigned v) {
    v &= 0xff;
    if (v < 16) Serial.print('0');
    Serial.print(v, HEX);
}

// LMIC event handler
void onEvent (ev_t ev) {
    Serial.print(os_getTime());
    Serial.print(": ");
    switch(ev) {
        case EV_JOINING:
            Serial.println(F("EV_JOINING"));
            break;
        case EV_JOINED:
            Serial.println(F("EV_JOINED"));
            {
                u4_t netid = 0;
                devaddr_t devaddr = 0;
                u1_t nwkKey[16];
                u1_t artKey[16];
                LMIC_getSessionKeys(&netid, &devaddr, nwkKey, artKey);
                Serial.print("netid: "); Serial.println(netid, DEC);
                Serial.print("devaddr: "); Serial.println(devaddr, HEX);
                Serial.print("AppSKey: ");
                for (size_t i = 0; i < sizeof(artKey); ++i) {
                    if (i != 0) Serial.print("-");
                    printHex2(artKey[i]);
                }
                Serial.println();
                Serial.print("NwkSKey: ");
                for (size_t i = 0; i < sizeof(nwkKey); ++i) {
                    if (i != 0) Serial.print("-");
                    printHex2(nwkKey[i]);
                }
                Serial.println();
            }
            //LMIC_setDrTxpow(DR_SF7, 14);

            LMIC_setLinkCheckMode(0);

            break;
        case EV_TXCOMPLETE:
            Serial.println(F("EV_TXCOMPLETE"));
            if (awaitingAck) {
                latency = millis() - lastSendTime;
                Serial.print(F("Latency (ms): "));
                Serial.println(latency);
                awaitingAck = false;
            }

            if (LMIC.txrxFlags & TXRX_ACK) {
                Serial.println(F("Received ACK"));
                packetsAcked++;
            } else {
                Serial.println(F("No ACK received"));
            }

            float lossRate = (packetsSent > 0) ? 100.0 * (packetsSent - packetsAcked) / packetsSent : 0.0;
            Serial.print(F("Packets Sent: ")); Serial.println(packetsSent);
            Serial.print(F("Packets Acked: ")); Serial.println(packetsAcked);
            Serial.print(F("Packet Loss (%): ")); Serial.println(lossRate);

            if (LMIC.dataLen) {
                Serial.print(F("Received "));
                Serial.print(LMIC.dataLen);
                Serial.println(F(" bytes of payload"));
            }

            os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(TX_INTERVAL), do_send);
            break;
        case EV_JOIN_FAILED:
            Serial.println(F("EV_JOIN_FAILED"));
            break;
        case EV_REJOIN_FAILED:
            Serial.println(F("EV_REJOIN_FAILED"));
            break;
        default:
            Serial.print(F("Unknown event: "));
            Serial.println((unsigned) ev);
            break;
    }
}

// Read MQ2 sensor
int readMQ2Analog() {
    int val = analogRead(MQ2_ANALOG_PIN);
    Serial.print("Analog Gas Value: ");
    Serial.println(val);
    return val;
}

int readMQ2Digital() {
    int state = digitalRead(MQ2_DIGITAL_PIN);
    Serial.print("Digital Gas Detected: ");
    Serial.println(state ? "NO" : "YES");
    return state;
}

// Send data via LoRa
void do_send(osjob_t* j) {
    if (LMIC.opmode & OP_TXRXPEND) {
        Serial.println(F("OP_TXRXPEND, not sending"));
    } else {
        int analogVal = readMQ2Analog();    // 0–1023
        int digitalVal = readMQ2Digital();  // 1 = safe, 0 = gas detected

        uint8_t payload[3];
        payload[0] = (analogVal >> 8) & 0xFF;
        payload[1] = analogVal & 0xFF;
        payload[2] = digitalVal;

        packetsSent++;
        lastSendTime = millis();  // Timestamp before send
        awaitingAck = true;

        LMIC_setTxData2(1, payload, sizeof(payload), 1);  // Confirmed uplink
        Serial.println(F("Gas packet queued (confirmed)"));
    }
}

void setup() {
    Serial.begin(9600);
    Serial.println(F("Starting MQ2 Flying Fish with LoRa"));

    pinMode(MQ2_ANALOG_PIN, INPUT);
    pinMode(MQ2_DIGITAL_PIN, INPUT);

    os_init();
    LMIC_reset();

    do_send(&sendjob);
}

void loop() {
    os_runloop_once();
}


