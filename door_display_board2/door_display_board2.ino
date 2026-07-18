/* =====================================================================
   DOOR DISPLAY  -  Board 2 of 2   (NEW HARDWARE)
   DIYmalls ESP32-2424S012C-I : ESP32-C3 + onboard 1.28" GC9A01 round LCD
   ---------------------------------------------------------------------
   Drop-in replacement for the old "ESP32-S3 + separate GC9A01". Same
   ESP-NOW protocol, PAIRING_ID and channel, so it still pairs with the
   Board 1 door sensor with NO changes to Board 1.

   The screen is wired to the ESP32-C3 internally (fixed pins) - nothing
   to solder. It shows:
     DOOR OPEN   -> full-screen RED "OPEN", backlight full
     DOOR CLOSED -> DIMMED screen, "CLOSED", small green alive-pulse
     LINK ERROR  -> full-screen AMBER "NO SIGNAL" with a blinking rim
     PAIRING     -> shown at power-up until the first packet arrives

   Arduino IDE:
     Board:            "ESP32C3 Dev Module"
     USB CDC On Boot:  Enabled
     Flash Size:       4MB (32Mb)
   Library: "GFX Library for Arduino" (Arduino_GFX) by moononournation.
   Power:   5V via the USB-C port. NOTE: use a USB-C -> USB-A cable; a
            USB-C -> USB-C cable often will NOT power/enumerate this board.
   ===================================================================== */

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Arduino_GFX_Library.h>

// Screen states (declared before any function for the .ino auto-prototypes)
enum ScreenState { S_BOOT, S_CLOSED, S_OPEN, S_ERROR };

// ----------------------------- USER CONFIG -----------------------------
#define PAIRING_ID   0x42    // MUST match Board 1
#define WIFI_CHANNEL 1       // must match Board 1

// Onboard GC9A01 pins for the ESP32-2424S012 (fixed by the board - do not change)
#define LCD_DC     2
#define LCD_CS    10
#define LCD_MOSI   7
#define LCD_SCK    6
#define LCD_RST   GFX_NOT_DEFINED   // this board has no display reset pin
#define LCD_BL     3               // backlight (PWM-dimmable)
#define LCD_ROTATION 2             // 0/1/2/3 = 0/90/180/270 deg; 2 = flipped (upside down)

const uint32_t HEARTBEAT_MS    = 1500;  // how often we ping Board 1 back
const uint32_t LINK_TIMEOUT_MS = 5000;  // no packet within this -> error screen
const uint8_t  BL_FULL = 255;           // backlight for OPEN / ERROR / BOOT
const uint8_t  BL_DIM  = 24;            // dim backlight for CLOSED
// -----------------------------------------------------------------------

Arduino_DataBus *bus = new Arduino_ESP32SPI(LCD_DC, LCD_CS, LCD_SCK, LCD_MOSI, GFX_NOT_DEFINED /*MISO*/);
Arduino_GFX     *gfx = new Arduino_GC9A01(bus, LCD_RST, LCD_ROTATION, true /*IPS*/);

// ---- backlight PWM (API differs between core 2.x and 3.x) ----
#define BL_FREQ 5000
#define BL_RES  8
#define BL_CH   0
void blSetup() {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(LCD_BL, BL_FREQ, BL_RES);
#else
  ledcSetup(BL_CH, BL_FREQ, BL_RES);
  ledcAttachPin(LCD_BL, BL_CH);
#endif
}
void blWrite(uint8_t duty) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(LCD_BL, duty);
#else
  ledcWrite(BL_CH, duty);
#endif
}

// ---- ESP-NOW wire message (IDENTICAL struct on BOTH boards) ----
typedef struct __attribute__((packed)) {
  uint8_t  pairId;
  uint8_t  msgType;   // 0 = door state (from sensor), 1 = ack/heartbeat (from us)
  uint8_t  doorOpen;  // 1 = open, 0 = closed
  uint32_t seq;
} Msg;

