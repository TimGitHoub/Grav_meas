#include<Arduino.h> 
#include<Wire.h> 
#include "libSender.h"
#include "pressureSensor.h"

#define pegel 2000 //Pegel = 2000 für 1ms
#define triggerPegel pegel/2
//#define TCAADDR 0x70
#define AddSensor 0x28
#define xSamples 5
#define MittelwertAnzahl 30

//For Data Logger:
#include <SPI.h>
#include <SD.h>
const int chipSelect = 10;

 
uint8_t id1 = 1; //id1 i2c Kanal
uint8_t id2 = 2; //id2 i2c Kanal
float press_counts = 0; // digital pressure reading [counts] 
float temp_counts = 0; // digital temperature reading [counts] 
float pressure = 0.0; // pressure reading [bar, psi, kPa, etc.] 
float temperature = 0.0; // temperature reading in deg C 
float outputmax = 15099494.0; // output at maximum pressure [counts] 
float outputmin = 1677722.0; // output at minimum pressure [counts] 
float pmax = 6000.0; // maximum value of pressure range [Pa] here 6kPa 
float pmin = 0.0; // minimum value of pressure range [bar, psi, kPa, etc.] 
float percentage = 0; // holds percentage of full scale data 
char printBuffer[200], cBuff[20], percBuff[20], pBuff[20], tBuff[20]; 
double pressureDifference=0.0;
float pressOffsetS1=0;
float pressOffsetS2=0;
//int MittelwertAnzahl=30;

//void getPressureCalibrationValue(&pressOffsetS1, &pressOffsetS2, press_counts, temp_counts, MittelwerAnzahl)


void setup() { 
  DDRC|=(1<<PC0); //PC0 als Ausgang für Sender
  //  TIMER1 Einstellungen
  TCCR1A = 0x00; // OC2A and OC2B disconnected; Wave Form Generator: Normal Mode
  TCCR1B = (1<<CS11); // prescaler = 8. Eine Zähleinheit= 0.5us => 1ms=2000Einheiten => 1OVF=32768us

/*
//DATA LOGGER
    Serial.print("Initializing SD card...");
  
    // see if the card is present and can be initialized:
    if (!SD.begin(chipSelect)) {
      Serial.println("Card failed, or not present");
      // don't do anything more:
      while (1);
    }
    Serial.println("card initialized.");
//DATA LOGGER END
*/
  Serial.begin(9600); 
  while (!Serial) {                   // wait for serial port to connect. Needed for native USB
    delay(10); 
  } 
  Wire.begin();                       //as not specified, joins the i2c bus as a master
  Serial.println("Sensor 1 und Sensor 2 Datenausgabe");
  // removed calibration function to ensure a safer data capture
  /*
  Serial.println("Ermittle Kalibrierungswerte für S1 und S2. Bitte warten...");
  getPressureCalibrationValue(&pressOffsetS1, &pressOffsetS2, &press_counts, &temp_counts, MittelwertAnzahl);
  Serial.println("Kalibrierungswerte S1     S2");
  Serial.print(pressOffsetS1);
  Serial.print("        ");
  Serial.println(pressOffsetS2);
  Serial.println("Druck: \t\t Temp: \t\t PressCounts: \t\t TempCounts: \t Pressure Diff 1-2:" );
  */
} 

void loop() {
  float pressure1 = 0; // pressure reading [bar, psi, kPa, etc.]
  float pressure2 = 0; // pressure reading [bar, psi, kPa, etc.] 
  float temp1 = 0; // temperature reading in deg C 
  float temp2 = 0; // temperature reading in deg C
  float results[2][4];    // gererates an array to save the pressure, pressure counts, temp, temp counts for each sensor
  for(int i=0; i<2;i++){  //save values from the 2 sensors into the array
    tcaselect(i);
    //Taking x samples
    float samples[xSamples][2];
    for(int n=0;n<xSamples;n++){
      readSensor(&press_counts, &temp_counts);
      samples[n][0] = press_counts;
      samples[n][1] = temp_counts;
    };
    long long pressBuff=0;
    long long tempBuff=0;
    for(int j=0; j<xSamples;j++){   //summiert die Messwerte von xSamples auf
    pressBuff+=samples[j][0];
    tempBuff+=samples[j][1];
    };
    if(i==0){
    press_counts=pressBuff/xSamples+pressOffsetS1; //speichert den Mittelwert der Druckzähler ab
    temp_counts=tempBuff/xSamples+pressOffsetS1;  //speichert den Mittelwert der Temperaturzähler ab
    }
    else{
    press_counts=pressBuff/xSamples+pressOffsetS2; //speichert den Mittelwert der Druckzähler ab
    temp_counts=tempBuff/xSamples+pressOffsetS2;  //speichert den Mittelwert der Temperaturzähler ab  
    };
    calcTemp(&temperature, temp_counts);
    calcPressure(&pressure, press_counts, pmax, pmin, outputmax, outputmin);
    results[i][0] = pressure;
    results[i][1] = press_counts;
    results[i][2] = temperature;
    results[i][3] = temp_counts;

  };
  //Calibration Correction for Sensor 1
  results[0][0] -= pressureDifference;
  Serial.print(results[0][1]); Serial.print(" | "); 
  Serial.print(results[1][1]); Serial.print(" | ");
  Serial.print(results[1][0]-results[0][0]); Serial.print(" | ");
  Serial.print(results[0][2]); Serial.print(" | ");
  Serial.print(results[1][2]);
  Serial.println();
  uint32_t t1=(uint32_t)((results[0][2])*40.0);   // modifies the temp1 value to be sent as a 12bit value bsp: 23,1°C *10 => 231 *4=> 924
  t1=(t1<<12);                                // shifts t1 12bit up to combine it later with t2 into a 32bit value
  uint32_t t2=(uint32_t)((results[1][2])*40.0);   // modifies the temp2 value to be sent as a 12bit value bsp: 23,1°C *10 => 231 *4=> 924
  uint32_t t24=t1|t2;
  uint32_t p1=(uint32_t)results[0][1];
  uint32_t p2=(uint32_t)results[1][1];

  transmitting(p1+5000,p2+5000,t24); //sends the raw pressure counts of sensor 1 and 2 and the combined temp data 
                                     // added +5000 to ensure a positive pressue value to be sent

/*
 // Writing Data Logger
  String dataString = "";
  dataString +=results[0][1];
  dataString += "\t\t";
  dataString +=results[1][1];
  dataString += "\t\t";
  dataString +=(results[1][0]-results[0][0]);
  File dataFile = SD.open("datalog.txt", FILE_WRITE);
    // if the file is available, write to it:
  if (dataFile) {
    dataFile.println(dataString);
    dataFile.close();
    // print to the serial port too:
    Serial.println(dataString);
  }
  // if the file isn't open, pop up an error:
  else {
    Serial.println("error opening datalog.txt");
  }
*/

  delay(2000);
   

} 
