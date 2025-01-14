#include <Adafruit_DPS310.h>
#include "Seeed_SHT35.h"
#include <Arduino.h>
#include <U8g2lib.h>

#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE); 


const unsigned char SCLPIN = 21;
const unsigned char SHT35_IIC_ADDR = 0x45;
Adafruit_DPS310 dps;
SHT35 sensor(SCLPIN, SHT35_IIC_ADDR);

const int promjenaButtonPin = 15;  
const int resetButtonPin = 12;  
const int tempUnitButtonPin = 14; 
int displayState = 0;      // Trenutno stanje prikaza
bool lastButtonState = HIGH;
bool lastResetButtonState = HIGH;   //Pohrana prethodnog stanja odgovarajućih gumba
bool lastTempUnitButtonState = HIGH;
unsigned long lastDebounceTime = 0;   //Izbjegavanje višestrukog pritiska gumba
const unsigned long debounceDelay = 50;

float minTemp = 100.0;   
float maxTemp = -100.0;  
float sumTemp = 0.0;    
int count = 0;    

float minHum = 100;
float maxHum = 0.0;
float sumHum = 0.0;
float countH = 0;

float minPres = 5000;
float maxPres = 0.0;
float sumPres = 0.0;
float countP = 0;

bool isCelsius = true;  // Početno postavljanje na Celsius

float toFahrenheit(float celsiusTemp) {
  return celsiusTemp * 9.0 / 5.0 + 32;
}

float toCelsius(float fahrenheitTemp) {
  return (fahrenheitTemp - 32) * 5.0 / 9.0;
}

void setup() {
  pinMode(promjenaButtonPin, INPUT_PULLUP);  // Postavljanje gumba kao ulaz s pull-up otpornikom
  pinMode(resetButtonPin, INPUT_PULLUP);  
  pinMode(tempUnitButtonPin, INPUT_PULLUP);  
  Serial.begin(115200);
  while (!Serial) delay(10);

  Serial.println("DPS310");
  if (!dps.begin_I2C()) {             
    Serial.println("Failed to find DPS");
    while (1) yield();
  }
  Serial.println("DPS OK!");

  dps.configurePressure(DPS310_64HZ, DPS310_64SAMPLES);
  dps.configureTemperature(DPS310_64HZ, DPS310_64SAMPLES);

  if (sensor.init())
    Serial.println("Sensor init failed!");
  
  u8g2.begin();
}

