#include <Stepper.h>

/* ---------- STEPMOTOR ---------- */
// 28BYJ-48 / lignende med gear
const int stepsPerRevolution = 2048;

// IN1, IN3, IN2, IN4 (rækkefølgen er vigtig)
Stepper stepMotor(stepsPerRevolution,
                  5,   // GPIO5  (D1)
                  0,   // GPIO0  (D3)
                  4,   // GPIO4  (D2)
                  2);  // GPIO2  (D4)

/* ---------- ULTRASONIC SENSOR ---------- */
const int trigPin = 14; // D5
const int echoPin = 12; // D6 (VIA SPÆNDINGSDELER!)

/* ---------- RESET & LED ---------- */
const int resetPin = 13; // D7 (knap til GND)
const int ledPin   = 15; // D8 (LED via modstand til GND)

/* ---------- KALIBRERING ---------- */
const float emptyDistance = 40.0;          // cm – MÅL SELV
const float fullThreshold = emptyDistance * 0.2;

/* ---------- TILSTAND ---------- */
bool bagClosed = false;

void setup() {
  // Pins
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(resetPin, INPUT_PULLUP);
  pinMode(ledPin, OUTPUT);

  digitalWrite(ledPin, LOW); // LED slukket ved start

  // Stepmotor
  stepMotor.setSpeed(15); // RPM (justér om nødvendigt)

  // Serial
  Serial.begin(9600);
  Serial.println("ESP8266 skraldesystem startet");
}

/* ---------- AFSTANDSMÅLING ---------- */
float getDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 30000);
  if (duration == 0) return -1;

  return duration * 0.034 / 2.0;
}

void loop() {

  /* ---------- RESET: ÅBN POSEN ---------- */
  if (digitalRead(resetPin) == LOW) {
    Serial.println("Reset trykket – åbner pose");

    digitalWrite(ledPin, HIGH);   // LED tændt
    stepMotor.step(-3072);        // MODSAT RETNING (1.5 omgang)
    bagClosed = false;

    delay(1000);                  // debounce / synlig handling
    digitalWrite(ledPin, LOW);    // LED slukket

    Serial.println("Klar til ny pose");
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

    Serial.println("80% fuld – lukker posen");

    stepMotor.step(3072); // 1.5 omgang frem
    bagClosed = true;

    Serial.println("Pose lukket");
  }

  delay(1000);
}
