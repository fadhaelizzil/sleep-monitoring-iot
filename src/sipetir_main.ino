#define BLYNK_TEMPLATE_ID   "TMPL6ddAZfd3d"
#define BLYNK_TEMPLATE_NAME "SiPETIR"
#define BLYNK_AUTH_TOKEN    "oKiFzpPXnMgnMcUsGC87tBRWkFI7_oCL"

#define BLYNK_PRINT Serial

#include <Arduino.h>
#include <math.h>
#include <time.h>
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include "HX711.h"

char ssid[] = "knock knock";
char pass[] = "whatsinthebox?";

// =========================
// Blynk
// =========================
constexpr uint8_t VPIN_WEIGHT = V1;
constexpr uint8_t VPIN_STATUS = V2;

const char EVENT_ABSENT[]   = "student_absent";
const char EVENT_OVERLOAD[] = "mattress_overload";

const char ABSENT_MESSAGE[]   = "Student is not to be in bed after curfew!";
const char OVERLOAD_MESSAGE[] = u8"⚠️ DANGER: Mattress overload detected! Potential risk of structural collapse!";

BlynkTimer timer;

constexpr long WIB_UTC_OFFSET_SEC = 7L * 3600L;
constexpr int ACTIVE_START_HOUR = 23;
constexpr int ACTIVE_END_HOUR   = 5;

// =========================
// Threshold
// =========================
constexpr float NORMAL_MIN_KG = 52.0f;
constexpr float NORMAL_MAX_KG = 62.0f;

// =========================
// Timing
// =========================
constexpr unsigned long SENSOR_SAMPLE_INTERVAL_MS = 500UL;
constexpr unsigned long TELEMETRY_INTERVAL_MS     = 2000UL;
constexpr unsigned long ABSENT_CONFIRM_MS         = 15UL * 60UL * 1000UL;
constexpr unsigned long STABILITY_HOLD_MS         = 5000UL;

// =========================
// Filter
// =========================
constexpr size_t MOVING_AVG_WINDOW = 6;
constexpr float STABILITY_EPSILON_KG = 1.5f;
constexpr float NOISE_FLOOR_KG = 0.30f;

// =========================
// HX711
// Pin aman yang dipakai:
// HX1 -> DOUT=D5, SCK=D1
// HX2 -> DOUT=D6, SCK=D2
// HX3 -> DOUT=D7, SCK=D0
// =========================
constexpr uint8_t HX_READ_SAMPLES = 1;
constexpr unsigned long HX_READY_TIMEOUT_MS = 50UL;
constexpr uint8_t HX_TARE_SAMPLES = 15;

HX711 hx1;
HX711 hx2;
HX711 hx3;

struct LoadCellModule {
  HX711* amp;
  uint8_t doutPin;
  uint8_t sckPin;
  float calibrationFactor;
  bool enabled;
  const char* name;
};

// Ganti calibrationFactor di bawah dengan hasil sketch kalibrasi
LoadCellModule modules[] = {
  { &hx1, D5, D1, -7050.0f, true,  "HX1" },
  { &hx2, D6, D2, -7050.0f, true,  "HX2" },
  { &hx3, D7, D0, -7050.0f, true,  "HX3" }  // ubah false jika hanya 2 modul
};

constexpr size_t MODULE_COUNT = sizeof(modules) / sizeof(modules[0]);

enum class BedStatus : uint8_t {
  INACTIVE = 0,
  UNDERLOAD,
  NORMAL,
  OVERLOAD,
  NO_TIME
};

// =========================
// Runtime state
// =========================
float movingAvgBuffer[MOVING_AVG_WINDOW] = {0.0f};
size_t movingAvgIndex = 0;
size_t movingAvgCount = 0;
float movingAvgSum = 0.0f;

float rawWeightKg = 0.0f;
float filteredWeightKg = 0.0f;
float stableWeightKg = 0.0f;

bool stableInitialized = false;
float pendingStableKg = 0.0f;
unsigned long pendingStableSinceMs = 0;

BedStatus currentStatus = BedStatus::NO_TIME;

unsigned long underloadSinceMs = 0;
bool absentAlertSent = false;
bool overloadAlertSent = false;

// =========================
// Prototypes
// =========================
void initLoadCells();
void tareLoadCells();
float readTotalWeightKg();

float updateMovingAverage(float sampleKg);
void updateStableWeight(float filteredKg, unsigned long nowMs);

void syncTimeWithNTP();
bool isTimeSynced();
bool getLocalTimeSafe(struct tm& timeinfo);
bool isMonitoringWindowActive();
void resetMonitoringLatches();

BedStatus classifyWeight(float kg);
const char* statusToText(BedStatus status);

void sampleWeightTask();
void sendTelemetryTask();
void evaluateState(unsigned long nowMs);
bool tryLogEvent(const char* eventCode, const char* description);
void printDebugLine();

