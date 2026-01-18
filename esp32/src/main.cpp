#include <BluetoothSerial.h>
#include <Wire.h>
#include <math.h>

/*
  KneeGuard – firmware ESP32 (Arduino)

  Cel: pomiar kąta zgięcia kolana z wykorzystaniem dwóch modułów MPU6050
       (IMU1: udo, IMU2: podudzie) oraz wysyłanie danych przez USB Serial i Bluetooth.

  Najważniejsze założenia:
  - odczyt: akcelerometr + żyroskop (MPU6050)
  - roll/pitch: filtr Kalmana 1D (osobno dla każdej osi)
  - yaw: integracja żyroskopu (bez magnetometru -> dryf)
  - kąt kolana: stabilna, dodatnia różnica kątowa roll (0..180°)
*/

// ============================================================================
// 1) Konfiguracja sprzętu i parametrów
// ============================================================================

static const uint8_t I2C_SDA = 21;
static const uint8_t I2C_SCL = 22;

static const uint8_t MPU1_ADDR = 0x68; // IMU1 (udo)      – AD0 = GND/NC
static const uint8_t MPU2_ADDR = 0x69; // IMU2 (podudzie) – AD0 = 3.3V

static const uint32_t SEND_FREQ_HZ   = 50;                    // docelowa częstotliwość telemetrii
static const uint32_t SEND_PERIOD_US = 1000000UL / SEND_FREQ_HZ;

static const char* BT_DEVICE_NAME = "KneeGuard"; // nazwa widoczna przy parowaniu

// Skale MPU6050 (konfiguracja: ±8 g, ±500 dps)
static const float ACC_LSB_PER_G    = 4096.0f;
static const float GYRO_LSB_PER_DPS = 65.5f;

// ============================================================================
// 2) Struktury danych
// ============================================================================

struct Kalman1D {
  float angle_deg = 0.0f; // estymowana wartość kąta
  float uncert    = 4.0f; // niepewność estymacji
};

struct ImuState {
  // surowe próbki (po przeskalowaniu)
  float ax = 0, ay = 0, az = 0;
  float gx = 0, gy = 0, gz = 0;

  // bias żyroskopu (wyznaczony w kalibracji "keep still")
  float bgx = 0, bgy = 0, bgz = 0;

  // fuzja: roll/pitch z Kalmana, yaw integrowany z gz
  Kalman1D k_roll, k_pitch;
  float yaw = 0;

  // offsety po komendzie "calib" (referencja dla montażu na nodze)
  float off_roll = 0, off_pitch = 0, off_yaw = 0;
};

// ============================================================================
// 3) Zmienne globalne
// ============================================================================

BluetoothSerial BT; // Bluetooth Classic SPP

ImuState imu1, imu2;

uint32_t last_us       = 0;
uint32_t last_send_us  = 0;
uint32_t err_count1    = 0;
uint32_t err_count2    = 0;
uint32_t last_err_print = 0;

String usbBuf;
String btBuf;

// ============================================================================
// 4) Matematyka (kąty, filtr Kalmana)
// ============================================================================

// Q = niepewność modelu (żyroskop: dryf/szum), R = niepewność pomiaru (akcelerometr)
static const float KALMAN_Q = 16.0f; // (deg/s)^2
static const float KALMAN_R =  1.0f; // (deg)^2

static inline void kalmanUpdate(Kalman1D& k, float rate_dps, float meas_deg, float dt) {
  k.angle_deg += dt * rate_dps;
  k.uncert    += dt * dt * KALMAN_Q;

  const float K = k.uncert / (k.uncert + KALMAN_R);
  k.angle_deg += K * (meas_deg - k.angle_deg);
  k.uncert     = (1.0f - K) * k.uncert;
}

static inline float wrap180(float deg) {
  while (deg > 180) deg -= 360;
  while (deg < -180) deg += 360;
  return deg;
}

