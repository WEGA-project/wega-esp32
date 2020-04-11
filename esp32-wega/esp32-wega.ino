#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <WebServer.h>
WebServer server(80);

#include <WiFiClient.h>
#include <HTTPClient.h>

const char* ssid = "dd-wrt";
const char* password = "HruBse1447209";

float pH,pHraw,tempRAW,dtem1,dst;

// Библиотека работы с DS18b20
#include <OneWire.h> 
#include <DallasTemperature.h>
OneWire oneWire(23);
DallasTemperature sensors(&oneWire);

#include <Wire.h>
// Библиотека работы с AM2320
#include <OneWire.h>
#include <AM2320.h>
AM2320 th;
float am2320temp, am2320hum;
double Ap,An;

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

  ArduinoOTA.setHostname("myesp32");



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
}

void loop() {
    server.handleClient();
    ArduinoOTA.handle();

  // read the input on analog pin 0:
  long n=0;
  float a,b;

  float x1=3730; float y1=4.01;
  float x2=2977; float y2=6.86;
  a=(-x2*y1+y2*x1)/(-x2+x1);
  b=(-y2+y1)/(-x2+x1);
  
//pH shield
//  pHraw=AnalogReadMid(33,5);
//  pH=a+b*pHraw;
  
//BS18B20
 sensors.requestTemperatures();
 dtem1=sensors.getTempCByIndex(0); 
tempRAW=AnalogReadMid(32,50);

// DHT2320
 th.Read();
  am2320temp = th.t;
  am2320hum = th.h;

// EC
ec(18,19,33,500000);

// Level
dst=us(13,14,25,5);
 
WiFiClient client;
HTTPClient http;

String httpstr="http://192.168.237.107/remote/esp32wega.php?";
httpstr +=  "pHraw=" + fFTS(pHraw,2);
httpstr +=  "&pH=" + fFTS(pH,3);
httpstr +=  "&tempRAW=" + fFTS(tempRAW,3);
httpstr +=  "&dtem1=" + fFTS(dtem1,3);
httpstr +=  "&am2320temp=" +fFTS(am2320temp, 1);
httpstr +=  "&am2320hum=" +fFTS(am2320hum, 1);
httpstr +=  "&Ap=" +fFTS(Ap, 3);
httpstr +=  "&An=" +fFTS(An, 3);
httpstr +=  "&Dst=" +fFTS(dst, 3);

http.begin(client, httpstr);
http.GET();
http.end();
}


void handleRoot() {
sensors.requestTemperatures();
//Wire.begin(21, 22);

String httpstr="<meta http-equiv='refresh' content='10'>";
       httpstr += "Teperature 18b20: " + fFTS(dtem1,3) + "<br>";
       httpstr += "pH: " + fFTS(pH,3) + "<br>";
       httpstr +=  "Analog temperature RAW: " + fFTS(tempRAW,3)+ "<br>";
       httpstr +=  "am2320temp0: " +fFTS(am2320temp, 1) + "<br>";
       httpstr +=  "am2320hum0: " +fFTS(am2320hum, 1) + "<br>";
       httpstr +=  "Ap: " +fFTS(Ap, 3) + "<br>";
       httpstr +=  "An: " +fFTS(An, 3) + "<br>";
       httpstr +=  "Dst_raw: " +fFTS(dst, 3) + "<br>";
       
server.send(200, "text/html",  httpstr);

}

// Функция преобразования чисел с плавающей запятой в текст
String fFTS(float x, byte precision) {
  char tmp[50];
  dtostrf(x, 0, precision, tmp);
  return String(tmp);
}


float AnalogReadMid(int port, long count) {
    server.handleClient();
    ArduinoOTA.handle();

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

void ec(char d1,  char d2, char a0, long l) {

    Ap=0;
    An=0;
  double n = 0;

    pinMode(a0, INPUT_PULLUP);
    pinMode(d1, OUTPUT);
    pinMode(d2, OUTPUT);


  while (n < l) {
    n++;
            
                  //ArduinoOTA.handle();    
    digitalWrite(d1, HIGH);
    delayMicroseconds(1);
    Ap = analogRead(a0)+Ap; 
    digitalWrite(d1, LOW); 
    
    ArduinoOTA.handle();
    
    digitalWrite(d2, HIGH);
    delayMicroseconds(1);
    An = analogRead(a0)+An; 
    digitalWrite(d2, LOW); 

    ArduinoOTA.handle(); 
   

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
 
 float vSound=20.046796*sqrt(273.15+temp);
 return (vSound/10000)*((float(microssum)/count)/2);
 //return (float(microssum)/count);
}

