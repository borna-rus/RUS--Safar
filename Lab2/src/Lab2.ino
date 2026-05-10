#include <DHT.h>
#include "esp_sleep.h"

#define DHT_PIN        4
#define DHT_TYPE       DHT22
#define SLEEP_SECONDS  10
#define MAX_MEASUREMENTS 10

RTC_DATA_ATTR int measurementCount = 0;
RTC_DATA_ATTR int totalWakeups     = 0;
RTC_DATA_ATTR float temperatures[MAX_MEASUREMENTS];
RTC_DATA_ATTR float humidities[MAX_MEASUREMENTS];

DHT dht(DHT_PIN, DHT_TYPE);

//OBNAVLJANJE MODULA NAKON BUĐENJA
void wakeUp() {
  Serial.begin(115200);
  delay(200);
  dht.begin();
  delay(500);
  Serial.println("> BUĐENJE IZ LIGHT SLEEP <");
  Serial.print("Ukupno buđenja: ");
  Serial.println(totalWakeups);
}


void doMeasurement() {
  Serial.println("> AKTIVNA FAZA <");

  float temp = dht.readTemperature();
  float hum  = dht.readHumidity();

  if (isnan(temp) || isnan(hum)) {
    Serial.println("Greška pri čitanju senzora!");
    return;
  }

  // Spremanje u RTC memoriju
  temperatures[measurementCount] = temp;
  humidities[measurementCount]   = hum;
  measurementCount++;
  totalWakeups++;

  Serial.print("Mjerenje #");
  Serial.print(totalWakeups);
  Serial.print(" | Temp: ");
  Serial.print(temp);
  Serial.print(" C | Vlaga: ");
  Serial.print(hum);
  Serial.println(" %");
}

//ISPIS 10 MJERENJA
void printAllMeasurements() {
  Serial.println("\n=== SVIH 10 MJERENJA ===");
  for (int i = 0; i < MAX_MEASUREMENTS; i++) {
    Serial.print("  [");
    Serial.print(i + 1);
    Serial.print("] Temp: ");
    Serial.print(temperatures[i]);
    Serial.print(" C | Vlaga: ");
    Serial.print(humidities[i]);
    Serial.println(" %");
  }
  Serial.println("============================\n");

  // Reset brojača
  measurementCount = 0;
  Serial.println("Spremnik resetiran. Novi ciklus počinje...\n");
}

//GAŠENJE PERIFERIJA PRIJE SLEEPA
void disablePeripherals() {
  Serial.println("Gašenje periferija...");
  pinMode(DHT_PIN, OUTPUT);
  digitalWrite(DHT_PIN, LOW);
  delay(100);
  Serial.flush();
}

//ULAZAK U LIGHT SLEEP
void goToSleep() {
  Serial.print("> ULAZAK U LIGHT SLEEP (");
  Serial.print(SLEEP_SECONDS);
  Serial.println(" sekundi) <\n");
  delay(100);

  disablePeripherals();

  esp_sleep_enable_timer_wakeup(SLEEP_SECONDS * 1000000ULL);

  esp_light_sleep_start();

  wakeUp();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  dht.begin();
  delay(500);
  Serial.println("\n--- ESP32 Datalogger pokrenut ---");
  Serial.println("Platforma  : ESP32");
  Serial.println("Sleep mode : Light Sleep");
  Serial.println("Buđenje    : Timer (svakih 10s)");
  Serial.println("Memorija   : RTC_DATA_ATTR");
  Serial.println("--------------------------------\n");
}

void loop() {
  doMeasurement();

  if (measurementCount >= MAX_MEASUREMENTS) {
    printAllMeasurements();
  }

  goToSleep();
}
