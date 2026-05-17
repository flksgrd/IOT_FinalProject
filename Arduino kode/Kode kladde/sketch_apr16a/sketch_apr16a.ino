// Required library (Arduino IDE -> Manage Libraries): "U8g2" by oliver

#include <Arduino.h>
#include <Stepper.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <U8g2lib.h>

// 28BYJ-48: 2048 steps per revolution
const uint16_t stepsPerRevolution = 2048;
const uint16_t moveDistance = 512;
const uint8_t buttonPin = 15; // D8
const uint8_t echoPin = 2;    // D4 (boot pin: 10k pull-up to 3.3V)
const uint8_t trigPin = 0;    // D3


// Stepper wiring:
// D5 (GPIO14) -> IN1
// D7 (GPIO13) -> IN2
// D6 (GPIO12) -> IN3
// D0 (GPIO16)  -> IN4

// OLED SSD1306 (I2C):
// D2 (GPIO4)  -> SDA
// D1 (GPIO5) -> SCL
// LM35:
// A0          -> Vout (10 mV/°C)

#define SDA_PIN      4   // D2
#define SCL_PIN      5   // D1

// WiFi (fill in before upload)
const char* WIFI_SSID     = "EKB";
const char* WIFI_PASSWORD = "ekbballerup";

// ThingSpeak channel 3364403. Free tier needs >=15s between writes -> push throttle 16s.
// Field5 is read every 30s and used as a remote command to set the bag count.
const unsigned long TS_CHANNEL_ID    = 3364403UL;
const char*         TS_WRITE_KEY     = "DAFOBTGX2VWN91LU";
const char*         TS_READ_KEY      = "U38AWRSBJVBTPPJK";
const unsigned long TS_PUSH_INTERVAL = 16000UL;
const unsigned long TS_READ_INTERVAL = 30000UL;

// bagsRemaining counts down on EMPTY_ME->LOAD (new bag fitted).
// pendingBagFull is set on the CLOSE transition so IFTTT fires only once per fill.
// tsDataDirty marks that there are new values for the next throttle window.
uint8_t bagsRemaining = 10;
const uint8_t BAGS_LOW_THRESHOLD = 2;
long lastField5EntryId = -1;
bool pendingBagFull = false;
bool tsDataDirty = true;
float lastDistance = -1.0f;
unsigned long lastTsPush = 0;
unsigned long lastTsRead = 0;

// state machine states
enum State{

  LOAD,
  CHECK,
  CLOSE,
  EMPTY_ME

};

State CurrentState = LOAD;

Stepper myStepper(stepsPerRevolution, 14, 12, 13, 16);
U8G2_SH1106_128X64_NONAME_F_SW_I2C display(U8G2_R0, SCL_PIN, SDA_PIN, U8X8_PIN_NONE);

float avgTemp = 0;
float OldTemp = 0;
static uint8_t i = 0; 
const uint8_t Readings = 10;
void readLM35() {
  float Temp = 0;
  uint16_t raw = 0; 

  // LM35: 10 mV/°C; tune 2.667 to the board's real ADC reference

  for(i = 0; i < Readings; i++){
    raw += analogRead(A0);
    delay(10);

}

  float avgRaw = (float)raw / Readings;
  
  Temp = avgRaw * (2.667f / 1023.0f) * 100.0f;   

  if (OldTemp == 0) {
      OldTemp = Temp; 
  }

  avgTemp = ((3.0f * Temp + 7.0f * OldTemp) / 10.0f); // IIR filter

  OldTemp = avgTemp;

  Serial.print("[LM35] raw="); Serial.print(raw);
  Serial.print("  temp="); Serial.println(avgTemp, 1);
  tsDataDirty = true;
}




void updateDisplay() {
  static const char* stateNames[] = {"LOAD", "CHECK", "CLOSE", "EMPTY_ME"};
  char buf[32];
  display.clearBuffer();
  display.setFont(u8g2_font_ncenB08_tr);
  if (!isnan(avgTemp))
    snprintf(buf, sizeof(buf), "Temp: %.1f C", avgTemp);
  else
    strcpy(buf, "Temp: --");
  display.drawStr(0, 12, buf);
  snprintf(buf, sizeof(buf), "State: %s", stateNames[CurrentState]);
  display.drawStr(0, 28, buf);
  snprintf(buf, sizeof(buf), "WiFi: %s", WiFi.status() == WL_CONNECTED ? "OK" : "NO");
  display.drawStr(0, 44, buf);
  display.sendBuffer();
}

