// Programme systeme d'analyse d'eau de piscine
// avec correction PH par commande de pompe péristaltique

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>  // Bibliothèque pour SH1106

float temperature = 25;
#define ArrayLenth  10    //times of collection

//Section Capteur de température
#define ONE_WIRE_BUS 4  // GPIO4 - Broche du capteur de température
OneWire oneWire(ONE_WIRE_BUS); // Initialisation du capteur de température
DallasTemperature sensors(&oneWire);
float tempC; // Température en degrés Celsius lue du capteur

// Section Relais
#define RELAY_PIN 5  // GPIO5 - Broche du relais pour la pompe péristaltique

// Section ORP
#define ORP_PIN 35  // GPIO35 - Broche pour le capteur ORP
double orpValue;
int orpArray[ArrayLenth];
int orpArrayIndex=0;
static unsigned long orpTimer=millis();   //analog sampling interval

// Section Ecran
#define SCREEN_WIDTH 128 // Définir les dimensions de l'écran OLED 1.3"
#define SCREEN_HEIGHT 64
#define OLED_RESET -1  // Reset pin not used
#define OLED_ADDR 0x3C  // Adresse I2C (peut être 0x3C ou 0x3D)
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET); // Initialisation de l'écran SH1106

// Section rebel pH
float calibration_value = 3.42;
int phval = 0; 
unsigned long int avgval; 
int buffer_arr[10],temp;
float ph_act, volt;

// Section pH
#define PH_PIN 33  // GPIO33 - Broche du capteur de pH
//#define volt (float)avgval*5.0/1024/6.0 //voltage value of PH sensor
int phValue; // Valeur brute lue du capteur de pH
float ph; // Valeur de pH calculée à partir de la valeur brute
uint64_t averagePH = 0; // Moyenne des lectures pH
uint64_t PHarray[ArrayLenth]; // Tableau pour stocker les lectures
int PHIndex = 0; // Index de la lecture actuelle
uint64_t PHtotal = 0; // Total des lectures
uint32_t tensionMilliVolts = 0; // Tension en millivolts lue du capteur pH

// Section TDS
#define TDS_PIN 34  // GPIO34 - Broche du capteur de conductivité
#define kValue 1.8 //kValue = value of calibrator TDS / measurement to get TDS
#define VREF 3.3 // analog reference voltage(Volt) of the ADC
int tdsValue; // Valeur brute lue du capteur TDS
uint64_t averagetds = 0; // Moyenne des lectures TDS
uint64_t tdsarray[ArrayLenth]; // Tableau pour stocker les lectures
int tdsIndex = 0; // Index de la lecture actuelle
uint64_t tdstotal = 0; // Total des lectures
float averageVoltds = 0; // Moyenne des tensions lues
float compCoeftds; // Coefficient de compensation de température
float compVolttds; // Tension compensée pour la température
float tdsPPM; // Valeur TDS en PPM

#define VOLTAGE 5.00    //system voltage
#define OFFSET 0        //zero drift voltage

// Sous fonction pour calculer la moyenne d'un tableau
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

float Lire_TDS() {
  // Lire la valeur brute du capteur TDS
  tdsValue = analogRead(TDS_PIN);
  
  // Appliquer le filtrage
  tdstotal = tdstotal - tdsarray[tdsIndex];
  tdsarray[tdsIndex] = tdsValue;
  tdstotal = tdstotal + tdsarray[tdsIndex];
  tdsIndex = tdsIndex + 1;

  if (tdsIndex >= ArrayLenth) {
    tdsIndex = 0;
  }

  averagetds = (tdstotal / ArrayLenth) * (float)VREF / 4095.0; // Convertir la moyenne en tension (0-3.3V pour ESP32 ADC)
  averageVoltds = map(tdsValue, 0, 4095, 0, 1000); // Convertir la moyenne en tension (0-3.3V pour ESP32 ADC)
  
  compCoeftds = 1.0 + 0.02 * (tempC - 25.0); // Formule de compensation de température
  compVolttds = averagetds / compCoeftds; // Compensé par le coefficient de compensation de température
  tdsPPM = ((133.42 * compVolttds * compVolttds * compVolttds) - (255.86 * compVolttds * compVolttds) + (857.39 * compVolttds)) * 0.5 * kValue; // Convertir la tension en PPM

  return tdsPPM;
}


