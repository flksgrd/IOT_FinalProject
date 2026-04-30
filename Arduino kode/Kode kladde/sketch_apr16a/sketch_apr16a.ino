#include <Arduino.h>
#include <Stepper.h>
extern "C"{
  #include "user_interface.h"  
  }

// der skal sikres at stepperen kan køre begge veje når nu steps per revolution er uint
const uint16_t stepsPerRevolution = 2048; //16 bits to be able to hold 2048 in binary
const uint16_t moveDistance = 512; //16 bits to be able to hold 512 in binary
const uint8_t buttonPin = 15; // D8
volatile uint8_t CloseBin = 0; // volatile to make sure the variable value is not optimized out of memory
const uint8_t echoPin = 2;
const uint8_t trigPin = 0;
const uint8_t LED_pin = 7; //SD0/MISO
const uint8_t INT_pin = 9; //external interrupt pin

// Virkende wiring:
// D5 -> IN1
// D7 -> IN2
// D6 -> IN3
// D1 -> IN4

// enum variable (illustrative state variable)
enum State{

  LOAD,
  CHECK,
  CLOSE,
  EMPTY_ME

};

State CurrentState = LOAD; 

Stepper myStepper(stepsPerRevolution, 14, 12, 13, 5);

void moveForward(int x) {
  for (int stepCount = 0; stepCount < moveDistance; stepCount++) {
    myStepper.step(1);
    yield();
  }
}

void moveBackward(int x) {
  for (int stepCount = 0; stepCount < x; stepCount++) {
    myStepper.step(-1);
    yield();
  }
}
    

void setup() {
  pinMode(buttonPin, INPUT);
  pinMode(echoPin, INPUT);
  pinMode(trigPin, OUTPUT);
  myStepper.setSpeed(6);
  Serial.begin(9600);
  wifi_set_sleep_type(LIGHT_SLEEP_T);
 // attachInterrupt(digitalPinToInterrupt(INT_pin), Lys, RISING); //SKAL MÅSKE BRUGES
}



void Ultra_Sense(){
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 12000);
  float distance = duration * 0.034 / 2.0;
  Serial.print(distance);
  // *This if statement should trigger after a set amount of time ex 1min dosen't trigger imitietly
  // Trigger state switch when trash is 10cm or closer to being complately full.
  if (distance <= 10){

    CurrentState = CLOSE; 

  }else {

    CurrentState = CHECK; 

  }
  delay(20);
}


/*
void Ligthsens(){

int sensor = analogRead(A0);
int PWMValue = map(sensor, 0, 1023, 0, 1023);
analogWrite(LED_pin, pwmValue)

} 
*/

void loop() {
    
  // State machine switch case
  switch(CurrentState){

    case LOAD:
      Serial.println("LOAD");
      if (digitalRead(buttonPin) == HIGH) {
          delay(100);

        if (digitalRead(buttonPin) == HIGH) {
          moveBackward(50);
          CurrentState = CHECK; 

        }else{

        CurrentState = LOAD; 
            
        }
      }
    break;

    case CHECK: 
      Serial.println("CHECK");
      Serial.println("SLEEP_ACTIVE");
      delay(5000);
      Serial.println("AWAKE!");
      Ultra_Sense();
    break;

    case CLOSE: 
      Serial.println("CLOSE");
      moveBackward(8*512);
      CurrentState = EMPTY_ME;

    break; 
    case EMPTY_ME: //releases trash bag strings and returns to load when button is pressed.
      Serial.println("EMPTY_ME");
      delay(100);
      if(digitalRead(buttonPin) == HIGH){
        delay(100);

        if (digitalRead(buttonPin) == HIGH) {   
          delay(100);
          moveForward(8*512);
          CurrentState = LOAD;
        }
      }
    break; 

    default: 
    break;

    delay(50);
  }
}
