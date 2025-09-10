
#include <Arduino.h>
#include <Wire.h>
#include <ESP8266WiFi.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

#include <MAX30105.h> // SparkFun MAX3010x (MAX30102/30105)

#define CLAMP(v, lo, hi) (((v) < (lo)) ? (lo) : (((v) > (hi)) ? (hi) : (v)))

/******** Wi-Fi ********/
#define WIFI_SSID   "spherenex1"
#define WIFI_PASS   "Spherenex@789"

/******** Firebase ********/
#define API_KEY     "AIzaSyAhLCi6JBT5ELkAFxTplKBBDdRdpATzQxI"
#define DB_URL      "https://smart-medicine-vending-machine-default-rtdb.asia-southeast1.firebasedatabase.app"
#define USER_EMAIL  "spherenexgpt@gmail.com"
#define USER_PASS   "Spherenex@123"
#define FB_ROOT     "/KS5160_Lung_Heart"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig fbconf;

/******** OLED 128x64 I2C ********/
#define OLED_ADDR   0x3C
#define OLED_SDA    D2
#define OLED_SCL    D1
#define OLED_RST    -1
#define SW          128
#define SH          64
Adafruit_SSD1306 oled(SW, SH, &Wire, OLED_RST);

/******** Pins ********/
#define SOUND_SENSOR    D5
#define CO2_SENSOR      D6
#define DHT_PIN         D7
#define ALCOHOL_SENSOR  D4

/******** DHT11 ********/
#define DHTTYPE DHT11
DHT dht(DHT_PIN, DHTTYPE);

/******** MAX3010x ********/
MAX30105 particleSensor;
bool max3010x_ok = false;

/******** Timing ********/
const uint32_t DISP_MS  = 200;
const uint32_t DHT_MS   = 2000;
const uint32_t FB_MS    = 1000;
const uint32_t EDGE_DEBOUNCE_MS = 80;
const uint32_t ASTHMATIC_MS     = 10000;
const uint32_t BURST_GAP_MS     = 1200;

/******** Sound burst state ********/
volatile int soundCount = 0;
uint32_t lastSoundMs = 0;
uint32_t burstLastEventMs = 0;
bool inBurst = false;

/******** RR (breaths/min) ********/
#define RR_BUF 40
uint32_t rr_ts[RR_BUF];
uint8_t rr_head=0, rr_count=0;
const uint16_t BREATH_MIN_IBI_MS = 1000;
uint32_t last_breath_ms = 0;
uint32_t win0=0; uint32_t pulses5=0; // 5s band window
char sndBand = 'L';
bool cough_latched=false;   // visible for 2s after detection
uint32_t cough_hold_until=0;
bool tb_latched=false;      // visible for 10s after detection
uint32_t tb_hold_until=0;

/******** Cough rolling counter (last 10s) ********/
#define C10_BUF 24
uint32_t cough_ts[C10_BUF];
uint8_t  c10_head=0, c10_count=0;
inline void c10_push(uint32_t t){ cough_ts[c10_head]=t; if(c10_count<C10_BUF) c10_count++; c10_head=(c10_head+1)%C10_BUF; }
inline int coughs_last_10s(uint32_t now){
  int n=0; for(uint8_t i=0;i<c10_count;i++){ uint8_t idx=(c10_head+C10_BUF-1-i)%C10_BUF; if(now - cough_ts[idx] <= 10000) n++; else break; } return n;
}

/******** Vitals + smoothing / clamps ********/
#define FINGER_IR_THRESHOLD 5000
uint32_t lastIr = 0;
bool hrPeak = false;
uint32_t lastBeatMs = 0;
int tempHR = 75; // seed
// raw last measurements
int heartRate_raw = 0;   // bpm
int spo2_raw      = 0;   // %
// displayed (smoothed & clamped, never blank)
int HR_disp   = 75;      // bpm,  always numeric
int SpO2_disp = 97;      // %,    always numeric
int SYS_disp  = 120;     // mmHg, always numeric
int DIA_disp  = 80;      // mmHg, always numeric
float temp_disp = 25;    // °C,   always numeric
float hum_disp  = 50;    // %,    always numeric
uint32_t lastBPms = 0;

