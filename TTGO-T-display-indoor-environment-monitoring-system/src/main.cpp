//____________________INCLUDE____________________
#include <Arduino.h>
#include <Adafruit_BME280.h>
#include "Adafruit_CCS811.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <Wire.h>
#include <TFT_eSPI.h> // Graphics and font library for ST7735 driver chip
#include <SPI.h>

//____________________WIFI SETUP____________________
// const char* ssid = "SSID";
// const char* password = "PASSWORD";

//____________________Object Initialization____________________
Adafruit_CCS811 ccs;
Adafruit_BME280 bme;
TFT_eSPI tft = TFT_eSPI(); // Invoke library, pins defined in User_Setup.h
AsyncWebServer server(80);

#define TFT_GREY 0x5AEB // New colour

uint16_t TVOC = 0;
uint16_t eCO2 = 0;
volatile uint32_t DebounceTimer = 0;

float temperature = 0;
float humidity = 0;
char buffer0[20] = "";
char buffer1[20] = "";
char buffer2[20] = "";
char buffer3[20] = "";
String IP;

bool flag0 = 0,flag1 = 0, wifi_enabled = 0;
int screenWidth = 240;
int screenHeight = 135;
int x = 0, y = 0;

//____________________Button interrupt____________________
void IRAM_ATTR ButtonR()
{
  if (millis() - 250 >= DebounceTimer)
  {
    DebounceTimer = millis();
    flag0 = 1;
    Serial.println("ButtonR");
  }
}

//____________________Function____________________
// Read/check sensor data and return into String format
String readBME280Temperature()
{
  // Read temperature as Celsius (the default)
  float t = bme.readTemperature();
  // Convert temperature to Fahrenheit
  // t = 1.8 * t + 32;
  if (isnan(t))
  {
    Serial.println("Failed to read from BME280 sensor!");
    return "";
  }
  else
  {
    Serial.println(t);
    return String(t);
  }
}
String readBME280Humidity()
{
  float h = bme.readHumidity();
  if (isnan(h))
  {
    Serial.println("Failed to read from BME280 sensor!");
    return "";
  }
  else
  {
    Serial.println(h);
    return String(h);
  }
}
String readBME280Pressure()
{
  float p = bme.readPressure() / 100.0F;
  if (isnan(p))
  {
    Serial.println("Failed to read from BME280 sensor!");
    return "";
  }
  else
  {
    Serial.println(p);
    return String(p);
  }
}
String readCCS811eCO2()
{
  float eCO2 = ccs.geteCO2();
  return String(eCO2);
}
String readCCS811TVOC()
{
  float TVOC = ccs.getTVOC();
  return String(TVOC);
}

//____________________SETUP____________________
void setup()
{
  Serial.begin(115200);
  //____________________Button interrupt____________________
  pinMode(35, INPUT);
  attachInterrupt(35, ButtonR, RISING);
  //____________________Screen init____________________
  tft.init();
  tft.fillScreen(TFT_RGB);
  tft.setRotation(3);
  tft.setTextPadding(5);
  // tft.drawRoundRect(20,20,200,100,10,TFT_GREY);
  // tft.fillRoundRect(20,20,200,100,10,TFT_DARKGREY);
  // tft.setCursor((screenWidth/2)-30,(screenHeight/2)-10 , 1);
  tft.setTextDatum(10);
  tft.drawString("Sensor test", (screenWidth / 2), (screenHeight / 2)-10, 4);
  Serial.println("Sensor initialisation");
  if (!ccs.begin())
  {
    tft.fillScreen(TFT_RGB);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("Could not find a valid CSS811 sensor", (screenWidth / 2), (screenHeight / 2), 2);
    Serial.println("Could not find a valid CSS811 sensor, check wiring!");
    while (1)
      ;
  }
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("BME280 initialisation...", (screenWidth / 2), (screenHeight / 2) + 10, 1);
  if (!bme.begin(0x76, &Wire))
  {
    tft.fillScreen(TFT_RGB);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("Could not find a valid BME280 sensor", (screenWidth / 2), (screenHeight / 2), 2);
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
  }
  Serial.println("Waiting for CSS811 to be ready...");
  tft.drawString("Waiting for CSS811 to be ready...", (screenWidth / 2), (screenHeight / 2) + 25, 1);
  // Wait for the sensor to be ready
  while (!ccs.available())
    ;
  ccs.setDriveMode(CCS811_DRIVE_MODE_1SEC);
  tft.fillScreen(TFT_RGB);
  // Initialize SPIFFS
  if (!SPIFFS.begin())
  {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
  Serial.println("SPIFFS initialized");

  // Connect to Wi-Fi
  tft.drawString("Wifi initialisation", (screenWidth / 2), (screenHeight / 2), 4);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    tft.drawString(".", (screenWidth / 2) + x - 20, (screenHeight / 2) + 5, 1);
    Serial.println("Connecting to WiFi..");
    x = x + 5;
    wifi_enabled = 1;
    if (x > 20)
    {
      tft.fillScreen(TFT_RGB);
      tft.drawString("Wifi not initialized !", (screenWidth / 2), (screenHeight / 2), 4);
      wifi_enabled = 0;
      delay(2000);
      break;
    }
  }
  x = 0;

  // Print ESP32 Local IP Address
  if (wifi_enabled == 1)
  {
    tft.fillScreen(TFT_RGB);
    tft.setCursor((screenWidth / 2) - 50, (screenHeight / 2) - 30, 4);
    tft.print("Local IP: ");
    tft.setCursor((screenWidth / 2) - 80, (screenHeight / 2) + 15, 4);
    tft.print(WiFi.localIP());
    delay(2000);

    // Route for root / web page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/index.html"); });
    server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send_P(200, "text/plain", readBME280Temperature().c_str()); });
    server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send_P(200, "text/plain", readBME280Humidity().c_str()); });
    server.on("/pressure", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send_P(200, "text/plain", readBME280Pressure().c_str()); });
    server.on("/eCO2", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send_P(200, "text/plain", readCCS811eCO2().c_str()); });
    server.on("/TVOC", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send_P(200, "text/plain", readCCS811TVOC().c_str()); });

    // Start server
    server.begin();
  }
  // CCS Temperature and Humidity calibration
  ccs.setEnvironmentalData(bme.readHumidity(), bme.readTemperature());
}

