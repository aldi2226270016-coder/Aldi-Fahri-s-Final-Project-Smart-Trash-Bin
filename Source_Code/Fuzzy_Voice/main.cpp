#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>
#include <HardwareSerial.h>
#include <DFRobotDFPlayerMini.h>

// ==========================================
// PANGGIL LIBRARY TEMPLATE HC-SR04 (Martinsos)
// ==========================================
#include <HC_SR04.h> 

// ==========================================
// KONFIGURASI LOGIKA SENSOR
// ==========================================
#define LOGIKA_IR_MENDETEKSI          LOW 
#define LOGIKA_KAPASITIF_MENDETEKSI   HIGH  // NPN NC (Logika Active HIGH)
#define LOGIKA_INDUKTIF_MENDETEKSI    LOW

// ==========================================
// DEKLARASI PIN I/O
// ==========================================
// PIN ULTRASONIK (Sesuai Kodingan Sempurna Terbaru Anda)
#define TRIG_SINGLE 5 
#define ECHO_P_DPN 34 // Plastik Depan
#define ECHO_P_BLK 18 // Plastik Belakang
#define ECHO_K_DPN 19 // Kertas Depan
#define ECHO_K_BLK 35 // Kertas Belakang
#define ECHO_L_DPN 4  // Logam Depan
#define ECHO_L_BLK 14 // Logam Belakang

// PIN PERIFERAL LAINNYA
#define RADAR_PIN 32 // Interupsi Wake-Up
#define IR1       26
#define IR2       25
#define PROX_KAP  27 
#define PROX_IND  33 
#define SERVO1_PIN 13 // POS 1
#define SERVO2_PIN 12 // POS 2
#define RX_PIN 16
#define TX_PIN 17

// ==========================================
// OBJEK & VARIABEL GLOBAL
// ==========================================
LiquidCrystal_I2C lcd(0x27, 20, 4); 
Servo servo1;
Servo servo2;
HardwareSerial mySoftwareSerial(2); 
DFRobotDFPlayerMini myDFPlayer;

unsigned long waktuRadarTerakhir = 0;
unsigned long waktuUpdateStandbyTerakhir = 0; 
bool modeAktif = false;
bool isLocked = false;
float capP = 0, capK = 0, capL = 0;

// ==========================================
// INISIALISASI 6 SENSOR ULTRASONIK
// ==========================================
HC_SR04_BASE *slaves_Group[] = {
    new HC_SR04<ECHO_P_DPN>(), // [0] P_Dpn
    new HC_SR04<ECHO_P_BLK>(), // [1] P_Blk
    new HC_SR04<ECHO_K_DPN>(), // [2] K_Dpn
    new HC_SR04<ECHO_K_BLK>(), // [3] K_Blk
    new HC_SR04<ECHO_L_DPN>(), // [4] L_Dpn
    new HC_SR04<ECHO_L_BLK>()  // [5] L_Blk
};
HC_SR04<TRIG_SINGLE> sonicMaster(TRIG_SINGLE, slaves_Group, 6);

// ==========================================
// FUNGSI FUZZY SUGENO
// ==========================================
float mu_dekat(float x) { if(x<=8) return 1.0; if(x>=15) return 0.0; return (15.0-x)/7.0; }
float mu_sedang(float x) { if(x<=10 || x>=35) return 0.0; if(x<=23) return (x-10.0)/13.0; return (35.0-x)/12.0; }
float mu_jauh(float x) { if(x<=25) return 0.0; if(x>=38) return 1.0; return (x-25.0)/13.0; }

float hitungKapasitasFuzzy(float jarakDepan, float jarakBelakang) {
  if (jarakDepan < 0) jarakDepan = 0; if (jarakDepan > 43) jarakDepan = 43;
  if (jarakBelakang < 0) jarakBelakang = 0; if (jarakBelakang > 43) jarakBelakang = 43;

  float dpn_dkt = mu_dekat(jarakDepan); float dpn_sdg = mu_sedang(jarakDepan); float dpn_jau = mu_jauh(jarakDepan);
  float blk_dkt = mu_dekat(jarakBelakang); float blk_sdg = mu_sedang(jarakBelakang); float blk_jau = mu_jauh(jarakBelakang);

  float a1 = min(dpn_jau, blk_jau); float a2 = min(dpn_jau, blk_sdg); float a3 = min(dpn_jau, blk_dkt);
  float a4 = min(dpn_sdg, blk_jau); float a5 = min(dpn_sdg, blk_sdg); float a6 = min(dpn_sdg, blk_dkt);
  float a7 = min(dpn_dkt, blk_jau); float a8 = min(dpn_dkt, blk_sdg); float a9 = min(dpn_dkt, blk_dkt);

  float sumAlpha = a1+a2+a3+a4+a5+a6+a7+a8+a9;
  if(sumAlpha == 0) return 0; 
  return (a1*0 + a2*50 + a3*100 + a4*50 + a5*50 + a6*100 + a7*100 + a8*100 + a9*100) / sumAlpha;
}