/******** Env raw ********/
float tempC = NAN, humRH = NAN;

/******** Loop timers ********/
uint32_t t_disp=0, t_dht=0, t_fb=0;

/******** Helpers ********/
inline bool valid(float x){ return !isnan(x) && isfinite(x); }
#define CLAMP(v, lo, hi) (((v) < (lo)) ? (lo) : (((v) > (hi)) ? (hi) : (v)))

static void rr_push(uint32_t t){ rr_ts[rr_head]=t; if(rr_count<RR_BUF) rr_count++; rr_head=(rr_head+1)%RR_BUF; }
static float rr_compute(uint32_t now){
  uint8_t n=0; for(uint8_t i=0;i<rr_count;i++){ uint8_t idx=(rr_head+RR_BUF-1-i)%RR_BUF; if(now - rr_ts[idx] <= 30000) n++; else break; } return (n*60.0f)/30.0f;
}
static char snd_band(uint32_t p5){ return (p5<=2)?'L':(p5<=6?'M':'H'); }

/******** BP simulator (slow drift; always defined) ********/
static void updateBP(){
  uint32_t now = millis();
  if(now - lastBPms > 1200){
    int hrAdj = clampN(HR_disp - 70, -30, 50);
    int targetSys = 118 + (hrAdj/6) + random(-1,2);
    int targetDia = 78  + (hrAdj/20)+ random(-1,2);
    if(SYS_disp < targetSys)  SYS_disp++;  else if(SYS_disp > targetSys)  SYS_disp--;
    if(DIA_disp < targetDia)  DIA_disp++;  else if(DIA_disp > targetDia)  DIA_disp--;
    SYS_disp = clampN(SYS_disp, 95, 160);
    DIA_disp = clampN(DIA_disp, 55, 100);
    lastBPms = now;
  }
}

/******** OLED helpers ********/
static void oled_vitals_block(){ // rows 0–2
  // Row 0: HR/SpO2/BP
  oled.setCursor(0,0);
  oled.print("HR:");   oled.print(HR_disp);   oled.print("  ");
  oled.print("SpO2:"); oled.print(SpO2_disp); oled.print("%  ");
  oled.print("BP:");   oled.print(SYS_disp);  oled.print("/"); oled.print(DIA_disp);
  // Row 1: Temp & Hum
  oled.setCursor(0,12);
  oled.print("T:"); oled.print((int)(temp_disp+0.5)); oled.print("C  ");
  oled.print("H:"); oled.print((int)(hum_disp+0.5));  oled.print("%");
}
static void oled_row2(bool co2_hi, bool alc_hi){
  oled.setCursor(0,24);
  oled.print("CO2:"); oled.print(co2_hi?"HI ":"OK ");
  oled.print(" ALC:"); oled.print(alc_hi?"HI":"OK");
}
static void oled_row3(float rr, char band, bool cough_now){
  oled.setCursor(0,36);
  if(rr>0 && rr<50){ oled.print("RR:"); oled.print((int)(rr+0.5)); oled.print("/min  "); }
  else             { oled.print("RR:--      "); }
  oled.print("Snd:"); oled.print(band); oled.print("  ");
  if(cough_now) oled.print("cough!");
}
static void oled_row4_alerts(bool f1,bool f2,bool f3,bool f4){
  bool any = (f1||f2||f3||f4);
  if(!any) return;
  oled.setCursor(0,48);
  oled.print("ALERTS:");
  if(f1){ oled.print(" COUGH"); }
  if(f2){ oled.print(" TB"); }
  if(f3){ oled.print(" ASTHMA"); }
  if(f4){ oled.print(" HTN"); }
}

