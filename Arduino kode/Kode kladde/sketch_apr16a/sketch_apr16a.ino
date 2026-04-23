#include <AccelStepper.h>

/* =========================================================
   STEPMOTOR – 28BYJ‑48 + ULN2003
   ---------------------------------------------------------
   VIGTIGT:
   - HALF‑STEP for bedre moment
   - KUN sikre ESP8266‑GPIO’er
   - Rækkefølge: IN1, IN2, IN3, IN4
   ========================================================= */

AccelStepper stepMotor(
  AccelStepper::HALF4WIRE,
  14, // D5  ->  IN1
  13, // D6  ->  IN2
  12, // D7  ->  IN3
  5   // D1  ->  IN4
);

/* =========================================================
   ULTRASONIC SENSOR (HC‑SR04 eller lign.)
   ---------------------------------------------------------
   Disse pins må gerne være boot‑pins, da sensoren er passiv
   ========================================================= */

const int trigPin = 0;  // D3 (GPIO0)
const int echoPin = 2;  // D4 (GPIO2) – VIA SPÆNDINGSDELER!

/* =========================================================
   RESET‑KNAP & STATUS‑LED
   ========================================================= */

const int resetPin = 15; // D8 (GPIO15) – INPUT_PULLUP
const int ledPin   = 16; // D0 (GPIO16)

/* =========================================================
   KALIBRERING
   ========================================================= */

const float emptyDistance = 40.0;              // cm – mål selv
const float fullThreshold = emptyDistance*0.2; // 80 % fuld

/* =========================================================
   TILSTAND
   ========================================================= */

bool bagClosed = false;

/* =========================================================
   SETUP
   ========================================================= */

void setup() {

  /* --- I/O --- */
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  pinMode(resetPin, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  /* --- STEPMOTOR PARAMETRE ---
     LAVE værdier = stabil drift */
  stepMotor.setMaxSpeed(800);       // steps/sek
  stepMotor.setAcceleration(100);   // steps/sek²
  stepMotor.setCurrentPosition(0);

  /* --- SERIAL --- */
  Serial.begin(9600);
  Serial.println("ESP8266 skraldesystem – HALF‑STEP startet");
}

/* =========================================================
   AFSTANDSMÅLING
   ========================================================= */

float getDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 30000); // 30 ms timeout
  if (duration == 0) return -1;

  return duration * 0.034 / 2.0;
}

/* =========================================================
   LOOP
   ========================================================= */

void loop() {

  /* ---------- RESET: ÅBN POSEN ---------- */
  if (digitalRead(resetPin) == LOW) {

    Serial.println("Reset trykket – åbner pose");
    digitalWrite(ledPin, HIGH);

    stepMotor.move(-3072);          // 1,5 omgang tilbage
    stepMotor.runToPosition();

    bagClosed = false;

    delay(1000);                    // debounce
    digitalWrite(ledPin, LOW);

    Serial.println("Pose åbnet – klar");
  }

  /* ---------- MÅLING ---------- */
  float distance = getDistance();

  Serial.print("Afstand: ");
  Serial.print(distance);
  Serial.println(" cm");

  /* ---------- LUK POSEN ---------- */
  if (distance > 0 &&
      distance <= fullThreshold &&
      !bagClosed) {

    Serial.println("80 % fuld – lukker posen");

    stepMotor.move(3072);           // 1,5 omgang frem
    stepMotor.runToPosition();

    bagClosed = true;

    Serial.println("Pose lukket");
  }

  delay(1000);
}
