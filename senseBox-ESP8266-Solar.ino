/*
  senseBox Home - Citizen Sensingplatform
  Version: 2.1 - ESP8266
  Date: 2015-09-09
  Homepage: http://www.sensebox.de
  Author: Gerald Pape Original by Jan Wirwahn, Institute for Geoinformatics, University of Muenster
  Note: Sketch for senseBox:home ESP8266
  Email: support@sensebox.de
*/

#include <Wire.h>
#include "HDC100X.h"
#include <BME280.h>
#include <Makerblog_TSL45315.h>
#include <SPI.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

#define ssid "WiFi Name"
#define password "WiFi Password"

IPAddress static_ip = IPAddress(192, 168, 0, 30);
IPAddress gateway_ip = IPAddress(192, 168, 0, 1);
IPAddress netmask = IPAddress(255, 255, 255, 0);

#define sleepSec 60e6 // 60 seconds. This value is in microseconds

//SenseBox ID
#define SENSEBOX_ID "<SENSEBOX_ID>"

//Sensor IDs
#define TEMPSENSOR_ID "<TEMPSENSOR_ID>"
#define HUMISENSOR_ID "<HUMISENSOR_ID"
#define PRESSURESENSOR_ID "<PRESSURESENSOR_ID>"
#define LUXSENSOR_ID "<LUXSENSOR_ID>"
#define UVSENSOR_ID "<UVSENSOR_ID>"
//#define WIFI_STRENGTH_ID "<WIFI_STRENGTH_ID>"

#define urlTemplateBulk "http://opensensemap.org:8000/boxes/%s/data"

WiFiClient client;
HTTPClient http;

//Load sensors
Makerblog_TSL45315 TSL = Makerblog_TSL45315(TSL45315_TIME_M4);
HDC100X HDC(0x43);
BME280 bme;

//measurement variables
float temp(NAN), hum(NAN), pres(NAN), humidity(NAN), temperature(NAN);
uint8_t pressureUnit(1); //hPa
uint32_t lux(NAN);
uint16_t uv(NAN);
int wifidBm;
#define UV_ADDR 0x38
#define IT_1   0x1

void initSensors() {
  Serial.println("Initializing sensors...");
  Wire.begin();
  Wire.beginTransmission(UV_ADDR);
  Wire.write((IT_1 << 2) | 0x02);
  Wire.endTransmission();
  delay(500);
  HDC.begin(HDC100X_TEMP_HUMI, HDC100X_14BIT, HDC100X_14BIT, DISABLE);
  TSL.begin();
  bme.begin();
  Serial.println("done!");
  temperature = HDC.getTemp();
}

String formatValue(float value, int decimals) {
  char obs[10];
  dtostrf(value, 5, decimals, obs);
  return obs;
}

void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  // start the Ethernet connection:
  Serial.println();
  Serial.print("Starting internet connection...");
  WiFi.mode(WIFI_STA);
  WiFi.config(static_ip, gateway_ip, netmask);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("connected");
  delay(500);
  //Initialize sensors
  initSensors();
  delay(100);

  DynamicJsonBuffer  jsonBuffer;
  JsonArray& array = jsonBuffer.createArray();
  Serial.println("Reading sensors..");

  bme.ReadData(pres, temp, hum, pressureUnit, true);
  JsonObject& pressureObject = array.createNestedObject();
  pressureObject["sensor"] = PRESSURESENSOR_ID;
  pressureObject["value"] = formatValue(pres, 2);
  delay(200);

  humidity = HDC.getHumi();
  JsonObject& humidityObject = array.createNestedObject();
  humidityObject["sensor"] = HUMISENSOR_ID;
  humidityObject["value"] = formatValue(humidity, 2);

  // wait some more time because humidity and temperature are read by the same sensor
  delay(1500);

  temperature = HDC.getTemp();
  JsonObject& temperatureObject = array.createNestedObject();
  temperatureObject["sensor"] = TEMPSENSOR_ID;
  temperatureObject["value"] = formatValue(temperature, 2);
  delay(200);

  lux = TSL.readLux();
  JsonObject& luxObject = array.createNestedObject();
  luxObject["sensor"] = LUXSENSOR_ID;
  luxObject["value"] = formatValue(lux, 0);
  delay(200);

  uv = getUV();
  JsonObject& uvObject = array.createNestedObject();
  uvObject["sensor"] = UVSENSOR_ID;
  uvObject["value"] = formatValue(uv, 0);
  delay(200);

  //wifi.. Remember you have to register every additional sensor before you can submit values for it
  // JsonObject& wifiObject = array.createNestedObject();
  // wifiObject["sensor"] = WIFI_STRENGTH_ID;
  // wifiObject["value"] = formatValue(WiFi.RSSI(), 0);

  delay(1000);

  char buffer[500];
  array.printTo(buffer, sizeof(buffer));

  postObservations(buffer);

}

void loop() {
  // never reached
}

void postObservations(char* str) {
  char postUrl[100];

  sprintf(postUrl, urlTemplateBulk, SENSEBOX_ID);

  http.begin(postUrl);

  http.addHeader("Content-Type", "application/json");

  int httpCode = http.POST(str);
  if (httpCode > 0) {
    Serial.printf("[HTTP] POST %s to %s ... code: %d\n", str, postUrl, httpCode);

    // file found at server
    if (httpCode == HTTP_CODE_CREATED) {
      http.writeToStream(&Serial);
    }
    Serial.println("Going into deep sleep");
    delay(10);
    ESP.deepSleep(sleepSec);
  } else {
    Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
    Serial.println("Going into deep sleep");
    delay(10);
    ESP.deepSleep(sleepSec);
  }

  http.end();
}


uint16_t getUV() {
  byte msb = 0, lsb = 0;
  uint16_t uvValue;

  Wire.requestFrom(UV_ADDR + 1, 1); //MSB
  delay(1);
  if (Wire.available()) msb = Wire.read();

  Wire.requestFrom(UV_ADDR + 0, 1); //LSB
  delay(1);
  if (Wire.available()) lsb = Wire.read();

  uvValue = (msb << 8) | lsb;

  return uvValue * 5;
}
