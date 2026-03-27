/**
 * @file Lab1.ino
 * @brief Smart Garage System - ESP32 (Wokwi kompatibilna verzija)
 *
 * Prioriteti prekida:
 *   P1 (najviši) - STOP tipka       (GPIO 26) - emergency stop
 *   P2           - Hardware Timer   (1 Hz)    - periodička logika
 *   P3           - DOOR tipka       (GPIO 13) - upravljanje vratima
 *   P4           - SERVICE tipka    (GPIO 25) - servis mod
 *   P5 (najniži) - HC-SR04 senzor   (polling) - detekcija auta
 *
 * Zaštita resursa:
 *   - portMUX_TYPE + portENTER_CRITICAL / portEXIT_CRITICAL u loop()
 *   - portENTER_CRITICAL_ISR / portEXIT_CRITICAL_ISR u ISR-ima
 *   - Volatile zastavice za komunikaciju ISR -> loop()
 */

#include <Arduino.h>

// PINOVI

#define LED_DOOR     18   //< LED zelena  - status vrata
#define LED_STOP     19   //< LED crvena  - emergency stop
#define LED_SERVICE  21   //< LED zuta    - servis mod
#define LED_ALERT    22   //< LED zuta    - detekcija auta
#define LED_TIMER    23   //< LED plava   - timer tik

#define BUTTON_DOOR     13  ///< Tipka zelena  - otvori/zatvori vrata (P3)
#define BUTTON_STOP     26  ///< Tipka crvena  - emergency stop (P1) [bio GPIO 33 - input only!]
#define BUTTON_SERVICE  25  ///< Tipka zuta    - servis mod (P4)

#define TRIG_PIN  4   ///< HC-SR04 trigger
#define ECHO_PIN  16  ///< HC-SR04 echo [bio GPIO 5 - strapping pin!]

// KONSTANTE

#define DEBOUNCE_MS      200  ///< Debounce prag u ms
#define CAR_DISTANCE_CM   50  ///< Prag detekcije auta u cm
#define AUTO_CLOSE_TICKS   5  ///< Timer tikova do auto-zatvaranja vrata

// MUTEX

static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

// VOLATILE ZASTAVICE

volatile bool flag_door    = false;  ///< Tipka vrata pritisnuta
volatile bool flag_stop    = false;  ///< Emergency stop aktiviran
volatile bool flag_service = false;  ///< Tipka servis pritisnuta
volatile bool flag_timer   = false;  ///< Hardware timer okidan

// STANJE SUSTAVA

bool doorOpen      = false;  ///< Vrata otvorena
bool emergencyStop = false;  ///< Sustav u emergency stopu
bool serviceMode   = false;  ///< Servis mod aktivan
bool carDetected   = false;  ///< Auto detektiran

int timerTickCount = 0;  ///< Broj tikova od zadnjeg otvaranja vrata

// DEBOUNCE

volatile unsigned long lastInterrupt[3] = {0, 0, 0};

// HARDWARE TIMER

hw_timer_t *hwTimer = NULL;

/**
 * @brief ISR hardware timera - okida se svaku sekundu.
 *
 * Postavlja flag_timer koji loop() obrađuje.
 * Nema debounce logike jer je hardware timer precizan i stabilan izvor.
 */
void IRAM_ATTR ISR_HWTimer() {
  portENTER_CRITICAL_ISR(&mux);
  flag_timer = true;
  portEXIT_CRITICAL_ISR(&mux);
}

// ISR - TIPKE

/**
 * @brief ISR za STOP tipku - najviši prioritet (P1).
 *
 * Postavlja flag_stop. Debounce sprječava višestruko okidanje
 * od jednog pritiska tipke.
 */
void IRAM_ATTR ISR_STOP() {
  unsigned long now = millis();
  if (now - lastInterrupt[1] < DEBOUNCE_MS) return;
  lastInterrupt[1] = now;
  portENTER_CRITICAL_ISR(&mux);
  flag_stop = true;
  portEXIT_CRITICAL_ISR(&mux);
}

/**
 * @brief ISR za DOOR tipku - srednji prioritet (P3).
 *
 * Postavlja flag_door. Obrađuje se u loop() nakon provjere
 * flag_stop i flag_timer.
 */
void IRAM_ATTR ISR_DOOR() {
  unsigned long now = millis();
  if (now - lastInterrupt[0] < DEBOUNCE_MS) return;
  lastInterrupt[0] = now;
  portENTER_CRITICAL_ISR(&mux);
  flag_door = true;
  portEXIT_CRITICAL_ISR(&mux);
}

