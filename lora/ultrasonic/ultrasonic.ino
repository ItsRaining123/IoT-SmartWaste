#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>

// Ultrasonic sensor pin definitions
#define TRIG_PIN 8  // Trig pin connected to D8
#define ECHO_PIN 9  // Echo pin connected to D9

// LoRaWAN credentials (replace with your own TTN values)
static const u1_t PROGMEM APPEUI[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; // dummy credential
void os_getArtEui (u1_t* buf) { memcpy_P(buf, APPEUI, 8); }

//https://www.mobilefish.com/download/lora/eui_key_converter.html
static const u1_t PROGMEM DEVEUI[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; // dummy credential
void os_getDevEui (u1_t* buf) { memcpy_P(buf, DEVEUI, 8); }

static const u1_t PROGMEM APPKEY[16] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; // dummy credential
void os_getDevKey (u1_t* buf) { memcpy_P(buf, APPKEY, 16); }

// LoRaWAN job and timing
static osjob_t sendjob;
const unsigned TX_INTERVAL = 3; 
// Pin mapping for LoRa module (adjust if necessary)
const lmic_pinmap lmic_pins = {
    .nss = 10,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = 7,
    .dio = {2, 5, 6},
};

// Helper function to print hex values
void printHex2(unsigned v) {
    v &= 0xff;
    if (v < 16)
        Serial.print('0');
    Serial.print(v, HEX);
}

// ⭐ Stats and latency tracking
unsigned long txStartTime = 0;
int packetCount = 0;
int acknowledgedPackets = 0;
int lostPackets = 0;

// LoRaWAN event handler
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
                Serial.println("");
                Serial.print("NwkSKey: ");
                for (size_t i = 0; i < sizeof(nwkKey); ++i) {
                    if (i != 0) Serial.print("-");
                    printHex2(nwkKey[i]);
                }
                Serial.println();
            }
            LMIC_setLinkCheckMode(0);  // Disable link check
            break;
        case EV_TXCOMPLETE:
            Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));

            // ⭐ Latency calculation
            unsigned long latency = millis() - txStartTime;
            Serial.print("Latency: ");
            Serial.print(latency);
            Serial.println(" ms");

            // ⭐ ACK detection
            if (LMIC.txrxFlags & TXRX_ACK) {
                Serial.println(F("Received ACK"));
                acknowledgedPackets++;
            } else {
                Serial.println(F("No ACK → likely packet loss"));
                lostPackets++;
            }

            // ⭐ Print packet stats
            Serial.print("Packets Sent: "); Serial.println(packetCount);
            Serial.print("Acknowledged: "); Serial.println(acknowledgedPackets);
            Serial.print("Lost (no ACK): "); Serial.println(lostPackets);

            if (LMIC.dataLen) {
                Serial.print(F("Received "));
                Serial.print(LMIC.dataLen);
                Serial.println(F(" bytes of payload"));
            }

            // Schedule next transmission
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

// Function to read distance from ultrasonic sensor
float readUltrasonicDistance() {
    long duration;
    float distance;

    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    duration = pulseIn(ECHO_PIN, HIGH);
    distance = (duration * 0.0343) / 2;  // Distance in cm

    return distance;
}

// Function to send data via LoRaWAN
void do_send(osjob_t* j) {
    if (LMIC.opmode & OP_TXRXPEND) {
        Serial.println(F("OP_TXRXPEND, not sending"));
    } else {
        float distance = readUltrasonicDistance();
        Serial.print("Distance: ");
        Serial.print(distance);
        Serial.println(" cm");

        // Convert to scaled int for sending
        uint16_t distance_int = (uint16_t)(distance * 10);
        uint8_t payload[2];
        payload[0] = (distance_int >> 8) & 0xFF;
        payload[1] = distance_int & 0xFF;

        // ⭐ Track stats
        txStartTime = millis();
        packetCount++;

        // Send to TTN
        LMIC_setTxData2(1, payload, sizeof(payload), 1);
        Serial.println(F("Packet queued"));
    }
}

// Setup
void setup() {
    Serial.begin(9600);
    Serial.println(F("Starting"));

    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);

    os_init();
    LMIC_reset();

    do_send(&sendjob);
}

// Loop
void loop() {
    os_runloop_once();
}
