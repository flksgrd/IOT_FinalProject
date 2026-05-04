#include <DHT.h>

// --- Konfiguration for DHT11 ---
#define DHTPIN 2       // Digital pin forbundet til DHT11's data pin
#define DHTTYPE DHT11  // Vi fortæller biblioteket, at det er en DHT11
DHT dht(DHTPIN, DHTTYPE);

// --- Konfiguration for LM35 ---
#define LM35PIN A0     // Analog pin forbundet til LM35's output pin

void setup() {
  // Start den serielle kommunikation
  Serial.begin(9600);
  Serial.println("Starter temperaturmåling med LM35 og DHT11...");

  // Start DHT-sensoren
  dht.begin();
}

void loop() {
  // Vent 2 sekunder mellem målingerne. 
  // DHT11 er en langsom sensor, der kræver mindst 2 sekunder mellem aflæsninger.
  delay(2000);

  // --- Læs fra DHT11 ---
  float tempDHT = dht.readTemperature(); // Læser temperatur i Celsius
  
  // Tjek om læsningen fra DHT11 fejlede (NaN = Not a Number)
  if (isnan(tempDHT)) {
    Serial.println("Fejl: Kunne ikke læse fra DHT11 sensoren!");
  }

  // --- Læs fra LM35 ---
  int analogValue = analogRead(LM35PIN);
  
  // Konverter den analoge værdi (0-1023) til temperatur.
  // Først konverteres til spænding (5V / 1024 steps),
  // LM35 outputter 10mV (0.01V) per grad Celsius, så vi ganger med 100 for at få grader.
  float voltage = analogValue * (5.0 / 1024.0);
  float tempLM35 = voltage * 100.0;

  // --- Udskriv resultaterne i Serial Monitor ---
  Serial.print("DHT11 Temperatur: ");
  if (!isnan(tempDHT)) {
    Serial.print(tempDHT);
    Serial.print(" °C");
  }
  
  Serial.print("   |   LM35 Temperatur: ");
  Serial.print(tempLM35);
  Serial.println(" °C");
}