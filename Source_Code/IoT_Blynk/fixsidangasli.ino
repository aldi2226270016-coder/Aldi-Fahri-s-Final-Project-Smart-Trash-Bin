/*******************
 SMART TRASH MONITORING
 ESP32 + Blynk + Servo + Ultrasonik

 FITUR:
 ✓ Pemilah Plastik
 ✓ Pemilah Kertas
 ✓ Pemilah Logam
 ✓ Monitoring Blynk
 ✓ Counter Sampah
 ✓ LCD Monitoring
 ✓ Standby Mode
 ✓ Error Handling
*******************/

// ================== BLYNK ==================

#define BLYNK_PRINT Serial

#define BLYNK_TEMPLATE_ID "TMPL6TbA58rM-"
#define BLYNK_TEMPLATE_NAME "Smart Trash Monitoring"
#define BLYNK_AUTH_TOKEN "ujgIX1w9eLQxa-cb1kXgekOMSPWJlRRt"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <time.h>

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>

// ================== WIFI ==================

char ssid[] = "Aldi";
char pass[] = "11111111";

// ================== NTP ==================

const char* ntpServer = "pool.ntp.org";

long gmtOffset_sec = 7 * 3600;
int daylightOffset_sec = 0;

// ================== LCD ==================

LiquidCrystal_I2C lcd(0x27, 20, 4);

// ================== SERVO ==================

Servo servoAtas;
Servo servoBawah;

#define SERVO_ATAS   13
#define SERVO_BAWAH  12

// ================== SENSOR PEMILAH ==================

#define IR_ATAS      26
#define IR_BAWAH     25

#define KAPASITIF    27
#define INDUKTIF     33

// ================== ULTRASONIK ==================

#define TRIG         5

#define ECHO1        34   // Plastik
#define ECHO2        35   // Kertas
#define ECHO3        14   // Logam

// ================== PARAMETER ==================

int tinggiKotak = 40;
int batasPenuh  = 5;

const int BATAS_MACET = 6000;

// ================== VARIABEL ==================

bool izinBawah = false;

unsigned long waktuLempar = 0;

// ================== COUNTER ==================

int jumlahPlastik = 0;
int jumlahKertas  = 0;
int jumlahLogam   = 0;

// ================== DATA SENSOR GLOBAL ==================

int jarakP = 0;
int jarakK = 0;
int jarakL = 0;

int persenP = 0;
int persenK = 0;
int persenL = 0;

// ================== MODE STANDBY ==================

bool modeStandby = false;

unsigned long waktuAktif = 0;
unsigned long batasStandby = 30000;

// ================== STATUS SISTEM ==================

bool statusError = false;
bool statusPenuh = false;

// ================== TIMER ==================

BlynkTimer timer;

// =====================================================
// ================== AMBIL WAKTU ======================
// =====================================================

String getWaktu()
{
  struct tm timeinfo;

  if (!getLocalTime(&timeinfo))
  {
    return "No Time";
  }

  char buffer[30];

  strftime(
    buffer,
    sizeof(buffer),
    "%d-%m-%Y %H:%M:%S",
    &timeinfo
  );

  return String(buffer);
}

// =====================================================
// ================== BACA ULTRASONIK ==================
// =====================================================

int bacaUltrasonic(int trig, int echo)
{
  long total = 0;

  int jumlahBaca = 5;

  for (int i = 0; i < jumlahBaca; i++)
  {
    digitalWrite(trig, LOW);
    delayMicroseconds(2);

    digitalWrite(trig, HIGH);
    delayMicroseconds(10);

    digitalWrite(trig, LOW);

    long durasi = pulseIn(echo, HIGH, 30000);

    int jarak = durasi * 0.034 / 2;

    if (durasi == 0)
    {
      jarak = tinggiKotak;
    }

    total += jarak;

    delay(10);
  }

  int rataJarak = total / jumlahBaca;

  if (rataJarak > tinggiKotak)
  {
    rataJarak = tinggiKotak;
  }

  if (rataJarak < 0)
  {
    rataJarak = 0;
  }

  return rataJarak;
}

