const uint32_t BAUD_RATE = 115200;
const int LED_PIN = 5;
const int BUTTON_PIN = 6;
const uint32_t DEBOUNCE_MS = 35;

bool ledOn = false;
bool lastReading = HIGH;
bool stableState = HIGH;
uint32_t lastDebounceMs = 0;

void setLed(bool on) {
  ledOn = on;
  digitalWrite(LED_PIN, on ? HIGH : LOW);
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  setLed(false);

  Serial.begin(BAUD_RATE);
  delay(300);
  Serial.println("ESP32-S3 simple button+LED test");
  Serial.println("Button IO6 -> GND, LED IO5");
}

void loop() {
  bool reading = digitalRead(BUTTON_PIN);

  if (reading != lastReading) {
    lastDebounceMs = millis();
    lastReading = reading;
  }

  if ((millis() - lastDebounceMs) > DEBOUNCE_MS && stableState != reading) {
    stableState = reading;
    if (stableState == LOW) {
      setLed(!ledOn);
      Serial.println(ledOn ? "LED ON" : "LED OFF");
    }
  }

  delay(2);
}