void moveForward(int x) {
  for (int stepCount = 0; stepCount < x; stepCount++) {
    myStepper.step(1);
    yield();
  }
  digitalWrite(14, LOW);
  digitalWrite(12, LOW);
  digitalWrite(13, LOW);
  digitalWrite(16, LOW);
}

void moveBackward(int x) {
  for (int stepCount = 0; stepCount < x; stepCount++) {
    myStepper.step(-1);
    yield();
  }
  digitalWrite(14, LOW);
  digitalWrite(12, LOW);
  digitalWrite(13, LOW);
  digitalWrite(16, LOW);
}


// Connects to WiFi at boot with a 15s timeout. tsTick() skips when no WiFi.
void wifiConnect() {
  Serial.print("WiFi: ");
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);  // SDK reconnects in the background if the router restarts
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
  Serial.begin(115200);
  delay(100);
  Serial.println("\n=== BOOT ===");

  pinMode(buttonPin, INPUT);
  pinMode(echoPin, INPUT);
  pinMode(trigPin, OUTPUT);
  digitalWrite(trigPin, LOW);
  myStepper.setSpeed(15);

  display.begin();
  display.clearBuffer();
  display.sendBuffer();

  wifiConnect();
  updateDisplay();
}



void Ultra_Sense(){
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 12000);
  if (duration == 0) { delay(20); return; } // no echo: lid open or sensor moved
  float distance = duration * 0.034 / 2.0;
  Serial.print(distance);
  lastDistance = distance;
  tsDataDirty = true;
  // TODO: only switch state after the reading stays low for a while, not instantly
  // switch to CLOSE when trash is within ~8 cm of full
  if (distance <= 8){

    if (CurrentState != CLOSE) pendingBagFull = true;
    CurrentState = CLOSE;

  }else {

    CurrentState = CHECK;

  }
  delay(20);
}


// Pushes field1=distance, field2=bags left, field3=state, field4=bag-full event,
//        field6=temperature (C)
void tsPush() {
  WiFiClient client;
  HTTPClient http;
  String url = String("http://api.thingspeak.com/update?api_key=") + TS_WRITE_KEY
             + "&field2=" + String(bagsRemaining)
             + "&field3=" + String((int)CurrentState)
             + "&field4=" + String(pendingBagFull ? 1 : 0);
  if (lastDistance >= 0) url += "&field1=" + String(lastDistance, 1);
  if (!isnan(avgTemp))
    url += "&field6=" + String(avgTemp, 1);
  http.begin(client, url);
  int code = http.GET();
  Serial.print("[TS push] HTTP "); Serial.println(code);
  http.end();
  if (pendingBagFull) pendingBagFull = false;
}

// Reads the latest Field5 from ThingSpeak; a new entry_id => bagsRemaining is updated
// (remote command from the phone via the ThingSpeak app -> "Write to channel" -> Field5)
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

// Called from loop(). Throttles push (16s) and read (30s). Non-blocking vs. the state machine.
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
  // state machine
  switch(CurrentState){

    case LOAD:
      delay(500);
      readLM35();
      if (digitalRead(buttonPin) == HIGH) {
          delay(100);

        if (digitalRead(buttonPin) == HIGH) {
          moveBackward(2*512);
          CurrentState = CHECK;
          tsDataDirty = true;
          updateDisplay();

        }else{

        CurrentState = LOAD;

        }
      }
    break;

    case CHECK:
      delay(5000);
      Ultra_Sense();
      readLM35();
      updateDisplay();


      char pbuf[32];
      snprintf(pbuf, sizeof(pbuf), "%.0f%% Full", constrain(100.0f * (28.0f - lastDistance) / 28.0f, 0.0f, 100.0f));
      display.drawStr(55, 55, pbuf);
      display.sendBuffer();

    break;

    case CLOSE:
      Serial.println("-> CLOSE");
      moveBackward(12*512);
      CurrentState = EMPTY_ME;
      tsDataDirty = true;
      updateDisplay();
    break;
    case EMPTY_ME: // release bag strings, return to LOAD on button press
      Serial.println("-> EMPTY_ME");
      delay(100);
      if(digitalRead(buttonPin) == HIGH){
        delay(100);

        if (digitalRead(buttonPin) == HIGH) {
          delay(100);
          moveForward(12*512);
          if (bagsRemaining > 0) bagsRemaining--;
          CurrentState = LOAD;
          tsDataDirty = true;
          updateDisplay();
        }
      }

    break;

    default:
    break;
  }

  tsTick();
}