void loop() {
  // Debounce logika za gumb za promjenu prikaza
  int reading = digitalRead(promjenaButtonPin);
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading == LOW) {
      displayState = (displayState + 1) % 3;  // Ciklično povećavanje stanja
    }
  }
  lastButtonState = reading;

  // Debounce logika za gumb za resetiranje
  int resetReading = digitalRead(resetButtonPin);
  if (resetReading != lastResetButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (resetReading == LOW) {
      // Resetiranje svih vrijednosti, nastavak na debounce logiku gore
      minTemp = 100.0;
      maxTemp = -100.0;
      sumTemp = 0.0;
      count = 0;    

      minHum = 100;
      maxHum = 0.0;
      sumHum = 0.0;
      countH = 0;

      minPres = 5000;
      maxPres = 0.0;
      sumPres = 0.0;
      countP = 0;
    }
  }
  lastResetButtonState = resetReading;

  // Debounce logika za gumb za promjenu unita
  int tempUnitReading = digitalRead(tempUnitButtonPin);
  if (tempUnitReading != lastTempUnitButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (tempUnitReading == LOW) {
      isCelsius = !isCelsius;  // Prebacivanje između C i F
    }
  }
  lastTempUnitButtonState = tempUnitReading;

  sensors_event_t temp_event, pressure_event;
  float temp = 0.0, hum = 0.0, pressure = 0.0;

  if (dps.temperatureAvailable() && dps.pressureAvailable()) {
    dps.getEvents(&temp_event, &pressure_event);
    temp = temp_event.temperature;
    pressure = pressure_event.pressure;
  }

  // Ako tlak nije dostupan, čekaj malo prije nego proba ponovo(bitno zbog greške)
  if (pressure == 0) {
    delay(100);  
    return;
  }
  
  if (NO_ERROR == sensor.read_meas_data_single_shot(HIGH_REP_WITH_STRCH, &temp, &hum)) {
  }


  if (temp < minTemp) minTemp = temp;
  if (temp > maxTemp) maxTemp = temp;

  if(hum < minHum) minHum = hum;
  if(hum > maxHum) maxHum = hum; 

  if(pressure < minPres) minPres = pressure;
  if(pressure > maxPres) maxPres = pressure;

  sumTemp += temp;
  count++;

  sumHum += hum;
  countH++;

  sumPres += pressure;
  countP++;

  float avgTemp = sumTemp / count;

  float avgHum = sumHum / countH;

  float avgPres = sumPres / countP;

 
  char tempStr[10], humStr[10], pressureStr[10];
  char minTempStr[10], maxTempStr[10], avgTempStr[10];
  char minHumStr[10], maxHumStr[10], avgHumStr[10];
  char minPresStr[10], maxPresStr[10], avgPresStr[10];

  // Ako je potrebno, pretvoriti temperature u Fahrenheit
  float displayTemp = temp;
  float displayMinTemp = minTemp;
  float displayMaxTemp = maxTemp;
  float displayAvgTemp = avgTemp;

  // Ako je potrebno, pretvoriti temperature u Fahrenheit, nastavak
  if (!isCelsius) {
    displayTemp = toFahrenheit(temp);
    displayMinTemp = toFahrenheit(minTemp);
    displayMaxTemp = toFahrenheit(maxTemp);
    displayAvgTemp = toFahrenheit(avgTemp);
  }

  dtostrf(displayTemp, 6, 2, tempStr);
  dtostrf(hum, 6, 2, humStr);
  dtostrf(pressure, 6, 2, pressureStr);
  
  dtostrf(displayMinTemp, 6, 2, minTempStr);
  dtostrf(displayMaxTemp, 6, 2, maxTempStr);
  dtostrf(displayAvgTemp, 6, 2, avgTempStr);

  dtostrf(minHum, 6, 2, minHumStr);
  dtostrf(maxHum, 6, 2, maxHumStr);
  dtostrf(avgHum, 6, 2, avgHumStr);

  dtostrf(minPres, 6, 2, minPresStr);
  dtostrf(maxPres, 6, 2, maxPresStr);
  dtostrf(avgPres, 6, 2, avgPresStr);

  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_ncenB08_tr);

    if (displayState == 0) {
      u8g2.setCursor(0, 10);
      u8g2.print(F("Temperatura: "));
      u8g2.print(tempStr);
      u8g2.print(isCelsius ? F(" C") : F(" F"));
      
      u8g2.setCursor(0, 25);
      u8g2.print(F("Min Temp: "));
      u8g2.print(minTempStr);
      u8g2.print(isCelsius ? F(" C") : F(" F"));

      u8g2.setCursor(0, 35);
      u8g2.print(F("Max Temp: "));
      u8g2.print(maxTempStr);
      u8g2.print(isCelsius ? F(" C") : F(" F"));

      u8g2.setCursor(0, 45);
      u8g2.print(F("Avg Temp: "));
      u8g2.print(avgTempStr);
      u8g2.print(isCelsius ? F(" C") : F(" F"));
    } 
    else if (displayState == 1) {
      u8g2.setCursor(0, 10);
      u8g2.print(F("Vlaznost: "));
      u8g2.print(humStr);
      u8g2.print(F(" %"));

      u8g2.setCursor(0, 25);
      u8g2.print(F("Min: "));
      u8g2.print(minHumStr);
      u8g2.print(F(" %"));

      u8g2.setCursor(0, 35);
      u8g2.print(F("Max: "));
      u8g2.print(maxHumStr);
      u8g2.print(F(" %"));

      u8g2.setCursor(0, 45);
      u8g2.print(F("Avg: "));
      u8g2.print(avgHumStr);
      u8g2.print(F(" %"));

    } 
    else if (displayState == 2) {
      u8g2.setCursor(0, 10);
      u8g2.print(F("Tlak: "));
      u8g2.print(pressureStr);
      u8g2.print(F(" hPa"));

      u8g2.setCursor(0, 25);
      u8g2.print(F("Min: "));
      u8g2.print(minPresStr);
      u8g2.print(F(" hPa"));

      u8g2.setCursor(0, 35);
      u8g2.print(F("Max: "));
      u8g2.print(maxPresStr);
      u8g2.print(F(" hPa"));

      u8g2.setCursor(0, 45);
      u8g2.print(F("Avg: "));
      u8g2.print(avgPresStr);
      u8g2.print(F(" hPa"));

    }
  } while (u8g2.nextPage());

  delay(100);  
}