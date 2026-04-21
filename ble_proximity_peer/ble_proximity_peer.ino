#include <ArduinoBLE.h>

// Set to 1 before uploading to a Nano 33 BLE Sense Rev2.
// Leave it at 0 for the earlier Nano 33 BLE Sense boards.
#ifndef USE_REV2_IMU
#define USE_REV2_IMU 1
#endif

#if USE_REV2_IMU
#include <Arduino_BMI270_BMM150.h>
#else
#include <Arduino_LSM9DS1.h>
#endif

#include <math.h>

// ============================================================
// Beginner setup section
// Change values here when you want to tune the motion response
// or the LED action.
// ============================================================

const char* DEVICE_NAME = "Nano33-Peer";
const char* SERVICE_UUID = "19B10010-E8F2-537E-4F6C-D104768A1214";
const char* OUTGOING_EVENT_UUID = "19B10011-E8F2-537E-4F6C-D104768A1214";
const char* INCOMING_EVENT_UUID = "19B10012-E8F2-537E-4F6C-D104768A1214";

const bool DEBUG_BLE = false;

const float MOTION_START_THRESHOLD = 0.85f;
const float MOTION_STOP_THRESHOLD = 0.45f;
const float MOTION_FULL_SCALE = 3.20f;
const float MOTION_SMOOTHING = 0.35f;
const byte MOTION_LEVEL_STEPS = 12;

const unsigned long SENSOR_SAMPLE_MS = 40;
const unsigned long LINK_UPDATE_MS = 120;
const unsigned long PEER_TIMEOUT_MS = 1200;
const unsigned long ROLE_RETRY_MS = 8000;
const unsigned long SEARCH_ADVERTISE_MS = 4000;
const unsigned long SEARCH_SCAN_MS = 6000;
const unsigned long BLINK_INTERVAL_FAST_MS = 55;
const unsigned long BLINK_INTERVAL_SLOW_MS = 280;

BLEService peerService(SERVICE_UUID);
BLEByteCharacteristic outgoingEventCharacteristic(OUTGOING_EVENT_UUID, BLERead | BLENotify);
BLEByteCharacteristic incomingEventCharacteristic(INCOMING_EVENT_UUID, BLERead | BLEWrite | BLEWriteWithoutResponse);

String myAddress;
bool remotePeerSeen = false;
bool blinkState = false;

byte localMotionLevel = 0;
byte sentMotionLevel = 255;
byte remoteMotionLevel = 0;

float filteredMotion = 0.0f;
float previousAccelX = 0.0f;
float previousAccelY = 0.0f;
float previousAccelZ = 0.0f;
bool hasPreviousAcceleration = false;

BLEDevice connectedPeripheral;
BLECharacteristic remoteOutgoingEventCharacteristic;
BLECharacteristic remoteIncomingEventCharacteristic;
String peerAddress;
bool roleLocked = false;
bool preferCentralRole = false;
bool searchScanMode = true;

unsigned long lastSensorSampleMs = 0;
unsigned long lastLinkUpdateMs = 0;
unsigned long lastPeerSeenMs = 0;
unsigned long lastBlinkMs = 0;
unsigned long lastOutgoingSendMs = 0;
unsigned long disconnectedSinceMs = 0;
unsigned long lastSearchModeChangeMs = 0;

enum LinkRole {
  ROLE_WAITING,
  ROLE_PERIPHERAL,
  ROLE_CENTRAL
};

LinkRole currentRole = ROLE_WAITING;

const char* roleName(LinkRole role) {
  if (role == ROLE_PERIPHERAL) {
    return "PERIPHERAL";
  }

  if (role == ROLE_CENTRAL) {
    return "CENTRAL";
  }

  return "WAITING";
}

void debugBle(String message) {
  if (!DEBUG_BLE) {
    return;
  }

  Serial.print("[");
  Serial.print(millis());
  Serial.print("] ");
  Serial.println(message);
}

