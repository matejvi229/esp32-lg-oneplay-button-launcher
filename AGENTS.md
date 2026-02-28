# AGENTS.md

Prakticky runbook pro budoucího agenta, aby šel projekt rychle zreplikovat.

## Cíl projektu

- `esp32_test/esp32_test.ino`: ESP32-S3 tlačítkem (`IO6`) spouští aplikaci OnePlay na LG webOS TV.
- LED na `IO5` slouží jako lokální indikace.
- Ověřené app ID pro OnePlay na této TV: `voyo.cz`.

## Hardware a piny

- Deska: `ESP32-S3 Dev Module` (uživatel používal „ESP32-S3-DevKit v8R8“).
- Tlačítko: `IO6 -> GND` (`INPUT_PULLUP`).
- LED: `IO5` (přes rezistor).
- Typický port: `COM7` (ověřit vždy přes `arduino-cli board list`).

## Nástroje

- Lokální CLI: `.tools/arduino-cli/arduino-cli.exe`
- Upload skript: `scripts/arduino_upload.ps1`

## Standardní upload

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\arduino_upload.ps1 -SketchPath .\esp32_test -Port COM7
```

## Jednoduchý test (button + LED)

- Sketch je uložený zvlášť: `examples/button_led_test/button_led_test.ino`
- Upload:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\arduino_upload.ps1 -SketchPath .\examples\button_led_test -Port COM7
```

## Nutné nastavení před reálným testem TV

V `esp32_test/esp32_test.ino` musí být správně:

- `WIFI_SSID`
- `WIFI_PASSWORD`
- `TV_IP`
- `DEFAULT_ONEPLAY_APP_ID` (aktuálně `voyo.cz`)

## Ověření přes serial

Číst `COM7` na `115200`. Očekávané logy:

- `WiFi OK, IP: ...`
- `WS connected`
- `Registered with TV`
- při stisku tlačítka:
  - `Button pressed`
  - `Launch requested for app id: voyo.cz`
  - `OnePlay launch command accepted`

## Známé problémy a řešení

- `401 insufficient permissions` u `listLaunchPoints`:
  - není kritické, launch může i tak fungovat.
- `errorText: "not exist"`:
  - špatné app ID, ověřit běžící app a upravit `DEFAULT_ONEPLAY_APP_ID`.
- CH340/COM chyby (`PermissionError(13)`):
  - ovladač/kabel/USB port; v minulosti pomohla reinstalace CH34x driveru.
- PSRAM warning na S3:
  - pro tento projekt nepoužívat PSRAM (`PSRAM=disabled`).

## Poznámka k citlivým údajům

- Wi-Fi údaje jsou v kódu. Při sdílení repa je před commitem anonymizovat.
