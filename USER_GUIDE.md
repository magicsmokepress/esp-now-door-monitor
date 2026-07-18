# Door Monitor — User Guide

A two‑piece wireless door monitor.

- **Unit 1 — Sensor:** sits by the door and senses open/closed.
- **Unit 2 — Display:** shows the door's status on a round screen, anywhere within wireless range.

The two units talk directly to each other — no Wi‑Fi network, app, or account required.

---

## Turning it on

- **Unit 1 (Sensor):** plug its USB port into any 5 V USB adapter, power bank, or computer.
- **Unit 2 (Display):** plug its **USB‑C** port into a 5 V adapter or a computer's USB‑A port using a **USB‑C → USB‑A** cable.
  - A **USB‑C → USB‑C** cable usually will **not** power this unit — use C‑to‑A.

They find each other automatically within a second or two of both being powered. There's nothing to set up.

---

## Unit 1 (Sensor) — the three lights

| Light | What it means |
|---|---|
| 🔴 **Red — Power** | On whenever the unit has power. It's just the power indicator (always on). |
| 🔵 **Comm** | Link to the display: **red** = not connected, **green** = connected, brief **blue** flash = a message was just sent/received. Normal at start: red for ~1 second, then green. |
| 🟢 **Green — Door** | **On = door CLOSED.**  Off = door OPEN. |

**A healthy power‑up looks like this:** the red power light comes on, the Comm light flashes red then settles on **green** once it finds the display, and the green Door light matches your door.

---

## Unit 2 (Display) — the screen

| Screen | What it means |
|---|---|
| **"PAIRING…"** | Starting up / looking for the sensor (first couple of seconds). |
| **Red "OPEN"** | The door is **open**. |
| **Dimmed "CLOSED"** | The door is **closed**. A small green dot blinks with each update, so you can see it's live. |
| **Amber "NO SIGNAL"** (blinking edge) | Lost contact with the sensor — sensor unpowered, out of range, or restarting. |

---

## Known issue: Unit 1 sometimes doesn't start (only the red light)

Once in a while at power‑up, **only the red power light** comes on — the Comm light and the green Door light stay dark — and the unit does nothing.

This is a start‑up glitch: a brief voltage dip as the unit powers on stops it from booting.

**Firmware v1.1 fixes this in almost all cases** — it turns off the chip's brownout detector before the radio powers up, so the power-on dip no longer resets the board. If you still see it on a very weak supply, the steps below still apply.

**Fix:** unplug Unit 1, wait a couple of seconds, and plug it back in. It will start normally.

**How to recognize it:** on a good start you'll see the Comm light (red → green) and the green Door light within a second or two. If **only** the red power light is lit and nothing else ever happens, do the unplug / re‑plug.

**To make it stop happening:** use a good‑quality 5 V adapter and a short, thick USB cable — avoid thin or long cables, weak wall chargers, and cheap USB hubs. A small add‑on capacitor makes it start reliably every time; ask if you'd like that added.

---

## Quick help

- **Display shows "NO SIGNAL":** the sensor isn't reaching it. Check that Unit 1 is powered and actually started (see the known issue above), and that it's within range. Indoors, expect roughly **10–30 m** through walls; **100–200 m** with a clear line of sight.
- **It seems backwards** (says CLOSED when the door is open, or the green light is inverted): the magnet/switch alignment or a setting is reversed — let me know and I'll flip it.
- **Nothing ever pairs** (Comm light stays red / screen stays "NO SIGNAL"): make sure both units are powered, and that you have the **sensor + the display** (not two of the same unit).
- **Moving a unit:** you can relocate either one anytime; they re‑pair automatically when both are powered and in range.
