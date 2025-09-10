#include <SPI.h>
#include <RF24.h>

RF24 radio(D2, D8);  // CE, CSN pins on ESP8266

const byte address[6] = "00001";

void setup() {
  Serial.begin(9600);
  radio.begin();
  radio.setPALevel(RF24_PA_LOW);  
  radio.openWritingPipe(address);
  radio.stopListening();
}
void loop() {
  const char text[] = "Hello!";
  radio.write(&text, sizeof(text));
  Serial.println("Sent:Hello!");
  delay(2000);
}
