#define FOTO_PIN 12    
#define LED_PIN 32     
#define POT_PIN 35      
#define DODIR_PIN 33    

const int minimalnoSvjetlo = 10;  
const int maksimalnoSvjetlo = 1000; 

enum Nacin { AUTOMATSKI, RUCNI, ISKLJUCENO };
Nacin trenutniNacin = AUTOMATSKI;

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  touchAttachInterrupt(DODIR_PIN, promijeniNacin, 40);
}

void loop() {
  switch (trenutniNacin) {
    case AUTOMATSKI:
      automatskiNacin();
      break;
    case RUCNI:
      rucniNacin();
      break;
    case ISKLJUCENO:
      iskljuciNacin();
      break;
  }
  delay(100);
}

void automatskiNacin() {
  int vrijednostSenzora = analogRead(FOTO_PIN);
  float napon = vrijednostSenzora * (3.3 / 4095.0); 
  int luks = naponULukse(napon);
  int vrijednostLED = map(constrain(luks, minimalnoSvjetlo, maksimalnoSvjetlo), 
                          minimalnoSvjetlo, maksimalnoSvjetlo, 0, 255);
  analogWrite(LED_PIN, vrijednostLED);
  Serial.print("AUTOMATSKI način - Luksi: ");
  Serial.print(luks);
  Serial.print(", LED intenzitet: ");
  Serial.println(vrijednostLED);
}

void rucniNacin() {
  int vrijednostPOT = analogRead(POT_PIN); 
  int vrijednostLED = map(vrijednostPOT, 0, 4095, 0, 255); 
  analogWrite(LED_PIN, vrijednostLED);
  Serial.print("RUČNI način - Vrijednost potenciometra: ");
  Serial.print(vrijednostPOT);
  Serial.print(", LED intenzitet: ");
  Serial.println(vrijednostLED);
}

void iskljuciNacin() {
  analogWrite(LED_PIN, 0); 
  Serial.println("ISKLJUČENO - LED isključen");
}

void promijeniNacin() {
  trenutniNacin = static_cast<Nacin>((trenutniNacin + 1) % 3); 
  Serial.print("Način promijenjen u: ");
  switch (trenutniNacin) {
    case AUTOMATSKI: Serial.println("AUTOMATSKI"); break;
    case RUCNI: Serial.println("RUČNI"); break;
    case ISKLJUCENO: Serial.println("ISKLJUČENO"); break;
  }
}

int naponULukse(float napon) {
  return napon * 500;
}
