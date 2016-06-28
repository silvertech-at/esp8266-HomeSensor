//////////////////////////////////////////////////////////////
// Smarthome ESP
// V0.4.6
// by wi3
// http://blog.silvertech.at
//////////////////////////////////////////////////////////////
// MQTT Aktor und Sensor Code für verschiedenste 
// konfigurationen und Anwendungsfälle
// 
// Arduino IDE 1.6.8
// ESP8266 Lib 2.2.0
//////////////////////////////////////////////////////////////

#include <ESP8266WiFi.h>        //ESP8266 https://github.com/esp8266/Arduino
#include <OneWire.h>            //Onewire Temp Std. Lib (sollte ohne extra download funkt.)
#include <DallasTemperature.h>  //Onewire Temp. Lib https://github.com/milesburton/Arduino-Temperature-Control-Library
#include <DHT.h>                //Adafruit DHT Lib https://github.com/adafruit/DHT-sensor-library
#include <PubSubClient.h>       //MQTT Lib https://github.com/knolleary/pubsubclient
#include <i2c_sht21.h>          //SHT21/SI7021 Lib https://github.com/silvertech-at/esp8266-sht21/tree/master/I2C_SHT21
#include <Wire.h>       

///////////////////////////////////////////////////////////////////////////////////////////////
// Bereich unterhalb je Gerät anpassen                                                       //
///////////////////////////////////////////////////////////////////////////////////////////////

const char* ssid = "WLAN-SSID";                 //WLAN SSID
const char* password = "password-1";            //WLAN Kennwort
const char* mqtt_server = "192.168.88.103";     //MQTT Broker
const char* devicename = "TestESP";            //Geräte Namen
const bool akkumode = false;                    //Betrieb mit oder ohne Akku (true/false)
int deepsleeptime = 60;                         //DeepSleep Zeit in Sekunden


//Sensoren definieren
   //SensorID 99 = nothing / PIN -1 = nothing 
   //sSensorName {[SensorID],[PIN#1],<Interval in s>} //Bus-Type
int sOneWire[] = {99,13,10};        //1Wire
int sDHT22[] = {-1,16,-1};
int sBMP180[] = {-1,-1,-1};         //funktion fehlt noch
int sRelay1[] = {-1,14,-1};         //Relay/LED (Taster)
int sRelay2[] = {-1,15,-1};         //Relay/LED
int sRelay3[] = {-1,16,-1};         //Relay/LED
int sRelay4[] = {-1,5,-1};          //Relay/LED
int sMotionDetect[] = {-1,-1};      //Funktion fehlt noch
int sSHT21[] = {-1,-1,10};          //I2C GPIO 2+4 
int sTaster1[] = {-1,12};           //Taster
int sTaster2[] = {-1,-1};           //Taster

///////////////////////////////////////////////////////////////////////////////////////////////
// Ende Anpassungsbereich                                                                    //
///////////////////////////////////////////////////////////////////////////////////////////////
//Erklärung
//Code grob Beschreibung:
//Im Loop werden jene Sensoren die einen PIN oder Interval definiert haben
//ausgelesen und per MQTT in der Intervallzeit an den MQTT Broker gesendet.
//Alle Empfangenen Nachrichten werden am Ende der Durchlaufzeit behandelt.
//
//Akkumodus:
//Im Akkumodus ist automatisch die "deepsleeptime" auch die Sensoren Interval Zeit,
//jene am Sensor definierte Zeit wird overruled.
//
//MQTT:
//Es werden alle Topics per Wildcard subscribt mit "Devicenamen/"
//wie zb "TestESP/".
//Der Gerätename muss manuell per Find and Replace getauscht werden.
//
////////////////////////////////////////////////////////////////////////////////////////////////


#define ONE_WIRE_BUS sOneWire[1]
#define DHTPIN sDHT22[1]
#define DHTTYPE DHT22
#define TasterPin_1 sTaster1[1]  
#define TasterPin_2 sTaster2[1]  
#define Relay1  sRelay1[1] 
#define Relay2  sRelay2[1]
#define Relay3  sRelay3[1] 
#define Relay4  sRelay4[1]
#define SHT_address  0x40

volatile byte state = LOW;


//Sensor/Aktor Setup
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DHT dht(DHTPIN, DHTTYPE);
i2c_sht21 mySHT21;

