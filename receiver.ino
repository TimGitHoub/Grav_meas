#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "libRec.h"
#define errorTrigger 1                      //Gibt an, wie oft ein Fehlwert erhalten werden soll bevor eine Fehlermeldung kommt
#define pegel 2000
#include <avr/interrupt.h>

volatile unsigned int cnt_zero=0;
volatile uint8_t errorCounter=0;
volatile float wert=0;
/*
float outputmax = 15099494.0; // output at maximum pressure [counts] 
float outputmin = 1677722.0; // output at minimum pressure [counts]
float pmax = 6000.0; // maximum value of pressure range [Pa] here 6kPa 
float pmin = 0.0; // minimum value of pressure range [bar, psi, kPa, etc.] 
*/

LiquidCrystal_I2C lcd(0x27, 16, 2);

ISR(INT0_vect)                   // External interrupt_zero ISR
{
  cnt_zero++;
}

void setup() {
    Serial.begin(9600);
    DDRD&=~(1<<PD2);                                     //PD2 als Eingang

//  INT0 Einstellungen (Interrupt an PD2)
    EICRA=(1<<ISC01)|(1<<ISC00)|(1<<ISC11)|(1<<ISC10);   //Externer Interrupt INT0 & INT1 bei steigender Flanke
    EIMSK=(1<<INT0)|(1<<INT1);                           //Configure INT0 & INT1 active low level triggered
    
//  TIMER1 Einstellungen
    TCCR1A = 0x00;                                       // OC2A and OC2B disconnected; Wave Form Generator: Normal Mode
    TCCR1B = (1<<CS11);                                  // prescaler = 8. Eine Zähleinheit= 0.5us => 1ms=2000Einheiten => 1OVF=131072us

    lcd.init();                                            // LCD Init
    lcd.backlight();
    lcd.begin(16, 2);
  
    lcd.print("Warte auf Signal");
    Serial.println("PressCountsS1 PressCountsS2 PressDiff T1 T2");                                  //Begrüßungsanzeige LCD
    sei();
}