float Lire_PH() {
  // Lire la valeur brute du capteur pH
  uint32_t tensionMilliVolts2 = analogRead(PH_PIN) * (VOLTAGE / 4095) * 1000;

  // Appliquer le filtrage
  PHtotal = PHtotal - PHarray[PHIndex];
  PHarray[PHIndex] = tensionMilliVolts2; // Stocker la tension lue dans le tableau
  PHtotal = PHtotal + PHarray[PHIndex];
  PHIndex = PHIndex + 1;

  if (PHIndex >= ArrayLenth) {
    PHIndex = 0;
  }

  averagePH = (PHtotal / ArrayLenth); // Convertir la moyenne en tension
  return averagePH;
}

void setup() {
  Serial.begin(115200);
  sensors.begin();
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Assurez-vous que la pompe est éteinte au démarrage

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

  // Initialiser le tableau de lectures TDS/PH
  for (int thisReading = 0; thisReading < ArrayLenth; thisReading++) {
    tdsarray[thisReading] = 0;
    PHarray[thisReading] = 0;
  }
}

void loop() {
  // Lire la température
  sensors.requestTemperatures();
  tempC = sensors.getTempCByIndex(0);

  // Lire le TDS
  tdsPPM = Lire_TDS(); // Convertir la tension en PPM

  // Lire le pH
  tensionMilliVolts =  Lire_PH();
  ph = map(tensionMilliVolts, 3480, 4260, 9180, 6864) / 1000.0;

  // ph rebel
  for(int i=0;i<10;i++) { 
    buffer_arr[i]=analogRead(PH_PIN);
    delay(30);
  }
  for(int i=0;i<9;i++){
    for(int j=i+1;j<10;j++){
      if(buffer_arr[i]>buffer_arr[j]) {
        temp=buffer_arr[i];
        buffer_arr[i]=buffer_arr[j];
        buffer_arr[j]=temp;
      }
    }
  }
  avgval=0;
  for(int i=2;i<8;i++) {
      avgval+=buffer_arr[i];
  }
  volt=(float)avgval * VOLTAGE/4095/6;
  //ph_act = -5.706 + (25.106 * volt) - (26.106 * volt * volt) + (9.6 * volt * volt * volt); // Calculation to convert voltage to pH
  ph_act = (0.0178 * volt * 200.0) -1.889;
  //ph_act = volt + calibration_value;

  // Lire l'ORP
    //static unsigned long printTime=millis();
  if(millis() >= orpTimer)
  {
    orpTimer=millis()+20;
    orpArray[orpArrayIndex++]=analogRead(ORP_PIN);    //read an analog value every 20ms
    if (orpArrayIndex==ArrayLenth) {
      orpArrayIndex=0;
    }   
    orpValue=((30*(double)VOLTAGE*1000)-(75*avergearray(orpArray, ArrayLenth)*VOLTAGE*1000/4095))/75-OFFSET;   //convert the analog value to orp according the circuit
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
  display.println(" C");
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
  //display.print("compCoef : ");
  //display.println(compCoeftds);
  //display.print("compVolt : ");
  //display.println(compVolttds);
  display.print("Voltage1 : ");
  display.print(tensionMilliVolts);
  display.println(" mV");
  display.print("Ph rebel : ");
  display.println(ph_act);
  display.display();



  // Contrôler la pompe péristaltique
  if (ph > 7.4) {
    digitalWrite(RELAY_PIN, HIGH); // Allumer la pompe
    Serial.println("Pompe allumée");
  } else if (ph < 7.0) {
    digitalWrite(RELAY_PIN, LOW); // Éteindre la pompe
    Serial.println("Pompe éteinte");
  }

  delay(100);
}
