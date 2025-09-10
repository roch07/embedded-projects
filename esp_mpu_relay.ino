/*#define BUZZER_PIN 8  // Use pin 8 or any digital pin
//TO CHECK IF BUZZER IS 5V 
void setup() {
  pinMode(BUZZER_PIN, OUTPUT);
}

void loop() {
  digitalWrite(BUZZER_PIN, HIGH); // Buzzer ON
  delay(1000);                    // Wait 1 sec
  digitalWrite(BUZZER_PIN, LOW);  // Buzzer OFF
  delay(1000);                    // Wait 1 sec
}*/
/*#include <Wire.h>
#include <math.h>
#include "I2Cdev.h"
#include "MPU6050.h"

#define RESET_BUTTON_PIN D3
#define RELAY_PIN D5  // Relay control pin (GPIO14)

// MPU6050 instance
MPU6050 mpu;

// Calibration parameters
const int CALIBRATION_SAMPLES = 50;
float restPositionX = 0;
int calibrationCount = 0;
bool isCalibrated = false;

// Angle smoothing
float lastValidAngle = 0;

// Impact detection threshold (in g)
const float ACC_THRESHOLD = 2.5;

void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println("=================================");
  Serial.println("MPU6050 Knee Angle Monitor v1.0");
  Serial.println("=================================");

  // Button with internal pull-up
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);

  // Relay pin
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Relay OFF initially

  // I2C + MPU init
  Wire.begin();
  mpu.initialize();

  if (!mpu.testConnection()) {
    Serial.println("MPU6050 not found! Check wiring:");
    Serial.println("VCC -> 3.3V, GND -> GND");
    Serial.println("SCL -> D1, SDA -> D2");
    Serial.println("Halting program...");
    while (1) { delay(1000); }
  }

  Serial.println("MPU6050 detected and connected!");
  Serial.println("Keep leg straight for calibration...");
  Serial.println();
}

void loop() {
  // Handle reset button: re-start calibration
  if (digitalRead(RESET_BUTTON_PIN) == LOW) {
    delay(50);
    if (digitalRead(RESET_BUTTON_PIN) == LOW) {
      Serial.println("\nRESET! Re-calibrating system...");
      restPositionX = 0;
      calibrationCount = 0;
      isCalibrated = false;
      lastValidAngle = 0;
      digitalWrite(RELAY_PIN, LOW); // Turn buzzer off during calibration

      while (digitalRead(RESET_BUTTON_PIN) == LOW) {
        delay(50);
      }

      Serial.println("Keep leg straight for calibration...");
      Serial.println();
    }
  }

  // Read MPU6050 raw data
  int16_t ax, ay, az, gx, gy, gz;
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

  // Convert raw accel to g
  float ax_g = float(ax) / 16384.0;
  float ay_g = float(ay) / 16384.0;
  float az_g = float(az) / 16384.0;

  // Compute total acceleration magnitude
  float a_mag = sqrt(ax_g*ax_g + ay_g*ay_g + az_g*az_g);

  // Print acceleration data
  Serial.print("Acc (g): ");
  Serial.print(ax_g, 2); Serial.print(", ");
  Serial.print(ay_g, 2); Serial.print(", ");
  Serial.print(az_g, 2);
  Serial.print(" | |A| = ");
  Serial.print(a_mag, 2);
  Serial.print(" g");

  // Impact detection
  if (a_mag > ACC_THRESHOLD) {
    Serial.print(" - IMPACT DETECTED!");
  }
  Serial.println();

  // Calibration phase
  if (!isCalibrated) {
    restPositionX += ax;
    calibrationCount++;

    if (calibrationCount % 10 == 0) {
      Serial.print("Calibrating: ");
      Serial.print((calibrationCount * 100) / CALIBRATION_SAMPLES);
      Serial.println("%");
    }

    if (calibrationCount >= CALIBRATION_SAMPLES) {
      restPositionX /= CALIBRATION_SAMPLES;
      isCalibrated = true;
      Serial.println("Calibration complete!");
      Serial.print("Rest position X = ");
      Serial.println(restPositionX);
      Serial.println("Starting angle measurements...");
      Serial.println("---------------------------");
    }
  }
  // Angle calculation & buzzer control
  else {
    // Compute X-axis offset and angle
    float accelX = float(ax) - restPositionX;
    float angle = atan2(accelX, 16384.0) * 180.0 / PI;

    // Reject impossible readings, then low-pass filter
    if (fabs(angle) > 90) {
      angle = lastValidAngle;
    } else {
      angle = 0.2 * angle + 0.8 * lastValidAngle;
      lastValidAngle = angle;
    }

    // Print knee angle
    Serial.print("Knee Angle: ");
    if (angle > 0) Serial.print("+");
    Serial.print(angle, 1);
    Serial.println("°");

    // Buzzer control via relay
    if (angle > 50) {
      digitalWrite(RELAY_PIN, HIGH);  // Turn buzzer ON
      Serial.println("!!! BUZZER ON - Angle exceeded 50° !!!");
    } else {
      digitalWrite(RELAY_PIN, LOW);   // Turn buzzer OFF
    }
  }

  delay(100);
}
/*#include <Wire.h>
#include <math.h>
#include "I2Cdev.h"
#include "MPU6050.h"

// Reset-button pin (connect one side to D3, the other to GND)
#define RESET_BUTTON_PIN D3

// MPU6050 instance
MPU6050 mpu;

// Calibration parameters
const int   CALIBRATION_SAMPLES = 50;
float       restPositionX      = 0;
int         calibrationCount   = 0;
bool        isCalibrated       = false;

// Angle smoothing
float lastValidAngle = 0;

// Impact detection threshold (in g)
const float ACC_THRESHOLD = 2.5;

void setup() {
  Serial.begin(115200);
  delay(100);
  
  Serial.println("=================================");
  Serial.println("MPU6050 Knee Angle Monitor v1.0");
  Serial.println("=================================");
  
  // Button with internal pull-up
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
  
  // I²C + MPU init
  Wire.begin();
  mpu.initialize();
  
  if (!mpu.testConnection()) {
    Serial.println("   MPU6050 not found! Check wiring:");
    Serial.println("   VCC -> 3.3V, GND -> GND");
    Serial.println("   SCL -> D1, SDA -> D2");
    Serial.println("   Halting program...");
    while (1) { delay(1000); }
  }
  
  Serial.println("   MPU6050 detected and connected!");
  Serial.println("   Keep leg straight for calibration...");
  Serial.println();
}

void loop() {
  // --- Handle reset button: re-start calibration ---
  if (digitalRead(RESET_BUTTON_PIN) == LOW) {
    delay(50);
    if (digitalRead(RESET_BUTTON_PIN) == LOW) {
      Serial.println("\n RESET! Re-calibrating system...");
      restPositionX    = 0;
      calibrationCount = 0;
      isCalibrated     = false;
      lastValidAngle   = 0;
      
      while (digitalRead(RESET_BUTTON_PIN) == LOW) {
        delay(50);
      }
      
      Serial.println(" Keep leg straight for calibration...");
      Serial.println();
    }
  }
  
  // --- Read MPU6050 raw data ---
  int16_t ax, ay, az, gx, gy, gz;
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
  
  // --- 1) Convert raw accel to g ---
  float ax_g = float(ax) / 16384.0;
  float ay_g = float(ay) / 16384.0;
  float az_g = float(az) / 16384.0;
  
  // --- 2) Compute total acceleration magnitude ---
  float a_mag = sqrt(ax_g*ax_g + ay_g*ay_g + az_g*az_g);
  
  // --- 3) Print acceleration data ---
  Serial.print(" Acc (g): ");
  Serial.print(ax_g, 2); Serial.print(", ");
  Serial.print(ay_g, 2); Serial.print(", ");
  Serial.print(az_g, 2);
  Serial.print("  | |A| = ");
  Serial.print(a_mag, 2);
  Serial.print(" g");
  
  // --- 4) Impact detection ---
  if (a_mag > ACC_THRESHOLD) {
    Serial.print("  - IMPACT DETECTED! - ");
  }
  Serial.println();
  
  // --- Calibration phase ---
  if (!isCalibrated) {
    restPositionX += ax;
    calibrationCount++;
    
    if (calibrationCount % 10 == 0) {
      Serial.print("Calibrating: ");
      Serial.print((calibrationCount * 100) / CALIBRATION_SAMPLES);
      Serial.println("%");
    }
    
    if (calibrationCount >= CALIBRATION_SAMPLES) {
      restPositionX /= CALIBRATION_SAMPLES;
      isCalibrated = true;
      Serial.println("Calibration complete!");
      Serial.print("Rest position X = ");
      Serial.println(restPositionX);
      Serial.println("Starting angle measurements...");
      Serial.println("---------------------------");
    }
  }
  // --- Angle calculation & printing ---
  else {
    // Compute X-axis offset and angle
    float accelX = float(ax) - restPositionX;
    float angle  = atan2(accelX, 16384.0) * 180.0 / PI;
    
    // Reject impossible readings, then low-pass filter
    if (fabs(angle) > 90) {
      angle = lastValidAngle;
    } else {
      angle = 0.2 * angle + 0.8 * lastValidAngle;
      lastValidAngle = angle;
    }
    
    // Print only the knee angle
    Serial.print("Knee Angle: ");
    if (angle > 0) Serial.print("+");
    Serial.print(angle, 1);
    Serial.println("°");
  }
  
  // Adjust this delay to tune your sample rate
  delay(100);
}
*/
/*#include <Wire.h>
#include <math.h>
#include "I2Cdev.h"
#include "MPU6050.h"

#define RESET_BUTTON_PIN D3     // Optional button to recalibrate posture
#define BUZZER_PIN D5           // Buzzer connected here (active buzzer)

// MPU6050 object
MPU6050 mpu;

// Calibration settings
const int CALIBRATION_SAMPLES = 50;
float restPositionY = 0;
int calibrationCount = 0;
bool isCalibrated = false;

// Angle handling
float lastValidAngle = 0;
const float ANGLE_THRESHOLD = 40.0;  // Degrees: change to set posture sensitivity

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("Posture Monitor with ESP8266 + MPU6050 + Buzzer");

  // Pin modes
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW); // Buzzer OFF initially

  // Initialize MPU6050
  Wire.begin();  // SDA = D2, SCL = D1 by default
  mpu.initialize();

  if (!mpu.testConnection()) {
    Serial.println("MPU6050 not found. Check connections.");
    while (1) delay(1000);
  }

  Serial.println("MPU6050 connected. Sit upright to calibrate...");
}

void loop() {
  // Recalibrate posture when button is pressed
  if (digitalRead(RESET_BUTTON_PIN) == LOW) {
    delay(50);
    if (digitalRead(RESET_BUTTON_PIN) == LOW) {
      Serial.println("Recalibrating...");
      restPositionY = 0;
      calibrationCount = 0;
      isCalibrated = false;
      lastValidAngle = 0;
      digitalWrite(BUZZER_PIN, LOW); // Turn off buzzer
      while (digitalRead(RESET_BUTTON_PIN) == LOW) delay(50);
      Serial.println("Sit upright for calibration...");
    }
  }

  // Read MPU6050 values
  int16_t ax, ay, az, gx, gy, gz;
  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

  float ax_g = float(ax) / 16384.0;
  float ay_g = float(ay) / 16384.0;
  float az_g = float(az) / 16384.0;

  // Calibrate on startup
  if (!isCalibrated) {
    restPositionY += ay;
    calibrationCount++;

    if (calibrationCount % 10 == 0) {
      Serial.print("Calibrating: ");
      Serial.print((calibrationCount * 100) / CALIBRATION_SAMPLES);
      Serial.println("%");
    }

    if (calibrationCount >= CALIBRATION_SAMPLES) {
      restPositionY /= CALIBRATION_SAMPLES;
      isCalibrated = true;
      Serial.println("Calibration complete.");
      Serial.print("Rest Y = "); Serial.println(restPositionY);
    }

    delay(100);
    return;
  }

  // Calculate tilt angle
  float accelY = float(ay) - restPositionY;
  float angle = atan2(accelY, 16384.0) * 180.0 / PI;

  if (fabs(angle) > 90) {
    angle = lastValidAngle; // Reject invalid angles
  } else {
    angle = 0.2 * angle + 0.8 * lastValidAngle; // Smooth out changes
    lastValidAngle = angle;
  }

  Serial.print("Back Tilt Angle: ");
  Serial.print(angle, 1);
  Serial.println("°");

  // Buzzer logic
  if (angle > ANGLE_THRESHOLD) {
    digitalWrite(BUZZER_PIN, HIGH); // Bad posture → buzzer ON
    Serial.println("BAD POSTURE! BUZZER ON");
  } else {
    digitalWrite(BUZZER_PIN, LOW); // Good posture → buzzer OFF
  }

  delay(100);
}
*/
/*#include <Wire.h>
#include <MPU6050.h>

MPU6050 mpu;
const int buzzerPin = 0;  // D3 (GPIO0), adjust if needed

void setup() {
  Serial.begin(115200);
  Wire.begin();
  mpu.initialize();

  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);

  if (!mpu.testConnection()) {
    Serial.println("MPU6050 connection failed!");
    while (1);
  }
  Serial.println("MPU6050 connected.");
}

void loop() {
  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);

  // Convert raw accel to g (assuming 16-bit range +-2g)
  float axg = ax / 16384.0;
  float ayg = ay / 16384.0;
  float azg = az / 16384.0;

  // Calculate pitch angle
  float pitch = atan2(ayg, sqrt(axg * axg + azg * azg)) * 180 / PI;

  Serial.print("Pitch: ");
  Serial.println(pitch);

  if (abs(pitch) > 40) {
    digitalWrite(buzzerPin, HIGH);  // Buzzer ON
    delay(500);                     // Wait 0.5 sec
    digitalWrite(buzzerPin, LOW);   // Buzzer OFF
  } else {
    digitalWrite(buzzerPin, LOW);   // Ensure buzzer stays OFF
    delay(200);                     // Small delay
  }
}
*/
/* 
//I2C DEVICE
#include <Wire.h>

void setup() {
  Wire.begin();
  Serial.begin(115200);
  Serial.println("Scanning I2C devices...");
}

void loop() {
  byte error, address;
  int nDevices = 0;

  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("I2C device found at ");
      Serial.println(address, DEC);
      nDevices++;
    }
  }

  if (nDevices == 0)
    Serial.println("No I2C devices found.");
  else
    Serial.println("Done.");

  delay(5000);
}
 */