void loop() {
 
// *****  Empfang  ****
   uint8_t eingang[24][6]={0};                         //Array anlegen für die 4x2 Bytes Datenübertragung
   sync();                                             //Synchronisationsvorgang
//Datenübertragung beginnt
   for(int j=0; j<6; j++)                              //Liest 4x die Pegel ein und Speichert sie in einem Array
    {
     for(int i=0; i<24; i++)  
      {
        cnt_zero=0;
        eingang[i][j]=checkeHighpegel(&cnt_zero);      //Fülle das Array mit den empfangenen Datensignalen
        while(TCNT1<(pegel*10*(i+1))){};               //warte bis 10x Pegel vorbei ist - legt den neuen Einlesepunkt fest
      };
      TCNT1=0;                                         //TCNT1 auf Null setzen, damit 2. Einleseschleife beginnen kann
    }; 
//Datenübertragung endet

  uint8_t ausgang[24][3]={0};
  uint8_t error=0;                                    //Fehlervariable
  for(int j=0;j<3;j++){
    for(int i=0; i<24; i++)                           //Auswertung der ersten beiden Spalten 
    {
      if(eingang[i][2*j]==1 & eingang[i][(2*j)+1]==1){ausgang[i][j]=0;}       //sind beide Einträge gleich 1 so interpretiere als 0
      else if(eingang[i][2*j]==4 & eingang[i][(2*j)+1]==4){ausgang[i][j]=1;}  //sind beide Einträge gleich 4, so interpretiere als 1
      else{error++;};                                                         //bei einer Abweichung wird ein Fehlerzähler hochgezählt
    };
  };

  if(error){
    Serial.print("Fehleranzahl: ");
    Serial.println(error);
  }
  else{
      uint32_t recData[3]={0};   //recData[0]=P1 recData[1]=P2 recData[2]=T1&T2
      for(int j=0;j<3;j++){
        uint16_t low16b=0;
        uint16_t high16b=0;
        for(int i=0; i<16; i++)                                      //Umwandlung der einzelnen Bits in den 2Byte (lower Word) Ausgabewert
        {
          if(ausgang[i][j]>0){low16b|=(1<<i);};                      //Speichere die Bits in der variable und erzeuge so den übertragenen Werte in einem Array
        };
        for(int i=0; i<8; i++)                                       //Umwandlung der einzelnen Bits in den 2Byte (higher Word) Ausgabewert
        {
          if(ausgang[i+16][j]>0){high16b|=(1<<i);};                  //Speichere die Bits in der variable und erzeuge so den übertragenen Werte in einem Array
        };
        recData[j]|=low16b;
        recData[j]+=(high16b*65536);
      };
      uint32_t pressCounts1=recData[0]-5000;   //Beim Senden wurden 5000 addiert um einen positiven Wert sicher zu stellen
      uint32_t pressCounts2=recData[1]-5000;   //daher wird hier wieder 5000 abgezogen
      uint16_t t1=0;
      uint16_t t2=0;
      uint32_t t12=recData[2];    //Kombinierte t1 und t2 Variable t12 wird zwischengespeichert
      t1=(t12>>12);               //obere 12 Bit verschieben und speichern
      float temp1=t1/40.0;        //temp1 ist der übertragene Temp-Wert geteilt durch 40
      t2=0xFFF&recData[2];        //untere 12 Bit maskieren
      float temp2=t2/40.0;        //temp2 ist der übertragene Temp-Wert geteilt durch 40
      float p1 = ((pressCounts1 - outputmin) * (pmax - pmin)) / (outputmax - outputmin) + pmin; 
      float p2 = ((pressCounts2 - outputmin) * (pmax - pmin)) / (outputmax - outputmin) + pmin; 
      int pDiff = p2-p1;
      Serial.print(pressCounts1);
      Serial.print("|");
      Serial.print(pressCounts2);
      Serial.print("|");
      Serial.print(pDiff);
      Serial.print("|");
      Serial.print(temp1);
      Serial.print("|");
      Serial.println(temp2);
      
      
  };
// *****  Empfang ENDE ****

/* LCD AUSGABE temporaer ABGESCHALTET!!!
lcd.clear();
lcd.setCursor(0, 0);
lcd.print("P1: ");                                  //
lcd.print(recData[0]);
lcd.setCursor(0, 1);
lcd.print("P2: ");                                  //
lcd.print(recData[1]);
warte(1000);
lcd.clear();
lcd.setCursor(0, 0);
lcd.print("Druckdiffcounts:");                                  //
lcd.setCursor(0, 1);
lcd.print(pDiff);
warte(1000);
lcd.clear();
lcd.setCursor(0, 0);
lcd.print("Druckdiff:");                                  //
lcd.setCursor(0, 1);
lcd.print(pDiff);
*/

/*
            // ***Pegel Anzeige der jeweils 24bit ANFANG***
            for(int j=0; j<6;j++){
              for(int i=23; i>=0; i--){
                Serial.print(eingang[i][j]);
              };
                Serial.println();
            };
            Serial.println();
            // ***Pegel Anzeige der jeweils 24bit ENDE***
            */
 
/*
  Serial.println("P1: \t\t T1: \t\t P2: \t\t T2:");
  Serial.print(recData[0]);
  Serial.print("\t\t");
  Serial.print(temp1);
  Serial.print("\t\t");
  Serial.print(recData[1]);
  Serial.print("\t\t");
  Serial.println(temp2);
  Serial.println(recData[0]-recData[1]);
  double p1= (((recData[0]) - outputmin) * (pmax - pmin)) / (outputmax - outputmin) + pmin;
  double p2= (((recData[1]) - outputmin) * (pmax - pmin)) / (outputmax - outputmin) + pmin;
  Serial.println(p1-p2);
  lcd.setCursor(0, 1);
  lcd.clear();
  lcd.print(p1-p2);
*/

}
