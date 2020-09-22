#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Adafruit_SGP30.h>

#include <NTPClient.h>
#include <WiFiUdp.h>

Adafruit_BME280 bme;
Adafruit_SGP30 sgp;

#include "SSD1306.h"
#define I2C_ADDR_SSD1306 0x3c
SSD1306 display(I2C_ADDR_SSD1306, 21, 22); // SDA and SCL pins (default)

float temperatureSum = 0;
float humiditySum = 0;
float pressureSum = 0;
float co2Sum = 0;
float freeMemSum = 0;
int count = 0;

char jsonOutput[128];
String formattedDate = "";

const char* NETWORKSSID = ""; // Add your WiFi SSID within the quotes
const char* NETWORKPASSWORD = ""; // Network password
const char* USERNAME = ""; // Username for HTTP auth
const char* USERPASSWORD = "; // Password for HTTP auth
const char* POSTURL = ""; // URL to POST to

WiFiUDP ntpUDP;

// ntpUDP, NTP server, time offset, refresh interval
NTPClient timeClient(ntpUDP, "ca.pool.ntp.org", 0, 15 * 60 * 1000);

void setup() {
  Serial.begin(115200);

  bme.begin(0x76);
  display_init();

  WiFi.begin(NETWORKSSID, NETWORKPASSWORD);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println("\nConnected to the WiFi network");
  Serial.print("IP address:");
  Serial.println(WiFi.localIP());

  timeClient.begin();
  
}

void loop() {
  temperatureSum += bme.readTemperature();
  humiditySum += bme.readHumidity();
  pressureSum += bme.readPressure();
  co2Sum += sgp.eCO2;
  freeMemSum += ESP.getFreeHeap();

  count++;

  delay(30000);

  display_sensor_data();

  // Publish data every 5 minutes
  if (count == 10) {
    publish();
    temperatureSum = 0;
    humiditySum = 0;
    pressureSum = 0;
    co2Sum = 0;
    freeMemSum = 0;
    count = 0;
  }
}

void publish() {
  if ((WiFi.status() == WL_CONNECTED)) {
    timeClient.update();

    HTTPClient client;

    client.begin(POSTURL);
    client.setAuthorization(USERNAME, USERPASSWORD);
    client.addHeader("Content-Type", "application/json");

    // 14 is an intentional overestimate, smaller capacities had inconsistent parsing issues
    const size_t CAPACITY = JSON_OBJECT_SIZE(14);
    DynamicJsonDocument doc(CAPACITY);

    formattedDate = timeClient.getFormattedDate();

    doc["temperature"] = roundf((temperatureSum/count)*100.0)/100.0;
    doc["humidity"] = roundf((humiditySum/count)*10.0)/10.0;
    doc["pressure"] = roundf((pressureSum/count/100.0F)*100.0)/100.0;
    doc["co2"] = co2Sum/count;
    doc["freeMem"] = roundf((freeMemSum/count)*100.0)/100.0;
    doc["timestamp"] = formattedDate;

    serializeJson(doc, jsonOutput);

    Serial.println(jsonOutput);

    int httpCode = client.POST(String(jsonOutput));

    if (httpCode > 0) {
      String payload = client.getString();
      Serial.println("\nStatuscode: " + String(httpCode));
      Serial.println(payload);
      client.end();

    } else {
      Serial.printf("Error on HTTP request: %d", httpCode);
    }

  } else {
    Serial.println("Connection lost");
  }

}

void display_init() {
  Serial.println("Initializing SSD1306 OLED");
  display.init();
  display.displayOn();
  display.clear();
  display.setColor(WHITE);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
}

void display_sensor_data() {
  char m[100];
  String data;

  display.clear();
  
  sprintf(m, "Temperature: %s °C", roundf((temperatureSum/count)*100.0)/100.0);
  display.drawString(0, 0, m);
  sprintf(m, "Humidity: %s %", roundf((humiditySum/count)*10.0)/10.0);
  display.drawString(0, 20, m);
  sprintf(m, "Pressure: %s hPa", roundf((pressureSum/count/100.0F)*100.0)/100.0);
  display.drawString(0, 40, m);
  sprintf(m, "CO2: %s ppm", co2Sum/count);
  display.drawString(0, 60, m);
  display.display();

}
