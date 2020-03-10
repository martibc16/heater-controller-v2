#include <Arduino.h>
#include <TM1637Display.h>
#include <TimeLib.h>
#define TINY_GSM_MODEM_SIM808
#include <TinyGsmClient.h>
#include <PubSubClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <DS3232RTC.h>
#include <Wire.h>
#include <SI7021.h>
#include <OneWire.h>
#include <DallasTemperature.h>

void initializeModem();
void mqttCallback(char *topic, byte *payload, unsigned int len);
void maintainMQTT();
void publishInfo(float menjador, int TAIACS, int TAICal, int DeltaTACS, float deltaTCal, int TACS, float termo, int tSetACS, int modeACS);
void toCharPub(float num, bool dec);
void toCharPub(float num);
void updateDisplays();
void transmitToCaleino();
boolean mqttConnect();
String toLower(String d);
void logicaACSCale();
void onProg();
void offProg();



#define CLKTEMP 6
#define DIOTEMP 7
#define CLKTIME 4
#define DIOTIME 25

#define ONEWIREPIN 26

#define NUM(off, mult) ((t[(off)] - '0') * (mult))

#define LONGUPDATEINTERVAL 600 //in seconds
#define SHORTUPDATEINTERVAL 10 //in seconds

#define STANDBYTIMESETTERMOSTAT 1000

//#define STANDBYTIMESETTERMOSTAT 5000 //temps en que el termostat parpadejera quan es toquin els botons, en segons

// See all AT commands, if wanted
//#define DUMP_AT_COMMANDS

// Define the serial console for debug prints, if needed
//#define TINY_GSM_DEBUG SerialMon

// Set serial for debug console (to the Serial Monitor, default speed 115200)
#define SerialMon Serial

// Set serial for AT commands (to the module)
// Use Hardware Serial on Mega, Leonardo, Micro
#define SerialAT Serial1

#define SerialCOM Serial2

#define NETSTAT 49
#define PWRSTAT 48
#define PWRKEY 5

TM1637Display displayTemp(CLKTEMP, DIOTEMP);
TM1637Display displayTime(CLKTIME, DIOTIME);
int prevMinute;

OneWire oneWireBus(ONEWIREPIN);
DallasTemperature sensors(&oneWireBus);

// Your GPRS credentials
// Leave empty, if missing user or pass
const char apn[] = "";
const char gprsUser[] = "";
const char gprsPass[] = "";

// MQTT details
const char *broker = "introduce your MQTT server here";

const char *topicTACS = "/TACS";
const char *topicTSetACS = "/TSetACS";
const char *topicTAIACS = "/TAIACS";
const char *topiTAICal = "/TAICal";
const char *topicMenajdor = "/menjador";
const char *topicDeltaTACS = "/deltaTACS";
const char *topicDeltaTCal = "/deltaTCal";
const char *topicTermostat = "/termostat";
const char *topicmodeACS = "/modeACS";
const char *topicDownTermo = "/down/termostat";
const char *topicDownTSetACS = "/down/tsetACS";
const char *topicDownDeltaTACS = "/down/deltaTACS";
const char *topicDownDeltaTCal = "/down/deltaTCal";
const char *topicTime = "/time";
const char *topicDownFastRefresh = "/down/FastRef";
const char *topicDownACSSchedule = "/down/scheduxACS";
const char *topicDownModeACS = "/down/modeACS";
const char *topicForceRefresh = "/forceRefresh"; 
const char *topicDisplay = "/down/display";

bool firstTime = true;
long int timeSinceUpdated;
unsigned long int updateInterval;
char buff[10];

DeviceAddress addressTempMen = {0x28, 0xD3, 0x27, 0xFF, 0x0A, 0x00, 0x00, 0xAB};
/*Physical variables*/

float termostat = 5;
int tSetACS = 40;
float tempMen;

int TAIACS;
int TAICal;
int TACS;

bool calOn = false;
bool ACSOn = false;

/*Other variables*/

