#include "Arduino.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>

void updateTermoino();
void readSensors();
void updateActuators();
void checkForStateChanges();

/*
 * Outputs to relays
*/
#define PINACS 31
#define PINCALE 32
#define PINFAT 33 //Wood furnace output
#define PINCALDERA 30

/*
 * Digital inputs
 */
#define PINTERMOSTAT 49
#define PINBOMBACIR 8
#define PINTERMOFAT 10
 

short int tACS ;
bool ACSON;
short int tTAICal;
short int tTAIACS;
bool termostatActiuTermoino;
bool termostatActiuTermostat;
bool FATactiu;
bool canviEstat;


/*
   Onewire variables
*/
const int pinDatosDQ = 5; //onewire
OneWire oneWireBus(pinDatosDQ);
DallasTemperature sensors(&oneWireBus);

DeviceAddress TAICal = {0x28, 0xFF, 0x6A, 0x1B, 0x32, 0x18, 0x02, 0xFF};
DeviceAddress TAIACS = {0x28, 0xD6, 0x08, 0x30, 0x0A, 0x00, 0x00, 0x74};
DeviceAddress ACS = {0x28, 0x24, 0xD2, 0x45, 0x92, 0x11, 0x02, 0x3B};
int  resolution = 12;
unsigned long lastTempRequest = 0;
int  delayInMillis = 0;

float RawHighACS = 100.37;
float RawLowACS = 0.44;
float RawHighTAIACS = 101.31;
float RawLowTAIACS = 1.06;
float RawHighTAICal=100;
float RawLowTAICal=0;

float ReferenceHigh = 99.8;
float ReferenceLow = 0;
float ReferenceRange = ReferenceHigh - ReferenceLow;

unsigned long tempsActiu=0;

unsigned long int sinceUpdatedCaleino;
unsigned long int disconnectedTimeFromTermoino=0;
int index = 0;
char data[30];


void setup()
{
  Serial.begin(19200);
  Serial1.begin(9600);

  pinMode(PINACS, OUTPUT);
  pinMode(PINCALE, OUTPUT);
  pinMode(PINFAT, OUTPUT);
  pinMode(PINCALDERA, OUTPUT);
  pinMode(PINTERMOSTAT, INPUT_PULLUP);
  pinMode(PINBOMBACIR, INPUT_PULLUP);
  pinMode(PINTERMOFAT, INPUT_PULLUP);
  pinMode(A6, INPUT);
  termostatActiuTermoino = false;
  termostatActiuTermostat=false;
  canviEstat = false;
  ACSON = false;
  sinceUpdatedCaleino=-5000;

 
   
  tACS=35;
  tTAIACS=75;
  tTAICal=90;

  sensors.begin();
  sensors.setResolution(ACS, resolution);

  sensors.setWaitForConversion(false);
  sensors.requestTemperatures();

  delayInMillis = 750 / (1 << (12 - resolution));
  lastTempRequest = millis();

  index = 0;
  disconnectedTimeFromTermoino=millis();
}

void loop()
{

  readSensors();

  updateTermoino();
  checkForStateChanges();

  if(millis()-disconnectedTimeFromTermoino>300000)
  {
    ACSON=false;
    termostatActiuTermoino=false;
    canviEstat=true;
    disconnectedTimeFromTermoino=millis();
  }

/* Used for debugging purposes */
  /*Serial.print(hour());
    Serial.print("\t");
    Serial.print(minute());
    Serial.print("\t");
    Serial.print(second());
    Serial.print("\t");
    */

   /*Serial.print(termostatActiuTermoino);
    Serial.print("\t");
    Serial.print(termostatActiuTermostat);
    Serial.print("\t");
    Serial.print(ACSON);
    Serial.println("\t");*/
}

void updateTermoino()
{
  if (millis() - sinceUpdatedCaleino > 10000)
  {
    String r = String(',');
    r.concat(tACS);
    r.concat(',');
    r.concat(tTAIACS);
    r.concat(',');
    r.concat(tTAICal);
    r.concat('!');

    Serial.println(r);

    Serial1.print(r);
    sinceUpdatedCaleino = millis();
  }

  while (Serial1.available())
  {
    data[index] = Serial1.read();
    //Serial.print(data[index]);
    index++;

    if (data[index - 1] == '!')
    {
      int i = 0;
      char *valPosition = strtok(data, ",!");

      while (valPosition != NULL)
      {
        switch (i)
        {
        case 0:
          if (termostatActiuTermoino!=(atoi(valPosition)==0 ? false : true))
          {
            termostatActiuTermoino= atoi(valPosition)==0 ? false : true;

            canviEstat = true;
          }
        case 1:
          if (ACSON != (atoi(valPosition)==0 ? false : true))
          {
            ACSON = atoi(valPosition)==0 ? false : true;
            canviEstat = true;
          }
          break;
        }
        i++;
        
        Serial.println(atoi(valPosition));
        valPosition = strtok(NULL, ",!");
      }
      disconnectedTimeFromTermoino=millis();
      index = 0;
      for (int j = 0; j < 30; j++)
        data[j] = '\0';
      Serial.println();
    }
  }
}