BLYNK_CONNECTED() {
  sendTelemetryTask();
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println(F("SiPETIR starting..."));
  Serial.println(F("Pastikan kasur kosong saat boot untuk auto-tare."));

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  syncTimeWithNTP();
  initLoadCells();
  tareLoadCells();

  timer.setInterval(SENSOR_SAMPLE_INTERVAL_MS, sampleWeightTask);
  timer.setInterval(TELEMETRY_INTERVAL_MS, sendTelemetryTask);

  Serial.println(F("System ready."));
}

void loop() {
  Blynk.run();
  timer.run();
}

// =========================
// Time / Schedule
// =========================
void syncTimeWithNTP() {
  configTime(WIB_UTC_OFFSET_SEC, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");

  Serial.print(F("Syncing NTP"));
  unsigned long startMs = millis();

  while (!isTimeSynced() && millis() - startMs < 15000UL) {
    delay(250);
    Serial.print(F("."));
  }
  Serial.println();

  if (isTimeSynced()) {
    struct tm timeinfo;
    if (getLocalTimeSafe(timeinfo)) {
      Serial.printf("Time synced: %02d:%02d:%02d WIB\n",
                    timeinfo.tm_hour,
                    timeinfo.tm_min,
                    timeinfo.tm_sec);
    }
  } else {
    Serial.println(F("NTP sync failed. System will stay NO_TIME until time becomes valid."));
  }
}

bool isTimeSynced() {
  time_t now = time(nullptr);
  return now > 1700000000;
}

bool getLocalTimeSafe(struct tm& timeinfo) {
  time_t now = time(nullptr);
  if (now <= 0) {
    return false;
  }

  localtime_r(&now, &timeinfo);
  return timeinfo.tm_year > (2024 - 1900);
}

bool isMonitoringWindowActive() {
  struct tm timeinfo;
  if (!getLocalTimeSafe(timeinfo)) {
    return false;
  }

  const int hour = timeinfo.tm_hour;
  return (hour >= ACTIVE_START_HOUR || hour < ACTIVE_END_HOUR);
}

void resetMonitoringLatches() {
  underloadSinceMs = 0;
  absentAlertSent = false;
  overloadAlertSent = false;
}

// =========================
// HX711
// =========================
void initLoadCells() {
  for (size_t i = 0; i < MODULE_COUNT; i++) {
    if (!modules[i].enabled) {
      continue;
    }

    modules[i].amp->begin(modules[i].doutPin, modules[i].sckPin);
    modules[i].amp->set_scale(modules[i].calibrationFactor);

    Serial.printf("[%s] ready | DOUT=%u | SCK=%u | CAL=%.4f\n",
                  modules[i].name,
                  modules[i].doutPin,
                  modules[i].sckPin,
                  modules[i].calibrationFactor);
  }
}

void tareLoadCells() {
  for (size_t i = 0; i < MODULE_COUNT; i++) {
    if (!modules[i].enabled) {
      continue;
    }

    Serial.printf("[%s] tare...\n", modules[i].name);

    if (!modules[i].amp->wait_ready_timeout(3000)) {
      Serial.printf("[%s] not ready during tare.\n", modules[i].name);
      continue;
    }

    modules[i].amp->tare(HX_TARE_SAMPLES);
    Serial.printf("[%s] tare done.\n", modules[i].name);
  }
}

float readTotalWeightKg() {
  float totalKg = 0.0f;

  for (size_t i = 0; i < MODULE_COUNT; i++) {
    if (!modules[i].enabled) {
      continue;
    }

    if (!modules[i].amp->wait_ready_timeout(HX_READY_TIMEOUT_MS)) {
      Serial.printf("[%s] read timeout.\n", modules[i].name);
      return NAN;
    }

    const float moduleKg = modules[i].amp->get_units(HX_READ_SAMPLES);
    totalKg += moduleKg;
  }

  if (fabsf(totalKg) <= NOISE_FLOOR_KG) {
    totalKg = 0.0f;
  }

  if (totalKg < 0.0f) {
    totalKg = 0.0f;
  }

  return totalKg;
}

// =========================
// Filter
// =========================
float updateMovingAverage(float sampleKg) {
  if (movingAvgCount < MOVING_AVG_WINDOW) {
    movingAvgBuffer[movingAvgIndex] = sampleKg;
    movingAvgSum += sampleKg;
    movingAvgCount++;
    movingAvgIndex = (movingAvgIndex + 1) % MOVING_AVG_WINDOW;
    return movingAvgSum / static_cast<float>(movingAvgCount);
  }

  movingAvgSum -= movingAvgBuffer[movingAvgIndex];
  movingAvgBuffer[movingAvgIndex] = sampleKg;
  movingAvgSum += sampleKg;
  movingAvgIndex = (movingAvgIndex + 1) % MOVING_AVG_WINDOW;

  return movingAvgSum / static_cast<float>(MOVING_AVG_WINDOW);
}

void updateStableWeight(float filteredKg, unsigned long nowMs) {
  if (!stableInitialized) {
    stableWeightKg = filteredKg;
    pendingStableKg = filteredKg;
    pendingStableSinceMs = nowMs;
    stableInitialized = true;
    return;
  }

  if (fabsf(filteredKg - stableWeightKg) <= STABILITY_EPSILON_KG) {
    pendingStableKg = filteredKg;
    pendingStableSinceMs = nowMs;
    return;
  }

  if (fabsf(filteredKg - pendingStableKg) > STABILITY_EPSILON_KG) {
    pendingStableKg = filteredKg;
    pendingStableSinceMs = nowMs;
    return;
  }

  if (nowMs - pendingStableSinceMs >= STABILITY_HOLD_MS) {
    stableWeightKg = pendingStableKg;
  }
}

// =========================
// State
// =========================
BedStatus classifyWeight(float kg) {
  if (kg > NORMAL_MAX_KG) {
    return BedStatus::OVERLOAD;
  }

  if (kg >= NORMAL_MIN_KG && kg <= NORMAL_MAX_KG) {
    return BedStatus::NORMAL;
  }

  return BedStatus::UNDERLOAD;
}

const char* statusToText(BedStatus status) {
  switch (status) {
    case BedStatus::INACTIVE:  return "INACTIVE";
    case BedStatus::UNDERLOAD: return "UNDERLOAD";
    case BedStatus::NORMAL:    return "NORMAL";
    case BedStatus::OVERLOAD:  return "OVERLOAD";
    case BedStatus::NO_TIME:   return "NO_TIME";
    default:                   return "UNKNOWN";
  }
}

void evaluateState(unsigned long nowMs) {
  if (!isTimeSynced()) {
    currentStatus = BedStatus::NO_TIME;
    resetMonitoringLatches();
    return;
  }

  if (!isMonitoringWindowActive()) {
    currentStatus = BedStatus::INACTIVE;
    resetMonitoringLatches();
    return;
  }

  currentStatus = classifyWeight(stableWeightKg);

  switch (currentStatus) {
    case BedStatus::UNDERLOAD:
      overloadAlertSent = false;

      if (underloadSinceMs == 0) {
        underloadSinceMs = nowMs;
      }

      if (!absentAlertSent && (nowMs - underloadSinceMs >= ABSENT_CONFIRM_MS)) {
        absentAlertSent = tryLogEvent(EVENT_ABSENT, ABSENT_MESSAGE);
      }
      break;

    case BedStatus::NORMAL:
      resetMonitoringLatches();
      break;

    case BedStatus::OVERLOAD:
      underloadSinceMs = 0;
      absentAlertSent = false;

      if (!overloadAlertSent) {
        overloadAlertSent = tryLogEvent(EVENT_OVERLOAD, OVERLOAD_MESSAGE);
      }
      break;

    case BedStatus::INACTIVE:
    case BedStatus::NO_TIME:
    default:
      resetMonitoringLatches();
      break;
  }
}

// =========================
// Tasks
// =========================
void sampleWeightTask() {
  const float totalKg = readTotalWeightKg();
  if (isnan(totalKg)) {
    return;
  }

  rawWeightKg = totalKg;
  filteredWeightKg = updateMovingAverage(rawWeightKg);
  updateStableWeight(filteredWeightKg, millis());
  evaluateState(millis());
  printDebugLine();
}

void sendTelemetryTask() {
  if (!Blynk.connected()) {
    return;
  }

  Blynk.virtualWrite(VPIN_WEIGHT, stableWeightKg);
  Blynk.virtualWrite(VPIN_STATUS, statusToText(currentStatus));
}

bool tryLogEvent(const char* eventCode, const char* description) {
  if (!Blynk.connected()) {
    return false;
  }

  Blynk.logEvent(eventCode, description);
  Serial.printf("Event sent: %s | %s\n", eventCode, description);
  return true;
}

void printDebugLine() {
  struct tm timeinfo;
  if (getLocalTimeSafe(timeinfo)) {
    Serial.printf("[%02d:%02d:%02d] Raw=%.2f kg | Avg=%.2f kg | Stable=%.2f kg | Status=%s\n",
                  timeinfo.tm_hour,
                  timeinfo.tm_min,
                  timeinfo.tm_sec,
                  rawWeightKg,
                  filteredWeightKg,
                  stableWeightKg,
                  statusToText(currentStatus));
  } else {
    Serial.printf("[--:--:--] Raw=%.2f kg | Avg=%.2f kg | Stable=%.2f kg | Status=%s\n",
                  rawWeightKg,
                  filteredWeightKg,
                  stableWeightKg,
                  statusToText(currentStatus));
  }
}