/*#include <Wire.h>
#include <MPU6050.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define BUZZER_PIN    D3  // GPIO0

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
MPU6050 mpu;

bool badPostureDetected = false;
unsigned long lastBeepTime = 0;
const int beepDuration = 500; // ms
const float pitchThreshold = 40.0;
const float rollThreshold  = 25.0;

void setup() {
  Serial.begin(115200);
  Wire.begin();

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  mpu.initialize();
  if (!mpu.testConnection()) {
    Serial.println("MPU6050 connection failed!");
    while (1);
  }
  Serial.println("MPU6050 connected.");

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED init failed!"));
    while (1);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Posture Monitor Ready");
  display.display();
  delay(1000);
}

void loop() {
  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);

  float axg = ax / 16384.0;
  float ayg = ay / 16384.0;
  float azg = az / 16384.0;

  float pitch = atan2(ayg, sqrt(axg * axg + azg * azg)) * 180.0 / PI;
  float roll  = atan2(-axg, sqrt(ayg * ayg + azg * azg)) * 180.0 / PI;

  Serial.print("Pitch: ");
  Serial.print(pitch, 2);
  Serial.print(" | Roll: ");
  Serial.println(roll, 2);

  // OLED display
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.print("Pitch: ");
  display.print(pitch, 2);
  display.println(" deg");

  display.print("Roll : ");
  display.print(roll, 2);
  display.println(" deg");

  bool isBadPosture = abs(pitch) > pitchThreshold || abs(roll) > rollThreshold;

  if (isBadPosture) {
    display.setCursor(0, 40);
    display.setTextSize(1);
    display.println("!! BAD POSTURE !!");

    if (!badPostureDetected) {
      digitalWrite(BUZZER_PIN, HIGH);
      lastBeepTime = millis();
      badPostureDetected = true;
    }
  } else {
    display.setCursor(0, 40);
    display.setTextSize(1);
    display.println("Posture OK");
    badPostureDetected = false;
  }

  if (digitalRead(BUZZER_PIN) == HIGH && millis() - lastBeepTime >= beepDuration) {
    digitalWrite(BUZZER_PIN, LOW);
  }
  display.display();
  delay(100);
}

*/
#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>
#include <Wire.h>
#include <MPU6050.h>

