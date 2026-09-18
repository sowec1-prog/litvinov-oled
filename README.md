# Litvínov OLED scoreboard

Autonomní hokejový panel pro **ESP32-C3 SuperMini** a OLED SSD1306 128×64. Firmware čte kompaktní stav z `https://litvinov-server.onrender.com/api/live`; PC ani LVGL nejsou potřeba.

## Funkce

- živé skóre, týmové kódy a poslední střelec bez diakritiky;
- `TRES-LIT`, `TRES-PCE` nebo `TRES-LIT/PCE` podle strukturované události píšťalky;
- přestávka s lokálním odpočtem bez cloudových dotazů;
- lokální odpočet příštího zápasu;
- gól Litvínova: znělka a 30sekundová blikající animace `GOOL` zprava doleva;
- první přijatý gól po restartu animaci ani zvuk nespustí;
- konečný výsledek se na cloudovém API drží pět minut jako `ZAPAS SKONCIL`.

## Použitý hardware

- **ESP32-C3 SuperMini** — ESP32-C3, 4 MB flash, Wi-Fi 2,4 GHz; projekt používá PlatformIO profil `esp32-c3-devkitm-1`.
- **OLED SSD1306 128 × 64 px**, I²C, adresa `0x3C`.
- **Pasivní piezo bzučák** ověřený pro PWM tóny.
- USB datový kabel pro napájení a nahrání firmwaru.
- Propojovací vodiče: 4× pro OLED, 2× pro bzučák.

## Zapojení

Všechny moduly musí mít společnou zem (**GND**). OLED i GPIO ESP32-C3 jsou 3,3 V logika; OLED proto napájejte z pinu **3V3**, ne z 5 V.

| ESP32-C3 SuperMini | Připojit na | Poznámka |
|---|---|---|
| GPIO8 | OLED SDA | I²C data |
| GPIO9 | OLED SCL | I²C hodiny |
| 3V3 | OLED VCC | napájení OLED |
| GND | OLED GND | společná zem |
| GPIO4 | piezo `+` / signál | PWM znělka |
| GND | piezo `-` | společná zem |

OLED používá I²C adresu `0x3C`.

## Nastavení Wi-Fi

1. Zkopírujte `include/secrets.h.example` jako `include/secrets.h`.
2. Doplňte SSID a heslo.
3. Soubor `include/secrets.h` nikdy necommitujte; `.gitignore` jej vylučuje.

## Sestavení a nahrání

Vyžaduje PlatformIO a desku `esp32-c3-devkitm-1`.

```bash
uv run --with platformio pio run
uv run --with esptool esptool --chip esp32c3 --port COM3 --baud 460800 --no-stub write-flash \
  0x0 .pio/build/esp32-c3-devkitm-1/bootloader.bin \
  0x8000 .pio/build/esp32-c3-devkitm-1/partitions.bin \
  0x10000 .pio/build/esp32-c3-devkitm-1/firmware.bin
uv run --with esptool esptool --chip esp32c3 --port COM3 --baud 460800 --no-stub verify-flash \
  0x10000 .pio/build/esp32-c3-devkitm-1/firmware.bin
```

Předchozí funkční flash i zdrojová záloha zůstávají lokálně. Fyzické chování OLED a animace se ověřuje při skutečné nové události v přenosu.

## Použité knihovny

- Adafruit GFX 1.11.11
- Adafruit SSD1306 2.5.13
- ArduinoJson 7.1.0
- espressif32 6.7.0
