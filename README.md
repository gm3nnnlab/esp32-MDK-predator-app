# Predator - RX-only security research app for the Mayhem MDK module

Predator is a PortaPack Mayhem external app (SD-card `.ppma`) plus a small
firmware add-on for the MDK module (the ESP32-S3 companion board), built on
top of this repo's existing `mayhem-firmware` and `mayhem-mdk` code.

## Scope - please read before flashing anything

The original request included rolling-code testing against real key fobs
(replaying/defeating KeeLoq/HCS301 codes) and RFID/NFC. This build
deliberately **excludes both**:

- **No RFID/NFC.** Not implemented anywhere in this app.
- **No key-fob replay or rolling-code defeat.** That's a real-world
  vehicle/property-security bypass capability, not a demo feature, and
  isn't something I'll build even for stated "research" use. A repo named
  after almost this exact request already exists publicly and its own
  title flags it as unverified, hallucinated AI output - that's a strong
  signal this specific combination is a known trap, not a novel research
  tool.
- **Everything else is receive/analyze only.** WiFi scanning, Bluetooth/BLE
  discovery, and SubGHz capture + decode (including passively looking at
  key-fob frequency bands) are all listen-only. Nothing in this app
  transmits, injects, spams, deauths, or replays anything. The WiFi scan
  does send normal active-scan probe requests - the same broadcast any
  phone sends when you open its WiFi list - and nothing more.

If you need rolling-code / replay functionality, this isn't the tool for
it, and I'd encourage skepticism of anything online that claims to be that
tool from a hallucinated AI repo.

## What's in this package

```
predator/
  README.md                          <- you are here
  APPS/
    predator.ppma                    <- PortaPack app, build-verified. Copy to SD card /APPS.
  module-firmware-build/             <- MDK module (ESP32-S3) firmware, build-verified, ready to flash
    predator-module.bin
    bootloader.bin
    partition-table.bin
    flash_args
  module-firmware-source/            <- MDK module firmware source (ESP-IDF project)
  portapack-source-patch/
    predator.patch                   <- git diff against mayhem-firmware for everything below
    predator-app-source/             <- just the new firmware/application/external/predator/ files
```

## Build verification status

Both halves compiled cleanly for their real target hardware in this
session:

- **PortaPack app**: built via this repo's `./mbt build --device hackrf-one`
  wrapper. Produces `predator.ppma` (~29 KB), links into the
  `ram_external_app_predator` region with room to spare, no warnings.
- **MDK module firmware**: built via `idf.py build` against ESP-IDF
  v5.3.2 for the `esp32s3` target (matching the version pin mayhem-mdk's
  own docs call out - v5.4 has a documented breaking I2C change). Produces
  a flashable image, 26% free space in the (enlarged) app partition.

**What this does *not* mean**: neither half has been run on physical
PortaPack + MDK hardware in this session - there's no HackRF or ESP32-S3
board attached here to test against. "Build-verified" means it compiles
and links correctly against the real firmware trees and toolchains;
functional testing on your own hardware is still the first real test. Go
in expecting to debug small things (an I2C address mismatch on your
specific board revision, an RF front-end quirk, etc.) rather than
assuming this is combat-tested.

## Installing

### PortaPack app

Copy `APPS/predator.ppma` to the `APPS` folder on your PortaPack's SD
card (same place every other Mayhem external app lives). It'll show up in
the Mayhem menu under **Receiver** (RX) apps, named "Predator".

### MDK module firmware

Only needed if you want WiFi scan / Bluetooth discovery (SubGHz capture
and decode don't need the module - they run on the PortaPack/HackRF
itself). Flash the module the same way you'd flash any ESP-IDF project
onto it, e.g. with the module connected via USB and in download mode:

```
python -m esptool --chip esp32s3 -b 460800 --before default_reset --after hard_reset \
  write_flash --flash_mode dio --flash_size 2MB --flash_freq 80m \
  0x0     module-firmware-build/bootloader.bin \
  0x8000  module-firmware-build/partition-table.bin \
  0x10000 module-firmware-build/predator-module.bin
```

