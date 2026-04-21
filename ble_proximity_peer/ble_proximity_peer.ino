#include <ArduinoBLE.h>
#include <Arduino_APDS9960.h>

// ============================================================
// Beginner setup section
// Change values here when you want to try a different sensor
// or a different action.
// ============================================================

const char* DEVICE_NAME = "Nano33-Peer";
const char* SERVICE_UUID = "19B10010-E8F2-537E-4F6C-D104768A1214";
const char* OUTGOING_EVENT_UUID = "19B10011-E8F2-537E-4F6C-D104768A1214";
const char* INCOMING_EVENT_UUID = "19B10012-E8F2-537E-4F6C-D104768A1214";

const bool DEBUG_BLE = false;

const int EVENT_ON_THRESHOLD = 200;
const int EVENT_OFF_THRESHOLD = 150;
const bool EVENT_IS_ON_WHEN_VALUE_IS_HIGH = false;

const unsigned long SENSOR_SAMPLE_MS = 50;
const unsigned long LINK_UPDATE_MS = 80;
const unsigned long PEER_TIMEOUT_MS = 1000;
const unsigned long ROLE_RETRY_MS = 8000;
const unsigned long SEARCH_ADVERTISE_MS = 4000;
const unsigned long SEARCH_SCAN_MS = 6000;
const unsigned long BLINK_INTERVAL_MS = 120;

BLEService peerService(SERVICE_UUID);
BLEByteCharacteristic outgoingEventCharacteristic(OUTGOING_EVENT_UUID, BLERead | BLENotify);
BLEByteCharacteristic incomingEventCharacteristic(INCOMING_EVENT_UUID, BLERead | BLEWrite | BLEWriteWithoutResponse);

String myAddress;
bool localEventActive = false;
bool remoteEventActive = false;
bool remotePeerSeen = false;
bool blinkState = false;

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
// Event section
// Replace the inside of this function if you want to use
// another sensor as the thing that triggers the event.
// ------------------------------------------------------------
int readEventSensorValue() {
  if (!APDS.proximityAvailable()) {
    return -1;
  }

  return APDS.readProximity();
}

bool shouldEventBeActive(int sensorValue, bool previousState) {
  if (sensorValue < 0) {
    return previousState;
  }

  if (EVENT_IS_ON_WHEN_VALUE_IS_HIGH) {
    if (!previousState && sensorValue >= EVENT_ON_THRESHOLD) {
      return true;
    }

    if (previousState && sensorValue <= EVENT_OFF_THRESHOLD) {
      return false;
    }
  } else {
    if (!previousState && sensorValue <= EVENT_ON_THRESHOLD) {
      return true;
    }

    if (previousState && sensorValue >= EVENT_OFF_THRESHOLD) {
      return false;
    }
  }

  return previousState;
}

void updateLocalEvent() {
  if (millis() - lastSensorSampleMs < SENSOR_SAMPLE_MS) {
    return;
  }

  lastSensorSampleMs = millis();
  int sensorValue = readEventSensorValue();
  localEventActive = shouldEventBeActive(sensorValue, localEventActive);
}

// ------------------------------------------------------------
// BLE communication section
// This part keeps one BLE connection alive and moves event
// state in both directions over that single connection.
// ------------------------------------------------------------
void updateOutgoingCharacteristic() {
  outgoingEventCharacteristic.writeValue(localEventActive ? 1 : 0);
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

void enterPeripheralWaitMode() {
  debugBle("enterPeripheralWaitMode: advertise only");
  BLE.stopScan();
  BLE.advertise();
}

void resetLinkState() {
  debugBle("resetLinkState from role " + String(roleName(currentRole)));
  connectedPeripheral = BLEDevice();
  remoteOutgoingEventCharacteristic = BLECharacteristic();
  remoteIncomingEventCharacteristic = BLECharacteristic();
  remoteEventActive = false;
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
    remoteEventActive = (incomingEventCharacteristic.value() != 0);
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
    remoteEventActive = (remoteValue != 0);
    remotePeerSeen = true;
    lastPeerSeenMs = millis();
  }

  remoteIncomingEventCharacteristic.writeValue((byte)(localEventActive ? 1 : 0));
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
    remoteEventActive = false;
    blinkState = false;
  }
}

// ------------------------------------------------------------
// Action section
// Replace this function if you want a different response when
// the other board's event becomes active.
// ------------------------------------------------------------
void runActionFromRemoteEvent() {
  if (remoteEventActive) {
    if (millis() - lastBlinkMs >= BLINK_INTERVAL_MS) {
      lastBlinkMs = millis();
      blinkState = !blinkState;
      setRgb(blinkState, false, false);
    }
    return;
  }

  blinkState = false;

  if (localEventActive) {
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

  if (!APDS.begin()) {
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

  updateLocalEvent();
  updateLink();
  runActionFromRemoteEvent();
}