bool termoChange = true;
bool termoACSChange = false;
bool setTermostatMode;
float prevTemp;
float prevTermostat;
int screenState=0; //0-off 1-off but changes already applied 2-on but from off state 3-on
bool timeUpdate;
int deltaTACS = 4;

SI7021 sensor;

//Histèrsi termostat menjador
float delaTTermo = 0.5;
float deltaTCal = 0.2;
int sch[168];

int index = 0;
char data[30];
unsigned long int sinceUpdatedCaleino;

//time since the buttons where last clicked
unsigned long int termostatUpdate;

bool autoProgACS = false;
unsigned short int progACS = 3;

//for managing the DHW programming hours
bool prevACSSch = true;

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm modem(debugger);
#else

TinyGsm modem(SerialAT);
#endif
TinyGsmClient client(modem);
PubSubClient mqtt(client);

long lastReconnectAttempt = 0;
unsigned int numberOfReconnects;

void setup()
{

  pinMode(2, OUTPUT);
  pinMode(3, OUTPUT);
  pinMode(PWRSTAT, INPUT);
  pinMode(PWRKEY, OUTPUT);

/* button inputs, not yet implemented*/
  //pinMode(5, OUTPUT);
  //digitalWrite(5,HIGH);
  //attachInterrupt(digitalPinToInterrupt(2), buttonPlus, RISING);
  //attachInterrupt(digitalPinToInterrupt(2), buttonMinus, RISING);
  prevMinute = -3;
  prevTemp = -1;
  updateInterval = LONGUPDATEINTERVAL;
  updateInterval *= 1000;
  numberOfReconnects = 0;
  prevTermostat = termostat;

  // Set console baud rate
  SerialMon.begin(19200);
  SerialCOM.begin(9600);
  delay(10);

  // Set your reset, enable, power pins here

  // Set GSM module baud rate
  SerialAT.begin(19200);

  initializeModem();
  //setTime(19, 59, 0, 22, 9, 2019);

  sensor.begin();

  setSyncProvider(RTC.get); // the function to get the time from the RTC
  if (timeStatus() != timeSet)
    Serial.println("Unable to sync with the RTC");
  else
    Serial.println("RTC has set the system time");

  sensors.begin();
  sensors.setResolution(addressTempMen, 12);

  //sensors.setWaitForConversion(false);
  sensors.requestTemperatures();

  transmitToCaleino();
}

void loop()
{
  transmitToCaleino();
  maintainMQTT();

  mqtt.loop();
  publishInfo(tempMen, TAIACS, TAICal, deltaTACS, deltaTCal, TACS, termostat, tSetACS, progACS);

/*Used for debugging purposes*/
  /*SerialMon.print("Prog ACS:");
  SerialMon.print(autoProgACS);
  SerialMon.print("\t");
  SerialMon.print(ACSOn);
  SerialMon.print("\t");
  SerialMon.print(prevACSSch);
  SerialMon.print("\t");
  SerialMon.print(sch[24 * (weekday() - 1) + hour()]);
  SerialMon.print("\t");
  Serial.print(hour());
  Serial.print(" ");
  Serial.print(minute());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(" ");
  Serial.print(month());
  Serial.print(" ");
  Serial.print(year());
  Serial.println();*/

  logicaACSCale();
  updateDisplays();

  
}

void updateDisplays()
{
  char out1[5];
  String string3(out1);

  switch (screenState)
  {
  case 0:
    displayTime.setBrightness(7, false);
    displayTemp.setBrightness(7, false);
    displayTime.clear();
    displayTemp.clear();
    screenState = 1;
    break;

  case 2:
    displayTime.setBrightness(7, true);
    displayTemp.setBrightness(7, true);

    sprintf(out1, "%.2d%.2d", hour(), minute());
    displayTime.setBrightness(10);
    displayTime.clear();
    displayTime.showNumberDecEx(string3.toInt(), (0x80 >> 1), true);
    prevMinute = minute();

    displayTemp.setBrightness(10);
    displayTemp.clear();
    displayTemp.showNumberDecEx(tempMen * 100, (0x80 >> 1), true);
    prevTemp = tempMen;
    screenState = 3;
    break;
  case 3:
    if (prevMinute != minute())
    {
      sprintf(out1, "%.2d%.2d", hour(), minute());
      
      displayTime.setBrightness(10);
      displayTime.clear();
      displayTime.showNumberDecEx(string3.toInt(), (0x80 >> 1), true);
      prevMinute = minute();
    }

    if (prevTemp != tempMen)
    {
      displayTemp.setBrightness(10);
      displayTemp.clear();
      displayTemp.showNumberDecEx(tempMen * 100, (0x80 >> 1), true); 
      prevTemp = tempMen;
    }
    break;
  }
}

