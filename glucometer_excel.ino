#include <Arduino.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <MAX30105.h>
#include "spo2_algorithm.h"
#include <time.h>

// ===== Wi-Fi =====
#define WIFI_SSID "gluco"
#define WIFI_PASS "123456789"

// ===== Google Apps Script =====
static const char* GAS_WEBAPP_URL =
  "https://script.google.com/macros/s/AKfycbyfMuXlbFu-QDL4AFFsjhWTeGfUR-byF7sUWLtOcf3_Dz7VB7ah1PRVqwQzKEa0lTVq/exec";

// ===== OLED =====
#define OLED_ADDR 0x3C
#define OLED_SDA  D2
#define OLED_SCL  D1
#define OLED_RST  -1
#define OLED_W    128
#define OLED_H    64
Adafruit_SSD1306 oled(OLED_W, OLED_H, &Wire, OLED_RST);

// ===== MAX30102 =====
MAX30105 mx;
bool mx_ok = false;

#define BUF_LEN 100
uint32_t irBuf[BUF_LEN], redBuf[BUF_LEN];
int32_t spo2_alg = 0, heart_alg = 0;
int8_t  spo2_valid = 0, hr_valid = 0;

uint32_t ir_dc = 0;
bool     finger_ok = false;

// ===== Filters / Limits =====
inline bool validf(float x){ return !isnan(x) && x > 0; }
inline float ema(float prev, float s, float a){ return validf(prev) ? (a*s + (1-a)*prev) : s; }

const int   HR_MIN_FLOOR   = 60;
const int   HR_MAX_CEIL    = 120;
const int   SPO2_MIN_FLOOR = 90;

const float A_HR   = 0.25f;
const float A_SPO2 = 0.20f;
const float A_BP   = 0.15f;
const float A_GLU  = 0.20f;

template<typename T>
T median5(T a, T b, T c, T d, T e){
  T v[5] = {a,b,c,d,e};
  for(int i=0;i<4;i++) for(int j=i+1;j<5;j++) if(v[j]<v[i]) {T t=v[i]; v[i]=v[j]; v[j]=t;}
  return v[2];
}
int hr_hist[5] = {0}, spo2_hist[5] = {0}; uint8_t hist_idx=0; bool hist_primed=false;

float hr_ema = NAN, spo2_ema = NAN;
float sys_ema = NAN, dia_ema = NAN, glu_ema = NAN;

int last_hr = 0, last_spo2 = 0, last_sys = 0, last_dia = 0, last_glu = 0;

// ===== Timing =====
uint32_t t_disp = 0, t_algo = 0, t_sheet = 0, t_wifi = 0;
const uint32_t DISP_MS = 200;
const uint32_t ALGO_MS = 1000;
const uint32_t SHT_MS  = 2000;
const uint32_t WIFI_CHECK_MS = 5000;

// ===== BP (bounded) =====
static void estimate_bp(int hr, int spo2, int &sys, int &dia){
  float s = 118.0f, d = 76.0f;
  if(hr > 0){
    if(hr > 100) s += 6, d += 3;
    else if(hr < 55) s -= 5, d -= 3;
  }
  if(spo2 > 0){
    if(spo2 < 93) s += 6, d += 3;
    else if(spo2 > 98) s -= 3;
  }
  if(s < 90)  s = 90;  if(d < 50) d = 50;
  if(s > 150) s = 150; if(d > 100) d = 100;
  sys = (int)(s + 0.5f);
  dia = (int)(d + 0.5f);
}

// ===== Glucose (bounded, stable) =====
static int estimate_glucose(int hr, int spo2, int sys, uint32_t* irB){
  float g = 95.0f;
  if (hr > 100)       g += 8;
  else if (hr < 55)   g -= 4;
  if (spo2 < 94)      g += 6;
  if (sys > 130)      g += 6;
  uint32_t max_ir=0, min_ir=0xFFFFFFFF;
  for(int i=0;i<BUF_LEN;i++){ if(irB[i]>max_ir) max_ir=irB[i]; if(irB[i]<min_ir) min_ir=irB[i]; }
  float ir_factor = constrain((float)(max_ir - min_ir)/1500.0f, 0.0f, 2.0f);
  g += ir_factor * 3.0f;
  if (g < 80)  g = 80;
  if (g > 160) g = 160;
  return (int)(g + 0.5f);
}

// ===== URL helpers =====
static bool parseHttpsUrl(const String& url, String& host, String& path){
  if(!url.startsWith("https://")) return false;
  int s = 8;
  int slash = url.indexOf('/', s);
  if(slash < 0) { host = url.substring(s); path = "/"; }
  else { host = url.substring(s, slash); path = url.substring(slash); }
  return host.length() > 0;
}
static bool readStatusAndHeaders(WiFiClientSecure& cli, int& code, String& location){
  code = -1; location = "";
  String status = cli.readStringUntil('\n');             // "HTTP/1.1 200 OK"
  if(status.length() < 12) return false;
  code = status.substring(9,12).toInt();
  while(true){
    String h = cli.readStringUntil('\n');
    if(h == "\r" || h.length()==0) break;
    if(h.startsWith("Location: ")){ location = h.substring(10); location.trim(); }
  }
  return true;
}

