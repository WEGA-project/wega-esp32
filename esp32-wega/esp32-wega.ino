#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <WebServer.h>
WebServer server(80);

#include <WiFiClient.h>
#include <HTTPClient.h>

// Параметры подключения к WiFi
const char* ssid = "SSID";
const char* password = "PASSWORD";

// Параметры подключения к WEGA-Api
String      wegaapi  = "http://192.168.237.107/wega-api/esp32wega.php";  // Адрес wega-api
String      wegaauth = "adab637320e5c47624cdd15169276981";               // Код доступа к api
String      wegadb   = "esp32wega";                                      // Имя базы данных


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

// ADS1115 for pH
#include <Adafruit_ADS1015.h>
Adafruit_ADS1115 ads;
  //ads.setGain(GAIN_SIXTEEN);    // 16x gain  +/- 0.256V  1 bit = 0.125mV  0.0078125mV
  
  
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
  ads.begin(); 
  ads.setGain(GAIN_TWOTHIRDS);
}

void loop() {
    server.handleClient();
    ArduinoOTA.handle();


//BS18B20 > temperature
 sensors.requestTemperatures();
 dtem1=sensors.getTempCByIndex(0); 


// DHT2320 > temperature and humidity
 th.Read();
  am2320temp = th.t;
  am2320hum = th.h;

// EC sensor
// Electrode (d-port 1,d-port 2, a-port, averaging counter) > RAW
ec(18,19,33,80000);

// Termistor for EC (port, averaging counter) > RAW
tempRAW=AnalogReadMid(32,10000);

// Level ultrasound (echo, trig, temp, averaging counter) > cm 
dst=us(13,14,25,60);

//pH RAW over ADS1115 > RAW
float pHraw = adsdiff01(5000); 

// Sending to WEGA-API 
WiFiClient client;
HTTPClient http;

String httpstr=wegaapi;
httpstr +=  "?db=" + wegadb;
httpstr +=  "&auth=" + wegaauth;
httpstr +=  "&pHraw=" + fFTS(pHraw,2);
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

String httpstr="<meta http-equiv='refresh' content='10'>";
       httpstr += "Teperature 18b20: " + fFTS(dtem1,3) + "<br>";
       httpstr += "pH: " + fFTS(pH,0) + "<br>";
       httpstr +=  "Analog temperature RAW: " + fFTS(tempRAW,3)+ "<br>";
       httpstr +=  "am2320temp0: " +fFTS(am2320temp, 1) + "<br>";
       httpstr +=  "am2320hum0: " +fFTS(am2320hum, 1) + "<br>";
       httpstr +=  "Ap: " +fFTS(Ap, 3) + "<br>";
       httpstr +=  "An: " +fFTS(An, 3) + "<br>";
       httpstr +=  "Dst_raw: " +fFTS(dst, 3) + "<br>";
       
server.send(200, "text/html",  httpstr);

}

// Функция преобразования чисел с плавающей запятой в текст с округлением
String fFTS(float x, byte precision) {
  char tmp[50];
  dtostrf(x, 0, precision, tmp);
  return String(tmp);
}

// Функция усреднения значений измерения напряжения на ADS1117 между портами 0 и 1
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

// Функция устреднения измерения аналогово порта
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

