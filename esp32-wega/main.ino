#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <WebServer.h>
WebServer server(80);

#include <WiFiClient.h>
#include <HTTPClient.h>

float pH,pHraw,tempRAW,luxRAW,dtem1,dst,hall;
double Ap,An;


#include <Wire.h>      // Поддержка шины i2c
#include <OneWire.h>   // Поддержка шины 1-Wire

// Библиотека работы с DS18b20
#include "src/DallasTemperature/DallasTemperature.h"
OneWire oneWire(23);
DallasTemperature sensors(&oneWire);

// Библиотека работы с AHT10
#include "src/Adafruit_AHTX0/Adafruit_AHTX0.h"
Adafruit_AHTX0 aht;

// ADS1115 for pH
#include "src/Adafruit_ADS1X15/Adafruit_ADS1015.h"
Adafruit_ADS1115 ads;


void setup() {
  Serial.begin(9600);
  //Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    //Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  ArduinoOTA.setHostname("esp32wega-2");

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  MDNS.begin("esp32-wega");
  MDNS.addService("http", "tcp", 80);
  server.on("/", handleRoot);
  server.begin();

  Wire.begin();
  ads.begin(); 
  //ads.setGain(GAIN_TWOTHIRDS);
  //ads.setGain(GAIN_SIXTEEN);
  ads.setGain(GAIN_TWOTHIRDS);  // 2/3x gain +/- 6.144V  1 bit = 3mV      0.1875mV (default) 

// AHT10 > temperature and humidity
aht.begin();

//BS18B20 > temperature
sensors.begin();
delay(300);
sensors.requestTemperatures();
sensors.getTempCByIndex(0);
}

void loop() {
    server.handleClient();
    ArduinoOTA.handle();


// AHT10 > temperature and humidity
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);

// Termistor for EC (port, averaging counter) > RAW
tempRAW=AnalogReadMid(32,500000);

// EC sensor
// Electrode (d-port 1,d-port 2, a-port, averaging counter) > RAW
ec(18,19,33,80000);



// Photoresistor for Luxmetter 
luxRAW=AnalogReadMid(35,10000);


// Level ultrasound (echo, trig, temp, averaging counter) > cm 
dst=us(13,14,25,60);

//pH RAW over ADS1115 > RAW
pHraw = adsdiff01(5000); 

// Integrated Hall Sensor read
hall=hallmid(100);

//BS18B20 > temperature
 sensors.requestTemperatures();
 dtem1=sensors.getTempCByIndex(0); 
 
// Sending to WEGA-API 
WiFiClient client;
HTTPClient http;

String httpstr=wegaapi;
httpstr +=  "?db=" + wegadb;
httpstr +=  "&auth=" + wegaauth;
httpstr +=  "&pHraw=" + fFTS(pHraw,2);
httpstr +=  "&ECtempRAW=" + fFTS(tempRAW,3);
httpstr +=  "&LightRAW=" + fFTS(luxRAW,3);
httpstr +=  "&RootTemp=" + fFTS(dtem1,3);
httpstr +=  "&AirTemp=" +fFTS(temp.temperature, 3);
httpstr +=  "&AirHum=" +fFTS(humidity.relative_humidity, 3);
httpstr +=  "&Ap=" +fFTS(Ap, 3);
httpstr +=  "&An=" +fFTS(An, 3);
httpstr +=  "&Dst=" +fFTS(dst, 3);
httpstr +=  "&hall=" +fFTS(hall, 3);


http.begin(client, httpstr);
http.GET();
http.end();


  if (WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect(true);
    WiFi.begin(ssid, password);  }

}


void handleRoot() {
sensors.requestTemperatures();

  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);// populate temp and humidity objects with fresh data