unsigned long switch_bebouncetime = 250;  //in ms

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
unsigned long lastmillis_Switch = 0;

//MQTT Interrupt
boolean switch_high = false;


//////////////////////
//// SETUP
//////////////////////
void setup() {
  
  //Sensor start Bus/Work
  Wire.begin(2,4);             //SHT21
  sensors.begin();
  dht.begin();
  //pinMode(TasterPin_1, INPUT);     //Taster 1
  pinMode(TasterPin_2, INPUT);     //Taster 2
  pinMode(Relay1,OUTPUT);      //Relay Relay1
  pinMode(Relay2,OUTPUT);      //Relay Relay2
  pinMode(Relay3,OUTPUT);      //Relay Relay3
  pinMode(Relay4,OUTPUT);      //Relay Relay4
  pinMode(sTaster1[1], INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(sTaster1[1]), fSwitch, RISING);
  
  if (akkumode == true) digitalWrite(Relay1,LOW);    //Default Wert nach PowerOn
  if (akkumode == true) digitalWrite(Relay2,LOW);    //Default Wert nach PowerOn
  if (akkumode == true) digitalWrite(Relay3,LOW);    //Default Wert nach PowerOn
  if (akkumode == true) digitalWrite(Relay4,LOW);    //Default Wert nach PowerOn
  
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
  //Sensoren, Funktion startet nur wenn PIN/Intervall konfiguriert
  if (sDHT22[1] != -1) DHT22Hum(false);
  if (sOneWire[1] != -1) OneWireTemp(false); 
  if (sSHT21[2] != -1) SHT21Tmp(false);
  if (sSHT21[2] != -1) SHT21Hum(false);

  if (switch_high == true){
    client.publish("TestESP/switch/state", "changed");
    switch_high = false;
  }
  
  //Serial.println(" ");
  client.loop();
  if (akkumode == true){
    ESP.deepSleep((deepsleeptime * 1000000));
    }
}

////////////////////////
//// Taster - Funktion
////////////////////////

void fSwitch(){
 if ((millis() - lastmillis_Switch) >= switch_bebouncetime){
  switch_high = true;
  Serial.println("Light Changed");
  lastmillis_Switch = millis();
  
 }
}



////////////////////////
//// OneWire Temp Sensor
////////////////////////
  
float OneWireTemp(bool now){
  
if ((millis() - lastmillis_OneWire) >= (sOneWire[2] * 1000) || now == true || akkumode ==true ){
  //Get One Wire Data
  sensors.requestTemperatures();
  float TmpWert = sensors.getTempCByIndex(0);
  //MQTT
  char mTmpWert[20];
  dtostrf(TmpWert, 4, 1,mTmpWert);
  client.publish("TestESP/onewire/tmp/state", mTmpWert);
  lastmillis_OneWire = millis();
  return TmpWert;
  }
 }


////////////////////////
//// SHT21 Sensor - Temp
////////////////////////
  
float SHT21Tmp(bool now){
  
if ((millis() - lastmillis_SHT21_tmp) >= (sSHT21[2] * 1000) || now == true || akkumode ==true ){
  //sensors.requestTemperatures();
  float SHT_TmpWert = mySHT21.readTemp();
  //MQTT
  char mSHT_TmpWert[20];
  dtostrf(SHT_TmpWert, 4, 1,mSHT_TmpWert);
  client.publish("TestESP/SHT21/tmp/state", mSHT_TmpWert);
  lastmillis_SHT21_tmp = millis();
  return SHT_TmpWert;
  }
 }


////////////////////////
//// SHT21 Sensor - Hum
////////////////////////
  
float SHT21Hum(bool now){
  
if ((millis() - lastmillis_SHT21_hum) >= (sSHT21[2] * 1000) || now == true || akkumode ==true ){
  //sensors.requestTemperatures();
  float SHT_HumWert = mySHT21.readHumidity();
  //MQTT
  char mSHT_HumWert[20];
  dtostrf(SHT_HumWert, 4, 1,mSHT_HumWert);
  client.publish("TestESP/SHT21/hum/state", mSHT_HumWert);
  lastmillis_SHT21_hum = millis();
  return SHT_HumWert;
  }
 }
  
  
//////////////////////////
//// DHT22 Humidity Sensor
//////////////////////////

