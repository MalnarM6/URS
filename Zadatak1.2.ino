#define LED_PIN 21
#define DOT_DURATION 200
#define DASH_DURATION 600
#define SYMBOL_PAUSE 200
#define LETTER_PAUSE 600
#define WORD_PAUSE 1200

const char *morseCode[] = {
  ".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..", ".---",
  "-.-", ".-..", "--", "-.", "---", ".--.", "--.-", ".-.", "...", "-",
  "..-", "...-", ".--", "-..-", "-.--", "--..",
  "-----", ".----", "..---", "...--", "....-", ".....", "-....", "--...", "---..", "----."
};

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.println("Unesite poruku za Morseov kod:");
}

void loop() {
  if (Serial.available()) {
    String message = Serial.readStringUntil('\n');
    message.trim();

    Serial.println("Primljena poruka: " + message);
    sendMorse(message);
  }
}

void sendMorse(String message) {
  for (char c : message) {
    if (c == ' ') {
      delay(WORD_PAUSE);
    } else {
      c = toupper(c);
      if (c >= 'A' && c <= 'Z') {
        sendMorseSymbol(morseCode[c - 'A']);
      } else if (c >= '0' && c <= '9') {
        sendMorseSymbol(morseCode[c - '0' + 26]);
      }
      delay(LETTER_PAUSE);
    }
  }
}

void sendMorseSymbol(const char *symbol) {
  while (*symbol) {
    if (*symbol == '.') {
      digitalWrite(LED_PIN, HIGH);
      delay(DOT_DURATION);
    } else if (*symbol == '-') {
      digitalWrite(LED_PIN, HIGH);
      delay(DASH_DURATION);
    }
    digitalWrite(LED_PIN, LOW);
    delay(SYMBOL_PAUSE);
    symbol++;
  }
}
