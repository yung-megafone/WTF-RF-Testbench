<div align="center">

# WTF RF TESTBENCH
### Controlled over-the-air Wi-Fi transmitter

**One-shot beacons · Selectable channels · Adjustable transmit power · Headless GPIO controls**

[![Version](https://img.shields.io/badge/Release-v0.1.0-18a56a?style=for-the-badge)](https://github.com/yung-megafone/WTF-RF-Testbench/releases)
[![License](https://img.shields.io/badge/License-MIT-2868c7?style=for-the-badge)](LICENSE)
[![Board](https://img.shields.io/badge/Board-XIAO%20ESP32--C5-444?style=for-the-badge)](https://wiki.seeedstudio.com/xiao_esp32c5_getting_started/)
[![Framework](https://img.shields.io/badge/ESP--IDF-5.5.x-cc2020?style=for-the-badge)](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c5/)

[**Controls**](#controls) · [**LED patterns**](#onboard-led-feedback-active-low) · [**Flashing**](#flash-via-the-rom-bootloader) · [**Receiver test**](#using-it-with-what-the-flock)

**An optional standalone companion to [What-The-Flock](https://github.com/yung-megafone/What-The-Flock).**

</div>

---

> [!NOTE]
> This is a controlled laboratory beacon transmitter—not a Flock camera emulator. It uses the unmistakable `WTF-LAB-01` identifier and does **not** impersonate deployed hardware.

## Project dashboard

| | WTF-RF-Testbench |
|:--|:--|
| **Target** | Seeed Studio XIAO ESP32-C5 |
| **Trigger** | D5 → GND · single beacon or repeated transmit while held |
| **Channels** | D6 → GND · cycle 6 / 11 / 1 |
| **Requested power** | D7 → GND · cycle 2 / 8 / 15 dBm |
| **Feedback** | Nonblocking short (channel) and long (power) LED patterns |
| **Boot defaults** | Channel 6 · 2 dBm · not persisted |
| **Companion** | [What-The-Flock](https://github.com/yung-megafone/What-The-Flock) (optional) |

---

## What it does

- Constructs and transmits **real 802.11 beacon management frames** through `esp_wifi_80211_tx()`.
- Supports **one-shot transmission** and **held-button transmission** at approximately 10 frames per second.
- Switches between **2.4 GHz Wi-Fi channels 6, 11, and 1**.
- Requests **2, 8, or 15 dBm** transmit-power settings from the ESP-IDF driver.
- Gives local LED feedback without requiring a connected serial monitor.
- Uses an unambiguous lab SSID, `WTF-LAB-01`, rather than impersonating a deployed device.

This is an **over-the-air RF test**, not an internal simulated RSSI input. The receiving radio measures actual RSSI, so changing transmitter power can be used to explore the detector's LED mapping and signal tracking. Actual received RSSI depends on antenna orientation, distance, obstruction, multipath, and radio calibration—not just the requested TX power.

## Hardware

- 1 × **Seeed Studio XIAO ESP32-C5**, with its antenna connected
- USB-C power and a computer for initial flashing
- 1 × breadboard or momentary switch, or the classified instrument described below
- A second Wi-Fi-capable receiver for observing test traffic (for example, a What-The-Flock v0.1.4 development board)

### The proprietary, top-secret control interface

The entire human-machine interface runs on **classified, patent-pending, military-grade, ultra-high-security jumper wire technology**.

In practice, it's a Dupont jumper wire touched between a GPIO pin and **GND**. Please do not leak this information to competing laboratories. Their R&D departments may never recover.

No soldering is required for temporary tests. If using a jumper, touch and release it to simulate a button press. **Only connect the listed control pins to GND; never connect them to 5 V.**

## Controls

| Control | XIAO pin | GPIO | Action |
| --- | --- | --- | --- |
| Transmit | **D5** | GPIO24 | Brief tap: submit one beacon. Hold ~600 ms: continue transmitting at ~10 frames/s until released. |
| Channel | **D6** | GPIO11 | Cycle **6 → 11 → 1 → 6**. |
| TX power | **D7** | GPIO12 | Cycle requested power **2 → 8 → 15 → 2 dBm**. |
| Status LED | Built-in | GPIO27 | Nonblocking startup, channel and TX-power acknowledgment patterns; one-shot TX blip; idle heartbeat. |

### Onboard LED feedback (active-low)

The status patterns are **nonblocking**: the radio and input processing continue during flashes. A new channel or power selection replaces an in-progress acknowledgment. A brief transmission flash is lower priority than a configuration acknowledgment, so a packet can be sent successfully without its TX blip being visible.

| Event | LED indication |
| --- | --- |
| Boot completed successfully | One 600 ms steady flash |
| D5 transmission accepted by Wi-Fi driver | One brief 70 ms blip (unless a configuration pattern is active) |
| D6 channel 6 / 11 / 1 selected | **1 / 2 / 3 short flashes** (100 ms on, 140 ms between) |
| D7 requested power 2 / 8 / 15 dBm | **1 / 2 / 3 long flashes** (300 ms on, 170 ms between) |
| Idle | Brief heartbeat every 2.5 seconds |
| Startup initialization failure | Repeating groups of three error flashes |

The pattern identifies the **newly selected value**, not the old one. A TX blip means the ESP-IDF driver accepted the frame, **not** that another receiver captured it. If tapping GPIO with a bare jumper is too fast, hold it to GND for at least 55 ms and release before tapping again.

A press needs to remain stable for approximately **55 ms** to pass the firmware's debounce filter. D6 and D7 are used as GPIO controls in this build, **not as a UART TX/RX pair**.

### Power and channel defaults

On every boot, the testbench starts on **channel 6 at the lowest power request (2 dBm)**. The selected channel and power level are intentionally **not saved**: resetting the board restores the same predictable low-power starting point.

The three power values are **requests to the driver**, not calibrated measurements of radiated power. The driver may clamp or quantize them. Power settings and regulatory limits must be respected.

## Release status

**v0.1.0 has been exercised on real XIAO ESP32-C5 hardware**, including one-shot/held transmissions, requested power changes, nonblocking LED acknowledgments and reception by a What-The-Flock v0.1.4 development board. This is a laboratory utility, not a calibrated RF instrument.

## Build (ESP-IDF 5.5.x)

From the testbench project root:

```bash
source ~/esp/esp-idf/export.sh
idf.py set-target esp32c5  # first configuration only
idf.py build
```

A successful build produces:

```text
build/bootloader/bootloader.bin
build/partition_table/partition-table.bin
build/wtf_rf_testbench.bin
```

**Flashing the wrong board overwrites its application.** Keep your reference detector unplugged while programming the testbench.

### Flash via the ROM bootloader

1. Connect the testbench XIAO by USB.
2. Hold **BOOT**, tap **RESET**, then release BOOT.
3. Identify the serial port (`ls /dev/ttyACM*` on Linux).
4. Substitute the correct port in the following command:

```bash
python -m esptool --chip esp32c5 \
  -p /dev/ttyACM1 -b 115200 \
  --before no_reset --after no_reset --no-stub \
  write_flash --flash_mode dio --flash_freq 80m --flash_size 8MB \
  0x2000 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0x10000 build/wtf_rf_testbench.bin
```

Press **RESET** once flashing finishes. The firmware operates headlessly; a serial monitor is not needed for transmitting. The command above assumes the project's default ESP32-C5 partition layout—check the build output if you change partitions.

## Using it with What-The-Flock

The What-The-Flock development receiver has an optional laboratory matching rule. On the **receiver board's** source tree, edit `main/wtf_config.h`:

```c
#define WTF_ACCEPT_LAB_BEACONS 1
```

Rebuild and flash **the development receiver**, not your working reference unit. With this flag enabled, the receiver recognizes the exact lab SSID `WTF-LAB-01` from appropriate Wi-Fi management frames and can use the **actual received RSSI** to exercise its detection indicator and channel-locking logic.

The lab detection path is intentionally separate from normal Flock detection logging; it should **not** create a fictitious real-camera record. Set the flag back to `0` for normal detector deployments.

### Suggested test procedure

1. Connect antennas to both XIAOs. Place the transmitter and development receiver about **1–3 meters apart**, stationary.
2. Boot the transmitter; it starts on **channel 6 at 2 dBm requested power**.
3. Put the receiver in a scan profile that includes channel 6.
4. Briefly ground **D5** to transmit a single frame. Repeat trials; a one-shot packet can be missed while the receiver listens on another channel.
5. Hold **D5** for a couple of seconds to produce repeated beacon frames. Observe whether the detector responds more consistently and maintains its signal indication.
6. Tap **D6** to test channels 11 and 1, comparing acquisition and reacquisition.
7. With both boards in the same positions, tap **D7** to request 8 dBm and then 15 dBm. Compare the *receiver's measured behavior*, not assumed signal strength.
8. Stop transmitting and verify the receiver returns to its normal idle state after its tracking window expires.

**What counts as success?** A visible LED response confirms the end-to-end path is functioning for at least some frames, but it does **not** measure packet-delivery rate, prove that every one-shot frame was received, or independently confirm a channel-lock transition. Those require additional diagnostic counters or logs.

## LED behavior and troubleshooting

| Symptom | What to check |
| --- | --- |
| Idle heartbeat, no receiver indication | Is `WTF_ACCEPT_LAB_BEACONS` enabled on the development receiver? Is it scanning the transmitter's current channel? Are antennas connected? |
| Single tap doesn't trigger a visible response | Hold the jumper contact longer than the debounce interval; try repeated transmissions. A single frame may be missed during channel hopping. |
| Receive LED accelerates but doesn't become solid | Actual RSSI may remain below the receiver's solid-LED threshold. Try changing TX power, distance, and orientation. |
| Testbench LED flashes after sending but receiver stays idle | The LED shows the transmit API accepted the frame—not that a second radio received it. |
| Repeating groups of three LED flashes with a longer pause | The testbench encountered an initialization failure. |
| Native USB monitor misbehaves | The firmware is designed to run without a serial console; use the ROM flashing method and observe onboard LEDs. |

Keep transmit power appropriate for your environment, avoid transmitting on channels or at power levels not permitted in your jurisdiction, and use the testbench only on equipment and radio environments you are authorized to test.

## Project relationship

- **[What-The-Flock](https://github.com/yung-megafone/What-The-Flock)** — the detector firmware; operates independently of this tool.
- **WTF-RF-Testbench** — optional companion transmitter to test real radio reception, signature handling, channel scanning, and RSSI response.

The repositories are intentionally separate so test-only transmissions and detector firmware can evolve independently.

## License

**MIT License.** Copyright © 2026 yung-megafone.

See [`LICENSE`](LICENSE) for the complete terms. This project is provided **as is**, without warranty.
