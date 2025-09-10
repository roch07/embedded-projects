#include <SoftwareSerial.h>

SoftwareSerial HC12(9, 8); // RX, TX (D9 = RX, D8 = TX)

void setup() {
  Serial.begin(9600);    // For Serial Monitor
  HC12.begin(9600);      // HC-12 baud rate

  Serial.println("Receiver Ready");
}

void loop() {
  if (HC12.available()) {
  String data = HC12.readStringUntil('\n');
  Serial.println("Received: " + data);
} 
  delay(500);
}

