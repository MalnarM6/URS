#include "FS.h"
#include <LittleFS.h>
#include "Seeed_SHT35.h"
#include <U8g2lib.h>
#include <Wire.h>
#include <Dps3xx.h>
#include "FS.h"
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>

#define SPIFFS LittleFS
Preferences preferences;

const char* ssid = "Malnar";
const char* password = "Malnar*2024";

#define FORMAT_LittleFS_IF_FAILED true
WebServer server(80);

unsigned long lastResetTime = 0;
const unsigned long RESET_INTERVAL = 3600000; // 1 sat u milisekundama 

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

// Funkcija za dohvat ID-a uređaja
String getDeviceId() {
  String deviceId = preferences.getString("device_id", "");
  if (deviceId == "") {
    // Ako ID nije pohranjen, generiraj ga i pohrani
    deviceId = String(WiFi.macAddress());
    preferences.putString("device_id", deviceId);
  }
  return deviceId;
}

void resetLittleFS() {
    if (LittleFS.begin()) {
        LittleFS.format();  
        Serial.println("Memory reset.");
    } else {
        Serial.println("Failed to initialize LittleFS.");
    }
}


void handleRoot() {
    File file = LittleFS.open(LOGFILE, FILE_READ);
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

 //30 sec timer za povezivanje
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 30000) {
      delay(500);
      Serial.print(".");
  }

  if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Failed to connect to WiFi");
  }

  Serial.println();
  Serial.println("Connected to WiFi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP()); 

  if (!LittleFS.begin(FORMAT_LittleFS_IF_FAILED)) {
      Serial.println("LittleFS Mount Failed");
      return;
  }

  server.on("/", handleRoot);
  server.begin();
  Serial.println("Server started");

  displayFile(LittleFS, LOGFILE);

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


    String deviceId = getDeviceId();
    Serial.print("Device ID: ");
    Serial.println(deviceId);  

  delay(1000);
}

void logAlarm(const char *message) {
    File file = LittleFS.open("/alarm_log.txt", FILE_APPEND);  
    if (!file) {
        Serial.println("Failed to open alarm log file for writing!");
        return;
    }
    file.println(message);  
    file.close();
}

void saveAlarmThreshold(float threshold) {
  File file = LittleFS.open("/alarm_threshold.txt", FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open alarm threshold file for writing");
    return;
  }
  file.println(threshold);  
  file.close();
}

float loadAlarmThreshold() {
  File file = LittleFS.open("/alarm_threshold.txt", FILE_READ);
  if (!file) {
    Serial.println("Failed to open alarm threshold file for reading, using default value");
    return 37.0;  
  }
  float threshold = file.parseFloat();  
  file.close();
  return threshold;
}

void displayAlarmLog() {
    File file = LittleFS.open("/alarm_log.txt", FILE_READ);
    if (!file) {
        Serial.println("No alarm log found.");
        return;
    }
    
    Serial.println("Alarm Log");
    while (file.available()) {
        Serial.write(file.read());  
    }
    file.close();
}


void loop() {

    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');  


        if (input.startsWith("setTemp")) {
            float newThreshold = input.substring(7).toFloat();  
            if (newThreshold > 0) {
                Serial.print("Setting new alarm threshold to: ");
                Serial.println(newThreshold);
                saveAlarmThreshold(newThreshold); 
            } else {
                Serial.println("Invalid temperature value!");
            }
        }
        else if (input.startsWith("showAlarmLog")) {
        displayAlarmLog();  
       }
        else if (input.startsWith("resetFS")) {
            Serial.println("Resetiranje memorije");
            resetLittleFS(); 
        }
        else if (input.startsWith("LogFile")) {
            displayFile(LittleFS, LOGFILE);
        }

    }

    float temp, hum, pressure;
     float alarmThreshold = loadAlarmThreshold();

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

            if (temp >= alarmThreshold) {
            String alarmMessage = "ALARM: Temperature reached " + String(temp) + " C!";
            logAlarm(alarmMessage.c_str()); 
            Serial.println(alarmMessage);    
        }

    u8g2.clearBuffer();

    switch (currentState) {
        case CURRENT_VALUES:
            if (!isnan(temp)) u8g2.drawStr(0, 10, ("Temp: " + String(temp, 1) + " C").c_str());
            if (!isnan(hum)) u8g2.drawStr(0, 28, ("Humidity: " + String(hum, 1) + " %").c_str());
            if (!isnan(pressure)) u8g2.drawStr(0, 46, ("Pressure: " + String(pressure / 100.0, 2) + " hPa").c_str());
            break;
        case TEMP_STATS:
            u8g2.drawStr(0, 10, ("Temp Min: " + String(temp_min, 1) + " C").c_str());
            u8g2.drawStr(0, 28, ("Temp Max: " + String(temp_max, 1) + " C").c_str());
            u8g2.drawStr(0, 46, ("Temp Avg: " + (temp_count > 0 ? String(temp_sum / temp_count, 1) : "N/A") + " C").c_str());
            break;
        case HUMIDITY_STATS:
            u8g2.drawStr(0, 10, ("Hum Min: " + String(humidity_min, 1) + " %").c_str());
            u8g2.drawStr(0, 28, ("Hum Max: " + String(humidity_max, 1) + " %").c_str());
            u8g2.drawStr(0, 46, ("Hum Avg: " + (humidity_count > 0 ? String(humidity_sum / humidity_count, 1) : "N/A") + " %").c_str());
            break;
        case PRESSURE_STATS:
            u8g2.drawStr(0, 10, ("Press Min: " + String(pressure_min / 100.0, 2) + " hPa").c_str());
            u8g2.drawStr(0, 28, ("Press Max: " + String(pressure_max / 100.0, 2) + " hPa").c_str());
            u8g2.drawStr(0, 46, ("Press Avg: " + (pressure_count > 0 ? String(pressure_sum / pressure_count / 100.0, 2) : "N/A") + " hPa").c_str());
            break;
    }

    u8g2.sendBuffer();

    if (millis() - lastResetTime >= RESET_INTERVAL) {
        Serial.println("Reset statistike.");
        resetStats();
        lastResetTime = millis();
    }

    sample_count++;


    if (sample_count == LOG_EVERY) {
        String msg = String(temp) + ", " + String(hum) + ", " + String(pressure / 100.0, 2) + "\r\n";
        appendFile(LittleFS, LOGFILE, msg.c_str());
        sample_count = 0;
    }


    delay(1000);
}
