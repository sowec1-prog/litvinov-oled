# Krabička pro Litvínov OLED — revize A

Parametrický OpenSCAD návrh pro přenosný OLED panel. Zdroj: `litvinov_oled_case.scad`.

## Použité rozměry

| Díl | Rozměr použitý v modelu | Zdroj |
|---|---:|---|
| LiPol 855085 s ochranou | 85 × 50 × 8,5 mm | produktový list |
| OLED 1,3" / 7 pin | 35,5 × 33,7 mm; 4 × Ø3,0 mm | dodaný mechanický výkres |
| OLED uchycení | 4 × Ø2,2 mm zatavovací kolík; rozteč 31,5 × 29,7 mm | navrženo podle výkresu |
| ESP32-C3 s piny/bzučákem | 40 × 30 mm obrys | změřeno uživatelem |
| TP4056 USB-C | 20 × 15 × 5 mm | dodaný obrázek modulu |
| SS12F15 | otvor 14 × 3,4 mm | nutno doladit podle konkrétní výšky páčky |

## Konstrukce

- Vnější rozměr: **60 × 96 × 28 mm**.
- Zadní vana drží baterii na dně v prostoru 52 × 87 × 9,5 mm. Baterii nepřiskřípnout: vložit 0,5–1 mm pěnovou pásku, nepoužívat šrouby ani ostré výstupky v jejím okolí.
- Přední díl je bezel pro OLED. Čtyři pružné západky jej drží v zadní vaně; samotný OLED drží čtyři Ø2,2mm plastové kolíky vedené jeho otvory Ø3mm a po zkoušce se zataví páječkou.
- Konstrukce je **bez šroubů**: víčko drží pružné západky a OLED čtyři zatavovací piny. Pro sestavení tedy nejsou potřeba žádné šrouby ani matice.
- Baterie leží při zadní stěně. ESP32-C3 je nalepený svisle na vnitřní stranu předního bezelu pod OLEDem, takže piny zespodu nikdy neleží na LiPol a nahoře zůstává prostor pro bzučák.
- Na **zadní velké ploše** jsou vedle sebe USB-C otvor TP4056 a štěrbina posuvného vypínače. TP4056 se lepí uvnitř přímo za svůj otvor.
- Západky jsou vedené z předního bezelu do nízko umístěných oken zadní vany; bezel se zacvakává z přední strany, ne obráceně.

## Před tiskem: nutné suché ověření

1. Změř šířku a výšku **páčky** konkrétního SS12F15; v modelu jsou parametry `switch_slot_w` a `switch_slot_h`.
2. Zkus nejdřív pouze přední díl a ověř OLED. Rozměr prosvětlené plochy `oled_view_w/h` je startovní hodnota; není to rozměr desky.
3. Pro samostatné STL otevři a exportuj tyto dva soubory: `litvinov_oled_case_FRONT.scad` (přední bezel) a `litvinov_oled_case_REAR.scad` (zadní vana). Hlavní `litvinov_oled_case.scad` nyní ve výchozím nastavení ukazuje oba díly odděleně 8 mm od sebe na jedné tiskové ploše; režim `assembly` je jen kontrolní náhled, nikdy jej neexportuj pro tisk.
4. Tisk pro **FLSun T1 / ABS**: vrstva 0,20 mm, 4 perimetry, 5 horních i spodních vrstev, 18 % gyroid, 8mm brim. Připravený profil `0.20mm ABS enclosure @FLSun T1 - Litvinov` je už uložený přímo v OrcaSliceru. Použij existující filamentový profil **FLSun T1 ABS** (jeho systémové výchozí hodnoty jsou 280 °C tryska / 90 °C podložka). Před tiskem ABS tiskárnu uzavři, vysuš filament a zapni filtraci/odtah podle běžného postupu pro ABS.
5. Přední bezel tiskni čelem dolů, zadní vanu dnem dolů. **Podpěry jsou v profilu vypnuté.** Konstrukce nemá žádné vnitřní police ani kolejnice vyžadující podporu; krátké můstky západek jsou maximálně 1,8 mm.

## Elektrická bezpečnost

- TP4056 připojit k LiPol pouze po ověření polarity `B+` / `B−`.
- USB-C otvor je pro **nabíjení TP4056**, ne pro přímé připojení článku k ESP.
- Baterii nepropichovat, nemačkat, nešroubovat přes ni a nepoužívat ji, pokud je nafouklá nebo se nabíjení zahřívá.
- Než krabičku zavřeš napevno, vyzkoušej nabíjení, vypínač, OLED a bzučák.

## Stav ověření

Model i oba STL soubory byly nyní skutečně vyexportovány přes OpenSCAD a zkontrolovány: každý STL obsahuje právě jeden souvislý tiskový díl. Rozměrová zkouška prvního výtisku zůstává nutná pro konkrétní páčku vypínače, výšku bzučáku a skutečný OLED před zatavením kolíků.
