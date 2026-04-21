#include <ArduinoBLE.h>

const char* DEVICE_NAME = "Peacock-Shaker";
const char* SERVICE_UUID = "19B10010-E8F2-537E-4F6C-D104768A1214";
const char* MOTION_LEVEL_UUID = "19B10011-E8F2-537E-4F6C-D104768A1214";

const unsigned long LINK_UPDATE_MS = 120;
const unsigned long SEARCH_RESTART_MS = 3000;
const unsigned long PEER_TIMEOUT_MS = 1200;
const unsigned long BLINK_INTERVAL_FAST_MS = 55;
const unsigned long BLINK_INTERVAL_SLOW_MS = 280;
const byte MOTION_LEVEL_MAX = 5;

BLEDevice shakerPeripheral;
BLECharacteristic remoteMotionLevelCharacteristic;

byte remoteMotionLevel = 0;
bool blinkState = false;

unsigned long lastLinkUpdateMs = 0;
unsigned long lastSearchRestartMs = 0;
unsigned long lastRemoteMotionMs = 0;
unsigned long lastBlinkMs = 0;

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

void startScanning() {
  BLE.stopScan();
  BLE.scanForUuid(SERVICE_UUID, true);
  lastSearchRestartMs = millis();
}

void resetLinkState() {
  if (shakerPeripheral) {
    shakerPeripheral.disconnect();
  }

  shakerPeripheral = BLEDevice();
  remoteMotionLevelCharacteristic = BLECharacteristic();
  remoteMotionLevel = 0;
  blinkState = false;
  lastLinkUpdateMs = 0;
  lastRemoteMotionMs = 0;
  startScanning();
}

bool connectToShaker(BLEDevice candidate) {
  BLE.stopScan();

  if (!candidate.connect()) {
    startScanning();
    return false;
  }

  delay(250);

  bool attributesDiscovered = false;
  for (int attempt = 0; attempt < 3; attempt++) {
    if (candidate.discoverAttributes()) {
      attributesDiscovered = true;
      break;
    }
    delay(250);
  }

  if (!attributesDiscovered) {
    candidate.disconnect();
    startScanning();
    return false;
  }

  BLECharacteristic motionCharacteristic = candidate.characteristic(MOTION_LEVEL_UUID);
  if (!motionCharacteristic) {
    candidate.disconnect();
    startScanning();
    return false;
  }

  shakerPeripheral = candidate;
  remoteMotionLevelCharacteristic = motionCharacteristic;
  lastLinkUpdateMs = 0;
  return true;
}

void updateConnection() {
  if (shakerPeripheral && shakerPeripheral.connected()) {
    return;
  }

  BLEDevice candidate = BLE.available();
  if (candidate) {
    connectToShaker(candidate);
    return;
  }

  if (millis() - lastSearchRestartMs >= SEARCH_RESTART_MS) {
    startScanning();
  }
}

void updateRemoteMotion() {
  if (!shakerPeripheral || !shakerPeripheral.connected()) {
    resetLinkState();
    return;
  }

  if (millis() - lastLinkUpdateMs < LINK_UPDATE_MS) {
    return;
  }

  lastLinkUpdateMs = millis();

  byte receivedLevel = 0;
  if (remoteMotionLevelCharacteristic.readValue(receivedLevel)) {
    remoteMotionLevel = receivedLevel;
    lastRemoteMotionMs = millis();
  }

  if (lastRemoteMotionMs != 0 && millis() - lastRemoteMotionMs > PEER_TIMEOUT_MS) {
    remoteMotionLevel = 0;
    blinkState = false;
    lastRemoteMotionMs = 0;
  }
}

unsigned long blinkIntervalForLevel(byte motionLevel) {
  if (motionLevel == 0) {
    return BLINK_INTERVAL_SLOW_MS;
  }

  unsigned long intervalRange = BLINK_INTERVAL_SLOW_MS - BLINK_INTERVAL_FAST_MS;
  if (MOTION_LEVEL_MAX <= 1) {
    return BLINK_INTERVAL_FAST_MS;
  }

  return BLINK_INTERVAL_SLOW_MS - ((unsigned long)(motionLevel - 1) * intervalRange / (MOTION_LEVEL_MAX - 1));
}

void updateStatusLight() {
  if (!shakerPeripheral || !shakerPeripheral.connected()) {
    setRgb(false, false, true);
    return;
  }

  if (remoteMotionLevel > 0) {
    unsigned long blinkIntervalMs = blinkIntervalForLevel(remoteMotionLevel);
    if (millis() - lastBlinkMs >= blinkIntervalMs) {
      lastBlinkMs = millis();
      blinkState = !blinkState;
    }
    setRgb(blinkState, false, false);
    return;
  }

  blinkState = false;
  setRgb(false, true, false);
}

void setup() {
  pinMode(LEDR, OUTPUT);
  pinMode(LEDG, OUTPUT);
  pinMode(LEDB, OUTPUT);
  setRgb(false, false, false);

  if (!BLE.begin()) {
    stopWithErrorLight();
  }

  startScanning();
}

void loop() {
  BLE.poll();
  updateConnection();
  updateRemoteMotion();
  updateStatusLight();
}
