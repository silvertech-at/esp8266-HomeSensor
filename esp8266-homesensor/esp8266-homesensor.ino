//////////////////////////////////////////////////////////////
// Master ESP8266 Sketch
// V0.4.4
// by wi3
// http://blog.silvertech.at
//////////////////////////////////////////////////////////////
// Der Code Arbeitet verschiedene Sensoren ab und
// überträgt die Daten MQTT Broker.
// 
// Arduino IDE 1.6.5-R5
//////////////////////////////////////////////////////////////

#include <ESP8266WiFi.h>        //ESP8266
#include <OneWire.h>            //Onewire Temp Std. Lib (sollte ohne extra download funkt.)
#include <DallasTemperature.h>  //Onewire Temp. Lib https://github.com/milesburton/Arduino-Temperature-Control-Library
#include <DHT.h>                //Adafruit DHT Lib https://github.com/adafruit/DHT-sensor-library
#include <Bounce2.h>            //Taster Bounce Lib https://github.com/thomasfredericks/Bounce2
#include <PubSubClient.h>       //MQTT Lib https://github.com/knolleary/pubsubclient
#include <i2c_sht21.h>          //SHT21/SI7021 Lib https://github.com/silvertech-at/esp8266-sht21/tree/master/I2C_SHT21
#include <Wire.h>       

///////////////////////////////////////////////////////////////////////////////////////////////
// Bereich unterhalb je Gerät anpassen                                                       //
///////////////////////////////////////////////////////////////////////////////////////////////

const char* ssid = "WLAN-SSID";                //WLAN SSID
const char* password = "password-1";           //WLAN Kennwort
const char* mqtt_server = "192.168.88.103";    //MQTT Broker
const char* devicename = "ProtoESP";            //Geräte Namen
const bool akkumode = true;                    //Betrieb mit oder ohne Akku
int deepsleeptime = 60;                        //DeepSleep Zeit in Sekunden


//Sensoren definieren
   //SensorID 99 = nothing / PIN -1 = nothing 
   //sSensorName {[SensorID],[PIN#1],<Pin#2>,<Interval in s>} //Bus-Type
int sOneWire[] = {99,13,60};      //1Wire
int sDHT22[] = {-1,-1,16};
int sBMP180[] = {-1,-1,-1};
int sRelay1[] = {4,15,-1};        //Licht Relay
int sRelay2[] = {-1,14,-1};
int sMotionDetect[] = {-1,-1};
int sSHT21[] = {-1,-1,0};         //I2C
int sTaster1[] = {-1,-1};
int sTaster2[] = {-1,12};

///////////////////////////////////////////////////////////////////////////////////////////////
// Ende Anpassungsbereich                                                                    //
///////////////////////////////////////////////////////////////////////////////////////////////

#define ONE_WIRE_BUS sOneWire[1]
#define DHTPIN sDHT22[1]
#define DHTTYPE DHT22
#define taster_1 sTaster1[1]  
#define taster_2 sTaster2[1]  
#define Relay1  sRelay1[1] 
#define Relay2  sRelay2[1]
#define SHT_address  0x40


//Sensor Setup
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DHT dht(DHTPIN, DHTTYPE);
Bounce debouncer_t1 = Bounce();
Bounce debouncer_t2 = Bounce();
i2c_sht21 mySHT21;

//MQTT Setup
WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

//Old Millis Variablen pro Sensor
unsigned long lastmillis_DHT22 = 0;
unsigned long lastmillis_OneWire = 0;
unsigned long lastmillis_SHT21_tmp = 0;
unsigned long lastmillis_SHT21_hum = 0;
unsigned long lastmillis_BMP180 = 0;

//Taster Zustand bei letzten Durchlauf
int t1_old = 0;                                
int t2_old = 0;