String httpstr="<meta http-equiv='refresh' content='10'>";
       httpstr +=  "RootTemp: " + fFTS(dtem1,3) + "<br>";
       httpstr +=  "pHraw: " + fFTS(pHraw,2) + "<br>";
       httpstr +=  "ECtempRAW: " + fFTS(tempRAW,3)+ "<br>";
       httpstr +=  "LightRAW: " +fFTS(luxRAW, 3) + "<br>";
       httpstr +=  "AirTemp: " +fFTS(temp.temperature, 3) + "<br>";
       httpstr +=  "AirHum: " +fFTS(humidity.relative_humidity, 3) + "<br>";
       httpstr +=  "Ap: " +fFTS(Ap, 3) + "<br>";
       httpstr +=  "An: " +fFTS(An, 3) + "<br>";
       httpstr +=  "Dst: " +fFTS(dst, 3) + "<br>";
       httpstr +=  "hall: " +fFTS(hall, 3) + "<br>";

       
server.send(200, "text/html",  httpstr);

}

// Функция преобразования чисел с плавающей запятой в текст с округлением
String fFTS(float x, byte precision) {
  char tmp[50];
  dtostrf(x, 0, precision, tmp);
  return String(tmp);
}

// Функция усреднения значений измерения напряжения на ADS1115 между портами 0 и 1
float adsdiff01(long count) {
    ArduinoOTA.handle();

  long n=0;
  double sensorValue=0;
  while ( n< count){
    n++;
        server.handleClient();
    ArduinoOTA.handle();
  sensorValue = (ads.readADC_Differential_0_1())+sensorValue;
 }
 return sensorValue/n;

}

// Функция измерения датчиком холла
float hallmid(long count) {
    ArduinoOTA.handle();

  long n=0;
  double sensorValue=0;
  while ( n< count){
    n++;
        server.handleClient();
    ArduinoOTA.handle();
  sensorValue = hallRead()+sensorValue;
 }
 return sensorValue/n;
}



// Функция усреднения измерения аналогово порта
float AnalogReadMid(int port, long count) {
    server.handleClient();
    ArduinoOTA.handle();
    pinMode(port, OUTPUT);
    digitalWrite(port, HIGH);
    delayMicroseconds(1000);
    pinMode(port, INPUT);
    delayMicroseconds(1000);
  long n=0;
  double sensorValue=0;
  while ( n< count){
    n++;
        server.handleClient();
    ArduinoOTA.handle();
    
  
  sensorValue = (analogRead(port))+sensorValue;
 
 }
 return sensorValue/n;

}

// Функция измерения аналогового знаения ЕС
void ec(char d1,  char d2, char a0, long l) {

    Ap=0;
    An=0;
  double n = 0;

    //pinMode(a0, INPUT_PULLUP);
    pinMode(a0, INPUT);
    pinMode(d1, OUTPUT);
    pinMode(d2, OUTPUT);

  while (n < l) {

    ArduinoOTA.handle(); 
    
    n++;

    digitalWrite(d1, HIGH);
    delayMicroseconds(1);
    Ap = analogRead(a0)+Ap; 
    digitalWrite(d1, LOW); 
         
    digitalWrite(d2, HIGH);
    delayMicroseconds(1);
    An = analogRead(a0)+An; 
    digitalWrite(d2, LOW); 

   

  }
    //delay(300);
    pinMode(d1, INPUT);
    pinMode(d2, INPUT);
    pinMode(a0, INPUT);


    
    Ap=Ap/n;
    An=An/n;
    
    ArduinoOTA.handle();

}

// Функция прямого опроса ультразвукового датчика возврат в микросекундах эха
float us(int trg, int ech, float temp, long cnt) {
   long count=0;
 long microssum=0;
 while (count < cnt) {
  count++;
    ArduinoOTA.handle();
       
    pinMode(trg, OUTPUT );
    pinMode(ech, INPUT);

    digitalWrite(trg,1);
    delayMicroseconds(10);
    digitalWrite(trg,0);


    long n=0;
    long limit=100000;
    
    long startmic,endmicros;
    while (n<limit){
      n++;
    if(digitalRead(ech) == 1)  
      {
      startmic=micros();
      long z=0;
      while (digitalRead(ech) == 1 and z<20000) {
        z++;
        endmicros=micros();
        }
        n=limit;
        delay(200);
      } 

    }

  microssum=microssum+(endmicros-startmic);
      
 }
 
 float vSound=20.046796*sqrt(273.15+25);
 return (vSound/10000)*((float(microssum)/count)/2); // В сантиметрах
 //return (float(microssum)/count);  // В миллисекундах
}

