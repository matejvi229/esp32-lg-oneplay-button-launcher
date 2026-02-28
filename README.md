# Nastupce Setobuxu - ESP32 sketches

Repo obsahuje Arduino sketchy pro ESP32-S3 a pomocne skripty pro build/upload pres `arduino-cli`.

## Co je v repozitari

- `esp32_test/esp32_test.ino`
  - Aktualni sketch pro LG webOS OnePlay launcher.
  - Tlacitko na `IO6`, LED na `IO5`.
  - Konfigurace je v lokalnim souboru `esp32_test/config.h` (gitignored).
  - Vzorek je v `esp32_test/config.h.example`.

- `examples/button_led_test/button_led_test.ino`
  - Jednoduchy test: kazdy stisk tlacitka prepne LED.
  - Zapojeni:
    - tlacitko `IO6 -> GND` (pouziva `INPUT_PULLUP`)
    - LED na `IO5` pres rezistor

- `scripts/arduino_upload.ps1`
  - Skript pro compile/upload sketchu.

- `.tools/arduino-cli/arduino-cli.exe`
  - Lokalni binarka `arduino-cli`, kterou skript pouziva.

## Jak nahrat kod

Otevri PowerShell v rootu repozitare:

```powershell
.\scripts\arduino_upload.ps1
```

Pred prvnim nahranim:

```powershell
Copy-Item .\esp32_test\config.h.example .\esp32_test\config.h
```

Pak uprav `esp32_test/config.h` (Wi-Fi + TV IP + app ID).

Vychozi hodnoty:

- sketch: `.\esp32_test`
- deska/FQBN: `esp32:esp32:esp32s3`
- port: `COM7`
- board options: `FlashSize=8M,PSRAM=disabled,CDCOnBoot=cdc`
- upload speed: `115200`

## Uzitecne priklady

Nahrani jednoducheho LED+button testu:

```powershell
.\scripts\arduino_upload.ps1 -SketchPath .\examples\button_led_test -Port COM7
```

Pouze kompilace (bez uploadu):

```powershell
.\scripts\arduino_upload.ps1 -SketchPath .\examples\button_led_test -CompileOnly
```

Jiny COM port:

```powershell
.\scripts\arduino_upload.ps1 -Port COM4
```

## Poznamky

- Sketch slozka musi obsahovat hlavni `.ino` se stejnym nazvem jako slozka.
- Pokud upload pada, over port:

```powershell
.\.tools\arduino-cli\arduino-cli.exe board list
```

- Build artefakty skript uklada do `build\script_YYYYMMDD_HHMMSS`.
