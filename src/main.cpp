#include <Arduino.h>

void setup() {
  pinMode(8,  OUTPUT); // Blue
  pinMode(9,  OUTPUT); // Blue
  pinMode(10, OUTPUT); // Red
}

void loop() {
  for (int i = 8; i <= 10; i++) {
    digitalWrite(i, HIGH);
    delay(200);
    digitalWrite(i, LOW);
  }
}
