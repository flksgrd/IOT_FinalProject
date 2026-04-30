#include <Arduino.h>
#include <Stepper.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

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

// WiFi (UDFYLD inden upload)
const char* WIFI_SSID     = "EKB";
const char* WIFI_PASSWORD = "ekbballerup";

// ThingSpeak: kanal 3364403. Free tier kræver min. 15s mellem writes -> push throttle 16s.
// Field5 læses hvert 30s og bruges som fjern-kommando til at sætte antal poser.
const unsigned long TS_CHANNEL_ID    = 3364403UL;
const char*         TS_WRITE_KEY     = "DAFOBTGX2VWN91LU";
const char*         TS_READ_KEY      = "U38AWRSBJVBTPPJK";
const unsigned long TS_PUSH_INTERVAL = 16000UL;
const unsigned long TS_READ_INTERVAL = 30000UL;

// Posekontrol: bagsRemaining tæller ned ved EMPTY_ME->LOAD (ny pose isat).
// pendingBagFull sættes ved CLOSE-overgang så IFTTT kun fyres én gang pr. fyldning.
// tsDataDirty markerer at der er nye værdier til næste throttle-vindue.
uint16_t bagsRemaining = 10;
const uint16_t BAGS_LOW_THRESHOLD = 2;
long lastField5EntryId = -1;
bool pendingBagFull = false;
bool tsDataDirty = true;
float lastDistance = -1.0f;
unsigned long lastTsPush = 0;
unsigned long lastTsRead = 0;

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


// Forbinder til WiFi ved boot med 15s timeout. tsTick() springer over uden WiFi.
void wifiConnect() {
  Serial.print("WiFi: ");
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);  // SDK genopretter forbindelse i baggrunden hvis routeren genstarter
  WiFi.persistent(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(250);
    Serial.print(".");
  }
  Serial.println(WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : String("FAILED"));
}

void setup() {
  pinMode(buttonPin, INPUT);
  pinMode(echoPin, INPUT);
  pinMode(trigPin, OUTPUT);
  myStepper.setSpeed(6);
  Serial.begin(9600);
  wifi_set_sleep_type(LIGHT_SLEEP_T);
  wifiConnect();
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
  lastDistance = distance;
  tsDataDirty = true;
  // *This if statement should trigger after a set amount of time ex 1min dosen't trigger imitietly
  // Trigger state switch when trash is 10cm or closer to being complately full.
  if (distance <= 10){

    if (CurrentState != CLOSE) pendingBagFull = true;
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

// Pusher Field1=afstand, Field2=poser tilbage, Field3=state, Field4=bag-full event
void tsPush() {
  WiFiClient client;
  HTTPClient http;
  String url = String("http://api.thingspeak.com/update?api_key=") + TS_WRITE_KEY
             + "&field2=" + String(bagsRemaining)
             + "&field3=" + String((int)CurrentState)
             + "&field4=" + String(pendingBagFull ? 1 : 0);
  if (lastDistance >= 0) url += "&field1=" + String(lastDistance, 1);  // springer over hvis vi ikke har målt endnu
  http.begin(client, url);
  int code = http.GET();
  Serial.print("[TS push] HTTP "); Serial.println(code);
  http.end();
  if (pendingBagFull) pendingBagFull = false;
}

// Læser seneste Field5 fra ThingSpeak; ny entry_id => bagsRemaining opdateres
// (fjernkommando fra telefon via ThingSpeak app -> "Write to channel" -> Field5)
void tsCheckBagCommand() {
  WiFiClient client;
  HTTPClient http;
  String url = String("http://api.thingspeak.com/channels/") + TS_CHANNEL_ID
             + "/fields/5/last.json?api_key=" + TS_READ_KEY;
  http.begin(client, url);
  int code = http.GET();
  if (code == 200) {
    String payload = http.getString();
    int idIdx  = payload.indexOf("\"entry_id\":");
    int valIdx = payload.indexOf("\"field5\":\"");
    if (idIdx >= 0 && valIdx >= 0) {
      long entryId = payload.substring(idIdx + 11).toInt();
      int valStart = valIdx + 10;
      int valEnd   = payload.indexOf("\"", valStart);
      int newCount = payload.substring(valStart, valEnd).toInt();
      if (entryId != lastField5EntryId && newCount >= 0) {
        bagsRemaining = (uint16_t)newCount;
        lastField5EntryId = entryId;
        tsDataDirty = true;
        Serial.print("[TS] bagsRemaining = "); Serial.println(bagsRemaining);
      }
    }
  }
  http.end();
}

// Kaldes fra loop(). Throttler push (16s) og read (30s). Ikke-blokerende ift. state machine.
void tsTick() {
  if (WiFi.status() != WL_CONNECTED) return;
  unsigned long now = millis();
  if (tsDataDirty && (lastTsPush == 0 || now - lastTsPush >= TS_PUSH_INTERVAL)) {
    tsPush();
    lastTsPush = now;
    tsDataDirty = false;
  }
  if (now - lastTsRead >= TS_READ_INTERVAL) {
    tsCheckBagCommand();
    lastTsRead = now;
  }
}

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
          tsDataDirty = true;

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
      tsDataDirty = true;

    break;
    case EMPTY_ME: //releases trash bag strings and returns to load when button is pressed.
      Serial.println("EMPTY_ME");
      delay(100);
      if(digitalRead(buttonPin) == HIGH){
        delay(100);

        if (digitalRead(buttonPin) == HIGH) {
          delay(100);
          moveForward(8*512);
          if (bagsRemaining > 0) bagsRemaining--;
          CurrentState = LOAD;
          tsDataDirty = true;
        }
      }
    break;

    default:
    break;

    delay(50);
  }

  tsTick();
}