void loop()
{
  //____________________Sensor reading____________________
  temperature = bme.readTemperature();
  humidity = bme.readHumidity();

  if (ccs.available())
  {
    if (!ccs.readData())
    {
      TVOC = ccs.getTVOC();
      eCO2 = ccs.geteCO2();
    }
    else
    {
      Serial.println("CSS811 ERROR!");
      TVOC = -1;
      eCO2 = -1;
    }
  }
  //____________________Serial____________________
  // Format tu use with Serial Port Plotter available here: https://github.com/CieNTi/serial_port_plotter
  Serial.printf("$%d %d %f %f;\n", eCO2, TVOC, temperature, humidity);
  
  sprintf(buffer0, "eCO2: %d ppm", eCO2);
  sprintf(buffer1, "TVOC: %d ppb", TVOC);
  sprintf(buffer2, "Temp: %.2f `C", temperature);
  sprintf(buffer3, "Hum: %.2f %%", humidity);
  //____________________TFT SCREEN____________________
  tft.fillScreen(TFT_BLACK);
  int xpos = 0, ypos = 0;
  //__________eCO2__________
  tft.setCursor(xpos, ypos, 2);
  if (400 <= eCO2 && eCO2 < 1000)
  {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
  }
  else if (1000 < eCO2 && eCO2 < 2000)
  {
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  }
  else if (2000 < eCO2 && eCO2 < 5000)
  {
    tft.setTextColor(TFT_RED, TFT_BLACK);
  }
  else if (eCO2 > 5000)
  {
    tft.setTextColor(TFT_RED, TFT_RED);
  }
  tft.setTextSize(2);
  tft.print(buffer0);
  ypos += tft.fontHeight(2);
  tft.setCursor(xpos, ypos, 2);
  //__________TVOC__________
  if (0 < TVOC && TVOC < 250)
  {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
  }
  else if (250 < TVOC && TVOC < 2000)
  {
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  }
  else if (TVOC > 2000)
  {
    tft.setTextColor(TFT_RED, TFT_BLACK);
  }
  tft.print(buffer1);
  ypos += tft.fontHeight(2);
  //__________Temperature__________
  tft.setCursor(xpos, ypos, 2);
  if (temperature < 17)
  {
    tft.setTextColor(TFT_BLUE, TFT_BLACK);
  }
  else if (17 < temperature && temperature < 25)
  {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
  }
  else if (temperature > 25)
  {
    tft.setTextColor(TFT_RED, TFT_BLACK);
  }
  tft.print(buffer2);
  ypos += tft.fontHeight(2);
  //__________Humidity__________
  tft.setCursor(xpos, ypos, 2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.print(buffer3);

  delay(1000);
}