void readSensors()
{
  if (millis() - lastTempRequest >= delayInMillis) // waited long enough??
  {
    sensors.requestTemperatures();

    tACS = (((sensors.getTempC(ACS) - RawLowACS) * ReferenceRange) / (RawHighACS - RawLowACS)) + ReferenceLow;
    tTAIACS = (((sensors.getTempC(TAIACS) - RawLowTAIACS) * ReferenceRange) / (RawHighTAIACS - RawLowTAIACS)) + ReferenceLow;
    tTAICal = (((sensors.getTempC(TAICal) - RawLowTAICal) * ReferenceRange) / (RawHighTAICal - RawLowTAICal)) + ReferenceLow;
  }

  /*Used for making sure the sensosrs are working as expected*/
  /*
    if (tACS == -128 || tTAIACS == -128)// || tTAICal == -127) //change values depending on offset on readSensors()
      {
      oled.clear();
      oled.set2X();
      oled.println("Error");
      oled.set1X();
      oled.print("Sensor ");
      if (tACS == -128)
        oled.println("ACS");
      if (tTAIACS == -128)
        oled.println("TAIACS");
      if (tTAICal == -128)
        oled.println("TAICal");

      while (tACS == -128 || tTAIACS == -128) // || tTAICal == -127)
        readSensors();

      oled.clear();
      oled.println("Clica per continuar");

      }
  */
}

void updateActuators()
{
  if (ACSON)
  {
    digitalWrite(PINACS, HIGH);
    digitalWrite(PINCALDERA, HIGH);

    if (!termostatActiuTermoino && !termostatActiuTermostat && digitalRead(PINCALE) == HIGH) //if central heating actuator was onit can now turn off since the DHW turns on
      digitalWrite(PINCALE, LOW);
  }
  if (!ACSON)
  {
    if (!(termostatActiuTermoino || termostatActiuTermostat))
      digitalWrite(PINCALDERA, LOW);
    else
      digitalWrite(PINACS, LOW);
  }

  if (termostatActiuTermoino || termostatActiuTermostat)
  {
    digitalWrite(PINCALE, HIGH);
    digitalWrite(PINCALDERA, HIGH);

    if (!ACSON && digitalRead(PINACS) == HIGH) //if dhw actuator was on it can now turn off since the central heating turns on
      digitalWrite(PINACS, LOW);
  }
  if (!termostatActiuTermoino && !termostatActiuTermostat)
    if (!ACSON)
      digitalWrite(PINCALDERA, LOW);
    else if (!FATactiu)
      digitalWrite(PINCALE, LOW);

  if (FATactiu)
  {
    digitalWrite(PINFAT, HIGH);
    digitalWrite(PINCALE, HIGH);

    if (!ACSON && digitalRead(PINACS) == HIGH) //if dhw actuator was on it can now turn off since the central heating turns on
      digitalWrite(PINACS, LOW);
  }
  if (!FATactiu)
  {
    digitalWrite(PINFAT, LOW);
    if (!ACSON && !(termostatActiuTermoino || termostatActiuTermostat))
      digitalWrite(PINCALDERA, LOW);
    else if (!(termostatActiuTermoino || termostatActiuTermostat) && ACSON)
      digitalWrite(PINCALE, LOW);
  }
}



void checkForStateChanges()
{
  if (!termostatActiuTermoino && !termostatActiuTermostat && digitalRead(PINBOMBACIR) == HIGH && !FATactiu && digitalRead(PINCALE) == HIGH) //If circulating pump is already off
    digitalWrite(PINCALE, LOW);

  if (!ACSON && digitalRead(PINBOMBACIR) == HIGH && digitalRead(PINACS) == HIGH) //If circulating pump is already off
  {
   /* Used for debugging purposes */
   /* Serial.println();
    Serial.println();
    Serial.print(ACSON);
    Serial.print(digitalRead(PINBOMBACIR));
    Serial.print(digitalRead(PINACS));
    Serial.println();*/
    digitalWrite(PINACS, LOW);
  }

  if (digitalRead(PINTERMOSTAT) == LOW && !termostatActiuTermostat)
  {
    termostatActiuTermostat = true;
    canviEstat = true;
  }
  else if (digitalRead(PINTERMOSTAT) == HIGH && termostatActiuTermostat)
  {
    termostatActiuTermostat = false;
    canviEstat = true;
  }

  if (digitalRead(PINTERMOFAT) == LOW && !FATactiu)
  {
    FATactiu = true;
    canviEstat = true;
  }
  else if (digitalRead(PINTERMOFAT) == HIGH && FATactiu)
  {
    FATactiu = false;
    canviEstat = true;
  }

  if (canviEstat)
  {
    updateActuators();
    canviEstat = false;
  }
}
