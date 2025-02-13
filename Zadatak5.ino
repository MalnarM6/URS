#include <cmath>
#include <cstdlib>
#include <time.h>
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
#include <ArduinoJson.h>
#include "SI114X.h"
#include "Arduino.h"

#define SPIFFS LittleFS
Preferences preferences;

const char* ssid = "iPhone od: Malnar";
const char* password = "malnar2003";

#define FORMAT_LittleFS_IF_FAILED true
WebServer server(80);

unsigned long lastResetTime = 0;
const unsigned long RESET_INTERVAL = 3600000; // 1 sat u milisekundama 

const unsigned char SCLPIN = 22;
const unsigned char SHT35_IIC_ADDR = 0x45;
SHT35 sensor(SCLPIN, SHT35_IIC_ADDR);

const char* LOGFILE = "/sensor_log.csv";
int sample_count = 0;
int logInterval = 60; 

Dps3xx dps310;
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);
SI114X SI1145 = SI114X();

int senzorDodira = 4;
int vrijednost = 0;

char ssidBuffer[32];
char passBuffer[32];
char wifi_pass[32];

float temp = 0.0, hum = 0.0, pressure = 0.0, uv= 0.0;

float temp_min = 1000.0, temp_max = -1000.0, temp_sum = 0.0;
int temp_count = 0;
float humidity_min = 100.0, humidity_max = 0.0, humidity_sum = 0.0;
int humidity_count = 0;
float pressure_min = 1000000.0, pressure_max = 0.0, pressure_sum = 0.0;
int pressure_count = 0;
float uv_min = 10.0, uv_max = 0.0, uv_sum = 0.0;
int uv_count = 0;

const String www_header = R"(
<!DOCTYPE html>
<html>
<head>
    <title>Veleri-OI-meteo station</title>
    <style>
        body {background-color: white; text-align: center; color: black; font-family: Arial, Helvetica, sans-serif;}
    </style>
</head>
<body>
    <h1>Veleri-OI-meteo station</h1>
    <h1>Matej Malnar</h1>
)";

unsigned long touchStartTime = 0;
bool isTouching = false;

enum State {
  CURRENT_VALUES,
  TEMP_STATS,
  HUMIDITY_STATS,
  PRESSURE_STATS,
  UV_STATS
};

State currentState = CURRENT_VALUES;

const int ledPin = 5; 

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
  pressure_min = 1000000.0; pressure_max = 0.0; pressure_sum = 0.0; pressure_count = 0;
  uv_min = 10.0; uv_max = 0.0; uv_sum = 0.0; uv_count = 0;
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
            currentState = UV_STATS;       
            break;
          case UV_STATS:
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

void handleRoot(){
  const String part1 = "<p>Date and Time: ";
  const String part2 = "</p><p>Temperature: ";
  const String part3 = "&deg;C</p><p>Humidity: ";
  const String part4 = " %</p><p>Pressure: ";
  const String part5 = " hPa</p>";
  const String part6 = "</p><p>UV index: ";
  const String part7 = "<script>setTimeout(function(){ location.reload(); }, 10000);</script></body></html>";
  
  // Dohvat trenutnog vremena
  String currentTime = formatTime();

 
  server.send(200, "text/html", www_header + part1 + currentTime + part2 + String(temp, 1) + "   Min: " + String(temp_min, 1) + "   Max: " + String(temp_max, 1) + "   Avg: " + String(temp_sum / temp_count, 1)
    + part3 + String(hum) + "   Min: " + String(humidity_min, 1) + "   Max: " + String(humidity_max, 1) + "   Avg: "+ String(humidity_sum / humidity_count, 1) 
    + part4 + String(pressure / 100.0, 2) + "   Min: " + String(pressure_min / 100.0, 2) + "   Max: " + String(pressure_max / 100.0, 2) + "   Avg: "+ String(pressure_sum / pressure_count / 100.0, 2)
    + part5 + part6 + String(uv, 1) + "   Min: " + String(uv_min, 1) + "   Max: " + String(uv_max, 1) + "   Avg: " + String(uv_sum / uv_count, 1) + part7);
}

// www log/ page
void handleLog() {
 File file = SPIFFS.open(LOGFILE);
 if (!file || file.isDirectory()) {
 server.send(500, "text/html", "Error reading logfile.");
 return;
 }
 String buffer;
 while (file.available()) {
 char next = file.read();
 buffer += next;
 }
 server.send(200, "text/plain", buffer);
 file.close();
}

void initTime(){
  const char* ntpServer = "pool.ntp.org";
  
  const long UTCOffset_sec = 3600;  // UTC+1, 1 sat offset
  const int dstOffset_sec = 3600;   // Ljetno računanje vremena, dodatni 1 sat

  configTime(UTCOffset_sec, dstOffset_sec, ntpServer);
}

