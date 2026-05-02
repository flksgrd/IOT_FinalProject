#include <U8g2lib.h>
#include <Wire.h>

U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE);
const byte IntPin = 2;
int i = 0;
int SuccesCounter = 0;
int TimeBetween[10] = {200, 185, 170, 155, 140, 125, 110, 95, 500, 400};
int RanPin;
volatile int ActivePin = 0;
char buffer[20];


enum States {
  Play,    // Normal sequential LED sweep (pins 12 → 8)
  Won,     // Player pressed button on the correct (middle) LED
  Lost,    // Player pressed button on the wrong LED
  Random,  // Harder mode: the lit LED is chosen at random each cycle
  End,     // Player reached 10 correct presses – game over / winner screen
  Wait,    //wait for player input
};

volatile States CurrentState = Wait; // Start the game in Play mode

// Interrupt Service Routine – fires on the falling edge of the button pin.
// Checks which LED is currently active and updates the state accordingly.
// Only pin 10 (the middle LED) counts as a correct press.
void result() {
  if (SuccesCounter < 10)
  {
    if (ActivePin == 10) {
    CurrentState = Won;   // Correct! Player hit the target LED
  }
      else {
          CurrentState = Lost;  // Wrong LED was lit when the button was pressed
          SuccesCounter = 0;
  }
}
  else {
    CurrentState = End;
  }
  }



// the setup function runs once when you press reset or power the board
void setup() {
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(8, OUTPUT); // Led
  pinMode(9, OUTPUT); // Led
  pinMode(10, OUTPUT); // Led
  pinMode(11, OUTPUT); // Led
  pinMode(12, OUTPUT); // Led
  pinMode(IntPin, INPUT_PULLUP); // Interrupt
  attachInterrupt(digitalPinToInterrupt(IntPin), result, FALLING); // FALLING because of the pullup-button
  display.begin();
  display.clearBuffer();
  display.setFont(u8g2_font_ncenB14_tr);
  display.drawStr(0,32,"Press to Play");
  display.sendBuffer();
}

// the loop function runs over and over again forever
void loop() {
  switch (CurrentState) {
    case Play:
      ActivePin = 12;
      digitalWrite(12, HIGH);  // turn the LED on (HIGH is the voltage level)
      delay(TimeBetween[SuccesCounter]);
      if (CurrentState != Play) break;    // Button was pressed during this LED
      digitalWrite(12, LOW);   // turn the LED off by making the voltage LOW

      ActivePin = 11;
      digitalWrite(11, HIGH);  // turn the LED on (HIGH is the voltage level)
      delay(TimeBetween[SuccesCounter]);
      if (CurrentState != Play) break;    // Button was pressed during this LED
      digitalWrite(11, LOW);   // turn the LED off by making the voltage LOW

      ActivePin = 10;
      digitalWrite(10, HIGH);  // turn the LED on (HIGH is the voltage level)
      delay(TimeBetween[SuccesCounter]);
      if (CurrentState != Play) break;    // Button was pressed during this LED
      digitalWrite(10, LOW);   // turn the LED off by making the voltage LOW

      ActivePin = 9;
      digitalWrite(9, HIGH);  // turn the LED on (HIGH is the voltage level)
      delay(TimeBetween[SuccesCounter]);
      if (CurrentState != Play) break;    // Button was pressed during this LED
      digitalWrite(9, LOW);   // turn the LED off by making the voltage LOW

      ActivePin = 8;
      digitalWrite(8, HIGH);  // turn the LED on (HIGH is the voltage level)
      delay(TimeBetween[SuccesCounter]);
      if (CurrentState != Play) break;    // Button was pressed during this LED
      digitalWrite(8, LOW);   // turn the LED off by making the voltage LOW
    break;

    case Won:
      SuccesCounter++;
      display.clearBuffer();
      sprintf(buffer, "Score: %d", SuccesCounter);
      display.drawStr(0,32,buffer);
      display.sendBuffer();

      // Flash all 5 LEDs 5 times as positive feedback
      for (i = 1; i <= 5; i++) {
        PORTB |= 0b00011111;  // Turn all 5 LEDs on  (PORTB bits 0–4 = pins 8–12)
        delay(50);
        PORTB &= 0b11100000;  // Turn all 5 LEDs off
        delay(50);
      }

      i = 0;              // Reset loop counter for future use
      delay(TimeBetween[0]); // Brief pause before transitioning

      if (SuccesCounter >= 10) {
        CurrentState = End;    // Player has won the whole game
      } else if (SuccesCounter >= 8) {
        CurrentState = Random; // Switch to harder random mode after 8 correct
      } else {
        CurrentState = Play;   // Continue normal play
      }
    break;

    case Lost:
      CurrentState = Play;
      delay(10);
      display.clearBuffer();
      display.drawStr(0,32,"Score: 0");
      display.sendBuffer();

      PORTB &= 0b11100000;  // Turn all LEDs off immediately as penalty feedback
      delay(3000);          // 3-second penalty pause

      // Resume the appropriate mode depending on current score
      if (SuccesCounter >= 8) {
        CurrentState = Random;
      } else {
        CurrentState = Play;
      }
    break;

    case Random:
      RanPin = random(8,13);    // Pick a random number from pin 8 to 12
      ActivePin = RanPin;
      digitalWrite(RanPin, HIGH);  // turn the LED on (HIGH is the voltage level)
      delay(TimeBetween[SuccesCounter]);
      if (CurrentState != Random) break;
      digitalWrite(RanPin, LOW);   // turn the LED off by making the voltage LOW
    break;

    case End:
      display.clearBuffer();
      display.drawStr(12,15,"You Won!");
      display.drawStr(0,40,"Press to play");
      display.drawStr(0,55,"again!");
      display.sendBuffer();
      CurrentState = Wait;
      break;

    case Wait: {
        PORTB |= 0b00010001;  // Turn all 5 LEDs on  (PORTB bits 0–4 = pins 8–12)
        delay(50);
        PORTB &= 0b11101010;  // Turn all 5 LEDs off
        delay(50);
        PORTB |= 0b00001010;  // Turn all 5 LEDs on  (PORTB bits 0–4 = pins 8–12)
        delay(50);
        PORTB &= 0b11110001;  // Turn all 5 LEDs off
        delay(50);
      }
          if (digitalRead(IntPin) == LOW) {
        SuccesCounter = 0;      // Reset score for a new game
        CurrentState = Play;    // Restart from the beginning
        display.clearBuffer();
        display.drawStr(0,32,"Score: 0");
        display.sendBuffer();
      }
    break;
  }
}