float DHT22Hum(bool now){
  
//unsigned long time;
if ((millis() - lastmillis_DHT22) >= (sDHT22[2] * 1000) || now == true || akkumode ==true ){
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
        client.publish("TestESP/DHT22/hum/state", mDHT22_HumWert);
        lastmillis_DHT22 = millis();       //counter updaten
        return DHT22_HumWert;
        }
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
  if (String(topic) == "TestESP/switch/state"){
   //do simply nothing  
  }
  
  //Relay/Licht 1
  else if (String(topic) == "TestESP/relay1/set" && msgString == "on"){
    digitalWrite(Relay1,HIGH);
    client.publish("TestESP/relay1/state", "on");  
  }
  else if (String(topic) == "TestESP/relay1/set" && msgString == "off"){
    digitalWrite(Relay1,LOW);
    client.publish("TestESP/relay1/state", "off");
  }
  //Relay/Licht 2
  else if (String(topic) == "TestESP/relay2/set" && msgString == "on"){
    digitalWrite(Relay2,HIGH);
    client.publish("TestESP/relay2/state", "on");
  }
  else if (String(topic) == "TestESP/relay2/set" && msgString == "off"){
    digitalWrite(Relay2,LOW);
    client.publish("TestESP/relay2/state", "off");
  }
  //Relay/Licht 3
  else if (String(topic) == "TestESP/relay3/set" && msgString == "on"){
    digitalWrite(Relay3,HIGH);
    client.publish("TestESP/relay3/state", "on");  
  }
  else if (String(topic) == "TestESP/relay3/set" && msgString == "off"){
    digitalWrite(Relay3,LOW);
    client.publish("TestESP/relay3/state", "off");
  }
    //Relay/Licht 4
  else if (String(topic) == "TestESP/relay4/set" && msgString == "on"){
    digitalWrite(Relay4,HIGH);
    client.publish("TestESP/relay4/state", "on");  
    Serial.print("Relay 4 on");
  }
  else if (String(topic) == "TestESP/relay4/set" && msgString == "off"){
    digitalWrite(Relay4,LOW);
    client.publish("TestESP/relay4/state", "off");
    Serial.print("Relay 4 off");
  }
  //OneWire
  else if (String(topic) == "TestESP/onewire/" && msgString == "get"){
    client.publish("TestESP/onewire/tmp/state",dtostrf(OneWireTemp(true), 1, 0, OW2Buffer));
  }
  else if (String(topic) == "TestESP/onewire/setintv"){
    sOneWire[2] = msgString.toInt();
    client.publish("TestESP/onewire/interv_state" ,dtostrf(sOneWire[2], 1, 0, OW1Buffer));
  }
  //DHT22
    else if (String(topic) == "TestESP/DHT22" && msgString == "get"){
    client.publish("TestESP/DHT22/hum/state",dtostrf(DHT22Hum(true), 1, 0, DHT1Buffer));
  }
  else if (String(topic) == "TestESP/DHT22/setintv"){
    sDHT22[2] = msgString.toInt();
    client.publish("TestESP/DHT22/interv_state" ,dtostrf(sDHT22[2], 1, 0, DHT2Buffer));
  }
  //SHT21
  else if (String(topic) == "TestESP/SHT/setintv"){
    sSHT21[2] = msgString.toInt();
    client.publish("TestESP/SHT21/interv_state" ,dtostrf(sSHT21[2], 1, 0, SHT3Buffer));
  }
   else if (String(topic) == "TestESP/SHT/hum/" && msgString == "get"){
    client.publish("TestESP/SHT21/hum/state",dtostrf(SHT21Hum(true), 1, 0, SHT2Buffer));
  }
     else if (String(topic) == "TestESP/SHT/tmp/" && msgString == "get"){
    client.publish("TestESP/SHT21/tmp/state",dtostrf(SHT21Tmp(true), 1, 0, SHT1Buffer));
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
      client.publish("TestESP/boot", "up and running");
      
      // subscribe auf folgende Topics
      client.subscribe("TestESP/#");  //Wildcard
      //client.subscribe("TestESP/relay1/set");
      //client.subscribe("TestESP/relay2/set");
      //client.subscribe("TestESP/relay3/set");
      //client.subscribe("TestESP/onewire/state");
      //client.subscribe("TestESP/onewire/setintv");
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
