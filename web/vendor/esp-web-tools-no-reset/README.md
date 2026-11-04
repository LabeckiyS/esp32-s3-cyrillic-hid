# ESP Web Tools — manual bootloader variant

Based on `esp-web-tools` 10.4.0, licensed under Apache-2.0.

The installer calls `ESPLoader.main("no_reset")` so a LILYGO T-Dongle-S3
that was manually placed in the ESP32-S3 ROM downloader is not reset again
after the browser serial port is selected.
