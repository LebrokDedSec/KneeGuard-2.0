#include <BluetoothSerial.h>
#include <Wire.h>
#include <math.h>

// System monitorowania kąta zgięcia kolana (KneeGuard)
// Dwa MPU6050: jeden nad kolanem (udo), drugi pod kolanem (podudzie)
// Kąt zgięcia kolana = różnica orientacji między czujnikami
// Fuzja danych: filtr Kalmana dla roll/pitch, integracja żyroskopu dla yaw

// ===== I2C & adresy =====
#define I2C_SDA 21
#define I2C_SCL 22
#define MPU1_ADDR 0x68   // Czujnik 1: nad kolanem (udo) - AD0 = GND/NC
#define MPU2_ADDR 0x69   // Czujnik 2: pod kolanem (podudzie) - AD0 = 3.3V

// ===== Parametry wysyłania (Urządzenie → aplikacja) =====
#define SEND_FREQ_HZ 50                           // częstotliwość wysyłania danych
#define SEND_PERIOD_US (1000000 / SEND_FREQ_HZ)   // okres w mikrosekundach
//Tryb “sport/dynamiczny”: 50 Hz
//Tryb "standardowy": 25 Hz
//Tryb “rehab/oszczędny”: 10 Hz

BluetoothSerial BT;      // Bluetooth Classic SPP

// ===== Skale (±8 g, ±500 dps) =====
static const float ACC_LSB_PER_G    = 4096.0f;
static const float GYRO_LSB_PER_DPS = 65.5f;

// ===== Kalman 1D =====
struct Kalman1D { float angle_deg = 0.0f; float uncert = 4.0f; };
static const float KALMAN_Q = 16.0f; // (deg/s)^2
static const float KALMAN_R =  9.0f; // (deg)^2

static inline void kalman_update(Kalman1D& k, float rate_dps, float meas_deg, float dt) {
  k.angle_deg += dt * rate_dps;
  k.uncert    += dt * dt * KALMAN_Q;
  float K = k.uncert / (k.uncert + KALMAN_R);
  k.angle_deg += K * (meas_deg - k.angle_deg);
  k.uncert     = (1.0f - K) * k.uncert;
}

static inline float wrap180(float a) {
  while (a > 180) a -= 360;
  while (a < -180) a += 360;
  return a;
}

// Bezpieczna różnica kątowa - unika skoku przy przejściu przez ±180°
static inline float angleDiff(float a1, float a2) {
  float sinDiff = sinf((a1 - a2) * PI / 180.0f);
  float cosDiff = cosf((a1 - a2) * PI / 180.0f);
  return atan2f(sinDiff, cosDiff) * 180.0f / PI;
}

// ===== Stan IMU =====
struct ImuState {
  float ax = 0, ay = 0, az = 0, gx = 0, gy = 0, gz = 0;
  float bgx = 0, bgy = 0, bgz = 0;
  Kalman1D k_roll, k_pitch;
  float yaw = 0;
  float off_roll = 0, off_pitch = 0, off_yaw = 0;
};

ImuState imu1, imu2;
uint32_t last_us = 0;
uint32_t last_send_us = 0;              // throttling wysyłania (50 Hz)
uint32_t err_count1 = 0, err_count2 = 0; // liczniki błędów I2C
uint32_t last_err_print = 0;            // throttling komunikatów błędów

