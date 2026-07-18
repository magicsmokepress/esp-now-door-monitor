/* =====================================================================
   DOOR SENSOR  —  Board 1 of 2
   ESP32-S3-DevKitC-1 (N16R2)  +  reed switch
   ---------------------------------------------------------------------
   Detects door open/closed with a reed switch and broadcasts the state
   to Board 2 (the display) over ESP-NOW. The onboard WS2812 RGB LED shows
   pairing + communication status:
       GREEN  = paired (partner is alive)
       RED    = not paired / link lost
       BLUE   = brief flash on every message sent or received
   An optional external status LED (GPIO 18) is lit while the door is CLOSED.
   ---------------------------------------------------------------------
   Pairing model: AUTO-PAIR. Both boards broadcast and only accept
   packets carrying the same PAIRING_ID. No MAC addresses to configure.
   (See README for how to lock to specific MACs instead.)

   Libraries: none beyond the ESP32 Arduino core (WiFi + ESP-NOW built in).
   Board:     "ESP32S3 Dev Module"  (see README for full IDE settings)
   ===================================================================== */

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

// ----------------------------- USER CONFIG -----------------------------
#define PAIRING_ID            0x42   // MUST match Board 2. Change both to run multiple pairs nearby.
#define REED_PIN              4      // reed switch: one leg here, other leg to GND (internal pullup used)
#define DOOR_CLOSED_WHEN_LOW  true   // magnet present (door closed) pulls pin LOW. Set false to invert.
#define RGB_LED_PIN           48     // onboard WS2812. If LED never lights, try 38 (DevKitC-1 v1.1).
#define STATUS_LED_PIN        18     // external "door CLOSED" LED  (pin -> ~330R -> LED -> GND)
#define STATUS_LED_ACTIVE_HIGH true  // LED on = door CLOSED. Set false if wired inverted.
#define WIFI_CHANNEL          1      // must match Board 2

const uint32_t HEARTBEAT_MS    = 1500;  // resend current state at least this often
const uint32_t LINK_TIMEOUT_MS = 5000;  // no reply within this -> link considered down
const uint32_t DEBOUNCE_MS     = 40;    // reed switch debounce window
const uint32_t BLINK_MS        = 90;    // comms-flash duration
// -----------------------------------------------------------------------

static uint8_t broadcastMac[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

// Wire message — identical struct on BOTH boards.
typedef struct __attribute__((packed)) {
  uint8_t  pairId;    // shared pairing id
  uint8_t  msgType;   // 0 = door state (sensor -> display), 1 = ack/heartbeat (display -> sensor)
  uint8_t  doorOpen;  // 1 = open, 0 = closed  (valid when msgType == 0)
  uint32_t seq;       // sequence counter (debug)
} Msg;

// --- runtime state ---
volatile uint32_t lastRxMs = 0;     // last time we heard the partner (an ack/heartbeat)
uint32_t lastTxMs    = 0;
uint32_t lastBlinkMs = 0;
uint32_t seqCounter  = 0;

bool doorStateValid  = false;
bool lastDoorOpen    = false;

int      reedStable   = -1;         // debounced level
int      reedLastRead = -1;
uint32_t reedChangeMs = 0;

// --- onboard RGB LED (neopixelWrite is built into the ESP32 core) ---
inline void led(uint8_t r, uint8_t g, uint8_t b) { neopixelWrite(RGB_LED_PIN, r, g, b); }

inline bool linkUp() { return (millis() - lastRxMs) < LINK_TIMEOUT_MS; }

void sendDoorState(bool open) {
  Msg m;
  m.pairId   = PAIRING_ID;
  m.msgType  = 0;
  m.doorOpen = open ? 1 : 0;
  m.seq      = ++seqCounter;
  esp_now_send(broadcastMac, (const uint8_t *)&m, sizeof(m));
  lastTxMs    = millis();
  lastBlinkMs = millis();            // flash on TX
}

// ESP-NOW receive callback — signature changed in core 3.x.
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
void onDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
#else
void onDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
#endif
  if (len != sizeof(Msg)) return;
  Msg m;
  memcpy(&m, data, sizeof(m));
  if (m.pairId != PAIRING_ID) return;      // ignore other pairs / stray traffic
  if (m.msgType == 1) {                     // ack / heartbeat from the display board
    lastRxMs    = millis();
    lastBlinkMs = millis();                 // flash on RX
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  pinMode(REED_PIN, INPUT_PULLUP);
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, STATUS_LED_ACTIVE_HIGH ? LOW : HIGH);  // off until door state is known
  led(40, 0, 0);                            // start RED (not yet paired)

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed — restarting");
    delay(1000);
    ESP.restart();
  }
  esp_now_register_recv_cb(onDataRecv);   // no send callback: link is tracked via received heartbeats

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, broadcastMac, 6);
  peer.channel = WIFI_CHANNEL;
  peer.encrypt = false;
  esp_now_add_peer(&peer);

  Serial.print("Door SENSOR ready. This board's MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.println("Broadcasting door state via ESP-NOW.");
}

void loop() {
  const uint32_t now = millis();

  // ---- debounce the reed switch ----
  int raw = digitalRead(REED_PIN);
  if (raw != reedLastRead) { reedLastRead = raw; reedChangeMs = now; }
  if ((now - reedChangeMs) > DEBOUNCE_MS && raw != reedStable) {
    reedStable  = raw;
    bool closed = DOOR_CLOSED_WHEN_LOW ? (reedStable == LOW) : (reedStable == HIGH);
    bool open   = !closed;
    if (!doorStateValid || open != lastDoorOpen) {
      lastDoorOpen   = open;
      doorStateValid = true;
      sendDoorState(open);                 // report immediately on change
      Serial.printf("Door %s\n", open ? "OPEN" : "CLOSED");
    }
  }

  // ---- periodic heartbeat so the display always knows we're alive ----
  if (doorStateValid && (now - lastTxMs) > HEARTBEAT_MS) {
    sendDoorState(lastDoorOpen);
  }

  // ---- external status LED: lit while the door is CLOSED ----
  bool closedNow  = doorStateValid && !lastDoorOpen;
  bool statusLit  = STATUS_LED_ACTIVE_HIGH ? closedNow : !closedNow;
  digitalWrite(STATUS_LED_PIN, statusLit ? HIGH : LOW);

  // ---- onboard LED: link + comms indicator ----
  if ((now - lastBlinkMs) < BLINK_MS)      led(0, 0, 70);   // comms flash (blue)
  else if (linkUp())                       led(0, 45, 0);   // paired (green)
  else                                     led(45, 0, 0);   // not paired (red)

  delay(5);
}