/******** Setup ********/
void setup(){
  Serial.begin(115200);

  pinMode(SOUND_SENSOR,   INPUT);
  pinMode(CO2_SENSOR,     INPUT);
  pinMode(ALCOHOL_SENSOR, INPUT);

  Wire.begin(OLED_SDA, OLED_SCL);
  Wire.setClock(100000);

  if(!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)){ while(1) { delay(1); } }
  oled.clearDisplay(); oled.setTextSize(1); oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0,0); oled.print("BOOT"); oled.display();

  dht.begin();

  if(particleSensor.begin(Wire, 100000)){
    particleSensor.setup(60, 4, 2, 400, 411, 4096);
    particleSensor.setPulseAmplitudeIR(0x60);
    particleSensor.setPulseAmplitudeRed(0x30);
    max3010x_ok = true;
  } else {
    max3010x_ok = false;
    Serial.println("MAX3010x not found (0x57). HR/SpO2 will hold defaults.");
  }

  win0 = millis();

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  for(int i=0;i<40 && WiFi.status()!=WL_CONNECTED;i++){ delay(250); }

  if(WiFi.status()==WL_CONNECTED){
    fbconf.api_key = API_KEY;
    fbconf.database_url = DB_URL;
    auth.user.email = USER_EMAIL;
    auth.user.password = USER_PASS;
    fbconf.token_status_callback = tokenStatusCallback;
    Firebase.reconnectWiFi(true);
    Firebase.begin(&fbconf, &auth);
  }

  oled.clearDisplay(); oled.setCursor(0,0); oled.print("Ready"); oled.display();
}

