#define CRVENO 21
#define ZUTO 22
#define ZELENO 23

void setup() {
  Serial.begin(115200);
  pinMode(CRVENO, OUTPUT);
  pinMode(ZUTO, OUTPUT);
  pinMode(ZELENO, OUTPUT);
  digitalWrite(CRVENO, LOW);
  digitalWrite(ZUTO, LOW);
  digitalWrite(ZELENO, LOW);
}

void loop() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    if (command == "RGB") {
      digitalWrite(CRVENO, HIGH);
      digitalWrite(ZUTO, HIGH);
      digitalWrite(ZELENO, HIGH);
      Serial.println("SVE UPALJENO");
    } else if (command == "rgb") {
      digitalWrite(CRVENO, LOW);
      digitalWrite(ZUTO, LOW);
      digitalWrite(ZELENO, LOW);
      Serial.println("SVE ISKLJUCENO");
    } else if (command.length() == 1) {
      char singleCommand = command.charAt(0);
      switch (singleCommand) {
        case 'R':
          digitalWrite(CRVENO, HIGH);
          Serial.println("CRVENA LED ON");
          break;
        case 'r':
          digitalWrite(CRVENO, LOW);
          Serial.println("CRVENA LED OFF");
          break;
        case 'Y':
          digitalWrite(ZUTO, HIGH);
          Serial.println("ZUTA LED ON");
          break;
        case 'y':
          digitalWrite(ZUTO, LOW);
          Serial.println("ZUTA LED OFF");
          break;
        case 'G':
          digitalWrite(ZELENO, HIGH);
          Serial.println("ZELENA LED ON");
          break;
        case 'g':
          digitalWrite(ZELENO, LOW);
          Serial.println("ZELENA LED OFF");
          break;
      }
    } else {
      Serial.println("Nepoznata komanda. Koristi R/r, Y/y, G/g, RGB/rgb.");
    }
  }
}
