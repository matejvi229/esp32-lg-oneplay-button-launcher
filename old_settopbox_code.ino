#ifndef LED_BUILTIN
#define LED_BUILTIN 48
#endif

const uint32_t BAUD_RATE = 115200;
const uint32_t BLINK_MS = 500;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(BAUD_RATE);
  delay(300);
  Serial.println("ESP32-S3 test start");
}

void loop() {
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.println("LED ON");
  delay(BLINK_MS);

  digitalWrite(LED_BUILTIN, LOW);
  Serial.println("LED OFF");
  delay(BLINK_MS);
}