// =====================================================
// ================== HITUNG PERSEN ====================
// =====================================================

int hitungPersen(int jarak)
{
  int persen = map(
    jarak,
    tinggiKotak,
    batasPenuh,
    0,
    100
  );

  return constrain(persen, 0, 100);
}

// =====================================================
// =============== UPDATE SENSOR =======================
// =====================================================

void UpdateSensor()
{
  jarakP =
  bacaUltrasonic(
    TRIG,
    ECHO1
  );

  jarakK =
  bacaUltrasonic(
    TRIG,
    ECHO2
  );

  jarakL =
  bacaUltrasonic(
    TRIG,
    ECHO3
  );

  persenP =
  hitungPersen(
    jarakP
  );

  persenK =
  hitungPersen(
    jarakK
  );

  persenL =
  hitungPersen(
    jarakL
  );
}


// =====================================================
// ================== CEK PENUH ========================
// =====================================================

void CekPenuh()
{
  UpdateSensor();

  statusPenuh =
      (persenP >= 90) ||
      (persenK >= 90) ||
      (persenL >= 90);
}

// =====================================================
// ================== MODE STANDBY =====================
// =====================================================

void MasukStandby()
{
  modeStandby = true;

  lcd.clear();

  lcd.setCursor(2, 1);
  lcd.print("MODE STANDBY");

  lcd.setCursor(1, 2);
  lcd.print("MENUNGGU SAMPAH");
}

void KeluarStandby()
{
  modeStandby = false;

  waktuAktif = millis();

  lcd.clear();

  lcd.setCursor(2, 1);
  lcd.print("SISTEM AKTIF");

  delay(1000);

  lcd.clear();
}

// =====================================================
// ================== MONITOR LCD ======================
// =====================================================

void TampilMonitoring()
{
  lcd.clear();

  lcd.setCursor(2,0);
  lcd.print("KAPASITAS SAMPAH");

lcd.setCursor(0,1);
lcd.printf("P:%3d%%", persenP);

lcd.setCursor(10,1);
lcd.printf("JP:%2d", jumlahPlastik);

lcd.setCursor(0,2);
lcd.printf("K:%3d%%", persenK);

lcd.setCursor(10,2);
lcd.printf("JK:%2d", jumlahKertas);

lcd.setCursor(0,3);
lcd.printf("L:%3d%%", persenL);

lcd.setCursor(10,3);
lcd.printf("JL:%2d", jumlahLogam);
}

void TampilStatusLCD(String status)
{
  lcd.clear();

  lcd.setCursor(2,0);
  lcd.print("STATUS SISTEM");

  if(status == "PLASTIK PENUH")
  {
    lcd.setCursor(2,2);
    lcd.print("PLASTIK PENUH");
  }

  else if(status == "KERTAS PENUH")
  {
    lcd.setCursor(3,2);
    lcd.print("KERTAS PENUH");
  }

  else if(status == "LOGAM PENUH")
  {
    lcd.setCursor(4,2);
    lcd.print("LOGAM PENUH");
  }

  else if(status == "PLASTIK & LOGAM PENUH")
  {
    lcd.setCursor(2,2);
    lcd.print("PLASTIK & LOGAM");

    lcd.setCursor(7,3);
    lcd.print("PENUH");
  }

  else if(status == "PLASTIK & KERTAS PENUH")
  {
    lcd.setCursor(1,2);
    lcd.print("PLASTIK & KERTAS");

    lcd.setCursor(7,3);
    lcd.print("PENUH");
  }

  else if(status == "KERTAS & LOGAM PENUH")
  {
    lcd.setCursor(2,2);
    lcd.print("KERTAS & LOGAM");

    lcd.setCursor(7,3);
    lcd.print("PENUH");
  }

  else if(status == "SEMUA PENUH")
  {
    lcd.setCursor(3,2);
    lcd.print("SEMUA PENUH");
  }
  
  else if(status == "ERROR MACET")
  {
    lcd.setCursor(3,2);
    lcd.print("ERROR MACET");

  }

  delay(3000);
}

