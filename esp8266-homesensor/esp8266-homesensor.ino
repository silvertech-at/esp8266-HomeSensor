//////////////////////////////////////////////////////////////
// Master ESP8266 Sketch
// V0.4.1
// by wi3
// http://blog.silvertech.at
//////////////////////////////////////////////////////////////
// Der Code Arbeitet verschiedene Sensoren ab und
// überträgt die Daten an den Webserver und MQTT Broker.
// 
// Arduino IDE 1.6.5-R5
//////////////////////////////////////////////////////////////

#include <ESP8266WiFi.h>        //ESP8266
#include <OneWire.h>            //Onewire Temp Std. Lib (sollte ohne extra download funkt.)
#include <DallasTemperature.h>  //Onewire Temp. Lib https://github.com/milesburton/Arduino-Temperature-Control-Library
#include <DHT.h>                // Adafruit DHT Lib https://github.com/adafruit/DHT-sensor-library
#include <Bounce2.h>            //Taster Bounce Lib https://github.com/thomasfredericks/Bounce2
#include <PubSubClient.h>       //MQTT Lib https://github.com/knolleary/pubsubclient


const char* ssid = "WLAN-SSID";                //WLAN SSID
const char* password = "password-1";           //WLAN Kennwort
const char* host = "192.168.88.114";           //Webserver IP
const char* mqtt_server = "192.168.88.103";    //MQTT Broker
const char* devicename = "testESP";            //Geräte Namen
const int deviceid = 2;                        //Geräte ID


int t1_old = 0;                                //Taster Zustand bei letzten Durchlauf
int t2_old = 0;                                //Taster Zustand bei letzten Durchlauf


//Sensoren definieren
   //SensorID 99 = nothing / PIN -1 = nothing 
   //sSensorName {[SensorID],[PIN#1],<Pin#2>,<Interval in s>} //Bus-Type
int sOneWire[] = {99,13,60};      //1Wire
int sDHT22[] = {-1,2,-1};
int sBMP180[] = {-1,-1,-1};
int sRelay1[] = {4,15,-1};      //Licht Relay
int sRelay2[] = {-1,14,-1};
int sMotionDetect[] = {-1,-1};
int sSi7021[] = {-1,-1};        //I2C
int sTaster1[] = {-1,4};
int sTaster2[] = {-1,12};


#define ONE_WIRE_BUS sOneWire[1]
#define DHTPIN sDHT22[1]
#define DHTTYPE DHT22
#define taster_1 sTaster1[1]  
#define taster_2 sTaster2[1]  
#define Relay1  sRelay1[1] 
#define Relay2  sRelay2[1]



//Sensor Setup
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DHT dht(DHTPIN, DHTTYPE);
Bounce debouncer_t1 = Bounce();
Bounce debouncer_t2 = Bounce();

//MQTT Setup
WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;


//Old Millis Variablen pro Sensor
unsigned long lastmillis_DHT22 = 0;
unsigned long lastmillis_OneWire = 0;
unsigned long lastmillis_SI7021 = 0;
unsigned long lastmillis_BMP180 = 0;


//////////////////////
////SETUP
//////////////////////
void setup() {
  
  //Sensor start Bus/Work
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
////LOOP
//////////////////////
void loop() {
 
  //Produktiv
  TasterSchaltung( sRelay1[0]);
  //DHT22Hum(sDHT22[0]);
  OneWireTemp(false); //warning in function
  
  //MQTT
   if (!client.connected()) {
    reconnect();
  }
  client.loop();
}


//////////////////////
////Data to Webserver
//////////////////////
void Data2WebSrv(int SensorID,float Wert){
  if(SensorID == -1){        //-1 = default Wert
    return;
    }
    Serial.print("connecting to ");
  Serial.println(host);
  
  // Use WiFiClient class to create TCP connections
  WiFiClient client;
  const int httpPort = 80;
  if (!client.connect(host, httpPort)) {
    Serial.println("connection failed");
    return;
  }
  
  // We now create a URI for the request
  String url = "/get_val.php?";
  url += "SensorID=";
  url += SensorID;
  url += "&Wert=";
  url += Wert;
  
  //Serial.print("Requesting URL: ");
  //Serial.println(url);
  
  // This will send the request to the server
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" + 
               "Connection: close\r\n\r\n");
  Serial.println("SenID:" +String(SensorID)+ " Wert:" + String(Wert));
  delay(10);
  
  // Read all the lines of the reply from server and print them to Serial
  while(client.available()){
    String line = client.readStringUntil('\r');
    //Serial.print(line);
  }
  //Serial.println("closing connection");
  
  //Serial.print("Millis= ");
  //Serial.println(millis());
  return;
  }

