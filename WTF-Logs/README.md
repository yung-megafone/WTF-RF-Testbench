# WTF — SPIFFS Log Archive

Archive of raw SPIFFS flash dumps from the **What-The-Flock (WTF)** ESP32-C5 detector, used for firmware development, RF testing, and detection telemetry validation.

## Archived Files

| File | Description |
|---|---|
| `spiffs-20261118T165941Z.bin` | **Empty baseline.** Dumped from WTF v0.1.4. Entire 1 MiB partition contained `0xFF`. No SPIFFS filesystem or detection records were present. |
| `spiffs-lab-20261118T174452Z.bin` | **Successful laboratory test.** Dumped after testing WTF v0.1.4.1 against the WTF-RF-Testbench transmitting `WTF-LAB-01` beacons on Wi-Fi channel 6. |
| `partition-table.bin` | Raw 4 KiB partition table read directly from ESP32-C5 flash at `0x8000`. Confirms the SPIFFS partition location. |
| `*.sha256` | SHA-256 verification files for associated dumps, where available. |

## Verified Partition Layout

Hardware: **Seeed Studio XIAO ESP32-C5**, ESP32-C5HR8, 8 MB external flash.

| Partition | Offset | Size |
|---|---|---|
| NVS | `0x9000` | 24 KiB |
| PHY initialization | `0xF000` | 4 KiB |
| Factory application | `0x10000` | 2 MiB |
| SPIFFS (`storage`) | `0x210000` | 1 MiB |

Partition information was decoded from the actual device using ESP-IDF's `gen_esp32part.py`.

## November 18, 2026 — Lab Validation

The v0.1.4.1 test successfully demonstrated:

- Reception of testbench-generated Wi-Fi beacons.
- Detection and logging of `[LAB WIFI]` records.
- Persistent encounter summaries containing first, peak, and last RSSI measurements, plus sample counts.
- Successful recovery of those records through a read-only flash dump.

**Results:**
- Raw SPIFFS image: 1,048,576 bytes.
- Non-erased bytes: 2,964.
- Strongest observed testbench RSSI: **−58 dBm**.
- At least seven encounter summaries visible through `strings`.
- SHA-256 verification: passed.

Some records appeared fragmented when inspected with `strings`; proper SPIFFS filesystem extraction is preferable for complete analysis.

The successful lab test **does not establish real-world Flock camera detection accuracy**. Lab entries are explicitly labeled separately from field detections.

## Dumping SPIFFS

Enter ROM download mode (hold BOOT, tap RESET, release BOOT), then:

```bash
source ~/esp/esp-idf/export.sh

STAMP=$(date -u +%Y%m%dT%H%M%SZ)
OUT="$HOME/WTF-Logs/spiffs-$STAMP.bin"

python -m esptool --chip esp32c5 \
  -p /dev/ttyACM1 -b 115200 \
  --before no_reset --after no_reset --no-stub \
  read_flash 0x210000 0x100000 "$OUT"

sha256sum "$OUT" > "$OUT.sha256"
```

Adjust the serial port if necessary. The read operation does not modify flash.

### Quick inspection

```bash
strings -a -n 6 "$OUT" | \
  grep -Ei 'LAB WIFI|ENCOUNTER|WTF-LAB|RSSI|samples='
```

This is a preliminary inspection, not a full filesystem extraction.

### Verify integrity

```bash
sha256sum -c "$OUT.sha256"
```

## Notes for Future Development (WTF)

- **v0.1.4:** SPIFFS remained blank; first-run filesystem initialization was likely missing.
- **v0.1.4.1:** Lab telemetry and persistent encounter logging successfully validated on hardware.
- LED full-strength indication was provisionally changed from −45 dBm to **−70 dBm**.
- Actual RSSI measurements remain unmodified; LED thresholds affect only visual feedback.
- Future field tests should establish realistic peak RSSI levels from repeatable camera encounters.

**Preserve original `.bin` images and their hashes.** Perform any filesystem recovery or analysis on copies.