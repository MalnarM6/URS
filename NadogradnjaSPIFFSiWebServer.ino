#include "Seeed_SHT35.h"
#include <U8g2lib.h>
#include <Wire.h>
#include <Dps3xx.h>
#include "FS.h"
#include "SPIFFS.h"
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>

Preferences preferences;

const char* ssid = "Malnar";
const char* password = "malnar2003";

#define FORMAT_SPIFFS_IF_FAILED false 
WebServer server(80);


const unsigned char SCLPIN = 22;
const unsigned char SHT35_IIC_ADDR = 0x45;
SHT35 sensor(SCLPIN, SHT35_IIC_ADDR);

const char* LOGFILE = "/sensor_log.csv";
int sample_count = 0;
const int LOG_EVERY = 60;

Dps3xx dps310;
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);

int senzorDodira = 4;
int vrijednost = 0;

float temp_min = 1000.0, temp_max = -1000.0, temp_sum = 0.0;
int temp_count = 0;
float humidity_min = 100.0, humidity_max = 0.0, humidity_sum = 0.0;
int humidity_count = 0;
float pressure_min = 1000.0, pressure_max = 0.0, pressure_sum = 0.0;
int pressure_count = 0;

unsigned long touchStartTime = 0;
bool isTouching = false;

enum State {
  CURRENT_VALUES,
  TEMP_STATS,
  HUMIDITY_STATS,
  PRESSURE_STATS
};

State currentState = CURRENT_VALUES;

void appendFile(fs::FS &fs, const char * path, const char * message) {
  File file = fs.open(path, FILE_APPEND);
  if (!file) {
    Serial.print("File ");
    Serial.print(path);
    Serial.println(" not found. Attempting to create new file.");
    file = fs.open(path, FILE_WRITE);
    if (!file) {
      Serial.print("Unable to create file ");
      Serial.println(path);
      return;
    }
    Serial.println("File created.");
  }
  if (!file.print(message)) {
    Serial.print("Failed to append to file ");
    Serial.println(path);
  }
  file.close();
}


void updateStats(float value, float &minVal, float &maxVal, float &sum, int &count) {
  if (value < minVal) minVal = value;
  if (value > maxVal) maxVal = value;
  sum += value;
  count++;
}

void resetStats() {
  temp_min = 1000.0; temp_max = -1000.0; temp_sum = 0.0; temp_count = 0;
  humidity_min = 100.0; humidity_max = 0.0; humidity_sum = 0.0; humidity_count = 0;
  pressure_min = 1000.0; pressure_max = 0.0; pressure_sum = 0.0; pressure_count = 0;
}

void handleTouch() {
  vrijednost = touchRead(senzorDodira);
  Serial.println(vrijednost);

  if (vrijednost < 40) {
    if (!isTouching) {
      touchStartTime = millis();
      isTouching = true;
    } else {
      unsigned long touchDuration = millis() - touchStartTime;
      if (touchDuration >= 5000) {
        resetStats();
        currentState = CURRENT_VALUES;
        touchStartTime = millis();
      } else if (touchDuration >= 2000) {
        switch (currentState) {
          case CURRENT_VALUES:
            currentState = TEMP_STATS;
            break;
          case TEMP_STATS:
            currentState = HUMIDITY_STATS;
            break;
          case HUMIDITY_STATS:
            currentState = PRESSURE_STATS;
            break;
          case PRESSURE_STATS:
            currentState = CURRENT_VALUES;
            break;
        }
        touchStartTime = millis();
      }
    }
  } else {
    isTouching = false;
  }
}

void displayFile(fs::FS &fs, const char * path) {
  Serial.printf("Reading file: %s\r\n", path);
  File file = fs.open(path);
  if (!file || file.isDirectory()) {
    Serial.println("Failed to open file for reading!");
    return;
  }
  while (file.available()) {
    Serial.write(file.read());
  }
  file.close();
}


void handleRoot() {
    File file = SPIFFS.open(LOGFILE, FILE_READ);
    if (!file) {
        server.send(500, "text/plain", "Failed to open file");
        return;
    }

    server.setContentLength(file.size());
    server.send(200, "text/csv", "");
    while (file.available()) {
        server.client().write(file.read());
    }
    file.close();
}