// =====================================================
// ================== KIRIM DATA BLYNK =================
// =====================================================

void KirimBlynk()
{
  UpdateSensor();
  CekPenuh();

  // ===== Persentase =====
  Blynk.virtualWrite(V0, persenP);
  Blynk.virtualWrite(V1, persenK);
  Blynk.virtualWrite(V2, persenL);

  // ===== Counter =====
  Blynk.virtualWrite(V3, jumlahPlastik);
  Blynk.virtualWrite(V4, jumlahKertas);
  Blynk.virtualWrite(V5, jumlahLogam);

  // ===== Jarak =====
  Blynk.virtualWrite(V10, jarakP);
  Blynk.virtualWrite(V11, jarakK);
  Blynk.virtualWrite(V12, jarakL);

  // ===== Status =====
 String status = "NORMAL";

// ERROR MACET
if (statusError)
{
  status = "ERROR MACET";
}

// SEMUA PENUH
else if (
    persenP >= 90 &&
    persenK >= 90 &&
    persenL >= 90
)
{
  status = "SEMUA PENUH";
}

// DUA TEMPAT SAMPAH PENUH
else if (
    persenP >= 90 &&
    persenL >= 90
)
{
  status = "PLASTIK & LOGAM PENUH";
}
else if (
    persenP >= 90 &&
    persenK >= 90
)
{
  status = "PLASTIK & KERTAS PENUH";
}
else if (
    persenK >= 90 &&
    persenL >= 90
)
{
  status = "KERTAS & LOGAM PENUH";
}

// SATU TEMPAT SAMPAH PENUH
else if (persenP >= 90)
{
  status = "PLASTIK PENUH";
}
else if (persenK >= 90)
{
  status = "KERTAS PENUH";
}
else if (persenL >= 90)
{
  status = "LOGAM PENUH";
}

  Blynk.virtualWrite(V6, status);

  // ===== Waktu =====
  Blynk.virtualWrite(V7, getWaktu());

 if(status != "NORMAL")
{
   TampilStatusLCD(status);
}
else
{
   TampilMonitoring();
}

}
// =====================================================
// ================== POSISI 1 =========================
// =====================================================

void ProsesAtas()
{
  statusError = false;

  lcd.clear();

  lcd.setCursor(1, 0);
  lcd.print("SAMPAH TERDETEKSI");

  lcd.setCursor(3, 2);
  lcd.print("POS 1 DETEKSI");

  delay(3000);

  // ===== Sampah Padat =====
  if (digitalRead(KAPASITIF) == HIGH)
  {
    lcd.clear();

    lcd.setCursor(4, 1);
    lcd.print("LANJUT POS2");

    izinBawah = true;

    waktuLempar = millis();

    servoAtas.write(10);

    delay(2000);

    servoAtas.write(90);
  }

  // ===== Plastik =====
  else
  {
    jumlahPlastik++;

    lcd.clear();

    lcd.setCursor(2, 1);
    lcd.print("SAMPAH PLASTIK");

    servoAtas.write(170);

    delay(2000);

    servoAtas.write(90);
  }
}

// =====================================================
// ================== POSISI 2 =========================
// =====================================================

void ProsesBawah()
{
  lcd.clear();

  lcd.setCursor(3, 2);
  lcd.print("POS2 DETEKSI");

  delay(3000);

  // ===== Logam =====
  if (digitalRead(INDUKTIF) == LOW)
  {
    jumlahLogam++;

    lcd.clear();

    lcd.setCursor(3, 1);
    lcd.print(" SAMPAH LOGAM");

    servoBawah.write(10);
  }

  // ===== Kertas =====
  else
  {
    jumlahKertas++;

    lcd.clear();

    lcd.setCursor(2, 1);
    lcd.print(" SAMPAH KERTAS");

    servoBawah.write(170);
  }

  delay(2000);

  servoBawah.write(90);

  izinBawah = false;
}

// =====================================================
// ================== SETUP ============================
// =====================================================

