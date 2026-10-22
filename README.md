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
| GPIO3 | piezo `+` / signál | PWM znělka — opravené zapojení PCB rev. A |
| GND | piezo `-` | společná zem |
| GPIO2 | ADC_BAT z děliče 100 kΩ / 100 kΩ | pouze připravený vstup měření baterie; firmware jej zatím nečte |

OLED používá I²C adresu `0x3C`.

### Oprava osazené PCB rev. A

Deska byla vyrobena s posunem dvou signálů proti původnímu návrhu: PWM bzučáku je na **GPIO3** (ne GPIO4) a uzel `ADC_BAT` děliče 100 kΩ / 100 kΩ vede na **GPIO2** (ne GPIO3). Firmware v této revizi používá GPIO3 pro bzučák; měření baterie zatím není implementované a GPIO2 proto nepoužívá. GPIO2 je bootovací pin ESP32-C3, takže před budoucím zapnutím měření baterie musí proběhnout fyzický boot test s osazeným děličem. `B+` článku se nikdy nesmí připojit přímo na GPIO.

## Doporučené vylepšení Wi-Fi antény

U ESP32-C3 SuperMini může mít jednoduchá úprava antény významný přínos pro slabší 2,4GHz signál. Pro Litvínov OLED byla prakticky ověřena rychlejší asociace k Wi-Fi a lepší RSSI po této úpravě. Postup a zkušenosti komunity: [ESP32-C3 SuperMini antenna mod](https://www.reddit.com/r/esp32/comments/1k01wtp/the_esp32c3_supermini_antenna_mod_is_the_real_deal/).

Jde o fyzickou pájecí úpravu modulu: dělejte ji jen při odpojeném USB napájení a na vlastní odpovědnost.

## Nastavení Wi-Fi

1. Zkopírujte `include/secrets.h.example` jako `include/secrets.h`.
2. Doplňte primární SSID/heslo a případně `WIFI_SSID_2` / `WIFI_PASSWORD_2` a `WIFI_SSID_3` / `WIFI_PASSWORD_3` pro další místa.
3. ESP32-C3 vybere první dostupnou uloženou 2,4GHz síť; po návratu domů se automaticky připojí k domácí Wi-Fi.
4. Pokud se žádná uložená síť nepřipojí do 45 sekund, OLED vytvoří zabezpečený hotspot **`Litvinov-OLED-Setup`** s heslem **`litvinov`**. Připojte se k němu telefonem, otevřete `http://192.168.4.1`, vyberte nalezenou síť a zadejte její heslo. Seznam obsahuje pouze 2,4GHz sítě kompatibilní s ESP32-C3.
5. Po úspěšném připojení hotspot zmizí; nově vybraná síť se uloží do paměti ESP spolu s dříve zadanými sítěmi.
6. Soubor `include/secrets.h` nikdy necommitujte; `.gitignore` jej vylučuje.

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