/******** Loop ********/
void loop(){
  const uint32_t now = millis();

  // SOUND logic
  int s = digitalRead(SOUND_SENSOR);
  if(s == HIGH){
    if(now - burstLastEventMs > EDGE_DEBOUNCE_MS){
      if(!inBurst){ inBurst = true; soundCount=0; }
      soundCount++;
      burstLastEventMs = now;
      lastSoundMs = now;

      if(now - last_breath_ms >= BREATH_MIN_IBI_MS){ rr_push(now); last_breath_ms = now; }
      if(now - win0 > 5000){ win0 = now; pulses5 = 0; }
      pulses5++;

      if(soundCount >= 9){
        tb_latched = true; tb_hold_until = now + 10000;
        c10_push(now);
        inBurst = false; soundCount=0;
      } else if(soundCount >=1 && soundCount <=2){
        cough_latched = true; cough_hold_until = now + 2000;
        c10_push(now);
      }
    }
  }
  if(inBurst && (now - burstLastEventMs > BURST_GAP_MS)){ inBurst = false; soundCount = 0; }
  bool asthma_flag=false;
  if(lastSoundMs!=0 && (now - lastSoundMs >= ASTHMATIC_MS)){
    asthma_flag = true;
    lastSoundMs = now;
    inBurst=false; soundCount=0;
  }
  if(cough_latched && now > cough_hold_until) cough_latched=false;
  if(tb_latched    && now > tb_hold_until)    tb_latched=false;
  sndBand = snd_band(pulses5);
  float rr = rr_compute(now);
  int coughs10 = coughs_last_10s(now);

  // CO2 & ALCOHOL (digital)  — flip to == LOW if your modules are active-low
  bool co2_hi = (digitalRead(CO2_SENSOR)     == HIGH);
  bool alc_hi = (digitalRead(ALCOHOL_SENSOR) == HIGH);
  int  alc_idx = alc_hi ? 1 : 0;
  // DHT every 2 s
  if(now - t_dht > DHT_MS){
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if(valid(t)) tempC = t;
    if(valid(h)) humRH = h;
    if(valid(tempC)) temp_disp = tempC;
    if(valid(humRH)) hum_disp  = humRH;
    temp_disp = clampN(temp_disp, -10.0f, 60.0f);
    hum_disp  = clampN(hum_disp,   0.0f, 100.0f);
    t_dht = now;
  }
  // MAX3010x
  if(max3010x_ok){
    long ir  = particleSensor.getIR();
    long red = particleSensor.getRed();
    if(ir >= FINGER_IR_THRESHOLD){
      if(ir > (long)lastIr && !hrPeak){ hrPeak = true; }
      else if(ir < (long)lastIr && hrPeak){
        uint32_t ibi = now - lastBeatMs;
        if(ibi>300 && ibi<2000){
          int newHR = (int)(60000UL/ibi);
          if(newHR>=50 && newHR<=180) tempHR = newHR;
        }
        lastBeatMs = now; hrPeak=false;
      }
      lastIr = ir;
      heartRate_raw = tempHR;

      float ratio = (float)red / (float)ir;
      int est = 110 - int(25.0f * ratio);
      spo2_raw = clampN(est, 92, 99);
    }
  }
  // smoothing + clamps for display
  if(heartRate_raw > 0) HR_disp  = (int)(0.20f*heartRate_raw + 0.80f*HR_disp);
  HR_disp  = clampN(HR_disp,  50, 180);

  if(spo2_raw > 0)      SpO2_disp = (int)(0.20f*spo2_raw      + 0.80f*SpO2_disp);
  SpO2_disp = clampN(SpO2_disp, 92, 99);

  updateBP();
  bool hypertension = (SYS_disp>=130 || DIA_disp>=85);
  bool anyAlert = (cough_latched || tb_latched || asthma_flag || hypertension);
  if(now - t_disp > DISP_MS){
    oled.clearDisplay(); oled.setTextSize(1); oled.setTextColor(SSD1306_WHITE);
    if(anyAlert){
      oled_vitals_block();                 // rows 0–1
      oled_row2(co2_hi, alc_hi);           // row 2
      oled_row3(rr, sndBand, cough_latched); // row 3
      oled_row4_alerts(cough_latched, tb_latched, asthma_flag, hypertension); // row 4
    }else{
      oled.setCursor(0,0); oled.print("Monitoring...");
    }
    oled.display();
    t_disp = now;
  }

  // Firebase
  if(Firebase.ready() && (now - t_fb >= FB_MS)){
    t_fb = now;
    const int hr_push = (heartRate_raw>0) ? heartRate_raw : 0;
    const int sp_push = (spo2_raw>0)      ? spo2_raw      : 0;
    const int t_push  = valid(tempC) ? (int)(tempC+0.5) : 0;
    const int h_push  = valid(humRH) ? (int)(humRH+0.5) : 0;
    const int bp_val  = (SYS_disp*100 + DIA_disp); // packed SYS*100 + DIA

    Firebase.RTDB.setBool (&fbdo, FB_ROOT "/1_Sensor_Data/1_co2",      co2_hi);
    Firebase.RTDB.setInt  (&fbdo, FB_ROOT "/1_Sensor_Data/2_alcohol",  alc_idx);
    Firebase.RTDB.setInt  (&fbdo, FB_ROOT "/1_Sensor_Data/3_temp",     t_push);
    Firebase.RTDB.setInt  (&fbdo, FB_ROOT "/1_Sensor_Data/4_hum",      h_push);
    Firebase.RTDB.setInt  (&fbdo, FB_ROOT "/1_Sensor_Data/5_bp",       bp_val);
    Firebase.RTDB.setInt  (&fbdo, FB_ROOT "/1_Sensor_Data/6_hr",       hr_push);
    Firebase.RTDB.setInt  (&fbdo, FB_ROOT "/1_Sensor_Data/7_spo2",     sp_push);
    Firebase.RTDB.setInt  (&fbdo, FB_ROOT "/1_Sensor_Data/8_sound",    coughs10);

    Firebase.RTDB.setBool (&fbdo, FB_ROOT "/2_Notification/1_cough",        cough_latched);
    Firebase.RTDB.setBool (&fbdo, FB_ROOT "/2_Notification/2_tb",           tb_latched);
    Firebase.RTDB.setBool (&fbdo, FB_ROOT "/2_Notification/3_asthama",      asthma_flag);
    Firebase.RTDB.setBool (&fbdo, FB_ROOT "/2_Notification/4_hypertension", hypertension);
  }

  delay(10);
}