// ===== I2C helpers =====
bool writeReg(uint8_t addr, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

bool readBurst(uint8_t addr, uint8_t startReg, uint8_t* buf, size_t n) {
  Wire.beginTransmission(addr);
  Wire.write(startReg);
  if (Wire.endTransmission(false) != 0) return false; // RESTART
  if (Wire.requestFrom((int)addr, (int)n, (int)true) != n) return false;
  for (size_t i = 0; i < n; i++) buf[i] = Wire.read();
  return true;
}

bool mpuInit(uint8_t addr) {
  if (!writeReg(addr, 0x6B, 0x80)) return false; delay(100);  // reset
  if (!writeReg(addr, 0x6B, 0x01)) return false; delay(10);   // wake + PLL
  if (!writeReg(addr, 0x1A, 0x05)) return false;              // DLPF
  if (!writeReg(addr, 0x1C, 0x10)) return false;              // ±8 g
  if (!writeReg(addr, 0x1B, 0x08)) return false;              // ±500 dps
  delay(10);
  return true;
}

bool readIMU(uint8_t addr, float& ax, float& ay, float& az, float& gx, float& gy, float& gz) {
  uint8_t raw[14];
  if (!readBurst(addr, 0x3B, raw, 14)) return false;
  auto s16 = [&](int i) -> int16_t { return (int16_t)((raw[i] << 8) | raw[i + 1]); };
  int16_t AccX = s16(0), AccY = s16(2), AccZ = s16(4);
  int16_t GyX  = s16(8), GyY  = s16(10), GyZ = s16(12);
  ax = AccX / ACC_LSB_PER_G; ay = AccY / ACC_LSB_PER_G; az = AccZ / ACC_LSB_PER_G;
  gx = GyX  / GYRO_LSB_PER_DPS; gy = GyY  / GYRO_LSB_PER_DPS; gz = GyZ / GYRO_LSB_PER_DPS;
  return true;
}

static inline void accelAngles(float ax, float ay, float az, float& roll, float& pitch) {
  roll  = atan2f(ay, sqrtf(ax * ax + az * az)) * 180.0f / (float)PI;
  pitch = -atan2f(ax, sqrtf(ay * ay + az * az)) * 180.0f / (float)PI;
}

void calibrateGyro(ImuState& S, uint8_t addr, int N = 1200) {
  S.bgx = S.bgy = S.bgz = 0;
  for (int i = 0; i < N; i++) {
    float ax, ay, az, gx, gy, gz;
    if (readIMU(addr, ax, ay, az, gx, gy, gz)) { S.bgx += gx; S.bgy += gy; S.bgz += gz; }
    delay(1);
  }
  S.bgx /= N; S.bgy /= N; S.bgz /= N;
}

// ===== WHO_AM_I =====
int readWho(uint8_t addr) {
  Wire.beginTransmission(addr);
  Wire.write(0x75);
  if (Wire.endTransmission(false) == 0 && Wire.requestFrom((int)addr, 1, (int)true) == 1)
    return Wire.read();
  return -1;
}

// ===== Wspólna kalibracja =====
// Kalibracja kompensuje nieprecyzyjny montaż czujników (przekoszenie, obrót).
// PROCEDURA: 
//   1. Załóż urządzenie na nogę
//   2. Wyprostuj kolano (noga stojąca, oś kolana równolegle do podłogi)
//   3. Wyślij komendę 'calib'
// EFEKT:
//   - Aktualna pozycja każdego czujnika = 0° (bez względu na montaż)
//   - Kąt zgięcia kolana (pitch2-pitch1) = 0° dla wyprostowanej nogi
//   - Dalsze pomiary są względem tej pozycji referencyjnej
void processCalib(bool from_bt) {
  // Zerowanie aktualnej pozycji obu IMU (offsety roll/pitch/yaw).
  imu1.off_roll  = imu1.k_roll.angle_deg;
  imu1.off_pitch = imu1.k_pitch.angle_deg;
  imu1.off_yaw   = imu1.yaw;
  imu2.off_roll  = imu2.k_roll.angle_deg;
  imu2.off_pitch = imu2.k_pitch.angle_deg;
  imu2.off_yaw   = imu2.yaw;

  // Ustalamy referencję dla kąta kolana

  if (from_bt) {
    if (BT.hasClient()) BT.println("[CALIB] OK (both IMUs = 0/0/0)");
    Serial.println("[CALIB] OK via BT");
  } else {
    Serial.println("[CALIB] OK via USB");
    if (BT.hasClient()) BT.println("[CALIB] OK (both IMUs = 0/0/0)");
  }
}

// ===== Uniwersalny parser linii =====
bool readLine(Stream& io, String& out, String& buf, size_t maxLen = 64) {
  while (io.available()) {
    char c = (char)io.read();
    // Obsługa CR i LF jako zakończenie linii (kompatybilność z różnymi serial monitorami)
    if (c == '\n' || c == '\r') {
      if (buf.length() > 0) {
        out = buf; buf = ""; out.trim(); out.toLowerCase();
        return true;
      }
      continue; // ignoruj puste linie lub dodatkowy CR/LF
    }
    if (buf.length() < (int)maxLen) buf += c;
  }
  return false;
}

// Oddzielne bufory dla USB i BT
String usbBuf, btBuf;

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(5);
  delay(300);
  Serial.printf("\n[BOOT] Chip: %s | USB+BT calib commands\n", ESP.getChipModel());

  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000);
  delay(200);

  Serial.printf("[WHO] 0x68=0x%02X, 0x69=0x%02X (expect 0x68)\n", readWho(MPU1_ADDR), readWho(MPU2_ADDR));
  bool ok1 = mpuInit(MPU1_ADDR), ok2 = mpuInit(MPU2_ADDR);
  Serial.printf("[INIT] 0x68: %s | 0x69: %s\n", ok1 ? "OK" : "FAIL", ok2 ? "OK" : "FAIL");
  
  if (!ok1 || !ok2) {
    Serial.println("[ERROR] Sprawdz polaczenia I2C!");
    Serial.println("[ERROR] MPU1 (0x68): SDA=21, SCL=22, AD0=GND");
    Serial.println("[ERROR] MPU2 (0x69): SDA=21, SCL=22, AD0=3.3V");
    if (!ok1 && !ok2) Serial.println("[ERROR] Oba czujniki nie odpowiadaja - mozliwy problem z I2C");
  }

  Serial.println("[CAL] keep still ~1.2s...");
  calibrateGyro(imu1, MPU1_ADDR);
  calibrateGyro(imu2, MPU2_ADDR);

  Wire.setClock(400000); // szybciej po konfiguracji

  bool btok = BT.begin("KneeGuard");
  BT.setTimeout(5);
  Serial.printf("[BT] begin: %s\n", btok ? "OK" : "FAIL");
  BT.register_callback([](esp_spp_cb_event_t e, esp_spp_cb_param_t*) {
    if (e == ESP_SPP_SRV_OPEN_EVT)  Serial.println("[BT] client CONNECTED");
    if (e == ESP_SPP_CLOSE_EVT)     Serial.println("[BT] client DISCONNECTED");
  });

  last_us = micros();
  Serial.println("[INFO] labels: time, roll1, pitch1, yaw1, roll2, pitch2, yaw2, knee_angle, inv1, inv2");
  Serial.println("[INFO] KALIBRACJA: wyprostuj kolano, postaw noge pionowo, wyslij 'calib'");
  Serial.println("[INFO] Kalibracja kompensuje przekoszenie czujnikow wzgledem nogi");
  Serial.println("[INFO] IMU1=udo (thigh), IMU2=podudzie (shank), knee_angle=pitch2-pitch1");
}