// ==========================================
// FUNGSI CEK KAPASITAS & LOCK SYSTEM
// ==========================================
void cekKapasitasDanLock() {
  // -- PEMBACAAN HARDWARE (Sekuens Stabil 5 Tahap) --
  sonicMaster.startMeasure(50000); 
  delay(15); 
  sonicMaster.startMeasure(50000, 1); 
  delay(15);
  sonicMaster.startMeasure(50000, 2); 
  delay(15);
  sonicMaster.startMeasure(50000, 3); 
  delay(15);
  sonicMaster.startMeasure(50000, 4); 
  delay(15);
  sonicMaster.startMeasure(50000, 5); 
  delay(15);

  // -- AMBIL HASIL JARAK MURNI --
  float pDpn = slaves_Group[0]->getDist_cm();
  float pBlk = slaves_Group[1]->getDist_cm();
  float kDpn = slaves_Group[2]->getDist_cm();
  float kBlk = slaves_Group[3]->getDist_cm();
  float lDpn = slaves_Group[4]->getDist_cm();
  float lBlk = slaves_Group[5]->getDist_cm();

  // -- HITUNG KAPASITAS FUZZY --
  capP = hitungKapasitasFuzzy(pDpn, pBlk);
  capK = hitungKapasitasFuzzy(kDpn, kBlk);
  capL = hitungKapasitasFuzzy(lDpn, lBlk);

  // -- LOGIKA PENGUNCIAN --
  if (capP >= 75 || capK >= 75 || capL >= 75) {
    // Jika baru pertama kali mendeteksi Full, bersihkan layar dan bunyikan Audio Penuh
    if (!isLocked) {
      lcd.clear();
      myDFPlayer.play(5); // Audio Wadah Penuh (Track 5)
    }
    isLocked = true;
    
    // TAMPILAN KETIKA SISTEM TERKUNCI
    lcd.setCursor(0, 0); lcd.print("  SISTEM TERKUNCI!  ");
    lcd.setCursor(0, 1); lcd.print("    WADAH PENUH     ");
    
    // Baris 2: Tetap menampilkan kapasitas masing-masing agar bisa dipantau real-time
    lcd.setCursor(0, 2); lcd.print("P:"); lcd.print((int)capP); lcd.print("%   ");
    lcd.setCursor(7, 2); lcd.print("L:"); lcd.print((int)capL); lcd.print("%   ");
    lcd.setCursor(14, 2); lcd.print("K:"); lcd.print((int)capK); lcd.print("%   ");
    
    lcd.setCursor(0, 3); lcd.print(" [SILAKAN KOSONGKAN]");
    
    servo1.write(90); servo2.write(90); // Mengunci laci mekanik
  } else {
    // Jika sebelumnya terkunci dan sekarang tumpukan sampah sudah dikeluarkan (di bawah 75%)
    if (isLocked) {
      lcd.clear(); // Bersihkan sisa karakter menu penguncian
    }
    isLocked = false;
  }
}

// ==========================================
// FUNGSI KHUSUS UPDATE TAMPILAN STANDBY (NORMAL)
// ==========================================
void tampilkanStandby() {
  if (isLocked) return;
  
  lcd.setCursor(0, 0); lcd.print("P:"); lcd.print((int)capP); lcd.print("%   ");
  lcd.setCursor(7, 0); lcd.print("L:"); lcd.print((int)capL); lcd.print("%   ");
  lcd.setCursor(14, 0); lcd.print("K:"); lcd.print((int)capK); lcd.print("%   ");
  
  lcd.setCursor(0, 1); lcd.print(" ALAT SIAP MEMILAH  ");
  lcd.setCursor(0, 2); lcd.print("  MASUKKAN SAMPAH!  ");
  lcd.setCursor(0, 3); lcd.print("                    "); 
}

// ==========================================
// FUNGSI NOTIFIKASI SAMPAH MASUK
// ==========================================
void notifikasiSampah(String jenis, int trackAudio) {
  lcd.clear();
  lcd.setCursor(0, 1); lcd.print("TERDETEKSI SAMPAH:");
  int space = (20 - jenis.length()) / 2;
  lcd.setCursor(space, 2); lcd.print(jenis);
  myDFPlayer.play(trackAudio); 
  delay(2500); 
}

