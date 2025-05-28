// Programme systeme d'analyse d'eau de piscine
// avec correction PH par commande de pompe péristaltique

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Broche du capteur de température
#define ONE_WIRE_BUS 4  // GPIO4

// Initialisation du capteur de température
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// Broche du capteur de conductivité
#define CONDUCTIVITY_PIN 34  // GPIO34

// Broche du capteur de pH
#define PH_PIN 35  // GPIO35

// Broche du relais pour la pompe péristaltique
#define RELAY_PIN 5  // GPIO5

// Broche pour le capteur ORP
#define ORP_PIN 36  // GPIO36

// Définir les dimensions de l'écran OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// Initialisation de l'écran OLED
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void setup() {
  Serial.begin(115200);
  sensors.begin();
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Assurez-vous que la pompe est éteinte au démarrage

  // Initialiser l'écran OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Adresse I2C 0x3C pour 128x64
    Serial.println(F("Échec de l'initialisation de l'écran SSD1306"));
    for(;;);
  }
  display.display();
  delay(2000);
  display.clearDisplay();
}

void loop() {
  // Lire la température
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);

  // Lire la conductivité
  int conductivityValue = analogRead(CONDUCTIVITY_PIN);
  float conductivity = map(conductivityValue, 0, 4095, 0, 1000); // Ajustez selon votre capteur

  // Calculer la salinité (formule simplifiée)
  float salinity = conductivity * (1.0 + 0.02 * (tempC - 25.0));

  // Lire le pH
  int phValue = analogRead(PH_PIN);
  float ph = map(phValue, 0, 4095, 0, 1400) / 100.0; // Ajustez selon votre capteur

  // Lire l'ORP
  int orpValue = analogRead(ORP_PIN);
  float orp = map(orpValue, 0, 4095, 0, 3300) / 1000.0; // Convertir en millivolts

  // Afficher les résultats sur le moniteur série
  Serial.print("Température: ");
  Serial.print(tempC);
  Serial.print(" °C, Conductivité: ");
  Serial.print(conductivity);
  Serial.print(" µS/cm, Salinité: ");
  Serial.print(salinity);
  Serial.print(" ppm, pH: ");
  Serial.print(ph);
  Serial.print(", ORP: ");
  Serial.print(orp);
  Serial.println(" mV");

  // Afficher les résultats sur l'écran OLED
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Temp: ");
  display.print(tempC);
  display.println(" C");
  display.print("Cond: ");
  display.print(conductivity);
  display.println(" uS/cm");
  display.print("Sal: ");
  display.print(salinity);
  display.println(" ppm");
  display.print("pH: ");
  display.print(ph);
  display.println("");
  display.print("ORP: ");
  display.print(orp);
  display.println(" mV");
  display.display();

  // Contrôler la pompe péristaltique
  if (ph > 7.4) {
    digitalWrite(RELAY_PIN, HIGH); // Allumer la pompe
    Serial.println("Pompe allumée");
  } else if (ph < 7.0) {
    digitalWrite(RELAY_PIN, LOW); // Éteindre la pompe
    Serial.println("Pompe éteinte");
  }

  delay(1000);
}
