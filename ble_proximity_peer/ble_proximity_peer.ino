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

const int EVENT_ON_THRESHOLD = 200;
const int EVENT_OFF_THRESHOLD = 150;
const bool EVENT_IS_ON_WHEN_VALUE_IS_HIGH = false;

const unsigned long SENSOR_SAMPLE_MS = 50;
const unsigned long LINK_UPDATE_MS = 80;
const unsigned long PEER_TIMEOUT_MS = 1000;
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

unsigned long lastSensorSampleMs = 0;
unsigned long lastLinkUpdateMs = 0;
unsigned long lastPeerSeenMs = 0;
unsigned long lastBlinkMs = 0;

enum LinkRole {
  ROLE_WAITING,
  ROLE_PERIPHERAL,
  ROLE_CENTRAL
};

LinkRole currentRole = ROLE_WAITING;

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

void enterDiscoveryMode() {
  BLE.stopScan();
  BLE.advertise();
  BLE.scan(true);
}

void enterPeripheralWaitMode() {
  BLE.stopScan();
  BLE.advertise();
}

void resetLinkState() {
  connectedPeripheral = BLEDevice();
  remoteOutgoingEventCharacteristic = BLECharacteristic();
  remoteIncomingEventCharacteristic = BLECharacteristic();
  remoteEventActive = false;
  remotePeerSeen = false;
  currentRole = ROLE_WAITING;
  lastLinkUpdateMs = 0;

  if (roleLocked && !preferCentralRole) {
    enterPeripheralWaitMode();
  } else {
    enterDiscoveryMode();
  }
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

  if (preferCentralRole) {
    enterDiscoveryMode();
  } else {
    enterPeripheralWaitMode();
  }
}

bool connectAsCentral(BLEDevice candidate) {
  BLE.stopScan();

  if (!candidate.connect()) {
    BLE.scan(true);
    return false;
  }

  if (!candidate.discoverAttributes()) {
    candidate.disconnect();
    BLE.scan(true);
    return false;
  }

  BLEService remoteService = candidate.service(SERVICE_UUID);
  if (!remoteService) {
    candidate.disconnect();
    BLE.scan(true);
    return false;
  }

  BLECharacteristic remoteOutgoing = candidate.characteristic(OUTGOING_EVENT_UUID);
  BLECharacteristic remoteIncoming = candidate.characteristic(INCOMING_EVENT_UUID);
  if (!remoteOutgoing || !remoteIncoming) {
    candidate.disconnect();
    BLE.scan(true);
    return false;
  }

  connectedPeripheral = candidate;
  remoteOutgoingEventCharacteristic = remoteOutgoing;
  remoteIncomingEventCharacteristic = remoteIncoming;
  currentRole = ROLE_CENTRAL;
  remotePeerSeen = true;
  lastPeerSeenMs = millis();
  BLE.stopAdvertise();
  return true;
}

void tryToBecomeCentral() {
  if (BLE.connected()) {
    currentRole = ROLE_PERIPHERAL;
    remotePeerSeen = true;
    lastPeerSeenMs = millis();
    BLE.stopScan();
    return;
  }

  if (roleLocked && !preferCentralRole) {
    return;
  }

  BLEDevice candidate = BLE.available();
  if (!isCandidatePeer(candidate)) {
    return;
  }

  if (!roleLocked) {
    lockRoleFromCandidate(candidate);
  }

  if (!preferCentralRole) {
    return;
  }

  connectAsCentral(candidate);
}

void updatePeripheralLink() {
  if (!BLE.connected()) {
    resetLinkState();
    return;
  }

  currentRole = ROLE_PERIPHERAL;
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
    currentRole = ROLE_WAITING;
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

  enterDiscoveryMode();
}

void loop() {
  BLE.poll();

  updateLocalEvent();
  updateLink();
  runActionFromRemoteEvent();
}