void setRgb(bool redOn, bool greenOn, bool blueOn) {
  digitalWrite(LEDR, redOn ? LOW : HIGH);
  digitalWrite(LEDG, greenOn ? LOW : HIGH);
  digitalWrite(LEDB, blueOn ? LOW : HIGH);
}

// ------------------------------------------------------------
// Motion section
// Replace the inside of these functions if you want to use
// another kind of motion or a different threshold strategy.
// ------------------------------------------------------------
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
    return MOTION_LEVEL_STEPS;
  }

  float scaled = (motionAmount - threshold) / usableRange;
  if (scaled < 0.0f) {
    scaled = 0.0f;
  }
  if (scaled > 1.0f) {
    scaled = 1.0f;
  }

  return (byte)(1 + (scaled * (MOTION_LEVEL_STEPS - 1)));
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

  filteredMotion = (filteredMotion * (1.0f - MOTION_SMOOTHING)) + (motionAmount * MOTION_SMOOTHING);
  localMotionLevel = quantizeMotionLevel(filteredMotion, localMotionLevel);
}

bool localEventActive() {
  return localMotionLevel > 0;
}

bool remoteEventActive() {
  return remoteMotionLevel > 0;
}

// ------------------------------------------------------------
// BLE communication section
// This part keeps one BLE connection alive and moves motion
// levels in both directions over that single connection.
// ------------------------------------------------------------
void updateOutgoingCharacteristic(bool forceSend = false) {
  if (!forceSend && localMotionLevel == sentMotionLevel) {
    return;
  }

  if (!forceSend && millis() - lastOutgoingSendMs < LINK_UPDATE_MS) {
    return;
  }

  outgoingEventCharacteristic.writeValue(localMotionLevel);
  sentMotionLevel = localMotionLevel;
  lastOutgoingSendMs = millis();
}

void writeRemoteIncomingIfNeeded() {
  if (!remoteIncomingEventCharacteristic) {
    return;
  }

  if (millis() - lastOutgoingSendMs < LINK_UPDATE_MS) {
    return;
  }

  if (localMotionLevel == sentMotionLevel) {
    return;
  }

  if (remoteIncomingEventCharacteristic.writeValue(localMotionLevel)) {
    sentMotionLevel = localMotionLevel;
    lastOutgoingSendMs = millis();
  }
}

int hexValue(char value) {
  if (value >= '0' && value <= '9') {
    return value - '0';
  }

  if (value >= 'a' && value <= 'f') {
    return value - 'a' + 10;
  }

  if (value >= 'A' && value <= 'F') {
    return value - 'A' + 10;
  }

  return 0;
}

bool initialSearchModeFromAddress() {
  if (myAddress.length() == 0) {
    return true;
  }

  return (hexValue(myAddress.charAt(myAddress.length() - 1)) % 2) == 0;
}

void startSearchMode(bool scanMode) {
  searchScanMode = scanMode;
  lastSearchModeChangeMs = millis();
  BLE.stopScan();
  BLE.stopAdvertise();

  if (searchScanMode) {
    debugBle("search mode: scan only");
    BLE.scan(true);
  } else {
    debugBle("search mode: advertise only");
    BLE.advertise();
  }
}

void enterDiscoveryMode() {
  startSearchMode(searchScanMode);
}

void resetLinkState() {
  debugBle("resetLinkState from role " + String(roleName(currentRole)));
  connectedPeripheral = BLEDevice();
  remoteOutgoingEventCharacteristic = BLECharacteristic();
  remoteIncomingEventCharacteristic = BLECharacteristic();
  remoteMotionLevel = 0;
  remotePeerSeen = false;
  currentRole = ROLE_WAITING;
  lastLinkUpdateMs = 0;
  disconnectedSinceMs = millis();

  enterDiscoveryMode();
}

bool isCandidatePeer(BLEDevice candidate) {
  if (!candidate) {
    return false;
  }

  if (!candidate.hasLocalName()) {
    return false;
  }

  if (candidate.localName() != DEVICE_NAME) {
    return false;
  }

  if (candidate.address() == myAddress) {
    return false;
  }

  return true;
}

