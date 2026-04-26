#include <Arduino.h>
#include <Stepper.h>

const int stepsPerRevolution = 2048;
const int moveDistance = 512;
const int buttonPin = 15; // D8

// Virkende wiring:
// D5 -> IN1
// D7 -> IN2
// D6 -> IN3
// D1 -> IN4
Stepper myStepper(stepsPerRevolution, 14, 12, 13, 5);

uint8_t i = 0;

void moveForward() {
  for (int stepCount = 0; stepCount < moveDistance; stepCount++) {
    myStepper.step(1);
    yield();
  }
}

void moveBackward() {
  for (int stepCount = 0; stepCount < moveDistance; stepCount++) {
    myStepper.step(-1);
    yield();
  }
}

void setup() {
  pinMode(buttonPin, INPUT);
  myStepper.setSpeed(6);
  Serial.begin(9600);
}

void loop() {
  if (digitalRead(buttonPin) == HIGH) {
    delay(10);

    if (digitalRead(buttonPin) == HIGH) {
      if (i == 0) {
        Serial.println("Forward");
        moveForward();
        i = 1;
      } else {
        Serial.println("Backward");
        moveBackward();
        i = 0;
      }

      while (digitalRead(buttonPin) == HIGH) {
        delay(10);
      }
    }
  }
}