void setup()
{
  Serial.begin(115200);

  // ===== Input =====
  pinMode(IR_ATAS, INPUT);
  pinMode(IR_BAWAH, INPUT);

  pinMode(KAPASITIF, INPUT);
  pinMode(INDUKTIF, INPUT);

  // ===== Ultrasonik =====
  pinMode(TRIG, OUTPUT);

  pinMode(ECHO1, INPUT);
  pinMode(ECHO2, INPUT);
  pinMode(ECHO3, INPUT);

  // ===== Servo =====
  servoAtas.attach(SERVO_ATAS);
  servoBawah.attach(SERVO_BAWAH);

  servoAtas.write(90);
  servoBawah.write(90);

  // ===== LCD =====
  lcd.init();
  lcd.backlight();

  lcd.print("SISTEM SIAP");

  // ===== Blynk =====
  Blynk.begin(
    BLYNK_AUTH_TOKEN,
    ssid,
    pass
  );

  // ===== NTP =====
  configTime(
    gmtOffset_sec,
    daylightOffset_sec,
    ntpServer
  );

  // ===== Timer =====
  timer.setInterval(
    3000L,
    KirimBlynk
  );

  delay(2000);

  lcd.clear();

  waktuAktif = millis();
}

// =====================================================
// ================== LOOP =============================
// =====================================================

void loop()
{
  Blynk.run();
  timer.run();

  // =========================
  // CEK TEMPAT SAMPAH PENUH
  // =========================

  CekPenuh();

  if(statusPenuh)
  {
    servoAtas.write(90);
    servoBawah.write(90);

    String statusPenuhText = "";

    if(persenP >= 90 && persenK >= 90 && persenL >= 90)
      statusPenuhText = "SEMUA PENUH";

    else if(persenP >= 90 && persenK >= 90)
      statusPenuhText = "PLASTIK & KERTAS PENUH";

    else if(persenP >= 90 && persenL >= 90)
      statusPenuhText = "PLASTIK & LOGAM PENUH";

    else if(persenK >= 90 && persenL >= 90)
      statusPenuhText = "KERTAS & LOGAM PENUH";

    else if(persenP >= 90)
      statusPenuhText = "PLASTIK PENUH";

    else if(persenK >= 90)
      statusPenuhText = "KERTAS PENUH";

    else if(persenL >= 90)
      statusPenuhText = "LOGAM PENUH";

    lcd.clear();

    lcd.setCursor(0,0);
    lcd.print("KOSONGKAN DULU");

    lcd.setCursor(0,2);
    lcd.print(statusPenuhText);

    Blynk.virtualWrite(V6, statusPenuhText);

    delay(500);

    return;   // Hentikan seluruh proses pemilahan
  }

  // =========================
  // STANDBY
  // =========================

  if (
      millis() - waktuAktif > batasStandby &&
      !modeStandby
     )
  {
    MasukStandby();
  }

  // =========================
  // DETEKSI ATAS
  // =========================

  if (digitalRead(IR_ATAS) == LOW)
  {
    if (modeStandby)
    {
      KeluarStandby();
    }

    waktuAktif = millis();

    ProsesAtas();
  }

  // ===== Standby =====
  if (
    millis() - waktuAktif > batasStandby &&
    !modeStandby
  )
  {
    MasukStandby();
  }

  // ===== Deteksi Atas =====
  if (digitalRead(IR_ATAS) == LOW)
  {
    if (modeStandby)
    {
      KeluarStandby();
    }

    waktuAktif = millis();

    ProsesAtas();
  }

  // ===== Deteksi Bawah =====
  if (izinBawah)
  {
    if (digitalRead(IR_BAWAH) == LOW)
    {
      waktuAktif = millis();

      ProsesBawah();
    }

    // ===== Error Macet =====
    else if (
      millis() - waktuLempar > BATAS_MACET
)
{
    statusError = true;

    servoAtas.write(90);
    servoBawah.write(90);

    izinBawah = false;

    lcd.clear();
    lcd.setCursor(3,2);
    lcd.print("ERROR MACET");

    Blynk.virtualWrite(V6,"ERROR MACET");

    while(true)
    {
      Blynk.run();
    }
}
  }
}