//////////////////////
//// SETUP
//////////////////////
void setup() {
  
  //Sensor start Bus/Work
  Wire.begin(2,4);             //SHT21
  sensors.begin();
  dht.begin();
  pinMode(taster_1, INPUT);     //Taster 1
  pinMode(taster_2, INPUT);     //Taster 2
  pinMode(Relay1,OUTPUT);      //Relay Relay1
  pinMode(Relay2,OUTPUT);      //Relay Relay2
  digitalWrite(Relay1,LOW);    //Default Wert nach PowerOn
  digitalWrite(Relay2,LOW);    //Default Wert nach PowerOn
  debouncer_t1.attach(taster_1);
  debouncer_t1.interval(5);
  debouncer_t2.attach(taster_2);
  debouncer_t2.interval(5);
  mySHT21.init(SHT_address);    
  
  //Serial Start
  Serial.begin(115200);
  delay(10);
  Serial.println(devicename);
  // We start by connecting to a WiFi network
  WiFi.mode(WIFI_STA);
  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
    Serial.print(".");}
  Serial.println("");
  Serial.println("WiFi connected");  
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println(" ");
//Ende - Wifi Connect

//MQTT
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

//////////////////////
//// LOOP
//////////////////////
void loop() {

  //MQTT
   if (!client.connected()) {
    reconnect();
  }
  //Sensoren
  TasterSchaltung( sRelay1[0]);
  DHT22Hum(false);
  OneWireTemp(false); 
  SHT21Tmp(false);
  SHT21Hum(false);
  Serial.println(" ");
  client.loop();
  if (akkumode == true){
    ESP.deepSleep((deepsleeptime * 1000000));
    }
}


////////////////////////
//// OneWire Temp Sensor
////////////////////////
  
float OneWireTemp(bool now){
  
if ((millis() - lastmillis_OneWire) >= (sOneWire[2] * 1000) || now == true ){
  //Get One Wire Data
  sensors.requestTemperatures();
  float TmpWert = sensors.getTempCByIndex(0);
  //MQTT
  char mTmpWert[20];
  dtostrf(TmpWert, 4, 1,mTmpWert);
  client.publish("ProtoESP/onewire/state", mTmpWert);
  lastmillis_OneWire = millis();
  return TmpWert;
  }
 }


////////////////////////
//// SHT21 Sensor - Temp
////////////////////////
  
float SHT21Tmp(bool now){
  
if ((millis() - lastmillis_SHT21_tmp) >= (sSHT21[2] * 1000) || now == true ){
  //sensors.requestTemperatures();
  float SHT_TmpWert = mySHT21.readTemp();
  //MQTT
  char mSHT_TmpWert[20];
  dtostrf(SHT_TmpWert, 4, 1,mSHT_TmpWert);
  client.publish("ProtoESP/SHT21/tmp/state", mSHT_TmpWert);
  lastmillis_SHT21_tmp = millis();
  return SHT_TmpWert;
  }
 }


////////////////////////
//// SHT21 Sensor - Hum
////////////////////////
  
float SHT21Hum(bool now){
  
if ((millis() - lastmillis_SHT21_hum) >= (sSHT21[2] * 1000) || now == true ){
  //sensors.requestTemperatures();
  float SHT_HumWert = mySHT21.readHumidity();
  //MQTT
  char mSHT_HumWert[20];
  dtostrf(SHT_HumWert, 4, 1,mSHT_HumWert);
  client.publish("ProtoESP/SHT21/hum/state", mSHT_HumWert);
  lastmillis_SHT21_hum = millis();
  return SHT_HumWert;
  }
 }
  
  
//////////////////////////
//// DHT22 Humidity Sensor
//////////////////////////

float DHT22Hum(bool now){
  
//unsigned long time;
if ((millis() - lastmillis_DHT22) >= (sDHT22[2] * 1000) || now == true){
    float DHT22_HumWert = dht.readHumidity();   
      if (isnan(DHT22_HumWert))   //prüft auf gültige zahl                  
       {
         Serial.println("DHT kann nicht gelesen werden!");
         Serial.println(DHT22_HumWert);
         }
         else
         {
        char mDHT22_HumWert[20];
        dtostrf(DHT22_HumWert, 4, 1,mDHT22_HumWert);
        client.publish("ProtoESP/DHT22/hum/state", mDHT22_HumWert);
        lastmillis_DHT22 = millis();       //counter updaten
        return DHT22_HumWert;
        }
     }
  }
 
