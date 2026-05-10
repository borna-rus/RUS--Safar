# Analiza trajanja baterije

## Pretpostavke

| Stavka | Vrijednost |
|---|---|
| Kapacitet baterije | 2500 mAh |
| Aktivna struja (ESP32 + DHT22) | ~160 mA |
| Struja u Light Sleep | ~0.8 mA |
| Aktivno vrijeme po ciklusu | ~2 sekunde |
| Sleep vrijeme po ciklusu | 10 sekundi |
| Ukupno vrijeme ciklusa | 12 sekundi |

## Formula

I_avg = (I_active × t_active + I_sleep × t_sleep) / t_ukupno
I_avg = (160 mA × 2s + 0.8 mA × 10s) / 12s
I_avg = (320 + 8) / 12
I_avg = 27.3 mA

## Trajanje baterije

T = Kapacitet / I_avg
T = 2500 mAh / 27.3 mA
T = 91.5 sati = ~3.8 dana

## Usporedba bez sleep moda

T = 2500 mAh / 160 mA = 15.6 sati

## Zaključak

Korištenjem Light Sleep moda trajanje baterije se povećava
s ~15 sati na ~91 sat, poboljšanje od ~6x.

Napomena: Ovo su teorijske procjene. Stvarna potrosnja
ovisi o hardverskoj platformi i uvjetima rada.