void loop() {
  // --- Komendy USB ---
  String cmd;
  if (readLine(Serial, cmd, usbBuf)) {
    if (cmd == "calib") processCalib(false);
    else Serial.printf("[WARN] unknown cmd: %s\n", cmd.c_str());
  }

  // --- Komendy BT ---
  if (BT.hasClient() && readLine(BT, cmd, btBuf)) {
    if (cmd == "calib") processCalib(true);
    else if (BT.hasClient()) { BT.print("[WARN] unknown cmd: "); BT.println(cmd); }
  }

  // --- dt (zabezpieczenie przed skokami timera) ---
  uint32_t now_us = micros();
  float dt = (now_us - last_us) / 1e6f;
  if (dt <= 0) dt = 0.004f;  // typowy krok ~4 ms przy plotterze
  if (dt > 0.02f) dt = 0.02f; // nie integruj za duże przerwy
  last_us = now_us;

  // --- IMU1 ---
  float rAcc1 = 0, pAcc1 = 0; bool okR1 = false, okR2 = false;
  if (readIMU(MPU1_ADDR, imu1.ax, imu1.ay, imu1.az, imu1.gx, imu1.gy, imu1.gz)) {
    okR1 = true;
    imu1.gx -= imu1.bgx; imu1.gy -= imu1.bgy; imu1.gz -= imu1.bgz;
    accelAngles(imu1.ax, imu1.ay, imu1.az, rAcc1, pAcc1);
    kalman_update(imu1.k_roll,  imu1.gx, rAcc1, dt);
    kalman_update(imu1.k_pitch, imu1.gy, pAcc1, dt);
    imu1.yaw = wrap180(imu1.yaw + imu1.gz * dt); // yaw będzie dryfował (brak magnetometru)
  } else {
    err_count1++;
  }

  // --- IMU2 ---
  float rAcc2 = 0, pAcc2 = 0;
  if (readIMU(MPU2_ADDR, imu2.ax, imu2.ay, imu2.az, imu2.gx, imu2.gy, imu2.gz)) {
    okR2 = true;
    imu2.gx -= imu2.bgx; imu2.gy -= imu2.bgy; imu2.gz -= imu2.bgz;
    accelAngles(imu2.ax, imu2.ay, imu2.az, rAcc2, pAcc2);
    kalman_update(imu2.k_roll,  imu2.gx, rAcc2, dt);
    kalman_update(imu2.k_pitch, imu2.gy, pAcc2, dt);
    imu2.yaw = wrap180(imu2.yaw + imu2.gz * dt);
  } else {
    err_count2++;
  }

  // --- Diagnostyka błędów (raz na sekundę) ---
  if (millis() - last_err_print > 1000) {
    if (!okR1 || !okR2) {
      Serial.printf("[ERROR] IMU1(0x68):%s [%lu err] | IMU2(0x69):%s [%lu err]\n",
                    okR1 ? "OK" : "FAIL", err_count1,
                    okR2 ? "OK" : "FAIL", err_count2);
    }
    last_err_print = millis();
  }

  // --- Korekta o offset kalibracji ---
  float roll1  = okR1 ? (imu1.k_roll.angle_deg  - imu1.off_roll)  : -999.0f;
  float pitch1 = okR1 ? (imu1.k_pitch.angle_deg - imu1.off_pitch) : -999.0f;
  float yaw1   = okR1 ? wrap180(imu1.yaw - imu1.off_yaw) : -999.0f;
  float roll2  = okR2 ? (imu2.k_roll.angle_deg  - imu2.off_roll)  : -999.0f;
  float pitch2 = okR2 ? (imu2.k_pitch.angle_deg - imu2.off_pitch) : -999.0f;
  float yaw2   = okR2 ? wrap180(imu2.yaw - imu2.off_yaw) : -999.0f;

  // --- Surowe flagi odwrócenia (bez histerezy): az<0 => upside-down ---
  bool inv1_raw = (okR1 && (imu1.az < 0.0f));
  bool inv2_raw = (okR2 && (imu2.az < 0.0f));

  // --- Kąt zgięcia kolana: zależy od tego czy czujniki mają taki sam stan odwrócenia ---
  float knee_angle = -999.0f;
  if (okR1 && okR2) {
    if (inv1_raw == inv2_raw) {
      // Oba czujniki w tym samym stanie (oba normalne lub oba odwrócone)
      knee_angle = abs(roll2 - roll1);
    } else {
      // Czujniki w różnych stanach (jeden odwrócony, drugi nie)
      knee_angle = 180.0f - roll2 - roll1;
    }
  }

  // --- Wysyłanie danych na USB/BT (50 Hz = co 20 ms) ---
  if (now_us - last_send_us >= SEND_PERIOD_US) {
    last_send_us = now_us;

    // USB: etykiety pod Plotter + status odwrócenia
    Serial.print("time:");   Serial.print((unsigned long)now_us);
    Serial.print(" roll1:");  Serial.print(roll1, 2);
    Serial.print(" pitch1:"); Serial.print(pitch1, 2);
    Serial.print(" yaw1:");   Serial.print(yaw1, 2);
    Serial.print(" roll2:");  Serial.print(roll2, 2);
    Serial.print(" pitch2:"); Serial.print(pitch2, 2);
    Serial.print(" yaw2:");   Serial.print(yaw2, 2);
    Serial.print(" knee_angle:"); Serial.print(knee_angle, 2);
    Serial.print(" inv1:"); Serial.print(inv1_raw ? 1 : 0);
    Serial.print(" inv2:"); Serial.println(inv2_raw ? 1 : 0);

    // BT: CSV bez etykiet (szybszy parsing po stronie PC/telefonu)
    if (BT.hasClient()) {
      char out[128];
      int n = snprintf(out, sizeof(out),
                       "%lu,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%d,%d\n",
                       (unsigned long)now_us,
                       roll1, pitch1, yaw1, roll2, pitch2, yaw2, knee_angle,
                       inv1_raw ? 1 : 0, inv2_raw ? 1 : 0);
      BT.write((const uint8_t*)out, n);
    }
  }
}
