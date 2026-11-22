# FREEzzz NEON CLOCK — Cardputer ADV v0.2

GIF-backed neon clock firmware for M5Stack Cardputer ADV (ESP32-S3, 8 MB flash, 240×135 ST7789V2).

## First boot

1. Insert microSD with GIFs arranged in folders under `/CHAOS_TV/LIBRARIES/`.
2. On first start, choose a GIF folder using **A/D**, confirm with **ENTER**.
3. Choose Wi-Fi/NTP or manual date/time.
4. Manual time format: `HHMMDDMMYYYY` (24-hour time).
5. Setup is saved in NVS; later boots start the clock directly.

Example:
```
/CHAOS_TV/LIBRARIES/USER/your-animation.gif
/CHAOS_TV/LIBRARIES/GLITCH/loop.gif
/CHAOS_TV/CONFIG/wifi.txt
```

Optional Wi-Fi file:
```ini
SSID=your_wifi_name
PASSWORD=your_wifi_password
```

The firmware also attempts to reuse the existing FREEzzz Wi-Fi credentials stored in NVS.

## Display and controls

- GIF decoding is incremental; GIFs are not loaded in full into RAM.
- Decoded frames are staged in a framebuffer and transferred after decoding, not scanline-by-scanline.
- The clock overlay is restored from the background framebuffer before redrawing.
- Display rotation is 0 for the Cardputer landscape orientation.
- ENTER: pause/resume; N: next GIF; 1: digit color; 2/3: digit brightness; 4/5: background brightness; W: retry Wi-Fi/NTP; H: help; R: rescan.
- Time zone: Germany (CET/CEST).

## Manual time limitation

The Cardputer ADV has no battery-backed real-time clock assumed by this firmware. A manually entered time is saved as a fallback epoch, but time cannot advance accurately while the device is fully powered off. For accurate time after a cold power-on, use Wi-Fi/NTP or enter the time again.

## Build

```bash
pio run -e cardputer-adv
```

The CI workflow creates a merged 8 MB flash image and a separate application image. The merged image is the standalone flashable BIN.

Hardware flicker/tearing must still be checked on a physical Cardputer ADV; software build success alone cannot certify panel behavior.
