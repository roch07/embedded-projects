#include <arduinoFFT.h>

#define MIC_PIN        A0
#define SAMPLES        64
#define SAMPLING_FREQ  9000

#define SIREN_MIN_FREQ 850   // Hz
#define SIREN_MAX_FREQ 1500  // Hz
#define DETECTION_WINDOW 2000 // ms (2 seconds)
#define TRIGGER_THRESHOLD 3   // Number of times to detect within window

ArduinoFFT<double> FFT;

unsigned int sampling_period_us;
unsigned long microseconds;

double vReal[SAMPLES];
double vImag[SAMPLES];

unsigned long windowStart = 0;
int triggerCount = 0;
bool ambulanceDetected = false;

void setup() {
  Serial.begin(115200);
  sampling_period_us = round(1000000 * (1.0 / SAMPLING_FREQ));
  windowStart = millis();
}

void loop() {
  // 1. Collect Samples
  for (int i = 0; i < SAMPLES; i++) {
    microseconds = micros();
    vReal[i] = analogRead(MIC_PIN);
    vImag[i] = 0;
    while (micros() < (microseconds + sampling_period_us)) {
      // wait until next sample
    }
  }

  // 2. FFT processing
  FFT.windowing(vReal, SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFT.compute(vReal, vImag, SAMPLES, FFT_FORWARD);
  FFT.complexToMagnitude(vReal, vImag, SAMPLES);

  double peak = FFT.majorPeak(vReal, SAMPLES, SAMPLING_FREQ);

  // 3. Check for siren frequency in current window
  if (peak > SIREN_MIN_FREQ && peak < SIREN_MAX_FREQ) {
    triggerCount++;
    Serial.print("Siren-like freq detected: ");
    Serial.print(peak, 1);
    Serial.print(" Hz [Count in window: ");
    Serial.print(triggerCount);
    Serial.println("]");
  } else {
    Serial.print("No siren, Peak Freq: ");
    Serial.print(peak, 1);
    Serial.println(" Hz");
  }

  // 4. Check if window is over
  if (millis() - windowStart > DETECTION_WINDOW) {
    if (triggerCount >= TRIGGER_THRESHOLD && !ambulanceDetected) {
      Serial.println("**** AMBULANCE DETECTED! ****");
      ambulanceDetected = true;
    }
    // Reset window and trigger count
    windowStart = millis();
    triggerCount = 0;
    if (ambulanceDetected) {
      // Wait until no siren for at least one window before resetting
      if (peak < SIREN_MIN_FREQ || peak > SIREN_MAX_FREQ) {
        ambulanceDetected = false;
      }
    }
  }

  delay(100); // For serial readability
}