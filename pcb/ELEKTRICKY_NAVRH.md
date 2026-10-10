# Litvínov OLED — elektrický návrh modulové PCB rev. A

Tato deska je nosná PCB pro stávající ESP32-C3 SuperMini. OLED, bzučák, článek 18650 a USB-C nabíjení jsou připojené přes desku; firmware a Wi-Fi anténa SuperMini zůstávají beze změny.

> Stav: návrh zapojení před rozmístěním součástek. Neobjednávat PCB ani nepřipojovat baterii, dokud nebude ověřen skutečný pinový rastr SuperMini, nabíjecí modul a rozměr držáku 18650.

## OLED — univerzální čtyřpinový konektor

OLEDy, které připadají v úvahu, mají oba pořadí `SCL, SDA` na pinech 3/4, ale liší se prohozeným napájením na pinech 1/2:

| Pin OLED konektoru | Typ A | Typ B | Funkce na PCB |
|---:|---|---|---|
| 1 | GND | VIN | volitelné přes pájecí propojku |
| 2 | VIN | GND | volitelné přes pájecí propojku |
| 3 | SCL | SCL | GPIO9 |
| 4 | SDA | SDA | GPIO8 |

- `VIN` OLEDu je vždy **3,3 V**, nikdy 5 V.
- Piny 1 a 2 povedou přes dvě třípadové pájecí propojky, aby se bezpečně zvolilo pořadí napájení podle popisu konkrétního OLEDu.
- Piny SCL a SDA budou mít na PCB vyvedené testpady.

## Bzučák

| Signál | Připojení |
|---|---|
| BUZZER_PWM | GPIO4 ESP32-C3 |
| BUZZER_GND | GND |

Používá se ověřený pasivní piezo bzučák. Pokud se vybere bzučák s vyšším odběrem než bezpečný proud GPIO, doplní se tranzistorový budič; přímé připojení není automaticky předpokládáno.

## 18650, USB-C nabíjení a napájení ESP

- Článek 18650 se nepřipojuje přímo na ESP32 ani na OLED.
- USB-C 5 V vstup vede na nabíječ s ochranou 1S článku (TP4056 s ochranou DW01A/FS8205 nebo elektricky rovnocenné řešení).
- Výstup pro zátěž (`P+`, `P−` u chráněného nabíječe) vede přes vypínač na **stabilizovaný 3,3V buck-boost měnič** pro ESP32 a OLED.
- TP4056 je pouze nabíječ; není to 3,3V regulátor.
- Před finálním PCB je nutné vybrat konkrétní měnič podle špičkového odběru Wi-Fi ESP32-C3 a jeho rozměrů.

## Měření napětí článku

Použít bezpečný dělič přímo z `B+` článku před měničem:

```text
B+ 18650 ── R1 100 kΩ ──┬── ADC_BAT (kandidát GPIO3 ESP32-C3)
                         |
                       R2 100 kΩ
                         |
B− / GND ───────────────┴── GND

ADC_BAT ── C1 100 nF ── GND
```

- Při plně nabitém článku 4,20 V je na ADC maximálně **2,10 V**.
- `V_baterie = V_ADC × 2`.
- `GPIO3` je zatím jen kandidát: firmware ho nyní nepoužívá, ale před layoutem se musí na fyzickém ESP32-C3 SuperMini potvrdit, že je vyvedený a nepoužitý pro bootování.
- Nikdy nepřipojovat `B+` článku přímo na GPIO/ADC.
- Na PCB bude testpad `ADC_BAT` pro porovnání s multimetrem.

## Firmware — dosud nezměněn

Současný firmware používá pouze:

| Funkce | GPIO |
|---|---:|
| OLED SDA | GPIO8 |
| OLED SCL | GPIO9 |
| pasivní bzučák | GPIO4 |

Po fyzickém potvrzení GPIO3 se do firmware doplní průměrování více ADC vzorků, přepočet děličem ×2 a zobrazení napětí. SOC v procentech bude až orientační hodnota, kalibrovaná proti reálnému napětí článku pod běžnou zátěží. Bez zapojeného děliče se firmware nesmí flashovat.