//////////////////////////
//// 2 Taster Schaltung
//////////////////////////

void TasterSchaltung(int sID){
  
  debouncer_t1.update();
 debouncer_t2.update();
 int t1_stat = debouncer_t1.read();
 int t2_stat = debouncer_t2.read();
 int ausg_stat = digitalRead(Relay1);
 //get informations
 //Serial.println("    ");
 //Serial.print("Relay1= ");
 //Serial.println(ausg_stat);
 //Serial.print("taster1u2= ");
 //Serial.print(t1_stat );
 //Serial.println(t2_stat);
 
 
 //Zustände des Ausg. (zu Relay) behandeln
 switch (ausg_stat){
  case HIGH:                                           //LICHT IST EIN
   //Serial.println("Licht ist an");                    
     if ((t1_stat == HIGH) && (t1_old != t1_stat)){    //Taster1 - ausschalten
       digitalWrite(Relay1,LOW);
       t1_old = HIGH;
       client.publish("ProtoESP/relay1/state", "off");
     }
     else{
       if ((t1_stat == LOW) && (t1_old = HIGH)){       //Taster1 - do nothing
         t1_old = LOW;
       }
     }
     if ((t2_stat == HIGH) && (t2_old != t2_stat)){    //Taster2 - ausschalten
       digitalWrite(Relay1,LOW);
       t2_old = HIGH;
       client.publish("ProtoESP/relay1/state", "off");
     }
     else{
       if ((t2_stat == LOW) && (t2_old = HIGH)){       //Taster2 - do nothing
         t2_old = LOW;
       }
     }
     break;
  case LOW:                                            //LICHT IST AUS
   //Serial.println("Licht ist aus");
    if ((t1_stat == HIGH) && (t1_old != t1_stat)){     //Taster1 - einschalten
       digitalWrite(Relay1,HIGH);
       t1_old = HIGH;
       client.publish("ProtoESP/relay1/state", "on");
    }
    else{
     if ((t1_stat == LOW) && (t1_old = HIGH)){        //Taster1 - do nothing
         t1_old = LOW;
       }
    }
     if ((t2_stat == HIGH) && (t2_old != t2_stat)){     //Taster2 - einschalten
       digitalWrite(Relay1,HIGH);
       t2_old = HIGH;
       client.publish("ProtoESP/relay1/state", "on");
    }
    else{
     if ((t2_stat == LOW) && (t2_old = HIGH)){        //Taster2 - do nothing
         t2_old = LOW;
       }
    }
     break;
  default:
  Serial.println("PANIC - we have an issue");
  break;
   }
  }
  

//////////////////////////
//// MQTT Callback
//////////////////////////
  