/**
 * @brief ISR za SERVICE tipku - niski prioritet (P4).
 *
 * Postavlja flag_service. Obrađuje se u loop() zadnja
 * od svih tipkala.
 */
void IRAM_ATTR ISR_SERVICE() {
  unsigned long now = millis();
  if (now - lastInterrupt[2] < DEBOUNCE_MS) return;
  lastInterrupt[2] = now;
  portENTER_CRITICAL_ISR(&mux);
  flag_service = true;
  portEXIT_CRITICAL_ISR(&mux);
}

// SETUP

/**
 * @brief Inicijalizacija sustava - pinovi, timer, prekidi.
 *
 * Poziva se jednom pri pokretanju. Postavlja sve GPIO pinove,
 * inicijalizira hardware timer na 1 Hz i registrira ISR funkcije.
 */
void setup() {
  Serial.begin(115200);
  Serial.println("SMART GARAGE ESP32 START");
  delay(2000);  ///< Čekaj stabilizaciju pinova pri bootu

  // LED izlazi
  pinMode(LED_DOOR,    OUTPUT);
  pinMode(LED_STOP,    OUTPUT);
  pinMode(LED_SERVICE, OUTPUT);
  pinMode(LED_ALERT,   OUTPUT);
  pinMode(LED_TIMER,   OUTPUT);

  // Sve LED-ice OFF na startu
  digitalWrite(LED_DOOR,    LOW);
  digitalWrite(LED_STOP,    LOW);
  digitalWrite(LED_SERVICE, LOW);
  digitalWrite(LED_ALERT,   LOW);
  digitalWrite(LED_TIMER,   LOW);

  // Tipke s pull-up
  // NAPOMENA: GPIO 26 ima interni pull-up za razliku od GPIO 33
  pinMode(BUTTON_DOOR,    INPUT_PULLUP);
  pinMode(BUTTON_STOP,    INPUT_PULLUP);
  pinMode(BUTTON_SERVICE, INPUT_PULLUP);

  // HC-SR04
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

  // Hardware Timer (ESP32 Arduino Core 3.x API)
  // timerBegin prima frekvenciju timera u Hz
  // 1.000.000 Hz = 1 MHz = rezolucija 1 mikrosekunda
  hwTimer = timerBegin(1000000);
  timerAttachInterrupt(hwTimer, &ISR_HWTimer);
  // timerAlarm(timer, period_u_tikovima, auto_reload, reload_count)
  // 1.000.000 tikova * 1 us = 1 sekunda
  timerAlarm(hwTimer, 1000000UL, true, 0);

  // Vanjski prekidi za tipkala (FALLING = pritisak pull-up tipke)
  attachInterrupt(BUTTON_DOOR,    ISR_DOOR,    FALLING);
  attachInterrupt(BUTTON_STOP,    ISR_STOP,    FALLING);
  attachInterrupt(BUTTON_SERVICE, ISR_SERVICE, FALLING);

  Serial.println("Hardware timer : OK (1 Hz)");
  Serial.println("Interrupts     : OK (DOOR=13, STOP=26, SERVICE=25)");
  Serial.println("Senzor         : TRIG=4, ECHO=16");
  Serial.println("Prioriteti: STOP(P1) > TIMER(P2) > DOOR(P3) > SERVICE(P4) > SENZOR(P5)");
  Serial.println("Tip 'r' u Serial Monitor za reset emergency stopa.");
  Serial.println("-------------------------");
}

// LOOP

/**
 * @brief Glavna petlja - obrađuje zastavice prema prioritetu.
 *
 * Redosljed provjere definira softverski prioritet:
 *   1. flag_stop    - prekida sve ostalo odmah
 *   2. flag_timer   - periodička logika i auto-close
 *   3. flag_door    - toggle vrata
 *   4. flag_service - toggle servis mod
 *   5. senzor       - polling detekcije auta
 */
