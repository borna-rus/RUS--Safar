/**
 * Smart Garage System - ESP32 verzija
 */
/**
 * @brief Glavna funkcija programa
 */
#include <Arduino.h>

// ================= PINOVI =================

// LED
#define LED_DOOR 18
#define LED_STOP 19
#define LED_SERVICE 21
#define LED_ALERT 22
#define LED_TIMER 23

// TIPKE
#define BUTTON_DOOR 13
#define BUTTON_STOP 33
#define BUTTON_SERVICE 25

// SENZOR
#define TRIG_PIN 4
#define ECHO_PIN 5

// ================= VARIJABLE =================

volatile bool doorPressed = false;
volatile bool stopPressed = false;
volatile bool servicePressed = false;

// logika
bool doorOpen = false;
bool emergencyStop = false;
bool serviceMode = false;
bool carDetected = false;

// timer (software)
unsigned long lastTimer = 0;
const int TIMER_INTERVAL = 3000;

// debounce
unsigned long lastInterruptTime[3] = {0,0,0};
const int DEBOUNCE_DELAY = 200;

// ================= ISR =================

void IRAM_ATTR ISR_DOOR() {
  if (millis() - lastInterruptTime[0] < DEBOUNCE_DELAY) return;
  lastInterruptTime[0] = millis();
  doorPressed = true;
}

void IRAM_ATTR ISR_STOP() {
  if (millis() - lastInterruptTime[1] < DEBOUNCE_DELAY) return;
  lastInterruptTime[1] = millis();
  stopPressed = true;
}

void IRAM_ATTR ISR_SERVICE() {
  if (millis() - lastInterruptTime[2] < DEBOUNCE_DELAY) return;
  lastInterruptTime[2] = millis();
  servicePressed = true;
}

// ================= SETUP =================

void setup() {

  Serial.begin(115200);

  // LED
  pinMode(LED_DOOR, OUTPUT);
  pinMode(LED_STOP, OUTPUT);
  pinMode(LED_SERVICE, OUTPUT);
  pinMode(LED_ALERT, OUTPUT);
  pinMode(LED_TIMER, OUTPUT);

  // tipke
  pinMode(BUTTON_DOOR, INPUT_PULLUP);
  pinMode(BUTTON_STOP, INPUT_PULLUP);
  pinMode(BUTTON_SERVICE, INPUT_PULLUP);

  // senzor
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // prekidi
  attachInterrupt(BUTTON_DOOR, ISR_DOOR, FALLING);
  attachInterrupt(BUTTON_STOP, ISR_STOP, FALLING);
  attachInterrupt(BUTTON_SERVICE, ISR_SERVICE, FALLING);

  Serial.println("SMART GARAGE ESP32 START");
}

// ================= LOOP =================

void loop() {

  // STOP ima najveći prioritet
  if (stopPressed) {
    emergencyStop = true;
    stopPressed = false;
  }

  if (emergencyStop) {
    digitalWrite(LED_STOP, HIGH);
    Serial.println("EMERGENCY STOP!");
    delay(500);
    return;
  }

  // vrata
  if (doorPressed) {
    doorOpen = !doorOpen;
    Serial.println("DOOR TOGGLE");
    doorPressed = false;
  }

  // servis
  if (servicePressed) {
    serviceMode = !serviceMode;
    Serial.println("SERVICE MODE");
    servicePressed = false;
  }

  // SENZOR
  float distance = measureDistance();
  carDetected = (distance > 0 && distance < 50);

  digitalWrite(LED_ALERT, carDetected ? HIGH : LOW);

  // TIMER (software)
  if (millis() - lastTimer > TIMER_INTERVAL) {
    lastTimer = millis();

    digitalWrite(LED_TIMER, HIGH);
    delay(100);
    digitalWrite(LED_TIMER, LOW);

    if (doorOpen && !carDetected) {
      Serial.println("AUTO CLOSE");
      doorOpen = false;
    }
  }

  // LED status
  digitalWrite(LED_DOOR, doorOpen ? HIGH : LOW);
  digitalWrite(LED_SERVICE, serviceMode ? HIGH : LOW);
}

// ================= SENZOR =================

float measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH);
  float distance = (duration / 2.0) * 0.0343;

  if (duration <= 0 || duration > 30000) return 0;

  return distance;
}