/* Buttons logic, not yet implemented*/
/*
void buttonPlus()
{
  termostatUpdate = millis();
  setTermostatMode = true;
  termostat += 0.5;
}

void buttonMinus()
{
  termostatUpdate = millis();
  setTermostatMode = true;
  termostat -= 0.5;
}
*/

void initializeModem()
{
  // Restart takes quite some time
  // To skip it, call init() instead of restart()

  if (digitalRead(PWRSTAT) == HIGH)
  {
    digitalWrite(PWRKEY, HIGH);
    delay(1500);
    digitalWrite(PWRKEY, LOW);
  }

  while (digitalRead(PWRSTAT) == LOW)
  {

    digitalWrite(PWRKEY, LOW);
    digitalWrite(PWRKEY, HIGH);
    delay(1500);
  }

  SerialMon.println("Initializing modem...");
  modem.restart();

  //modem.init();

  String modemInfo = modem.getModemInfo();
  SerialMon.print("Modem: ");
  SerialMon.println(modemInfo);

  // Unlock your SIM card with a PIN
  modem.simUnlock("6128");

  SerialMon.print("Waiting for network...");
  if (!modem.waitForNetwork())
  {
    SerialMon.println(" fail");
    delay(10000);
    return;
  }
  SerialMon.println(" OK");

  if (modem.isNetworkConnected())
  {
    SerialMon.println("Network connected");
  }

  SerialMon.print(F("Connecting to "));
  SerialMon.print(apn);
  if (!modem.gprsConnect(apn, gprsUser, gprsPass))
  {
    SerialMon.println(" fail");
    delay(10000);
    return;
  }
  SerialMon.println(" OK");
  delay(1000);

  // MQTT Broker setup
  mqtt.setServer(broker, 1883);
  mqtt.setCallback(mqttCallback);
  SerialMon.println(modem.localIP());
}

boolean mqttConnect()
{
  SerialMon.print("Connecting to this server: ");
  SerialMon.print(broker);

  // Connect to MQTT Broker
  boolean status = mqtt.connect("GsmClientTest");

  // Or, if you want to authenticate MQTT:
  //boolean status = mqtt.connect("GsmClientName", "mqtt_user", "mqtt_pass");

  if (status == false)
  {
    SerialMon.println(" fail");
    return false;
  }
  SerialMon.println(" OK");
  mqtt.subscribe(topicDownTermo, 1);
  mqtt.subscribe(topicDownTSetACS, 1);
  mqtt.subscribe(topicTime);
  mqtt.subscribe(topicDownDeltaTACS, 1);
  mqtt.subscribe(topicDownDeltaTCal, 1);
  mqtt.subscribe(topicDownFastRefresh, 1);
  mqtt.subscribe(topicDownACSSchedule, 1);
  mqtt.subscribe(topicDownModeACS, 1);
  mqtt.subscribe(topicDisplay, 1);

  timeSinceUpdated = -LONGUPDATEINTERVAL;
  timeSinceUpdated *= 10000;

  return mqtt.connected();
}