bool shouldConnectToCandidate(BLEDevice candidate) {
  return myAddress.compareTo(candidate.address()) < 0;
}

void lockRoleFromCandidate(BLEDevice candidate) {
  peerAddress = candidate.address();
  preferCentralRole = shouldConnectToCandidate(candidate);
  roleLocked = true;
  disconnectedSinceMs = millis();

  debugBle("lockRole peer=" + peerAddress + " prefer=" + String(preferCentralRole ? "CENTRAL" : "PERIPHERAL"));
  enterDiscoveryMode();
}

void updateSearchModeIfNeeded() {
  unsigned long searchModeMs = searchScanMode ? SEARCH_SCAN_MS : SEARCH_ADVERTISE_MS;

  if (millis() - lastSearchModeChangeMs < searchModeMs) {
    return;
  }

  startSearchMode(!searchScanMode);
}

void retryDiscoveryIfStuck() {
  if (disconnectedSinceMs == 0) {
    disconnectedSinceMs = millis();
  }

  if (!roleLocked || millis() - disconnectedSinceMs < ROLE_RETRY_MS) {
    return;
  }

  roleLocked = false;
  preferCentralRole = false;
  peerAddress = "";
  disconnectedSinceMs = millis();
  debugBle("role retry timeout: clear locked role");
  enterDiscoveryMode();
}

bool connectAsCentral(BLEDevice candidate) {
  debugBle("connectAsCentral start peer=" + candidate.address());
  BLE.stopScan();
  BLE.stopAdvertise();

  if (!candidate.connect()) {
    debugBle("connectAsCentral failed: connect()");
    enterDiscoveryMode();
    return false;
  }

  debugBle("connectAsCentral connected, discovering attributes");
  delay(250);

  bool attributesDiscovered = false;
  for (int attempt = 1; attempt <= 3; attempt++) {
    if (candidate.discoverAttributes()) {
      attributesDiscovered = true;
      break;
    }

    debugBle("connectAsCentral discoverAttributes retry " + String(attempt));
    delay(250);
  }

  if (!attributesDiscovered) {
    debugBle("connectAsCentral failed: discoverAttributes()");
    candidate.disconnect();
    enterDiscoveryMode();
    return false;
  }

  BLEService remoteService = candidate.service(SERVICE_UUID);
  if (!remoteService) {
    debugBle("connectAsCentral failed: service missing");
    candidate.disconnect();
    enterDiscoveryMode();
    return false;
  }

  BLECharacteristic remoteOutgoing = candidate.characteristic(OUTGOING_EVENT_UUID);
  BLECharacteristic remoteIncoming = candidate.characteristic(INCOMING_EVENT_UUID);
  if (!remoteOutgoing || !remoteIncoming) {
    debugBle("connectAsCentral failed: characteristic missing");
    candidate.disconnect();
    enterDiscoveryMode();
    return false;
  }

  connectedPeripheral = candidate;
  remoteOutgoingEventCharacteristic = remoteOutgoing;
  remoteIncomingEventCharacteristic = remoteIncoming;
  currentRole = ROLE_CENTRAL;
  disconnectedSinceMs = 0;
  remotePeerSeen = true;
  lastPeerSeenMs = millis();
  BLE.stopAdvertise();
  sentMotionLevel = 255;
  debugBle("connectAsCentral success peer=" + candidate.address());
  return true;
}

void tryToBecomeCentral() {
  if (BLE.connected()) {
    currentRole = ROLE_PERIPHERAL;
    disconnectedSinceMs = 0;
    remotePeerSeen = true;
    lastPeerSeenMs = millis();
    BLE.stopScan();
    debugBle("connected as peripheral");
    return;
  }

  BLEDevice candidate = BLE.available();
  if (!isCandidatePeer(candidate)) {
    return;
  }

  debugBle("candidate found name=" + candidate.localName() + " address=" + candidate.address());

  if (!roleLocked) {
    lockRoleFromCandidate(candidate);
  }

  connectAsCentral(candidate);
}