////////////////////////
////OneWire Temp Sensor
////////////////////////
  
char *OneWireTemp(bool now){
  
if ((millis() - lastmillis_OneWire) >= (sOneWire[2] * 1000) || now == true ){
  //Get One Wire Data
  sensors.requestTemperatures();
  //Serial.println( sensors.getTempCByIndex(0));
  //Daten an WebSrv übergeben
  float TmpWert = sensors.getTempCByIndex(0);
  //Data2WebSrv(sID,TmpWert);
  //MQTT
  char mTmpWert[20];
  dtostrf(TmpWert, 4, 1,mTmpWert);
  client.publish("testESP/onewire/state", mTmpWert);
  lastmillis_OneWire = millis();
  return mTmpWert;
  }
 }
  
  
//////////////////////////
////DHT22 Humidity Sensor
//////////////////////////

void DHT22Hum(int sID){
  
unsigned long time;
//Serial.println(millis());                        //Debug Helpers
//Serial.println(lastmillis_DHT22);                //Debug Helpers
//Serial.println((millis() - lastmillis_DHT22) );  //Debug Helpers
if ((millis() - lastmillis_DHT22) >= (sDHT22[2] * 1000)){
    //Serial.println("DHT22 Hum messung");
    float HumWert = dht.readHumidity();   
      if (isnan(HumWert))   //prüft auf gültige zahl                  
       {
         Serial.println("DHT kann nicht gelesen werden!");
         Serial.println(HumWert);
         }
         else
         {
        Data2WebSrv(sID,HumWert);         //Daten an WebSrv übergeben
        lastmillis_DHT22 = millis();       //counter updaten
        }
     }
  }
 
//////////////////////////
////2 Taster Schaltung
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
       Data2WebSrv(sID,0);
     }
     else{
       if ((t1_stat == LOW) && (t1_old = HIGH)){       //Taster1 - do nothing
         t1_old = LOW;
       }
     }
     if ((t2_stat == HIGH) && (t2_old != t2_stat)){    //Taster2 - ausschalten
       digitalWrite(Relay1,LOW);
       t2_old = HIGH;
       Data2WebSrv(sID,0);
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
       Data2WebSrv(sID,1);
    }
    else{
     if ((t1_stat == LOW) && (t1_old = HIGH)){        //Taster1 - do nothing
         t1_old = LOW;
       }
    }
     if ((t2_stat == HIGH) && (t2_old != t2_stat)){     //Taster2 - einschalten
       digitalWrite(Relay1,HIGH);
       t2_old = HIGH;
       Data2WebSrv(sID,1);
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
////MQTT Callback
//////////////////////////
  
void callback(char* topic, byte* payload, unsigned int length) {

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
  Serial.println("Topic: ");
  Serial.print(topic);

  if (String(topic) == "testESP/relay1/set" && msgString == "on"){
    digitalWrite(Relay1,HIGH);
    client.publish("testESP/relay1/state", "on");  
  }
  else if (String(topic) == "testESP/relay1/set" && msgString == "off"){
    digitalWrite(Relay1,LOW);
    client.publish("testESP/relay1/state", "off");
  }
  else if (String(topic) == "testESP/relay2/set" && msgString == "on"){
    digitalWrite(Relay2,HIGH);
    client.publish("testESP/relay2/state", "on");
  }
  else if (String(topic) == "testESP/relay2/set" && msgString == "off"){
    digitalWrite(Relay2,LOW);
    client.publish("testESP/relay2/state", "off");
  }
  else if (String(topic) == "testESP/onewire/state" && msgString == "get"){
    digitalWrite(Relay2,LOW);
    client.publish("testESP/onewire/tmp",OneWireTemp(true));
  }
  else if (String(topic) == "testESP/onewire/setintv"){
    sOneWire[2] = msgString.toInt();
    //byte* nState = byte*(sOneWire[2]);
    client.publish("testESP/onewire/interv_state","changed");
  }
}

//////////////////////////
////MQTT Reconnect
//////////////////////////

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");

    if (client.connect("ESP8266Client")) {
      Serial.println("connected");
      
      // in OutTopic Nachricht ausgeben
      client.publish("outTopic", "hello world");
      
      // subscribe auf folgende Topics
      client.subscribe("inTopic");
      client.subscribe("testESP/relay1/set");
      client.subscribe("testESP/relay2/set");
      //client.subscribe("testESP/relay3/set");
      client.subscribe("testESP/onewire/state");
      client.subscribe("testESP/onewire/setintv");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // 5 Sekunden warten bis zu einen erneuten Verbinden
      delay(5000);
    }
  }
}
