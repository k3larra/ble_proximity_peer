#include <ArduinoBLE.h>
#include <Arduino_BMI270_BMM150.h>
#include <math.h>

const char* DEVICE_NAME = "Peacock-Shaker";
const char* SERVICE_UUID = "19B10010-E8F2-537E-4F6C-D104768A1214";
const char* MOTION_LEVEL_UUID = "19B10011-E8F2-537E-4F6C-D104768A1214";

// Raise MOTION_START_THRESHOLD if the chick reacts too easily.
// Lower it if stronger handling is needed before anything happens.
const float MOTION_START_THRESHOLD = 0.42f;

// Raise MOTION_STOP_THRESHOLD if the chick stays active too long after motion stops.
// Lower it if the trigger turns off too quickly.
const float MOTION_STOP_THRESHOLD = 0.22f;

// Lower MOTION_FULL_SCALE if strong shaking should reach the highest response more easily.
// Raise it if the strongest responses happen too often.
const float MOTION_FULL_SCALE = 1.80f;

// MOTION_LEVEL_MAX controls how many motion steps are sent to the responder.
const byte MOTION_LEVEL_MAX = 5;

const unsigned long SENSOR_SAMPLE_MS = 40;

// Lower MOTION_WINDOW_MS for quicker response with less averaging.
// Raise it for smoother response with more averaging.
const unsigned long MOTION_WINDOW_MS = 120;
const unsigned long BLE_SEND_MIN_MS = 120;
const unsigned long BLE_REFRESH_MS = 400;

BLEService motionService(SERVICE_UUID);
BLEByteCharacteristic motionLevelCharacteristic(MOTION_LEVEL_UUID, BLERead | BLENotify);

byte localMotionLevel = 0;
byte sentMotionLevel = 255;

float previousAccelX = 0.0f;
float previousAccelY = 0.0f;
float previousAccelZ = 0.0f;
bool hasPreviousAcceleration = false;
float motionEnergySum = 0.0f;
byte motionEnergySamples = 0;

unsigned long lastSensorSampleMs = 0;
unsigned long lastMotionWindowMs = 0;
unsigned long lastBleSendMs = 0;

void setRgb(bool redOn, bool greenOn, bool blueOn) {
  digitalWrite(LEDR, redOn ? LOW : HIGH);
  digitalWrite(LEDG, greenOn ? LOW : HIGH);
  digitalWrite(LEDB, blueOn ? LOW : HIGH);
}

void stopWithErrorLight() {
  setRgb(true, false, false);
  while (true) {
    delay(1000);
  }
}

bool readMotionSample(float& motionAmount) {
  if (!IMU.accelerationAvailable()) {
    return false;
  }

  float accelX = 0.0f;
  float accelY = 0.0f;
  float accelZ = 0.0f;
  IMU.readAcceleration(accelX, accelY, accelZ);

  if (!hasPreviousAcceleration) {
    previousAccelX = accelX;
    previousAccelY = accelY;
    previousAccelZ = accelZ;
    hasPreviousAcceleration = true;
    motionAmount = 0.0f;
    return true;
  }

  float deltaX = accelX - previousAccelX;
  float deltaY = accelY - previousAccelY;
  float deltaZ = accelZ - previousAccelZ;

  previousAccelX = accelX;
  previousAccelY = accelY;
  previousAccelZ = accelZ;

  motionAmount = sqrt(deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ);
  return true;
}

byte quantizeMotionLevel(float motionAmount, byte previousLevel) {
  float threshold = previousLevel > 0 ? MOTION_STOP_THRESHOLD : MOTION_START_THRESHOLD;
  if (motionAmount <= threshold) {
    return 0;
  }

  float usableRange = MOTION_FULL_SCALE - threshold;
  if (usableRange <= 0.0f) {
    return MOTION_LEVEL_MAX;
  }

  float scaled = (motionAmount - threshold) / usableRange;
  if (scaled < 0.0f) {
    scaled = 0.0f;
  }
  if (scaled > 1.0f) {
    scaled = 1.0f;
  }

  return (byte)(1 + (scaled * (float)(MOTION_LEVEL_MAX - 1)));
}

void updateLocalMotion() {
  if (millis() - lastSensorSampleMs < SENSOR_SAMPLE_MS) {
    return;
  }

  lastSensorSampleMs = millis();

  float motionAmount = 0.0f;
  if (!readMotionSample(motionAmount)) {
    return;
  }

  motionEnergySum += motionAmount * motionAmount;
  motionEnergySamples++;

  if (lastMotionWindowMs == 0) {
    lastMotionWindowMs = millis();
  }

  if (millis() - lastMotionWindowMs < MOTION_WINDOW_MS) {
    return;
  }

  if (motionEnergySamples == 0) {
    return;
  }

  float rmsMotion = sqrt(motionEnergySum / (float)motionEnergySamples);
  localMotionLevel = quantizeMotionLevel(rmsMotion, localMotionLevel);

  motionEnergySum = 0.0f;
  motionEnergySamples = 0;
  lastMotionWindowMs = millis();
}

void updateBleMotionLevel() {
  bool refreshNeeded = (millis() - lastBleSendMs) >= BLE_REFRESH_MS;

  if (!refreshNeeded && localMotionLevel == sentMotionLevel) {
    return;
  }

  if (millis() - lastBleSendMs < BLE_SEND_MIN_MS) {
    return;
  }

  motionLevelCharacteristic.writeValue(localMotionLevel);
  sentMotionLevel = localMotionLevel;
  lastBleSendMs = millis();
}

void updateStatusLight() {
  if (!BLE.connected()) {
    setRgb(false, false, true);
    return;
  }

  if (localMotionLevel > 0) {
    setRgb(true, false, true);
    return;
  }

  setRgb(false, true, true);
}

void setup() {
  pinMode(LEDR, OUTPUT);
  pinMode(LEDG, OUTPUT);
  pinMode(LEDB, OUTPUT);
  setRgb(false, false, false);

  if (!IMU.begin()) {
    stopWithErrorLight();
  }

  if (!BLE.begin()) {
    stopWithErrorLight();
  }

  motionService.addCharacteristic(motionLevelCharacteristic);
  BLE.addService(motionService);

  BLE.setLocalName(DEVICE_NAME);
  BLE.setAdvertisedService(motionService);
  motionLevelCharacteristic.writeValue((byte)0);
  BLE.advertise();
}

void loop() {
  BLE.poll();
  updateLocalMotion();
  updateBleMotionLevel();
  updateStatusLight();
}