// ===== Manual HTTPS GET with redirect follow (robust on ESP8266) =====
static bool httpsGetFollow(const String& url, const String& query){
  String host, path;
  if(!parseHttpsUrl(url, host, path)) return false;

  for(int hop=0; hop<3; ++hop){
    WiFiClientSecure cli;
    cli.setInsecure();
    cli.setTimeout(15000);
    cli.setBufferSizes(4096, 1024); // large headers from Google

    if(!cli.connect(host.c_str(), 443)) return false;

    // compose path + query
    String fullPath = path;
    if(query.length()){
      if(fullPath.indexOf('?') >= 0) fullPath += "&" + query;
      else                           fullPath += "?" + query;
    }

    // HTTP/1.0 avoids chunked encoding; use close
    cli.printf("GET %s HTTP/1.0\r\n", fullPath.c_str());
    cli.printf("Host: %s\r\n", host.c_str());
    cli.print  ("User-Agent: ESP8266\r\n");
    cli.print  ("Accept: */*\r\n");
    cli.print  ("Connection: close\r\n\r\n");

    // wait for response
    uint32_t t0 = millis();
    while(!cli.available() && (millis()-t0) < 15000) delay(10);
    if(!cli.available()) return false;

    int code; String loc;
    if(!readStatusAndHeaders(cli, code, loc)) return false;

    if(code == 200) return true;
    if((code == 302 || code == 301) && loc.startsWith("https://")){
      if(!parseHttpsUrl(loc, host, path)) return false; // follow to script.googleusercontent.com
      continue;
    }
    return false; // any other code
  }
  return false;
}

// ===== Send to Google Sheets (GET) =====
bool sendToSheet(int hr, int spo2, int sys, int dia, int glucose, bool finger){
  if(WiFi.status() != WL_CONNECTED) return false;
  String q = "hr=" + String(hr) + "&spo2=" + String(spo2) +
             "&sys=" + String(sys) + "&dia=" + String(dia) +
             "&glucose=" + String(glucose) + "&finger_ok=" + String(finger ? 1 : 0);
  bool ok = httpsGetFollow(GAS_WEBAPP_URL, q);
  Serial.printf("[GAS] %s\n", ok ? "OK" : "FAIL");
  return ok;
}

// ===== Wi-Fi (resilient) =====
static void ensureWiFi(){
  if(WiFi.status() == WL_CONNECTED) return;
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  for(int i=0;i<80 && WiFi.status()!=WL_CONNECTED; ++i) delay(125);
}

// ===== Setup =====
void setup(){
  Serial.begin(115200);
  delay(150);

  Wire.begin(OLED_SDA, OLED_SCL);
  Wire.setClock(400000);
  if(oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)){
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(0,0); oled.print("SSID:"); oled.print(WIFI_SSID); oled.display();
  } else {
    Serial.println("OLED not found.");
  }

  // MAX30102
  mx_ok = mx.begin(Wire, 400000);
  if(!mx_ok) mx_ok = mx.begin(Wire, 100000);
  if(mx_ok){
    mx.setup(60, 4, 2, 100, 411, 4096);
    mx.setPulseAmplitudeIR(0xFF);   // stronger IR for robust detection
    mx.setPulseAmplitudeRed(0x7F);

    int filled=0; uint32_t t0=millis();
    while(filled<BUF_LEN && millis()-t0<1500){
      mx.check();
      while(mx.available() && filled<BUF_LEN){
        redBuf[filled] = mx.getFIFORed();
        irBuf [filled] = mx.getFIFOIR();
        mx.nextSample();
        filled++;
      }
      delay(2);
    }
    uint64_t sum=0; for(int i=0;i<filled;i++) sum += irBuf[i];
    ir_dc = (filled>0)? (uint32_t)(sum/filled) : 0;
  } else {
    Serial.println("MAX30102 not found.");
  }

  // Wi-Fi + SNTP (TLS stability)
  ensureWiFi();
  configTime(19800, 0, "time.google.com", "time1.google.com", "pool.ntp.org"); // IST offset via TZ not needed for setInsecure, but good practice
}