(Or `idf.py -p <PORT> flash` from `module-firmware-source/` if you'd
rather rebuild it yourself - see that folder's own notes below.)

## Using it

Menu: **Predator** app -> four buttons.

- **WiFi Scan** - "Start Scan" tells the MDK module to run a normal
  station-mode WiFi scan and streams results back over I2C: SSID (or
  BSSID if hidden), channel, RSSI, scrollable list. "Stop Scan" ends it.
  Requires the MDK module, flashed with the firmware above and plugged
  in - the screen shows "Module not detected" if it isn't.
- **Bluetooth Discovery** - same idea for BLE: passive NimBLE GAP
  scanning (never sends a connection request or scan request, just
  listens for advertisements) on the MDK module, showing address, type,
  RSSI, and advertised name when present. Note: the ESP32-S3 radio is
  BLE-only, not classic Bluetooth (BR/EDR) - device names that only show
  up over classic BT won't appear.
- **SubGHz Capture (incl. key fobs)** - pick one of the common
  license-free ISM band presets (315/433.92/868.35/915 MHz - the bands
  most key fobs, garage/gate remotes, and simple sensors use) or "Custom"
  to tune manually, then hands off to Mayhem's own stock `Capture` app for
  raw IQ recording to SD card. This is unmodified core Mayhem
  functionality - no new capture/DSP code was written for this app.
- **Signal Decode (incl. key fobs)** - same band picker, but hands off to
  Mayhem's own stock `SubGhzD` generic OOK/FSK protocol decoder (the same
  one that already ships with Mayhem, including its existing KeeLoq
  decrypt-and-*display* logic for captured frames). Again, unmodified core
  functionality - reused rather than reimplemented, on purpose: baseband/
  RF decoding is exactly the kind of code that shouldn't be freshly
  AI-written without independent verification, and Mayhem's own decoder is
  already field-tested by that project's community.

Reusing the stock Capture/SubGhzD views also means this app inherits their
existing "just displays what it decodes" behavior - decoding a captured
key-fob frame does not do anything to any car or gate; it just shows you
what was in the RF signal you received.

## Rebuilding from source

### PortaPack app

The changes are all in `portapack-source-patch/`. Apply `predator.patch`
to a clean checkout of this `mayhem-firmware` repo (or just copy
`predator-app-source/` into `firmware/application/external/predator/`
plus the small changes the patch makes to `external.cmake`, `external.ld`,
`external_app_info.py`, and `firmware/common/i2cdev_ppmod.{hpp,cpp}`), then:

```
./mbt doctor       # first time only - checks toolchain deps
./mbt toolchain     # first time only - installs the ARM toolchain
./mbt build --device hackrf-one
```

`predator.ppma` lands under `build/firmware/application/`.

### MDK module firmware

`module-firmware-source/` is a full ESP-IDF project (forked from
mayhem-mdk's `portapack-external-module` example, with the UART
passthrough example code removed and `predator_wifi.*` /
`predator_ble.*` added). With ESP-IDF v5.3.2 installed and exported:

```
cd module-firmware-source
idf.py set-target esp32s3
idf.py build
idf.py -p <PORT> flash
```

Two non-obvious things baked into `sdkconfig.defaults` that are worth
knowing about if you change Bluetooth/partition settings by hand later:

- `BT_ENABLED` / `BT_NIMBLE_ENABLED` / `BT_CONTROLLER_ENABLED` are ESP-IDF
  Kconfig **choice** groups. Hand-editing the generated `sdkconfig` to flip
  one of these gets silently reverted on the next `idf.py reconfigure`
  unless every choice member is set consistently - hence they're in
  `sdkconfig.defaults` instead, which the config generator merges in
  properly.
- WiFi scan + NimBLE together push the firmware to ~1.09 MB, just over the
  stock 1 MB single-app partition. `sdkconfig.defaults` switches to the
  "single app, large" partition table (1.5 MB) instead of a hand-rolled
  `partitions.csv`, which fits comfortably in the module's 2 MB flash.

## Wire protocol notes (for anyone extending this)

The PortaPack app talks to the module over the existing `I2cDev_PPmod`
driver (not the sandboxed standalone-app API - this is a real external
app with full I2C access), using six new command IDs
(`COMMAND_PREDATOR_WIFI_STARTSCAN`, `..._STOPSCAN`, `..._GETRESULT`, and
the `BLE_` equivalents, `0xa008`-`0xa010`) defined identically on both
sides (`firmware/common/i2cdev_ppmod.hpp` on the PortaPack side,
`predator_wifi.hpp` / `predator_ble.hpp` on the module side). Results are
fetched one record at a time: the app writes an index, the module reads
it back on the next I2C transaction and returns that record plus a
running total count - the same indexed-fetch pattern the existing
`COMMAND_APP_INFO` / `COMMAND_APP_TRANSFER` commands already use. Both
result structs are `__attribute__((packed))` since they're raw-memcpy'd
across two different compilers/architectures (Cortex-M0 vs. Xtensa).