String formatTime() {

tm timeinfo;

 if(!getLocalTime(&timeinfo)){
 return String("xxxx-xx-xx,xx:xx:xx");
 }
 return String(timeinfo.tm_year+1900) + "-" +
 String(timeinfo.tm_mon+1) + "-" +
 String(timeinfo.tm_mday) + "," +
 String(timeinfo.tm_hour) + ":" +
 String(timeinfo.tm_min) + ":" +
 String(timeinfo.tm_sec);
}

// Rest API handler functions
void handleCurrentValues() {
  String response = "{";
  response += "\"temperature\": " + String(temp, 1) + ",";
  response += "\"humidity\": " + String(hum, 1) + ",";
  response += "\"pressure\": " + String(pressure / 100.0, 2);
  response += "\"uv\": " + String(uv, 1);
  response += "}";

  server.send(200, "application/json", response);
}

void handleStats() {
  String response = "{";
  response += "\"temperature\": {";
  response += "\"min\": " + String(temp_min, 1) + ",";
  response += "\"max\": " + String(temp_max, 1) + ",";
  response += "\"avg\": " + (temp_count > 0 ? String(temp_sum / temp_count, 1) : "null");
  response += "},";
  
  response += "\"humidity\": {";
  response += "\"min\": " + String(humidity_min, 1) + ",";
  response += "\"max\": " + String(humidity_max, 1) + ",";
  response += "\"avg\": " + (humidity_count > 0 ? String(humidity_sum / humidity_count, 1) : "null");
  response += "},";
  
  response += "\"pressure\": {";
  response += "\"min\": " + String(pressure_min / 100.0, 2) + ",";
  response += "\"max\": " + String(pressure_max / 100.0, 2) + ",";
  response += "\"avg\": " + (pressure_count > 0 ? String(pressure_sum / pressure_count / 100.0, 2) : "null");
  response += "}";

  response += "\"uv\": {";
  response += "\"min\": " + String(uv_min, 1) + ",";
  response += "\"max\": " + String(uv_max, 1) + ",";
  response += "\"avg\": " + (uv_count > 0 ? String(uv_sum / uv_count, 1) : "null");
  response += "}";

  
  response += "}";

  server.send(200, "application/json", response);
}

void handleSetAlarmThreshold() {
  if (server.hasArg("plain")) {
    String body = server.arg("plain");
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, body);

    if (error) {
      server.send(400, "application/json", "{\"error\": \"Invalid JSON format\"}");
      return;
    }

    float highThreshold = doc["high"];
    float lowThreshold = doc["low"];

    if (highThreshold > 0 && lowThreshold > 0) {
      saveAlarmThreshold(highThreshold, lowThreshold);
      server.send(200, "application/json", "{\"status\": \"Thresholds set successfully\"}");
    } else {
      server.send(400, "application/json", "{\"error\": \"Invalid thresholds\"}");
    }
  } else {
    server.send(400, "application/json", "{\"error\": \"No body found\"}");
  }
}

void handleGetAlarmThreshold() {
  float highThreshold, lowThreshold;
  loadAlarmThreshold(highThreshold, lowThreshold);

  String response = "{";
  response += "\"high\": " + String(highThreshold) + ",";
  response += "\"low\": " + String(lowThreshold);
  response += "}";

  server.send(200, "application/json", response);
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

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to WiFi");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    initTime();
  } else {
    WiFi.disconnect();
    Serial.println("Wifi connection failed, continuing without wifi.");
 }

  if (!LittleFS.begin(FORMAT_LittleFS_IF_FAILED)) {
      Serial.println("LittleFS Mount Failed");
      return;
  }

  pinMode(ledPin, OUTPUT); 

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

      while (!SI1145.Begin()) {
        Serial.println("Si1145 is not ready!");
        delay(1000);
    }
    Serial.println("Si1145 is ready!");

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
  
  logInterval = preferences.getInt("logInterval", 60); 
  Serial.print("Log interval loaded: ");
  Serial.println(logInterval);

  server.on("/api/current_values", HTTP_GET, handleCurrentValues);
  server.on("/api/stats", HTTP_GET, handleStats);
  server.on("/api/set_alarm_threshold", HTTP_POST, handleSetAlarmThreshold);
  server.on("/api/get_alarm_threshold", HTTP_GET, handleGetAlarmThreshold);
  server.on("/download_csv", HTTP_GET, []() {
    String csvData = "Date,Time,Temperature,Humidity,Pressure,UVIndex\n";
    String currentTime = formatTime();
    csvData += currentTime + "," + String(temp) + "," + String(hum) + "," + String(pressure / 100.0) + "," + String(uv) + "\n";
    
    server.send(200, "text/csv", csvData);
  });



  delay(1000);
}

