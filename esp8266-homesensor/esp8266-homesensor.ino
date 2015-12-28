//////////////////////////////////////////////////////////////
// Master ESP8266 Sketch
// V0.4.1
// by hessemar
// http://blog.silvertech.at
//////////////////////////////////////////////////////////////
// Der Code Arbeitet verschiedene Sensoren ab und
// überträgt die Daten per HTTP an den Webserver.
// 
// Arduino IDE 1.6.4
//////////////////////////////////////////////////////////////

#include <ESP8266WiFi.h>        //ESP8266
#include <OneWire.h>            //Onewire Temp Std. Lib (sollte ohne extra download funkt.)
#include <DallasTemperature.h>  //Onewire Temp. Lib https://github.com/milesburton/Arduino-Temperature-Control-Library
#include <DHT.h>                // Adafruit DHT Lib https://github.com/adafruit/DHT-sensor-library
#include <Bounce2.h>            //Taster Bounce Lib https://github.com/thomasfredericks/Bounce2


const char* ssid = "WLAN-SSID";           //WLAN SSID
const char* password = "password-1";       //WLAN Kennwort
const char* host = "192.168.88.114";     //Webserver IP
const int netdeviceid = 2;               //NetzwerkGeräte ID
int t1_old = 0;                          //Taster Zustand bei letzten Durchlauf
int t2_old = 0;                          //Taster Zustand bei letzten Durchlauf

//Sensoren definieren
   //SensorID 99 = nothing / PIN -1 = nothing 
   //sSensorName {[SensorID],[PIN#1],<Pin#2>,<HTTP_POST-Interval in s>} //Bus-Type
int sOneWire[] = {3,13,600};      //1Wire
int sDHT22[] = {-1,2,-1};
int sBMP180[] = {-1,-1,-1};
int sRelay1[] = {4,15,-1};      //Licht Relay
int sRelay2[] = {-1,99,-1};
int sMotionDetect[] = {-1,-1};
int sSi7021[] = {-1,-1};      //I2C
int sTaster1[] = {-1,4};
int sTaster2[] = {-1,12};


#define ONE_WIRE_BUS sOneWire[1]
#define DHTPIN sDHT22[1]
#define DHTTYPE DHT22
#define taster_1 sTaster1[1]  
#define taster_2 sTaster2[1]  
#define ausgang  sRelay1[1] 

//Sensor Setup
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DHT dht(DHTPIN, DHTTYPE);
Bounce debouncer_t1 = Bounce();
Bounce debouncer_t2 = Bounce();


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
  pinMode(ausgang,OUTPUT);      //Relay Ausgang
  digitalWrite(ausgang,HIGH);    //Default Wert nach PowerOn
  debouncer_t1.attach(taster_1);
  debouncer_t1.interval(5);
  debouncer_t2.attach(taster_2);
  debouncer_t2.interval(5);
  
  //Serial Start
  Serial.begin(115200);
  delay(10);
  
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
}


//////////////////////
////LOOP
//////////////////////
void loop() {
  
  
  //test
  //Data2WebSrv(99,33);
  //test ende
 
  //Produktiv
  TasterSchaltung( sRelay1[0]);
  //DHT22Hum(sDHT22[0]);
  OneWireTemp(sOneWire[0]);
  
  //no delay needed, every function is controlled by time interval
  //delay(2000);      //dev interval
  //delay(600000);    //long test 10min interval
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
  Serial.println("closing connection");
  return;
  Serial.print("Millis= ");
  Serial.println(millis());
  }

////////////////////////
////OneWire Temp Sensor
////////////////////////
  
void OneWireTemp(int sID){
  
if ((millis() - lastmillis_OneWire) >= (sOneWire[2] * 1000)){
  //Get One Wire Data
  sensors.requestTemperatures();
  //Serial.println( sensors.getTempCByIndex(0));
  float TmpWert = sensors.getTempCByIndex(0);
  
  //Daten an WebSrv übergeben
  Data2WebSrv(sID,TmpWert);
  lastmillis_OneWire = millis();
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
 int ausg_stat = digitalRead(ausgang);
 //get informations
 //Serial.println("    ");
 //Serial.print("ausgang= ");
 //Serial.println(ausg_stat);
 //Serial.print("taster1u2= ");
 //Serial.print(t1_stat );
 //Serial.println(t2_stat);
 
 
 //Zustände des Ausgang (zu Relay) behandeln
 switch (ausg_stat){
  case HIGH:                                           //LICHT IST EIN
   //Serial.println("Licht ist an");                    
     if ((t1_stat == HIGH) && (t1_old != t1_stat)){    //Taster1 - ausschalten
       digitalWrite(ausgang,LOW);
       t1_old = HIGH;
       Data2WebSrv(sID,0);
     }
     else{
       if ((t1_stat == LOW) && (t1_old = HIGH)){       //Taster1 - do nothing
         t1_old = LOW;
       }
     }
     if ((t2_stat == HIGH) && (t2_old != t2_stat)){    //Taster2 - ausschalten
       digitalWrite(ausgang,LOW);
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
       digitalWrite(ausgang,HIGH);
       t1_old = HIGH;
       Data2WebSrv(sID,1);
    }
    else{
     if ((t1_stat == LOW) && (t1_old = HIGH)){        //Taster1 - do nothing
         t1_old = LOW;
       }
    }
     if ((t2_stat == HIGH) && (t2_old != t2_stat)){     //Taster2 - einschalten
       digitalWrite(ausgang,HIGH);
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