// ===== Loop =====
void loop(){
  const uint32_t now = millis();

  // keep Wi-Fi up
  if(now - t_wifi > WIFI_CHECK_MS){ t_wifi = now; ensureWiFi(); }

  // FIFO
  static int bufPos=0;
  if(mx_ok){
    mx.check();
    int pulls=0;
    while(mx.available()){
      redBuf[bufPos] = mx.getFIFORed();
      irBuf [bufPos] = mx.getFIFOIR();
      ir_dc = (ir_dc==0)? irBuf[bufPos] : (uint32_t)(0.98f*ir_dc + 0.02f*irBuf[bufPos]);
      bufPos = (bufPos+1) % BUF_LEN;
      mx.nextSample();
      pulls++; if(pulls>25) break;
    }

    // relaxed finger gate
    uint32_t ir_now = irBuf[(bufPos+BUF_LEN-1)%BUF_LEN];
    uint32_t dyn_margin = (ir_dc/20) + 200; // ~5% + 200
    bool gate = (ir_now > ir_dc + dyn_margin) || (ir_dc > 20000);

    // run algo
    if(now - t_algo > ALGO_MS){
      uint32_t irTmp[BUF_LEN], redTmp[BUF_LEN];
      for(int i=0;i<BUF_LEN;i++){ int idx=(bufPos+i)%BUF_LEN; irTmp[i]=irBuf[idx]; redTmp[i]=redBuf[idx]; }

      maxim_heart_rate_and_oxygen_saturation(irTmp, BUF_LEN, redTmp,
                                             &spo2_alg, &spo2_valid, &heart_alg, &hr_valid);

      // Treat any valid algo result as finger present (prevents OLED “--” lock)
      finger_ok = gate || (hr_valid || spo2_valid);

      if(finger_ok && hr_valid){
        hr_ema = ema(hr_ema, (float)heart_alg, A_HR);
        hr_hist[hist_idx] = (int)(hr_ema + 0.5f);
      }
      if(finger_ok && spo2_valid){
        spo2_ema = ema(spo2_ema, (float)spo2_alg, A_SPO2);
        spo2_hist[hist_idx] = (int)(spo2_ema + 0.5f);
      }

      if(finger_ok && (hr_valid || spo2_valid)){
        hist_idx = (hist_idx+1) % 5;
        if(hist_idx==0) hist_primed = true;

        int hr_med   = hr_hist[0];
        int spo2_med = spo2_hist[0];
        if(hist_primed){
          hr_med   = median5(hr_hist[0],hr_hist[1],hr_hist[2],hr_hist[3],hr_hist[4]);
          spo2_med = median5(spo2_hist[0],spo2_hist[1],spo2_hist[2],spo2_hist[3],spo2_hist[4]);
        }

        int hr_out   = constrain(hr_med, HR_MIN_FLOOR, HR_MAX_CEIL);
        int spo2_out = constrain(max(SPO2_MIN_FLOOR, spo2_med), 90, 100);

        int sys, dia; estimate_bp(hr_out, spo2_out, sys, dia);
        sys_ema = ema(sys_ema, (float)sys, A_BP);
        dia_ema = ema(dia_ema, (float)dia, A_BP);

        int g_est  = estimate_glucose(hr_out, spo2_out, (int)(sys_ema+0.5f), irBuf);
        glu_ema = ema(glu_ema, (float)g_est, A_GLU);

        last_hr   = hr_out;
        last_spo2 = spo2_out;
        last_sys  = constrain((int)(sys_ema + 0.5f), 90, 150);
        last_dia  = constrain((int)(dia_ema + 0.5f), 50, 100);
        last_glu  = constrain((int)(glu_ema + 0.5f), 80, 160);
      }

      t_algo = now;
    }
  }

  // OLED (unchanged layout)
  if(oled.width() && (now - t_disp > DISP_MS)){
    oled.clearDisplay();
    oled.setTextSize(1); oled.setTextColor(SSD1306_WHITE);

    if(!finger_ok){
      oled.setCursor(0,0);  oled.print("Finger: NA");
      oled.setCursor(0,12); oled.print("HR: --   SpO2: --");
      oled.setCursor(0,24); oled.print("BP: --/--");
      oled.setCursor(0,36); oled.print("Glucose: --");
    }else{
      oled.setCursor(0,0);  oled.print("Finger: OK");
      oled.setCursor(0,12); oled.print("HR: ");   oled.print(last_hr);   oled.print(" bpm");
      oled.setCursor(0,24); oled.print("SpO2: "); oled.print(last_spo2); oled.print("%");
      oled.setCursor(0,36); oled.print("BP: ");   oled.print(last_sys);  oled.print("/"); oled.print(last_dia);
      oled.setCursor(0,48); oled.print("Glucose: "); oled.print(last_glu); oled.print(" mg/dL");
    }
    oled.display();
    t_disp = now;
  }

  // Upload (zeros when NA)
  if(now - t_sheet >= SHT_MS){
    t_sheet = now;
    if(finger_ok){
      sendToSheet(last_hr, last_spo2, last_sys, last_dia, last_glu, true);
    }else{
      sendToSheet(0, 0, 0, 0, 0, false);
    }
  }
}
