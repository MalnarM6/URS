#include "Seeed_SHT35.h"
#include <U8g2lib.h>
#include <Wire.h>
// Filesystem related headers:
#include "FS.h"
#include "SPIFFS.h"
#define FORMAT_SPIFFS_IF_FAILED false // set true on new flash, one-time only
const int SCLPIN = 22;
SHT35 sensor(SCLPIN);
const char* LOGFILE = "/sensor_log.csv";
// Log every 60th sample:
int sample_count = 0;
const int LOG_EVERY = 60;
// Init display:
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);
/*
Appends /message/ to file /path/ on filesystem /fs/, creating if necessary.
*/
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

/*
Reads file /path/ on filesystem /fs/ and sends it over serial port
and displays on screen.
*/
void displayFile(fs::FS &fs, U8G2& disp, const char * path) {
Serial.printf("Reading file: %s\r\n", path);
File file = fs.open(path);
if (!file || file.isDirectory()) {
Serial.println("Failed to open file for reading!");
return;
}
String buffer;
while (file.available()) {
char next = file.read();
buffer += next;
if (next == '\n'){
Serial.print(buffer);
disp.clearBuffer();
disp.setFont(u8g2_font_profont12_tr);
disp.drawStr(0, 10, "Log file:");
disp.drawStr(0, 28, buffer.c_str());
disp.sendBuffer();
buffer = "";
}
}
file.close();
}

void setup()
{
Serial.begin(115200);
u8g2.begin();
u8g2.setFont(u8g2_font_profont12_tr);
if (sensor.init())
Serial.println("Sensor init failed!");
if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)) {
Serial.println("SPIFFS Mount Failed");
return;
}
delay(1000);
displayFile(SPIFFS, u8g2, LOGFILE);
}

void loop()
{
float temp, hum;
if (NO_ERROR == sensor.read_meas_data_single_shot(
HIGH_REP_WITH_STRCH,
&temp,
&hum)
) {
sample_count++;
u8g2.clearBuffer();
u8g2.setFont(u8g2_font_profont12_tr);
String msg = "Temperature: " + String(temp) + " °C ";
u8g2.drawStr(0, 10, msg.c_str());
Serial.println(msg);
msg = "Humidity: " + String(hum) + " % ";
u8g2.drawStr(0, 28, msg.c_str());
Serial.println(msg);
u8g2.sendBuffer();
if (sample_count == LOG_EVERY){
// Format values for logfile:
msg = String(temp) + ", " + String(hum) + "\r\n";
appendFile(SPIFFS, LOGFILE, msg.c_str());
sample_count = 0;
}
} else {
Serial.println("Sensor read failed!");
Serial.println("");
}

if (Serial.available()){
String incomingRow = Serial.readString();
if (incomingRow.equals("dumpLogFile")){
displayFile(SPIFFS, u8g2, LOGFILE);
}
}
delay(1000);
}