void setup() {
  Wire.begin();
  u8g2.begin();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  unsigned long startAttemptTime = millis();

  // Keep trying to connect for 30 seconds
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 30000) {
      delay(500);
      Serial.print(".");
  }

  if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Failed to connect to WiFi");
      return; // Stop the setup if unable to connect
  }

  Serial.println();
  Serial.println("Connected to WiFi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP()); // Print the IP address

  if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)) {
      Serial.println("SPIFFS Mount Failed");
      return;
  }

  server.on("/", handleRoot);
  server.begin();
  Serial.println("Server started");

  displayFile(SPIFFS, LOGFILE);

  if (sensor.init()) {
    Serial.println("SHT35 Sensor init failed!");
  } else {
    Serial.println("SHT35 Sensor init successful.");
  }

  dps310.begin(Wire);
  Serial.println("DPS310 Sensor initialized.");

  Serial.println("I2C Scan beginning.");
  for (byte address = 1; address < 128; address++) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      Serial.print("I2C device found at address 0x");
      Serial.println(address, HEX);
    }
  }
  Serial.println("I2C Scan done.");
  delay(1000);
}


void loop() {
  float temp, hum, pressure;

  handleTouch();

  server.handleClient();

  if (NO_ERROR == sensor.read_meas_data_single_shot(HIGH_REP_WITH_STRCH, &temp, &hum)) {
    updateStats(temp, temp_min, temp_max, temp_sum, temp_count);
    updateStats(hum, humidity_min, humidity_max, humidity_sum, humidity_count);
  } else {
    Serial.println("Failed to read from SHT35 sensor!");
    temp = hum = NAN;
  }

  if (dps310.measurePressureOnce(pressure, 7) == 0) {
    updateStats(pressure, pressure_min, pressure_max, pressure_sum, pressure_count);
  } else {
    Serial.println("Failed to read from DPS310 sensor!");
    pressure = NAN;
  }

  u8g2.clearBuffer();

  switch (currentState) {
    case CURRENT_VALUES:
      if (!isnan(temp)) u8g2.drawStr(0, 10, ("Temp: " + String(temp, 1) + " C").c_str());
      if (!isnan(hum)) u8g2.drawStr(0, 28, ("Humidity: " + String(hum, 1) + " %").c_str());
      if (!isnan(pressure)) u8g2.drawStr(0, 46, ("Pressure: " + String(pressure / 100.0, 2) + " hPa").c_str());
      break;
    case TEMP_STATS:
      u8g2.drawStr(0, 10, ("Temp: " + String(temp, 1) + " C").c_str());
      u8g2.drawStr(0, 28, ("Min: " + String(temp_min, 1) + " C").c_str());
      u8g2.drawStr(0, 46, ("Max: " + String(temp_max, 1) + " C").c_str());
      u8g2.drawStr(0, 56, ("Avg: " + String(temp_sum / temp_count, 1) + " C").c_str());
      break;
    case HUMIDITY_STATS:
      u8g2.drawStr(0, 10, ("Humidity: " + String(hum, 1) + " %").c_str());
      u8g2.drawStr(0, 28, ("Min: " + String(humidity_min, 1) + " %").c_str());
      u8g2.drawStr(0, 46, ("Max: " + String(humidity_max, 1) + " %").c_str());
      u8g2.drawStr(0, 56, ("Avg: " + String(humidity_sum / humidity_count, 1) + " %").c_str());
      break;
    case PRESSURE_STATS:
      u8g2.drawStr(0, 10, ("Pressure: " + String(pressure / 100.0, 2) + " hPa").c_str());
      u8g2.drawStr(0, 28, ("Min: " + String(pressure_min / 100.0, 2) + " hPa").c_str());
      u8g2.drawStr(0, 46, ("Max: " + String(pressure_max / 100.0, 2) + " hPa").c_str());
      u8g2.drawStr(0, 56, ("Avg: " + String(pressure_sum / pressure_count / 100.0, 2) + " hPa").c_str());
      break;
  }

  sample_count++;
  u8g2.sendBuffer();
  delay(1000);

  if (sample_count == LOG_EVERY) {
    String msg = String(temp) + ", " + String(hum) + ", " + String(pressure / 100.0, 2) + "\r\n";
    appendFile(SPIFFS, LOGFILE, msg.c_str());
    sample_count = 0;
  }

  if (Serial.available()) {
    String incomingRow = Serial.readString();
    if (incomingRow.equals("dumpLogFile")) {
      displayFile(SPIFFS, LOGFILE);
    }
  }

  delay(1000);
}
