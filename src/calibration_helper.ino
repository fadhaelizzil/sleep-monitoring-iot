#include <Arduino.h>
#include "HX711.h"

HX711 hx1;
HX711 hx2;
HX711 hx3;

struct LoadCellModule {
  HX711* amp;
  uint8_t doutPin;
  uint8_t sckPin;
  bool enabled;
  const char* name;
};

LoadCellModule modules[] = {
  { &hx1, D5, D1, true,  "HX1" },
  { &hx2, D6, D2, true,  "HX2" },
  { &hx3, D7, D0, true,  "HX3" }   // ubah false jika tidak dipakai
};

constexpr size_t MODULE_COUNT = sizeof(modules) / sizeof(modules[0]);

// =========================
// Pilih modul yang mau dikalibrasi
// 0 = HX1
// 1 = HX2
// 2 = HX3
// =========================
constexpr int TARGET_MODULE_INDEX = 0;

// Berat referensi yang dipakai saat kalibrasi
constexpr float KNOWN_WEIGHT_KG = 10.0f;

// Nilai awal kasar
float calibrationFactor = -7050.0f;

// Kehalusan tuning manual
float adjustmentStep = 100.0f;

// Sampling
constexpr uint8_t RAW_SAMPLES = 15;
constexpr uint8_t UNIT_SAMPLES = 8;

HX711* targetAmp = nullptr;
const char* targetName = "UNKNOWN";

unsigned long lastPrintMs = 0;
bool streamEnabled = true;

void printHelp();
bool initTargetModule();
void tareScale();
void calculateCalibrationFactor();
void applyCalibration();
float readRawValue();
float readWeightKg();
void printLiveData();
void handleSerialCommand(char cmd);

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println(F("SiPETIR HX711 Calibration Tool"));

  if (!initTargetModule()) {
    Serial.println(F("Invalid TARGET_MODULE_INDEX or module disabled."));
    while (true) {
      delay(1000);
    }
  }

  printHelp();
  tareScale();
}

void loop() {
  if (Serial.available()) {
    const char cmd = static_cast<char>(Serial.read());
    handleSerialCommand(cmd);
  }

  if (streamEnabled && millis() - lastPrintMs >= 500UL) {
    lastPrintMs = millis();
    printLiveData();
  }
}

bool initTargetModule() {
  if (TARGET_MODULE_INDEX < 0 || TARGET_MODULE_INDEX >= static_cast<int>(MODULE_COUNT)) {
    return false;
  }

  if (!modules[TARGET_MODULE_INDEX].enabled) {
    return false;
  }

  targetAmp = modules[TARGET_MODULE_INDEX].amp;
  targetName = modules[TARGET_MODULE_INDEX].name;

  targetAmp->begin(modules[TARGET_MODULE_INDEX].doutPin, modules[TARGET_MODULE_INDEX].sckPin);
  targetAmp->set_scale(1.0f);

  Serial.printf("Target module: %s | DOUT=%u | SCK=%u\n",
                targetName,
                modules[TARGET_MODULE_INDEX].doutPin,
                modules[TARGET_MODULE_INDEX].sckPin);

  if (!targetAmp->wait_ready_timeout(3000)) {
    Serial.println(F("HX711 not ready."));
    return false;
  }

  return true;
}

void printHelp() {
  Serial.println(F("=========================================="));
  Serial.println(F("Commands:"));
  Serial.println(F("  t : tare (pastikan tanpa beban)"));
  Serial.println(F("  c : hitung calibration factor otomatis"));
  Serial.println(F("  + : naikkan factor"));
  Serial.println(F("  - : turunkan factor"));
  Serial.println(F("  * : step x10"));
  Serial.println(F("  / : step /10"));
  Serial.println(F("  p : print factor saat ini"));
  Serial.println(F("  s : toggle live stream"));
  Serial.println(F("  h : help"));
  Serial.println(F("=========================================="));
  Serial.printf("Known weight = %.3f kg\n", KNOWN_WEIGHT_KG);
  Serial.printf("Initial calibration factor = %.4f\n", calibrationFactor);
}

void tareScale() {
  Serial.println(F("Taring... remove all load from target module."));
  targetAmp->set_scale(1.0f);
  targetAmp->tare(20);
  applyCalibration();
  Serial.println(F("Tare done."));
}

void calculateCalibrationFactor() {
  Serial.println(F("Place the known weight on the target module area now."));
  delay(3000);

  const float rawValue = targetAmp->get_value(RAW_SAMPLES);

  if (fabs(rawValue) < 1.0f) {
    Serial.println(F("Raw value too small. Check wiring / placement / known weight."));
    return;
  }

  calibrationFactor = rawValue / KNOWN_WEIGHT_KG;
  applyCalibration();

  Serial.printf("Auto calibration done.\n");
  Serial.printf("Raw reference = %.4f\n", rawValue);
  Serial.printf("New calibration factor = %.6f\n", calibrationFactor);
  Serial.printf("Measured weight now = %.4f kg\n", readWeightKg());
}

void applyCalibration() {
  targetAmp->set_scale(calibrationFactor);
}

float readRawValue() {
  return targetAmp->get_value(RAW_SAMPLES);
}

float readWeightKg() {
  return targetAmp->get_units(UNIT_SAMPLES);
}

void printLiveData() {
  const float raw = readRawValue();
  const float kg = readWeightKg();

  Serial.printf("[%s] Raw=%.4f | Weight=%.4f kg | Factor=%.6f | Step=%.4f\n",
                targetName,
                raw,
                kg,
                calibrationFactor,
                adjustmentStep);
}

void handleSerialCommand(char cmd) {
  switch (cmd) {
    case 't':
    case 'T':
      tareScale();
      break;

    case 'c':
    case 'C':
      calculateCalibrationFactor();
      break;

    case '+':
      calibrationFactor += adjustmentStep;
      applyCalibration();
      Serial.printf("Factor increased -> %.6f\n", calibrationFactor);
      break;

    case '-':
      calibrationFactor -= adjustmentStep;
      applyCalibration();
      Serial.printf("Factor decreased -> %.6f\n", calibrationFactor);
      break;

    case '*':
      adjustmentStep *= 10.0f;
      Serial.printf("Step -> %.6f\n", adjustmentStep);
      break;

    case '/':
      adjustmentStep /= 10.0f;
      if (adjustmentStep < 0.0001f) {
        adjustmentStep = 0.0001f;
      }
      Serial.printf("Step -> %.6f\n", adjustmentStep);
      break;

    case 'p':
    case 'P':
      Serial.printf("Current calibration factor = %.6f\n", calibrationFactor);
      break;

    case 's':
    case 'S':
      streamEnabled = !streamEnabled;
      Serial.printf("Live stream = %s\n", streamEnabled ? "ON" : "OFF");
      break;

    case 'h':
    case 'H':
      printHelp();
      break;

    case '\n':
    case '\r':
      break;

    default:
      Serial.println(F("Unknown command. Press 'h' for help."));
      break;
  }
}