void loop() {

  // P1: EMERGENCY STOP - uvijek se provjerava prvi
  bool localStop = false;
  portENTER_CRITICAL(&mux);
  localStop = flag_stop;
  flag_stop = false;
  portEXIT_CRITICAL(&mux);

  if (localStop) {
    emergencyStop = true;
    doorOpen      = false;  // Zatvori vrata odmah
    Serial.println("[P1] !!! EMERGENCY STOP AKTIVIRAN !!!");
  }

  if (emergencyStop) {
    digitalWrite(LED_STOP,    HIGH);
    digitalWrite(LED_DOOR,    LOW);
    digitalWrite(LED_SERVICE, LOW);
    digitalWrite(LED_ALERT,   LOW);
    digitalWrite(LED_TIMER,   LOW);

    // Reset putem Serial naredbe 'r' ili 'R'
    if (Serial.available()) {
      char c = Serial.read();
      Serial.flush();
      if (c == 'r' || c == 'R') {
        emergencyStop  = false;
        timerTickCount = 0;
        digitalWrite(LED_STOP, LOW);
        Serial.println("[P1] RESET - sustav nastavlja normalan rad.");
      }
    }
    return;  // Blokiraj sve dok je emergency stop aktivan
  }

  // P2: HARDWARE TIMER
  bool localTimer = false;
  portENTER_CRITICAL(&mux);
  localTimer = flag_timer;
  flag_timer = false;
  portEXIT_CRITICAL(&mux);

  if (localTimer) {
    timerTickCount++;

    // Kratki puls na timer LED-ici
    digitalWrite(LED_TIMER, HIGH);
    delayMicroseconds(800);
    digitalWrite(LED_TIMER, LOW);

    Serial.print("[P2] Timer tik #");
    Serial.print(timerTickCount);

    if (doorOpen) {
      Serial.print(" | Vrata otvorena ");
      Serial.print(AUTO_CLOSE_TICKS - timerTickCount);
      Serial.println("s do auto-zatvaranja");
    } else {
      Serial.println();
    }

    // Auto-zatvori vrata nakon AUTO_CLOSE_TICKS sekundi
    if (doorOpen && !carDetected && timerTickCount >= AUTO_CLOSE_TICKS) {
      doorOpen       = false;
      timerTickCount = 0;
      Serial.println("[P2] AUTO CLOSE - vrata automatski zatvorena.");
    }
  }

  // P3: VRATA
  bool localDoor = false;
  portENTER_CRITICAL(&mux);
  localDoor = flag_door;
  flag_door = false;
  portEXIT_CRITICAL(&mux);

  if (localDoor) {
    if (!serviceMode) {
      doorOpen       = !doorOpen;
      timerTickCount = 0;  // Reset auto-close brojača
      Serial.print("[P3] VRATA: ");
      Serial.println(doorOpen ? "OTVORENA" : "ZATVORENA");
    } else {
      Serial.println("[P3] DOOR zanemaren - servis mod je aktivan!");
    }
  }

  // P4: SERVIS MOD
  bool localService = false;
  portENTER_CRITICAL(&mux);
  localService = flag_service;
  flag_service = false;
  portEXIT_CRITICAL(&mux);

  if (localService) {
    serviceMode = !serviceMode;
    Serial.print("[P4] SERVIS MOD: ");
    Serial.println(serviceMode ? "UKLJUCEN" : "ISKLJUCEN");
  }

  // P5: HC-SR04 SENZOR (polling, najniži prioritet)
  float dist   = measureDistance();
  bool prevCar = carDetected;
  carDetected  = (dist > 0 && dist < CAR_DISTANCE_CM);

  if (carDetected != prevCar) {
    Serial.print("[P5] SENZOR: auto ");
    Serial.print(carDetected ? "DETEKTIRAN" : "OTISAO");
    Serial.print(" (");
    Serial.print(dist, 1);
    Serial.println(" cm)");
  }

  // Ažuriraj LED status
  digitalWrite(LED_DOOR,    doorOpen      ? HIGH : LOW);
  digitalWrite(LED_SERVICE, serviceMode   ? HIGH : LOW);
  digitalWrite(LED_ALERT,   carDetected   ? HIGH : LOW);
  digitalWrite(LED_STOP,    emergencyStop ? HIGH : LOW);
}

// SENZOR

/**
 * @brief Mjeri udaljenost HC-SR04 senzorom.
 *
 * Šalje 10 µs trigger puls i mjeri echo.
 * Timeout 30 ms spriječava blokiranje loop-a.
 *
 * @return Udaljenost u cm, ili 0 ako je mjerenje neispravno.
 */
float measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000UL);  // timeout 30 ms

  if (duration <= 0) return 0;

  return (duration / 2.0f) * 0.0343f;
}
