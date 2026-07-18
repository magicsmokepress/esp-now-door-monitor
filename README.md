# ESP-NOW Door Monitor (ESP32-S3 sensor + ESP32-C3 display)

Two ESP32 boards that auto-pair over **ESP-NOW**:

- **Board 1 — Door Sensor:** a reed switch detects door open/closed and broadcasts the state.
- **Board 2 — Display:** a DIYmalls **ESP32-2424S012C-I** (ESP32-C3 with an onboard 1.28" round GC9A01 LCD) shows the door status.

No router or Wi-Fi network is used — the boards talk directly to each other.

## Why

Built because the garage door kept getting left open. Board 1 clips to the door; Board 2's round screen sits wherever you'll actually notice it — kitchen counter, hallway, by the back door — and glows a full-screen red **OPEN** until the door is shut. A glanceable, no-app, no-account nudge to close the door, with an at-a-glance "is it still working?" alive-pulse so a dark screen never means *dead* vs *closed*.

---

## Behavior

**Onboard RGB LED (both boards)** — your "are they alive?" indicator:

| Color | Meaning |
|---|---|
| 🟢 Green | Paired — partner is alive |
| 🔴 Red | Not paired / link lost |
| 🔵 Blue flash | A message was just sent or received |

**Round display (Board 2):**

| State | What you see |
|---|---|
| Door **OPEN** | Full-screen **red** alert, door icon, "OPEN" |
| Door **CLOSED** | **Dimmed** screen, "CLOSED", and a small green dot that pulses on each packet — proof the board is alive, not blank/dead |
| **Link error** | **Amber** "NO SIGNAL / CHECK SENSOR" screen with a **blinking rim** — deliberately different from red OPEN and from a dark CLOSED / blank screen |
| **Pairing** | Shown at power-up until the first packet arrives |

---

## Parts

- 1 × ESP32-S3-DevKitC-1 (N16R2) — **Board 1**, the door sensor
- 1 × DIYmalls **ESP32-2424S012C-I** (ESP32-C3 + onboard 1.28" GC9A01 round display) — **Board 2**, the display
- 1 × reed switch (door/window type, with magnet) — normally-open is typical
- 1 × LED + ~330 Ω resistor — Board 1 door-closed status light
- Jumper wires; a **USB-C → USB-A** cable to power Board 2

---

## Wiring

### Board 1 — reed switch

The reed switch has two terminals and no polarity.

| Reed terminal | Connects to |
|---|---|
| Terminal A | **GPIO 4** |
| Terminal B | **GND** |

Mount the magnet on the door and the switch on the frame so they line up when the door is **closed**. The internal pull-up is used, so no extra resistor is needed.

**Status LED (lit while the door is closed):**

| LED leg | Connects to |
|---|---|
| Anode (+, long leg) | **GPIO 18** via a ~330 Ω resistor |
| Cathode (−, short leg) | **GND** |

Set `STATUS_LED_ACTIVE_HIGH` to `false` in the sketch if your LED is wired the other way.

### Board 2 — ESP32-2424S012 (ESP32-C3 + onboard display)

**Nothing to wire** — the round GC9A01 screen is built onto the ESP32-C3, so Board 2 is a single self-powered board. Just power it:

- Plug the **USB-C** port in with a **USB-C → USB-A** cable (a USB-C → USB-C cable often won't power or enumerate this board), into a 5 V adapter or a PC's USB-A port.

The display pins are fixed internally and set in the sketch — **do not change them**:

| Onboard signal | ESP32-C3 GPIO |
|---|---|
| SCLK | 6 |
| MOSI (SDA) | 7 |
| DC | 2 |
| CS | 10 |
| BL (backlight) | 3 |
| RST | none (software reset) |

> Because the backlight is on a real GPIO here, the **CLOSED** state is genuinely **dimmed** again (not just dark). The board also has a capacitive touch layer (CST816) we don't use — it's on I²C if you ever want it.

---

## Arduino IDE setup

1. **Install the ESP32 core:** *Tools → Board → Boards Manager* → search **"esp32"** → install **esp32 by Espressif Systems**.
2. **Install the display library:** *Tools → Manage Libraries* → search **"GFX Library for Arduino"** by *moononournation* → install. (No config-file editing required — nicer than TFT_eSPI for this.)
3. **Board settings** (*Tools* menu) — **different for each board**:

   **Board 1 (sensor, ESP32-S3-DevKitC-1):**

   | Setting | Value |
   |---|---|
   | Board | **ESP32S3 Dev Module** |
   | USB CDC On Boot | **Enabled** |
   | Flash Size | **16MB (128Mb)** |
   | PSRAM | **QSPI PSRAM** |
   | Upload Speed | 921600 |

   **Board 2 (display, ESP32-2424S012):**

   | Setting | Value |
   |---|---|
   | Board | **ESP32C3 Dev Module** |
   | USB CDC On Boot | **Enabled** |
   | Flash Size | **4MB (32Mb)** |
   | Upload Speed | 921600 |

> **Two USB ports:** the DevKitC-1 has a **UART** port and a native **USB** port. Easiest is the **UART** port (auto-resets for upload, shows a COM port). If you use the native USB port, keep *USB CDC On Boot = Enabled* so the Serial Monitor works.

---

## Flash the boards

1. Open `door_sensor_board1/door_sensor_board1.ino` → select that board's port → **Upload**. This is your **sensor**.
2. Open `door_display_board2/door_display_board2.ino`, set the board to **ESP32C3 Dev Module**, select the ESP32-2424S012's port → **Upload**. This is your **display**.
3. Label them so you don't mix them up.

Power both. Within a second or two the LEDs should turn **green** (paired) and the display should show CLOSED (dark) or OPEN (red).

---

## Things you may need to tweak

All at the top of each `.ino` under **USER CONFIG**:

- **LED doesn't light?** Change `RGB_LED_PIN` from `48` to `38`. (DevKitC-1 v1.0 uses GPIO 48, v1.1 uses GPIO 38 — the two common versions.)
- **Door reads backwards** (shows OPEN when closed)? Flip `DOOR_CLOSED_WHEN_LOW` between `true`/`false` in Board 1.
- **Running more than one pair nearby?** Give each pair its own `PAIRING_ID` (same value on both its boards).
- **Display pins:** change the `LCD_*` defines in Board 2 if you wire it differently.

---

## Optional: lock to specific boards (instead of auto-pair)

The default uses broadcast + a shared `PAIRING_ID`, which is simple and reliable for one pair. If you'd rather only let *these two exact boards* talk:

1. Flash both, open the Serial Monitor at 115200. Each prints `This board's MAC: XX:XX:...` on boot. Note both.
2. In each sketch, replace the broadcast address with the **other** board's MAC:
   ```cpp
   static uint8_t broadcastMac[6] = {0x11,0x22,0x33,0x44,0x55,0x66}; // other board's MAC
   ```
3. (Optional) In `onDataRecv`, also ignore packets whose sender MAC isn't the partner.

The `PAIRING_ID` check still applies, so you get both address- and ID-level filtering.

---

## Troubleshooting

- **LEDs stay red (never pair):** confirm the **same `PAIRING_ID` and `WIFI_CHANNEL`** on both boards, both powered, and in range. Don't connect either board to a Wi-Fi network in code.
- **Board 2 won't power / no COM port:** use a **USB-C → USB-A** cable (not C-to-C), and prefer a 5 V adapter or a PC USB-A port.
- **Board 2 screen blank / garbled:** make sure you flashed `door_display_board2` as **ESP32C3 Dev Module** (its pins are onboard and fixed — don't edit them).
- **Display rotated wrong:** change `LCD_ROTATION` near the top of `door_display_board2` (0/1/2/3 = 0/90/180/270°; **2 = flipped upside down**).
- **No Serial output:** set *USB CDC On Boot = Enabled*, or use the **UART** port.
- **Won't upload:** hold **BOOT**, tap **RESET**, release BOOT, then upload.
