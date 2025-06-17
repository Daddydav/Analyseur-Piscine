// Programme systeme d'analyse d'eau de piscine
// avec correction PH par commande de pompe péristaltique

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>  // Bibliothèque pour SH1106
//#include <Adafruit_SSD1306.h>

// Broche du capteur de température
#define ONE_WIRE_BUS 4  // GPIO4

// Initialisation du capteur de température
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// Broche du capteur de conductivité
#define TDS_PIN 34  // GPIO34

// Broche du capteur de pH
#define PH_PIN 35  // GPIO35

// Broche du relais pour la pompe péristaltique
#define RELAY_PIN 5  // GPIO5

// Broche pour le capteur ORP
#define ORP_PIN 36  // GPIO36

// Définir les dimensions de l'écran OLED
//#define SCREEN_WIDTH 128
//#define SCREEN_HEIGHT 64

// Définir les dimensions de l'écran OLED 1.3"
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1  // Reset pin not used
#define OLED_ADDR 0x3C  // Adresse I2C (peut être 0x3C ou 0x3D)

// Initialisation de l'écran SH1106
//Adafruit_SH1106 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


#define kValue 1.8 //kValue = value of calibrator TDS / measurement to get TDS
#define VREF 3.3 // analog reference voltage(Volt) of the ADC

#define VOLTAGE 5.00    //system voltage
#define OFFSET 0        //zero drift voltage

double orpValue;

#define ArrayLenth  40    //times of collection

int orpArray[ArrayLenth];
int orpArrayIndex=0;


float averageVoltage = 0, temperature = 25;

//// Initialisation de l'écran OLED
////Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);


// Variables pour le filtrage des lectures TDS
const int numReadings = 10; // Nombre de lectures pour le filtrage
uint64_t readings[numReadings]; // Tableau pour stocker les lectures
int readIndex = 0; // Index de la lecture actuelle
uint64_t total = 0; // Total des lectures
uint64_t averageTDS = 0; // Moyenne des lectures TDS

double avergearray(int* arr, int number){
  int i;
  int max,min;
  double avg;
  long amount=0;
  if(number<=0){
    printf("Error number for the array to avraging!/n");
    return 0;
  }
  if(number<5){   //less than 5, calculated directly statistics
    for(i=0;i<number;i++){
      amount+=arr[i];
    }
    avg = amount/number;
    return avg;
  }else{
    if(arr[0]<arr[1]){
      min = arr[0];max=arr[1];
    }
    else{
      min=arr[1];max=arr[0];
    }
    for(i=2;i<number;i++){
      if(arr[i]<min){
        amount+=min;        //arr<min
        min=arr[i];
      }else {
        if(arr[i]>max){
          amount+=max;    //arr>max
          max=arr[i];
        }else{
          amount+=arr[i]; //min<=arr<=max
        }
      }//if
    }//for
    avg = (double)amount/(number-2);
  }//if
  return avg;
}


void setup() {
  Serial.begin(115200);
  sensors.begin();
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Assurez-vous que la pompe est éteinte au démarrage

  // Initialiser l'écran OLED
  //if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Adresse I2C 0x3C pour 128x64
  //  Serial.println(F("Échec de l'initialisation de l'écran SSD1306"));
  //  for(;;);
  //}
  //display.display();
  //delay(2000);
  //display.clearDisplay();

  // Initialiser l'écran SH1106
  if(!display.begin(OLED_ADDR, true)) { // true pour indiquer que c'est un SH1106
    Serial.println(F("Échec de l'initialisation de l'écran SH1106"));
    while(1); // Bloquer ici si l'initialisation échoue
  }

  display.display();
  delay(2000);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);


  // Initialiser le tableau de lectures TDS
  for (int thisReading = 0; thisReading < numReadings; thisReading++) {
    readings[thisReading] = 0;
  }
}

void loop() {
  // Lire la température
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);

  // Lire le TDS
  int tdsValue = analogRead(TDS_PIN);

  // Appliquer le filtrage
  total = total - readings[readIndex];
  readings[readIndex] = tdsValue;
  total = total + readings[readIndex];
  readIndex = readIndex + 1;

  if (readIndex >= numReadings) {
    readIndex = 0;
  }

  averageTDS = (total / numReadings)  * (float)VREF / 4095.0; // Convertir la moyenne en tension (0-3.3V pour ESP32 ADC)
  averageVoltage = map(tdsValue, 0, 4095, 0, 1000);; // Convertir la moyenne en tension (0-3.3V pour ESP32 ADC)
  //float tdsPPM = map(averageTDS, 0, 4095, 0, 1000); // Convertir en ppm (ajustez selon votre capteur)

  float compensationCoefficient = 1.0+0.02*(tempC-25.0); //temperature compensation formula: fFinalResult(25^C) = fFinalResult(current)/(1.0+0.02*(fTP-25.0));
  float compensationVolatge = averageTDS/compensationCoefficient; //temperature compensation
  float tdsPPM = ((133.42*compensationVolatge*compensationVolatge*compensationVolatge) - (255.86*compensationVolatge*compensationVolatge) + (857.39*compensationVolatge))*0.5*kValue; //convert voltage value to tds value

  // Lire le pH
  int phValue = analogRead(PH_PIN);
  float ph = map(phValue, 0, 4095, 0, 1400) / 100.0; // Ajustez selon votre capteur
  
  // Lire l'ORP
  static unsigned long orpTimer=millis();   //analog sampling interval
  static unsigned long printTime=millis();
  if(millis() >= orpTimer)
  {
    orpTimer=millis()+20;
    orpArray[orpArrayIndex++]=analogRead(ORP_PIN);    //read an analog value every 20ms
    if (orpArrayIndex==ArrayLenth) {
      orpArrayIndex=0;
    }   
    orpValue=((30*(double)VOLTAGE*1000)-(75*avergearray(orpArray, ArrayLenth)*VOLTAGE*1000/1024))/75-OFFSET;   //convert the analog value to orp according the circuit
  }
  
  //int orpValue = analogRead(ORP_PIN);
  //float orp = map(orpValue, 0, 4095, 0, 3300) / 1000.0; // Convertir en millivolts

  // Afficher les résultats sur le moniteur série
  Serial.print("Température: ");
  Serial.print(tempC);
  Serial.print(" °C, TDS: ");
  Serial.print(tdsPPM);
  Serial.print(" ppm, pH: ");
  Serial.print(ph);
  Serial.print(", ORP: ");
  Serial.print(orpValue);
  Serial.println(" mV");

  // Afficher les résultats sur l'écran OLED
  display.clearDisplay();
  //display.setTextSize(1);
  //display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Temp : ");
  display.print(tempC);
  display.println(" °C");
  display.print("TDS : ");
  display.print(tdsPPM);
  display.println(" ppm");
  display.print("pH : ");
  display.print(ph);
  display.println("");
  display.print("ORP : ");
  display.print(orpValue);
  display.println(" mV");
  display.print("Etat pompe : ");
  if (digitalRead(RELAY_PIN) == HIGH) {
    display.println("ON");
  } else {
    display.println("OFF");
  }
  display.print("compCoef : ");
  display.println(compensationCoefficient);
  display.print("compVolt : ");
  display.println(compensationVolatge);
  display.print("Voltage : ");
  display.println(averageVoltage);
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