void mqttCallback(char *topic, byte *payload, unsigned int len)
{
  SerialMon.print("Message arrived [");
  SerialMon.print(topic);
  SerialMon.print("]: ");
  SerialMon.write(payload, len);
  SerialMon.println();

  timeSinceUpdated = -LONGUPDATEINTERVAL;
  timeSinceUpdated *= 10000;

  if (strcmp(topic, topicDownTermo) == 0)
  {
    payload[len] = '\0';
    termostat = String((char *)payload).toFloat();
  }
  else if (strcmp(topic, topicDownTSetACS) == 0)
  {
    payload[len] = '\0';
    tSetACS = String((char *)payload).toInt();
  }
  else if (strcmp(topic, topicTime) == 0)
  {
    payload[len] = '\0';
    String t = String((char *)payload);

    int yeari = NUM(0, 1000) + NUM(1, 100) + NUM(2, 10) + NUM(3, 1);
    int monthi = NUM(5, 10) + NUM(6, 1);
    int dayi = NUM(8, 10) + NUM(9, 1);
    int houri = NUM(11, 10) + NUM(12, 1);
    int minutei = NUM(14, 10) + NUM(15, 1);
    int secondi = NUM(17, 10) + NUM(18, 1);
    setTime(houri, minutei, secondi, dayi, monthi, yeari);
    RTC.set(now());
    timeUpdate = true;
    /*Used for debugging purposes*/
    /*
    SerialMon.print(day());
    SerialMon.print("/");
    SerialMon.print(month());
    SerialMon.print("/") ;
    SerialMon.print(year()); 
    SerialMon.print( " ") ;
    SerialMon.print(hour());  
    SerialMon.print(":") ;
    SerialMon.print(minute());
    SerialMon.print(":") ;
    SerialMon.println(second());*/
  }
  else if (strcmp(topic, topicDownDeltaTACS) == 0)
  {
    payload[len] = '\0';
    deltaTACS = String((char *)payload).toInt();
  }
  else if (strcmp(topic, topicDownFastRefresh) == 0)
  {
    payload[len] = '\0';
    String r = String((char *)payload);
    if (r.equals("true"))
    {
      updateInterval = SHORTUPDATEINTERVAL;
    }
    else
    {
      updateInterval = LONGUPDATEINTERVAL;
    }
    updateInterval *= 1000;
  }
  else if (strcmp(topic, topicDownDeltaTCal) == 0)
  {
    payload[len] = '\0';
    deltaTCal = String((char *)payload).toFloat();
  }
  else if (strcmp(topic, topicDownACSSchedule) == 0)
  {
    payload[len] = '\0';

    for (int i = 0; i < 168; i++)
    {
      sch[i] = payload[2 * i + 1] - 48;
      //sch[i]=*payload[2*i+1];
      //sch[i] = String((char *)payload[2 * i + 1]).toInt();
      SerialMon.print(sch[i]);
    }

    SerialMon.println();

    SerialMon.println();
  }
  else if (strcmp(topic, topicDownModeACS) == 0)
  {
    payload[len] = '\0';
    progACS = String((char *)payload).toInt();
  }
  else if (strcmp(topic, topicDisplay) == 0)
  {
    payload[len] = '\0';
    screenState = 2 * String((char *)payload).toInt();
  }
}

void maintainMQTT()
{
  if (!mqtt.connected())
  {

    if (numberOfReconnects != 0)
      SerialMon.println("=== MQTT NOT CONNECTED ===");
    numberOfReconnects++;
    if (numberOfReconnects > 3)
    {
      numberOfReconnects = 0;
      initializeModem();
      lastReconnectAttempt = 0;
    }
    // Reconnect every 10 seconds
    unsigned long t = millis();
    if (t - lastReconnectAttempt > 10000L)
    {
      lastReconnectAttempt = t;
      if (mqttConnect())
      {
        lastReconnectAttempt = 0;
        numberOfReconnects = 0;
      }
    }

    return;
  }
}