void logHighTempAlarm(const char *message) {
    File file = LittleFS.open("/alarmH_log.txt", FILE_APPEND);  
    if (!file) {
        Serial.println("Failed to open high temp alarm log file for writing!");
        return;
    }
    file.println(message);  
    file.close();
}

void logLowTempAlarm(const char *message) {
    File file = LittleFS.open("/alarmL_log.txt", FILE_APPEND);  
    if (!file) {
        Serial.println("Failed to open low temp alarm log file for writing!");
        return;
    }
    file.println(message);  
    file.close();
}

void saveAlarmThreshold(float thresholdH, float thresholdL) {
  File file = LittleFS.open("/alarm_threshold.txt", FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open alarm threshold file for writing");
    return;
  }
  file.println(thresholdH);  
  file.println(thresholdL);
  file.close();
}

void loadAlarmThreshold(float &thresholdH, float &thresholdL) {
  File file = LittleFS.open("/alarm_threshold.txt", FILE_READ);
  if (!file) {
    Serial.println("Failed to open alarm threshold file for reading, using default values");
    thresholdH = 37.0;  
    thresholdL = 10.0;
    return;
  }
  thresholdH = file.parseFloat();  
  thresholdL = file.parseFloat();
  file.close();
}

void displayHighTempAlarmLog() {
    File file = LittleFS.open("/alarmH_log.txt", FILE_READ);
    if (!file) {
        Serial.println("No high temp alarm log found.");
        return;
    }
    
    Serial.println("High Temp Alarm Log");
    while (file.available()) {
        Serial.write(file.read());  
    }
    file.close();
}

void displayLowTempAlarmLog() {
    File file = LittleFS.open("/alarmL_log.txt", FILE_READ);
    if (!file) {
        Serial.println("No low temp alarm log found.");
        return;
    }
    
    Serial.println("Low Temp Alarm Log");
    while (file.available()) {
        Serial.write(file.read());  
    }
    file.close();
}

void handleHighTempThreshold(float newThresholdH) {
    if (newThresholdH > 0) {
        Serial.print("Setting new HIGH alarm threshold to: ");
        Serial.println(newThresholdH);
        float tempH, tempL;
        loadAlarmThreshold(tempH, tempL); 
        saveAlarmThreshold(newThresholdH, tempL); 
    } else {
        Serial.println("Invalid temperature value!");
    }
}

void handleLowTempThreshold(float newThresholdL) {
    if (newThresholdL > 0) {
        Serial.print("Setting new LOW alarm threshold to: ");
        Serial.println(newThresholdL);
        float tempH, tempL;
        loadAlarmThreshold(tempH, tempL);
        saveAlarmThreshold(tempH, newThresholdL); 
    } else {
        Serial.println("Invalid temperature value!");
    }
}

void handleSerialCommands() {
    while (Serial.available()) {
        String incomingRow = Serial.readStringUntil('\n');
        Serial.println("Command: " + incomingRow);

        if (incomingRow.equals("setSSID")) {
            Serial.println("Setting new SSID...");
            String newSSID = Serial.readStringUntil('\n');
            preferences.putString("ssid", newSSID);
            strncpy(ssidBuffer, newSSID.c_str(), sizeof(ssidBuffer));
            ssidBuffer[sizeof(ssidBuffer) - 1] = '\0'; 
            ssid = ssidBuffer;
        } else if (incomingRow.equals("setPass")) {
            Serial.println("Setting new passphrase...");
            String newPass = Serial.readStringUntil('\n');
            preferences.putString("passphrase", newPass);
            strncpy(passBuffer, newPass.c_str(), sizeof(passBuffer));
            passBuffer[sizeof(passBuffer) - 1] = '\0';
            strncpy(wifi_pass, passBuffer, sizeof(wifi_pass) - 1);  
            wifi_pass[sizeof(wifi_pass) - 1] = '\0'; 
        } else if (incomingRow.startsWith("setTempH")) {
            float newThresholdH = incomingRow.substring(8).toFloat();  
            handleHighTempThreshold(newThresholdH);
        } else if (incomingRow.startsWith("setTempL")) {
            float newThresholdL = incomingRow.substring(8).toFloat();  
            handleLowTempThreshold(newThresholdL);
        } else if (incomingRow.equals("showHighAlarmLog")) {
            displayHighTempAlarmLog();  
        } else if (incomingRow.equals("showLowAlarmLog")) {
            displayLowTempAlarmLog();  
        } else if (incomingRow.equals("resetFS")) {
            Serial.println("Resetiranje memorije...");
            resetLittleFS(); 
        } else if (incomingRow.equals("LogFile")) {
            displayFile(LittleFS, LOGFILE);
        } else if (incomingRow.startsWith("setLogInterval")) {
            int newInterval = incomingRow.substring(14).toInt();
            if (newInterval > 0) {
                logInterval = newInterval;
                preferences.putInt("logInterval", logInterval); 
                Serial.print("New logging interval set to: ");
                Serial.println(logInterval);
            } 
        }
          if (incomingRow.equalsIgnoreCase("whatismyIP")) {
            if (WiFi.status() == WL_CONNECTED) {
                Serial.print("IP address: ");
                Serial.println(WiFi.localIP());
            } else {
                Serial.println("Not connected to WiFi.");
            }
    }
            else if (incomingRow.equalsIgnoreCase("help")) {
            Serial.println("Commands:");
            Serial.println(" - whatismyip: Show current IP address.");
            Serial.println(" - help: List all available commands.");
            Serial.println(" - setTempH <value>: Set HIGH alarm threshold.");
            Serial.println(" - setTempL <value>: Set LOW alarm threshold.");
            Serial.println(" - setSSID change SSID of network.");
            Serial.println(" - setPass change password of network.");
            Serial.println(" - showHighAlarmLog alarm log file for HIGH temperatures");
            Serial.println(" - showLowAlarmLog alarm log file for LOW temperatures");
            Serial.println(" - resetFS format memory");
            Serial.println(" - setLogInterval change data log interval");
            Serial.println(" - LogFile print Stats log on serial monitor");
            Serial.println(" - helpRESTAPI shows commands for REST API");

        } 
            else if (incomingRow.equalsIgnoreCase("helpRESTAPI")) {
            Serial.println("Commands:");
            Serial.println(" - <IP ADDRESS> - All values (Homepage)");
            Serial.println(" - <IP ADDRESS>/api/current_values - prints current values");
            Serial.println(" - <IP ADDRESS>/api/stats - prints readed stats");
            Serial.println(" - <IP ADDRESS>/api/get_alarm_threshold - prints current alarm threshold");
            Serial.println(" - <IP ADDRESS>/api/set_alarm_threshold  - POST metod, changing alarm threshold values used format is: { high: <FLOAT>, low: <FLOAT>}");
            Serial.println("POST metod high and low should be in quotes");
            Serial.println(" - <IP ADDRESS>/download_csv downloading file with all logged values");



        } 

    }
}

