#include <DHT.h>

// --- DHT11 config ---
#define DHTPIN 2       // digital pin to DHT11 data
#define DHTTYPE DHT11  // sensor type
DHT dht(DHTPIN, DHTTYPE);

// --- LM35 config ---
#define LM35PIN A0     // analog pin to LM35 output

void setup() {
  // start serial
  Serial.begin(9600);
  Serial.println("Starter temperaturmåling med LM35 og DHT11...");

  // start the DHT sensor
  dht.begin();
}

void loop() {
  // DHT11 is slow: needs at least 2 s between reads
  delay(2000);

  // --- read DHT11 ---
  float tempDHT = dht.readTemperature(); // temperature in Celsius

  // DHT11 read failed if NaN
  if (isnan(tempDHT)) {
    Serial.println("Fejl: Kunne ikke læse fra DHT11 sensoren!");
  }

  // --- read LM35 ---
  int analogValue = analogRead(LM35PIN);

  // analog (0-1023) -> voltage (5V / 1024) -> degrees
  // LM35 = 10 mV/degC, so multiply volts by 100
  float voltage = analogValue * (5.0 / 1024.0);
  float tempLM35 = voltage * 100.0;

  // --- print results ---
  Serial.print("DHT11 Temperatur: ");
  if (!isnan(tempDHT)) {
    Serial.print(tempDHT);
    Serial.print(" °C");
  }
  
  Serial.print("   |   LM35 Temperatur: ");
  Serial.print(tempLM35);
  Serial.println(" °C");
}