void updatePeripheralLink() {
  if (!BLE.connected()) {
    resetLinkState();
    return;
  }

  currentRole = ROLE_PERIPHERAL;
  disconnectedSinceMs = 0;
  remotePeerSeen = true;
  lastPeerSeenMs = millis();
  updateOutgoingCharacteristic();

  if (incomingEventCharacteristic.written()) {
    remoteMotionLevel = incomingEventCharacteristic.value();
    remotePeerSeen = true;
    lastPeerSeenMs = millis();
  }
}

void updateCentralLink() {
  if (!connectedPeripheral || !connectedPeripheral.connected()) {
    resetLinkState();
    return;
  }

  if (millis() - lastLinkUpdateMs < LINK_UPDATE_MS) {
    return;
  }

  lastLinkUpdateMs = millis();

  byte remoteValue = 0;
  if (remoteOutgoingEventCharacteristic.readValue(remoteValue)) {
    remoteMotionLevel = remoteValue;
    remotePeerSeen = true;
    lastPeerSeenMs = millis();
  }

  writeRemoteIncomingIfNeeded();
}

void updateLink() {
  updateOutgoingCharacteristic();

  if (currentRole == ROLE_CENTRAL) {
    updateCentralLink();
  } else if (BLE.connected()) {
    updatePeripheralLink();
  } else {
    if (currentRole == ROLE_PERIPHERAL) {
      resetLinkState();
    }

    currentRole = ROLE_WAITING;
    retryDiscoveryIfStuck();
    updateSearchModeIfNeeded();
    tryToBecomeCentral();
  }

  if (remotePeerSeen && millis() - lastPeerSeenMs > PEER_TIMEOUT_MS) {
    remotePeerSeen = false;
    remoteMotionLevel = 0;
    blinkState = false;
  }
}

// ------------------------------------------------------------
// Action section
// Replace this function if you want a different response when
// the other board's motion level becomes active.
// ------------------------------------------------------------
unsigned long blinkIntervalForLevel(byte motionLevel) {
  if (motionLevel == 0) {
    return BLINK_INTERVAL_SLOW_MS;
  }

  unsigned long intervalRange = BLINK_INTERVAL_SLOW_MS - BLINK_INTERVAL_FAST_MS;
  return BLINK_INTERVAL_SLOW_MS - ((unsigned long)(motionLevel - 1) * intervalRange / (MOTION_LEVEL_STEPS - 1));
}

void runActionFromRemoteEvent() {
  if (remoteEventActive()) {
    unsigned long blinkIntervalMs = blinkIntervalForLevel(remoteMotionLevel);
    if (millis() - lastBlinkMs >= blinkIntervalMs) {
      lastBlinkMs = millis();
      blinkState = !blinkState;
      setRgb(blinkState, false, false);
    }
    return;
  }

  blinkState = false;

  if (localEventActive()) {
    setRgb(true, false, true);
  } else if (remotePeerSeen) {
    setRgb(false, true, false);
  } else {
    setRgb(false, false, true);
  }
}

void stopWithErrorLight() {
  setRgb(true, false, false);
  while (true) {
    delay(1000);
  }
}

void setup() {
  Serial.begin(115200);

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

  myAddress = BLE.address();

  peerService.addCharacteristic(outgoingEventCharacteristic);
  peerService.addCharacteristic(incomingEventCharacteristic);
  BLE.addService(peerService);

  BLE.setLocalName(DEVICE_NAME);
  BLE.setAdvertisedService(peerService);
  outgoingEventCharacteristic.writeValue((byte)0);
  incomingEventCharacteristic.writeValue((byte)0);

  searchScanMode = initialSearchModeFromAddress();
  enterDiscoveryMode();
}

void loop() {
  BLE.poll();

  updateLocalMotion();
  updateLink();
  runActionFromRemoteEvent();
}