void loop() {
    float alarmThresholdH, alarmThresholdL;
    loadAlarmThreshold(alarmThresholdH, alarmThresholdL);

    handleTouch();
    server.handleClient();
    handleSerialCommands();

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

    uv = SI1145.ReadUV();
    uv /= 100.0;
    updateStats(uv, uv_min, uv_max, uv_sum, uv_count);

    bool alarmTriggered = false;

    if (temp >= alarmThresholdH) {
        String alarmMessage = "ALARM: Temperature above " + String(temp) + " C!";
        logHighTempAlarm(alarmMessage.c_str()); 
        Serial.println(alarmMessage);    
        alarmTriggered = true;
    }

    if (temp <= alarmThresholdL) {
        String alarmMessage = "ALARM: Temperature below " + String(temp) + " C!";
        logLowTempAlarm(alarmMessage.c_str()); 
        Serial.println(alarmMessage);    
        alarmTriggered = true;
    }

    if (alarmTriggered) {
        digitalWrite(ledPin, HIGH); 
    } else {
        digitalWrite(ledPin, LOW); 
    }

    u8g2.clearBuffer();

    switch (currentState) {
        case CURRENT_VALUES:
            if (!isnan(temp)) u8g2.drawStr(0, 10, ("Temp: " + String(temp, 1) + " C").c_str());
            if (!isnan(hum)) u8g2.drawStr(0, 28, ("Humidity: " + String(hum, 1) + " %").c_str());
            if (!isnan(pressure)) u8g2.drawStr(0, 46, ("Pressure: " + String(pressure / 100.0, 2) + " hPa").c_str());
            if (!isnan(uv)) u8g2.drawStr(0, 64, ("UV Index: " + String(uv, 1)).c_str());
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

        case UV_STATS:
            u8g2.drawStr(0, 10, ("UV Min: " + String(uv_min, 1)).c_str());
            u8g2.drawStr(0, 28, ("UV Max: " + String(uv_max, 1)).c_str());
            u8g2.drawStr(0, 46, ("UV Avg: " + (uv_count > 0 ? String(uv_sum / uv_count, 1) : "N/A")).c_str());
            break;
    }

    u8g2.sendBuffer();

    if (millis() - lastResetTime >= RESET_INTERVAL) {
        Serial.println("Reseting stats");
        resetStats();
        lastResetTime = millis();
    }

    sample_count++;


    if (sample_count >= logInterval) {
    String msg = formatTime() + " " + String(temp) + ", " + String(hum) + ", " + String(pressure / 100.0) + "\r\n";
    appendFile(LittleFS, LOGFILE, msg.c_str());
    Serial.println("Logged data: " + msg);
    sample_count = 0;  
}
}

