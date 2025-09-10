/*const int radarPin = 2;     // RCWL OUT connected to D2
const int ledPin = 13;      // Optional: onboard LED

void setup() {
  pinMode(radarPin, INPUT);
  pinMode(ledPin, OUTPUT);
  Serial.begin(9600);
}

void loop() {
  int motion = digitalRead(radarPin);
  
  if (motion == HIGH) {
    Serial.println("Motion detected!");
    digitalWrite(ledPin, HIGH);
  } else {
    Serial.println("No motion.");
    digitalWrite(ledPin, LOW);
  }
  
  delay(200);
}*/
/* // === RCWL-0516 + Microphone Launch Detection System ===
const int motionPin = D5;     // RCWL-0516 OUT connected to digital pin D5
const int micPin = A0;        // Analog mic OUT connected to A0
const int threshold = 600;    // Sound threshold (adjust based on testing)
void setup() {
  pinMode(motionPin, INPUT);
  Serial.begin(9600);
}
void loop() {
  int motion = digitalRead(motionPin);
  int micValue = analogRead(micPin);

  Serial.print("Motion: ");
  Serial.print(motion);
  Serial.print(" | Sound Level: ");
  Serial.println(micValue);

  if (motion == HIGH && micValue > threshold) {
    Serial.println("Launch Detected!");
  }
  delay(100); // small delay
}*/
/*#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED config
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Sensor pins
#define SOUND_PIN A0          // Analog mic pin
#define RADAR_PIN D5          // RCWL-0516 OUT

// Thresholds
#define SOUND_THRESHOLD 550   // Adjust based on your testing
#define DETECTION_WINDOW 3000 // ms
#define TRIGGER_THRESHOLD 2   // Number of detections in window

// Tracking
unsigned long windowStart = 0;
int triggerCount = 0;
bool launchDetected = false;

void setup() {
  Serial.begin(115200);
  pinMode(RADAR_PIN, INPUT);

  // OLED init
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not found");
    while (true);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Initializing System...");
  display.display();
  delay(2000);
  display.clearDisplay();
  windowStart = millis();
}

void loop() {
  int soundValue = analogRead(SOUND_PIN);
  int motion = digitalRead(RADAR_PIN);

  // OLED update
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Sound: "); display.println(soundValue);
  display.print("Radar: "); display.println(motion == HIGH ? "MOTION" : "None");

  // Detection Logic
  if (soundValue > SOUND_THRESHOLD && motion == HIGH) {
    triggerCount++;
    Serial.println("⚠️ Potential Launch Signature Detected");
    display.println("** Potential Launch! **");
  }

  // Detection Window Handling
  if (millis() - windowStart > DETECTION_WINDOW) {
    if (triggerCount >= TRIGGER_THRESHOLD && !launchDetected) {
      Serial.println("🚀 LAUNCH DETECTED!");
      display.setCursor(0, 50);
      display.println("🚀 LAUNCH DETECTED!");
      launchDetected = true;
    }
    // Reset window
    windowStart = millis();
    triggerCount = 0;
    // Reset if quiet
    if (soundValue < SOUND_THRESHOLD || motion == LOW) {
      launchDetected = false;
    }
  }

  display.display();
  delay(200);
}*/