void publishInfo(float menjador, int TAIACS, int TAICal, int DeltaTACS, float deltaTCal, int TACS, float termo, int tSetACS, int modeACS)
{
  if (firstTime)
  {
    char b[1] = {'a'};
    mqtt.publish(topicForceRefresh, b);
    firstTime = false;
  }

  if (millis() - timeSinceUpdated > updateInterval)
  {
    toCharPub(TACS);
    mqtt.publish(topicTACS, buff);

    toCharPub(DeltaTACS);
    mqtt.publish(topicDeltaTACS, buff);

    toCharPub(TAIACS);
    mqtt.publish(topicTAIACS, buff);

    toCharPub(TAICal);
    mqtt.publish(topiTAICal, buff);

    toCharPub(menjador, true);
    mqtt.publish(topicMenajdor, buff);

    toCharPub(tSetACS);
    mqtt.publish(topicTSetACS, buff);

    toCharPub(deltaTCal, true);
    mqtt.publish(topicDeltaTCal, buff);

    toCharPub(progACS);
    mqtt.publish(topicmodeACS, buff);

    timeSinceUpdated = millis();
    SerialMon.print("publishing: ");
    SerialMon.println(updateInterval);
  }
  if (termoChange)
  {
    toCharPub(termostat, true);
    mqtt.publish(topicTermostat, buff);
    termoChange = false;
  }
}

void toCharPub(float value, bool decimal)
{
  if (decimal)
  {
    String(value, 1).toCharArray(buff, 10);
    /*Serial.println("\n\n");
    Serial.println(String(13.5, 1));
    Serial.println("\n\n");*/
  }
  else
  {
    String((int)value).toCharArray(buff, 10);
    /*Serial.println("\n\n");
    Serial.println(String(13.5, 1));
    Serial.println("\n\n");*/
  }
}

void toCharPub(float value)
{
  toCharPub(value, false);
}

void transmitToCaleino()
{

  if (millis() - sinceUpdatedCaleino > 10000)
  {
    String r = String(',');
    r.concat(calOn ? 1 : 0);
    r.concat(',');
    r.concat(ACSOn ? 1 : 0);
    r.concat('!');

    SerialMon.println(r);

    SerialCOM.print(r);

    sinceUpdatedCaleino = millis();

    sensors.requestTemperatures();
    tempMen = round(sensor.getCelsiusHundredths() / 10) / 10.0 - 1.0;
  }

  while (SerialCOM.available())
  {
    data[index] = SerialCOM.read();
    //SerialMon.print(data[index]);
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
          TACS = atoi(valPosition);
          break;
        case 1:
          TAIACS = atoi(valPosition);
          break;
        case 2:
          TAICal = atoi(valPosition);
          break;
        }
        i++;
        SerialMon.println(atoi(valPosition));
        valPosition = strtok(NULL, ",!");
      }

      index = 0;
      for (int j = 0; j < 30; j++)
        data[j] = '\0';
      SerialMon.println();
    }
  }
}

String toLower(String d)
{
  char buf[2];
  d.toLowerCase();
  d.toCharArray(buf, 2);

  return String(buf);
}

void onProg()
{
  autoProgACS = true;
  if (progACS == 1)
  {
    progACS = 0;
  }
}

void offProg()
{
  autoProgACS = false;
  if (progACS == 1)
  {
    progACS = 0;
  }
}

void logicaACSCale()
{
  if (sch[24 * (weekday() - 1) + hour()] == 1 && prevACSSch == false)
  {
    onProg();
    prevACSSch = true;
  }
  else if (sch[24 * (weekday() - 1) + hour()] == 0 && prevACSSch == true)
  {
    offProg();
    prevACSSch = false;
  }

  if ((autoProgACS || progACS == 1 || progACS == 2) && progACS != 3)
  {
    if ((TACS < tSetACS - deltaTACS) && !ACSOn)
    {
      ACSOn = true;
    }
    else if ((TACS > tSetACS + deltaTACS) && ACSOn)
    {
      ACSOn = false;
    }
  }
  else if (ACSOn && ((!autoProgACS && progACS != 2) || progACS == 3))
  {
    ACSOn = false;
  }



  if (calOn && termostat + deltaTCal < tempMen)
  {
    calOn = false;
    termoChange = true;
  }
  else if (!calOn && tempMen < termostat - deltaTCal)
  {
    calOn = true;
    termoChange = true;
  }
}