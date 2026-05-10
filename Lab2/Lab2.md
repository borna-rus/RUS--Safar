# Lab 2 – Upravljanje potrošnjom energije mikrokontrolera

## Opis rješenja
Implementacija periodičkog buđenja (Varijanta B – Datalogger okoliša) na ESP32 platformi.
Uređaj periodički mjeri temperaturu i vlagu putem DHT22 senzora, sprema zadnjih 10 mjerenja
u RTC memoriju i ulazi u Light Sleep između mjerenja.

## Wokwi link
https://wokwi.com/projects/463647009605644289

## Sažetak

| Stavka | Odgovor |
|---|---|
| Platforma | ESP32 |
| Varijanta | B – Datalogger okoliša |
| Sleep mode | Light Sleep |
| Buđenje | Timer (svakih 10 sekundi) |
| Čuvanje stanja | RTC_DATA_ATTR |
| Debouncing | Nije potrebno (timer wake-up) |
| Wokwi link | https://wokwi.com/projects/463647009605644289 |