// Minimalna różnica kątowa (wynik w [-180..180]), odporna na przejście przez ±180°.
static inline float angleDiffDeg(float a1_deg, float a2_deg) {
  const float rad = (a1_deg - a2_deg) * PI / 180.0f;
  return atan2f(sinf(rad), cosf(rad)) * 180.0f / (float)PI;
}

// Kąty z akcelerometru (roll/pitch) – atan2 daje poprawny znak i ćwiartkę.
static inline void accelAnglesDeg(float ax, float ay, float az, float& roll_deg, float& pitch_deg) {
  roll_deg  = atan2f(ay, az) * 180.0f / (float)PI;                       // [-180..180]
  pitch_deg = atan2f(-ax, sqrtf(ay * ay + az * az)) * 180.0f / (float)PI; // [-90..90]
}

// ============================================================================
// 5) I2C + MPU6050 (obsługa niskopoziomowa)
// ============================================================================

static bool writeReg(uint8_t addr, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

static bool readBurst(uint8_t addr, uint8_t startReg, uint8_t* buf, size_t n) {
  Wire.beginTransmission(addr);
  Wire.write(startReg);
  if (Wire.endTransmission(false) != 0) return false; // repeated start
  if (Wire.requestFrom((int)addr, (int)n, (int)true) != (int)n) return false;
  for (size_t i = 0; i < n; i++) buf[i] = Wire.read();
  return true;
}

static int readWhoAmI(uint8_t addr) {
  Wire.beginTransmission(addr);
  Wire.write(0x75); // WHO_AM_I
  if (Wire.endTransmission(false) == 0 && Wire.requestFrom((int)addr, 1, (int)true) == 1) return Wire.read();
  return -1;
}

static bool mpuInit(uint8_t addr) {
  if (!writeReg(addr, 0x6B, 0x80)) return false; delay(100); // reset
  if (!writeReg(addr, 0x6B, 0x01)) return false; delay(10);  // wake + PLL
  if (!writeReg(addr, 0x1A, 0x05)) return false;             // DLPF
  if (!writeReg(addr, 0x1C, 0x10)) return false;             // accel ±8 g
  if (!writeReg(addr, 0x1B, 0x08)) return false;             // gyro ±500 dps
  delay(10);
  return true;
}

static bool readIMU(uint8_t addr, float& ax, float& ay, float& az, float& gx, float& gy, float& gz) {
  uint8_t raw[14];
  if (!readBurst(addr, 0x3B, raw, sizeof(raw))) return false;

  auto s16 = [&](int i) -> int16_t { return (int16_t)((raw[i] << 8) | raw[i + 1]); };
  const int16_t AccX = s16(0), AccY = s16(2), AccZ = s16(4);
  const int16_t GyX  = s16(8), GyY  = s16(10), GyZ = s16(12);

  ax = AccX / ACC_LSB_PER_G;
  ay = AccY / ACC_LSB_PER_G;
  az = AccZ / ACC_LSB_PER_G;

  gx = GyX / GYRO_LSB_PER_DPS;
  gy = GyY / GYRO_LSB_PER_DPS;
  gz = GyZ / GYRO_LSB_PER_DPS;

  return true;
}

// ============================================================================
// 6) Kalibracje
// ============================================================================

static void calibrateGyro(ImuState& imu, uint8_t addr, int N = 1200) {
  imu.bgx = imu.bgy = imu.bgz = 0;
  for (int i = 0; i < N; i++) {
    float ax, ay, az, gx, gy, gz;
    if (readIMU(addr, ax, ay, az, gx, gy, gz)) {
      imu.bgx += gx;
      imu.bgy += gy;
      imu.bgz += gz;
    }
    delay(1);
  }
  imu.bgx /= N;
  imu.bgy /= N;
  imu.bgz /= N;
}

// Kalibracja "montażowa" – ustawia bieżącą pozycję obu czujników jako punkt odniesienia.
// PROCEDURA:
//   1) załóż urządzenie na nogę,
//   2) wyprostuj kolano (pozycja neutralna),
//   3) wyślij komendę: calib
static void processCalib(bool from_bt) {
  imu1.off_roll  = imu1.k_roll.angle_deg;
  imu1.off_pitch = imu1.k_pitch.angle_deg;
  imu1.off_yaw   = imu1.yaw;

  imu2.off_roll  = imu2.k_roll.angle_deg;
  imu2.off_pitch = imu2.k_pitch.angle_deg;
  imu2.off_yaw   = imu2.yaw;

  if (from_bt) {
    Serial.println("[CALIB] OK via BT");
    if (BT.hasClient()) BT.println("[CALIB] OK (both IMUs = 0/0/0)");
  } else {
    Serial.println("[CALIB] OK via USB");
    if (BT.hasClient()) BT.println("[CALIB] OK (both IMUs = 0/0/0)");
  }
}

// ============================================================================
// 7) Komendy (USB/BT) i pomocnicze funkcje runtime
// ============================================================================

static bool readLine(Stream& io, String& out, String& buf, size_t maxLen = 64) {
  while (io.available()) {
    const char c = (char)io.read();
    if (c == '\n' || c == '\r') {
      if (buf.length() > 0) {
        out = buf;
        buf = "";
        out.trim();
        out.toLowerCase();
        return true;
      }
      continue;
    }
    if (buf.length() < (int)maxLen) buf += c;
  }
  return false;
}

static void handleCommands() {
  String cmd;

  // USB
  if (readLine(Serial, cmd, usbBuf)) {
    if (cmd == "calib") processCalib(false);
    else Serial.printf("[WARN] unknown cmd: %s\n", cmd.c_str());
  }

  // Bluetooth
  if (BT.hasClient() && readLine(BT, cmd, btBuf)) {
    if (cmd == "calib") processCalib(true);
    else if (BT.hasClient()) {
      BT.print("[WARN] unknown cmd: ");
      BT.println(cmd);
    }
  }
}

static float computeDtSeconds(uint32_t now_us) {
  float dt = (now_us - last_us) / 1e6f;
  if (dt <= 0) dt = 0.004f;
  if (dt > 0.02f) dt = 0.02f;
  last_us = now_us;
  return dt;
}

static bool updateImu(ImuState& imu, uint8_t addr, float dt, uint32_t& errCount) {
  float rAcc = 0, pAcc = 0;

  if (!readIMU(addr, imu.ax, imu.ay, imu.az, imu.gx, imu.gy, imu.gz)) {
    errCount++;
    return false;
  }

  // korekcja bias
  imu.gx -= imu.bgx;
  imu.gy -= imu.bgy;
  imu.gz -= imu.bgz;

  // pomiar roll/pitch z akcelerometru + aktualizacja Kalmana
  accelAnglesDeg(imu.ax, imu.ay, imu.az, rAcc, pAcc);
  kalmanUpdate(imu.k_roll,  imu.gx, rAcc, dt);
  kalmanUpdate(imu.k_pitch, imu.gy, pAcc, dt);

  // yaw integrowany z żyroskopu (będzie dryfować)
  imu.yaw = wrap180(imu.yaw + imu.gz * dt);
  return true;
}

static void printI2cErrorsOncePerSecond(bool ok1, bool ok2) {
  if (millis() - last_err_print <= 1000) return;
  if (!ok1 || !ok2) {
    Serial.printf("[ERROR] IMU1(0x68):%s [%lu err] | IMU2(0x69):%s [%lu err]\n",
                  ok1 ? "OK" : "FAIL", err_count1,
                  ok2 ? "OK" : "FAIL", err_count2);
  }
  last_err_print = millis();
}

static void sendTelemetry(uint32_t now_us,
                          float roll1, float pitch1, float yaw1,
                          float roll2, float pitch2, float yaw2,
                          float knee_angle,
                          bool imu1_inverted, bool imu2_inverted) {
  // USB: format etykietowany (pod Serial Plotter / łatwe logowanie)
  Serial.print("time:"); Serial.print((unsigned long)now_us);
  Serial.print(" roll1:"); Serial.print(roll1, 2);
  Serial.print(" pitch1:"); Serial.print(pitch1, 2);
  Serial.print(" yaw1:"); Serial.print(yaw1, 2);
  Serial.print(" roll2:"); Serial.print(roll2, 2);
  Serial.print(" pitch2:"); Serial.print(pitch2, 2);
  Serial.print(" yaw2:"); Serial.print(yaw2, 2);
  Serial.print(" knee_angle:"); Serial.print(knee_angle, 2);
  Serial.print(" inv1:"); Serial.print(imu1_inverted ? 1 : 0);
  Serial.print(" inv2:"); Serial.println(imu2_inverted ? 1 : 0);

  // BT: szybki CSV bez etykiet (łatwy parsing w aplikacji)
  if (BT.hasClient()) {
    char out[128];
    const int n = snprintf(out, sizeof(out),
                           "%lu,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%d,%d\n",
                           (unsigned long)now_us,
                           roll1, pitch1, yaw1,
                           roll2, pitch2, yaw2,
                           knee_angle,
                           imu1_inverted ? 1 : 0,
                           imu2_inverted ? 1 : 0);
    BT.write((const uint8_t*)out, n);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(5);
  delay(300);
  Serial.printf("\n[BOOT] Chip: %s | USB+BT calib commands\n", ESP.getChipModel());

  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000);
  delay(200);

  Serial.printf("[WHO] 0x68=0x%02X, 0x69=0x%02X (expect 0x68)\n", readWhoAmI(MPU1_ADDR), readWhoAmI(MPU2_ADDR));
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

  bool btok = BT.begin(BT_DEVICE_NAME);
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
  Serial.println("[INFO] IMU1=udo (thigh), IMU2=podudzie (shank), knee_angle=|angleDiff(roll2, roll1)|");
}

void loop() {
  handleCommands();

  const uint32_t now_us = micros();
  const float dt = computeDtSeconds(now_us);

  const bool ok1 = updateImu(imu1, MPU1_ADDR, dt, err_count1);
  const bool ok2 = updateImu(imu2, MPU2_ADDR, dt, err_count2);
  printI2cErrorsOncePerSecond(ok1, ok2);

  // Korekta o offsety (po komendzie "calib")
  const float roll1  = ok1 ? (imu1.k_roll.angle_deg  - imu1.off_roll)  : -999.0f;
  const float pitch1 = ok1 ? (imu1.k_pitch.angle_deg - imu1.off_pitch) : -999.0f;
  const float yaw1   = ok1 ? wrap180(imu1.yaw - imu1.off_yaw) : -999.0f;

  const float roll2  = ok2 ? (imu2.k_roll.angle_deg  - imu2.off_roll)  : -999.0f;
  const float pitch2 = ok2 ? (imu2.k_pitch.angle_deg - imu2.off_pitch) : -999.0f;
  const float yaw2   = ok2 ? wrap180(imu2.yaw - imu2.off_yaw) : -999.0f;

  // Prosta diagnostyka orientacji (az < 0 oznacza, że IMU jest odwrócone)
  const bool imu1_inverted = ok1 && (imu1.az < 0.0f);
  const bool imu2_inverted = ok2 && (imu2.az < 0.0f);

  // Kąt zgięcia kolana: dodatnia minimalna różnica kątowa roll (0..180)
  const float knee_angle = (ok1 && ok2) ? fabsf(angleDiffDeg(roll2, roll1)) : -999.0f;

  // Telemetria z ograniczeniem częstotliwości
  if (now_us - last_send_us >= SEND_PERIOD_US) {
    last_send_us = now_us;
    sendTelemetry(now_us, roll1, pitch1, yaw1, roll2, pitch2, yaw2, knee_angle, imu1_inverted, imu2_inverted);
  }
}