// Wi-Fi credentials
#define WIFI_SSID "battery"
#define WIFI_PASSWORD "123456789"

// Firebase project credentials
#define API_KEY "AIzaSyA3HzYav0oTpEpIvSWjXNcNK7gRA8PTS3M"
#define DATABASE_URL "https://project1-ad808-default-rtdb.firebaseio.com"
#define USER_EMAIL "spherenexgpt@gmail.com"
#define USER_PASSWORD "Spherenex@123"

// Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// MPU6050
MPU6050 mpu;

void setup() {
  Serial.begin(115200);
  Wire.begin();

  // Init MPU6050
  mpu.initialize();
  if (!mpu.testConnection()) {
    Serial.println("MPU6050 connection failed!");
    while (1);
  }
  Serial.println("MPU6050 connected.");

  // Connect to Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected.");

  // Firebase config
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop() {
  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);

  // Convert to g
  float axg = ax / 16384.0;
  float ayg = ay / 16384.0;
  float azg = az / 16384.0;

  // Calculate pitch and roll
  float pitch = atan2(ayg, sqrt(axg * axg + azg * azg)) * 180.0 / PI;
  float roll  = atan2(-axg, sqrt(ayg * ayg + azg * azg)) * 180.0 / PI;

  Serial.print("Pitch: "); Serial.print(pitch);
  Serial.print(" | Roll: "); Serial.println(roll);

  if (Firebase.ready()) {
    Firebase.setFloat(fbdo, "/MPU6050/Pitch", pitch);
    Firebase.setFloat(fbdo, "/MPU6050/Roll", roll);
  }

  delay(1000); // Log every 1 sec
}





