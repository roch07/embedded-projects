#include <SoftwareSerial.h>

SoftwareSerial HC12(9, 8); // RX, TX (D9 = RX, D8 = TX)

void setup() {
  Serial.begin(9600);    // For debugging
  HC12.begin(9600);      // HC-12 baud rate

  Serial.println("Transmitter Ready");
}

void loop() {
  HC12.println("Hello from Transmitter");
  Serial.println("Sent: Hello");
  delay(1000); // Send every second
}
