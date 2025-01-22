#include <WiFi.h>
#include <WebServer.h>
#include "SPIFFS.h"

const char* ssid = "Malnar";         // Replace with your network SSID (name)
const char* password = "malnar2003"; // Replace with your network password

#define FORMAT_SPIFFS_IF_FAILED false   // Define the macro here

WebServer server(80);
const char* LOGFILE = "/sensor_log.csv"; // Define your log file name

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
    Serial.begin(115200); // Start the Serial communication
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
}

void loop() {
    server.handleClient();
}