static uint8_t broadcastMac[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

// ---- runtime state (written from the ESP-NOW receive callback) ----
volatile uint32_t lastRxMs    = 0;   // last packet from Board 1 (0 = never)
volatile bool     doorOpen    = false;
volatile bool     pendingAck  = false;
uint32_t lastTxMs   = 0;
uint32_t seqCounter = 0;

ScreenState current = S_BOOT;
bool alivePrev = false;
bool errRingOn = false;

uint16_t cRedBg, cWhite, cDimBg, cClosedFg, cAmber, cDarkOnAmber, cBootBg, cBootFg, cAliveOn, cAliveOff;

inline bool linkUp() { return (millis() - lastRxMs) < LINK_TIMEOUT_MS; }

// ---------- drawing helpers ----------
void centerText(const char *s, uint8_t size, int16_t yTop, uint16_t color) {
  gfx->setTextSize(size);
  gfx->setTextColor(color);
  int16_t w = (int16_t)strlen(s) * 6 * size;   // 6 px advance per char
  gfx->setCursor(120 - w / 2, yTop);
  gfx->print(s);
}

void drawDoorIcon(int16_t cx, int16_t cy, bool open, uint16_t col) {
  gfx->drawRect(cx - 24, cy - 32, 48, 64, col);          // door frame
  if (!open) {
    gfx->drawRect(cx - 19, cy - 27, 38, 54, col);        // closed panel
    gfx->fillCircle(cx + 11, cy, 2, col);                // knob
  } else {
    gfx->drawLine(cx - 19, cy - 27, cx - 3, cy - 33, col);
    gfx->drawLine(cx - 3,  cy - 33, cx - 3, cy + 33, col);
    gfx->drawLine(cx - 3,  cy + 33, cx - 19, cy + 27, col);
    gfx->drawLine(cx - 19, cy - 27, cx - 19, cy + 27, col);
  }
}

void drawWarnIcon(int16_t cx, int16_t cy, uint16_t col) {
  gfx->drawTriangle(cx, cy - 26, cx - 30, cy + 24, cx + 30, cy + 24, col);
  gfx->drawTriangle(cx, cy - 20, cx - 24, cy + 20, cx + 24, cy + 20, col);
  gfx->fillRect(cx - 2, cy - 8, 4, 18, col);   // "!" bar
  gfx->fillRect(cx - 2, cy + 14, 4, 4, col);   // "!" dot
}

void drawScreen(ScreenState s) {
  switch (s) {
    case S_OPEN:
      blWrite(BL_FULL);
      gfx->fillScreen(cRedBg);
      drawDoorIcon(120, 76, true, cWhite);
      centerText("OPEN", 6, 122, cWhite);
      break;

    case S_CLOSED:
      blWrite(BL_DIM);                    // dim the screen when closed
      gfx->fillScreen(cDimBg);
      drawDoorIcon(120, 78, false, cClosedFg);
      centerText("CLOSED", 4, 126, cClosedFg);
      alivePrev = false;
      gfx->fillCircle(120, 196, 6, cAliveOff);   // baseline "alive" dot
      break;

    case S_ERROR:
      blWrite(BL_FULL);
      gfx->fillScreen(cAmber);
      drawWarnIcon(120, 84, cDarkOnAmber);
      centerText("NO SIGNAL", 3, 132, cDarkOnAmber);
      centerText("CHECK SENSOR", 2, 168, cDarkOnAmber);
      break;

    case S_BOOT:
    default:
      blWrite(BL_FULL);
      gfx->fillScreen(cBootBg);
      centerText("PAIRING", 4, 96, cBootFg);
      centerText("...", 4, 140, cBootFg);
      break;
  }
}

// ---------- ESP-NOW ----------
void sendHeartbeat() {
  Msg m;
  m.pairId   = PAIRING_ID;
  m.msgType  = 1;               // ack / heartbeat from the display
  m.doorOpen = 0;
  m.seq      = ++seqCounter;
  esp_now_send(broadcastMac, (const uint8_t *)&m, sizeof(m));
  lastTxMs = millis();
}

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
void onDataRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
#else
void onDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
#endif
  if (len != sizeof(Msg)) return;
  Msg m;
  memcpy(&m, data, sizeof(m));
  if (m.pairId != PAIRING_ID) return;
  if (m.msgType == 0) {                 // door state from the sensor
    doorOpen   = (m.doorOpen != 0);
    lastRxMs   = millis();
    pendingAck = true;                  // reply from loop(), not from the callback
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  // display
  gfx->begin();
  blSetup();
  cRedBg       = gfx->color565(220, 0, 0);
  cWhite       = gfx->color565(255, 255, 255);
  cDimBg       = gfx->color565(0, 0, 0);
  cClosedFg    = gfx->color565(85, 110, 92);
  cAmber       = gfx->color565(255, 170, 0);
  cDarkOnAmber = gfx->color565(40, 25, 0);
  cBootBg      = gfx->color565(6, 8, 14);
  cBootFg      = gfx->color565(120, 140, 200);
  cAliveOn     = gfx->color565(0, 190, 70);
  cAliveOff    = gfx->color565(10, 14, 12);

  current = S_BOOT;
  drawScreen(current);

  // radio
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed - restarting");
    delay(1000);
    ESP.restart();
  }
  esp_now_register_recv_cb(onDataRecv);   // no send callback needed for broadcast

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, broadcastMac, 6);
  peer.channel = WIFI_CHANNEL;
  peer.encrypt = false;
  esp_now_add_peer(&peer);

  Serial.print("Door DISPLAY ready (ESP32-C3). This board's MAC: ");
  Serial.println(WiFi.macAddress());
}

void loop() {
  const uint32_t now = millis();

  // reply to any door-state packet, then keep a steady heartbeat going
  if (pendingAck)                      { pendingAck = false; sendHeartbeat(); }
  if ((now - lastTxMs) > HEARTBEAT_MS) { sendHeartbeat(); }

  // ---- decide what to show ----
  ScreenState desired;
  if (lastRxMs == 0)   desired = S_BOOT;    // nothing received yet
  else if (!linkUp())  desired = S_ERROR;   // had a link, lost it
  else                 desired = doorOpen ? S_OPEN : S_CLOSED;

  if (desired != current) {
    current = desired;
    drawScreen(current);
  }

  // ---- "alive" pulse on the dimmed CLOSED screen ----
  if (current == S_CLOSED) {
    bool aliveOn = (now - lastRxMs) < 250;       // lit briefly after each packet
    if (aliveOn != alivePrev) {
      alivePrev = aliveOn;
      gfx->fillCircle(120, 196, 6, aliveOn ? cAliveOn : cAliveOff);
    }
  }

  // ---- attention blink on the ERROR screen (rim ring toggles ~1 Hz) ----
  if (current == S_ERROR) {
    bool on = ((now / 500) % 2) == 0;
    if (on != errRingOn) {
      errRingOn = on;
      uint16_t c = on ? cDarkOnAmber : cAmber;
      for (int r = 118; r >= 112; r--) gfx->drawCircle(120, 120, r, c);
    }
  }

  delay(5);
}