void callback(char* topic, byte* payload, unsigned int length) {
  char OW1Buffer[100]; //OneWire Interval - Buffer
  char OW2Buffer[100]; //OneWire Temperatur Wert - Buffer
  char DHT1Buffer[100]; //DHT Hum - Buffer
  char DHT2Buffer[100]; //DHT Interval - Buffer
  char SHT1Buffer[100]; //SHT Tmp - Buffer
  char SHT2Buffer[100]; //SHT Hum - Buffer
  char SHT3Buffer[100]; //SHT Interval - Buffer
  
  // Hilfsvariablen um empfangene Daten als String zu behandeln
  int i = 0;
  char message_buff[100];

  // Epfangene Nachtricht ausgeben
  Serial.println("Message arrived: topic: " + String(topic));
  Serial.println("Length: " + String(length,DEC));

  // Nachricht kopieren
  for(i=0; i<length; i++) {
    message_buff[i] = payload[i];
    }

  // Nachricht mit einen Bit abschließen und zu String umwandeln und ausgeben
  message_buff[i] = '\0';
  String msgString = String(message_buff);
  Serial.println("Payload: " + msgString);
  Serial.print("Topic: ");
  Serial.println(topic);

  //Relay/Licht 1
  if (String(topic) == "ProtoESP/relay1/set" && msgString == "on"){
    digitalWrite(Relay1,HIGH);
    client.publish("ProtoESP/relay1/state", "on");  
  }
  else if (String(topic) == "ProtoESP/relay1/set" && msgString == "off"){
    digitalWrite(Relay1,LOW);
    client.publish("ProtoESP/relay1/state", "off");
  }
  //Relay/Licht 2
  else if (String(topic) == "ProtoESP/relay2/set" && msgString == "on"){
    digitalWrite(Relay2,HIGH);
    client.publish("ProtoESP/relay2/state", "on");
  }
  else if (String(topic) == "ProtoESP/relay2/set" && msgString == "off"){
    digitalWrite(Relay2,LOW);
    client.publish("ProtoESP/relay2/state", "off");
  }
  //OneWire
  else if (String(topic) == "ProtoESP/onewire/" && msgString == "get"){
    client.publish("ProtoESP/onewire/tmp/state",dtostrf(OneWireTemp(true), 1, 0, OW2Buffer));
  }
  else if (String(topic) == "ProtoESP/onewire/setintv"){
    sOneWire[2] = msgString.toInt();
    client.publish("ProtoESP/onewire/interv_state" ,dtostrf(sOneWire[2], 1, 0, OW1Buffer));
  }
  //DHT22
    else if (String(topic) == "ProtoESP/DHT22" && msgString == "get"){
    client.publish("ProtoESP/DHT22/hum/state",dtostrf(DHT22Hum(true), 1, 0, DHT1Buffer));
  }
  else if (String(topic) == "ProtoESP/DHT22/setintv"){
    sDHT22[2] = msgString.toInt();
    client.publish("ProtoESP/DHT22/interv_state" ,dtostrf(sDHT22[2], 1, 0, DHT2Buffer));
  }
  //SHT21
  else if (String(topic) == "ProtoESP/SHT/setintv"){
    sSHT21[2] = msgString.toInt();
    client.publish("ProtoESP/SHT21/interv_state" ,dtostrf(sSHT21[2], 1, 0, SHT3Buffer));
  }
   else if (String(topic) == "ProtoESP/SHT/hum/" && msgString == "get"){
    client.publish("ProtoESP/SHT21/hum/state",dtostrf(SHT21Hum(true), 1, 0, SHT2Buffer));
  }
     else if (String(topic) == "ProtoESP/SHT/tmp/" && msgString == "get"){
    client.publish("ProtoESP/SHT21/tmp/state",dtostrf(SHT21Tmp(true), 1, 0, SHT1Buffer));
  }
}

//////////////////////////
//// MQTT Reconnect
//////////////////////////

void reconnect() {
  while (!client.connected()) {
    Serial.println("Attempting MQTT connection...");
  // Generate client name based on MAC address
      String clientName;
      clientName += "esp8266-";
      uint8_t mac[6];
      WiFi.macAddress(mac);
      clientName += macToStr(mac);



    if (client.connect((char*) clientName.c_str())) {
      Serial.println("connected with client name ");
      Serial.println(clientName);
      client.publish("ProtoESP/boot", "up and running");
      
      // subscribe auf folgende Topics
      client.subscribe("ProtoESP/#");  //Wildcard
      //client.subscribe("ProtoESP/relay1/set");
      //client.subscribe("ProtoESP/relay2/set");
      //client.subscribe("ProtoESP/relay3/set");
      //client.subscribe("ProtoESP/onewire/state");
      //client.subscribe("ProtoESP/onewire/setintv");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // 5 Sekunden warten bis zu einen erneuten Verbinden
      delay(5000);
    }
  }
}

//generate unique name from MAC addr
String macToStr(const uint8_t* mac){
  String result;
  for (int i = 0; i < 6; ++i) {
    result += String(mac[i], 16);
    if (i < 5){
      result += ':';
    }
  }
  return result;
}