// ==========================================
// SETUP
// ==========================================
void setup() {
  Serial.begin(115200);
  
  pinMode(RADAR_PIN, INPUT);
  pinMode(IR1, INPUT_PULLUP); pinMode(IR2, INPUT_PULLUP);
  pinMode(PROX_KAP, INPUT); pinMode(PROX_IND, INPUT); 
  
  servo1.attach(SERVO1_PIN); servo2.attach(SERVO2_PIN);
  servo1.write(90); servo2.write(90); 
  
  Wire.begin(21, 22); lcd.init(); lcd.backlight();
  
  mySoftwareSerial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  if (myDFPlayer.begin(mySoftwareSerial)) { myDFPlayer.volume(25); delay(500); }

  sonicMaster.begin();

  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  
  lcd.clear();
  cekKapasitasDanLock();
  
  if (!isLocked) {
    modeAktif = true;
    waktuRadarTerakhir = millis(); 
    tampilkanStandby();

    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
      myDFPlayer.play(1); // Track 1
    } else {
      myDFPlayer.play(1); 
    }
  }
}

// ==========================================
// LOOP UTAMA
// ==========================================
void loop() {
  // 1. PANTAU SENSOR RADAR (Hanya berjalan jika tidak terkunci)
  if (!isLocked && digitalRead(RADAR_PIN) == HIGH) {
    waktuRadarTerakhir = millis(); 
  } 
  
  // 2. LOGIKA DEEP SLEEP (Hanya berjalan jika tidak terkunci)
  if (!isLocked && modeAktif && (millis() - waktuRadarTerakhir > 5000)) {
    modeAktif = false;
    lcd.clear(); lcd.setCursor(0, 1); lcd.print("   MEMASUKI MODE    ");
    lcd.setCursor(0, 2); lcd.print("    HEMAT DAYA...   ");
    delay(1000);
    
    lcd.noBacklight(); servo1.write(90); servo2.write(90); 
    esp_sleep_enable_ext0_wakeup((gpio_num_t)RADAR_PIN, 1);
    esp_deep_sleep_start(); 
  }

  // 3. SEKTOR PENGUNCIAN SISTEM (REAL-TIME BACKGROUND MONITORING)
  if (isLocked) {
    // Jika sistem terkunci, lakukan polling kapasitas senyap setiap 1,5 detik
    if (millis() - waktuUpdateStandbyTerakhir > 1500) {
      waktuUpdateStandbyTerakhir = millis();
      cekKapasitasDanLock(); // Mengukur, menghitung Fuzzy, dan meng-update angka persentase baru di LCD saat lock
    }
    return; // Melewati alur pemilahan di bawah selama masih terkunci
  }

  // 4. MODE STANDBY & ALUR PEMILAHAN NORMAL
  if (modeAktif) {
    
    if (digitalRead(IR1) == LOGIKA_IR_MENDETEKSI) {
      waktuRadarTerakhir = millis(); 
      delay(2500); 
      
      // -- CEK POS 1 (KAPASITIF) --
      if (digitalRead(PROX_KAP) != LOGIKA_KAPASITIF_MENDETEKSI) { 
        servo1.write(180); delay(3000); servo1.write(90);
        notifikasiSampah("PLASTIK", 2); 
      } 
      else { 
        lcd.clear(); lcd.setCursor(0, 1); lcd.print("MENGIRIM KE POS 2...");
        servo1.write(0); delay(2500); servo1.write(90);
        
        unsigned long tungguPos2 = millis();
        bool suksesKePos2 = false;
        
        while (millis() - tungguPos2 <= 3000) {
          if (digitalRead(IR2) == LOGIKA_IR_MENDETEKSI) { 
            suksesKePos2 = true; 
            break; 
          }
        }

        if (suksesKePos2) {
          delay(3000); 
          // -- CEK POS 2 (INDUKTIF) --
          if (digitalRead(PROX_IND) == LOGIKA_INDUKTIF_MENDETEKSI) { 
            servo2.write(0); delay(3000); servo2.write(90);
            notifikasiSampah("LOGAM", 4);
          } else { 
            servo2.write(180); delay(3000); servo2.write(90);
            notifikasiSampah("KERTAS", 3);
          }
        } else {
          lcd.clear(); lcd.setCursor(0, 1); lcd.print("  SAMPAH TERSANGKUT ");
          myDFPlayer.play(6); delay(2500);
        }
      }

      while (digitalRead(IR1) == LOGIKA_IR_MENDETEKSI) {
        delay(100);
        waktuRadarTerakhir = millis(); 
      }

      lcd.clear();
      cekKapasitasDanLock();
      if (!isLocked) {
        waktuRadarTerakhir = millis(); 
        tampilkanStandby();
      }
    } 
    // --- MODE STANDBY NORMAL: BACKGROUND POLLING ---
    else {
      if (millis() - waktuUpdateStandbyTerakhir > 1500) {
        waktuUpdateStandbyTerakhir = millis();
        cekKapasitasDanLock(); 
        if (!isLocked) {
          tampilkanStandby(); 
        }
      }
    }
  }
}