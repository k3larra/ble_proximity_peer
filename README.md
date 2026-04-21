# BLE Prototyping Examples

This repository now contains three Arduino sketches for BLE-based interactive prototyping with the Arduino Nano 33 BLE Sense family.

## Important Hardware Note

The split motion pair is for:

- Arduino Nano 33 BLE Sense Rev2 only

These Rev2-only sketches are:

- `ble_motion_shaker/ble_motion_shaker.ino`
- `ble_motion_responder/ble_motion_responder.ino`

They use the Rev2 IMU library:

- `Arduino_BMI270_BMM150`

The original symmetric sketch is kept as a separate teaching example:

- `ble_proximity_peer/ble_proximity_peer.ino`

## Sketches

### 1. Symmetric proximity example

- `ble_proximity_peer/ble_proximity_peer.ino`

Both boards run the same sketch and behave symmetrically.

- each board reads its own proximity sensor
- each board can trigger the other board
- the other board blinks red when a local event is active

This is the simpler peer-to-peer teaching example and still works well for slower, more stable sensor states.

### 2. One-way motion sender

- `ble_motion_shaker/ble_motion_shaker.ino`

This is the `SHAKER` sketch.

- it runs on the board inside the chick
- it reads the Rev2 IMU
- it computes a motion level from short RMS windows
- it advertises that motion level over BLE

### 3. One-way motion responder

- `ble_motion_responder/ble_motion_responder.ino`

This is the `RESPONDER` sketch.

- it runs on the board inside the father
- it scans for the shaker
- it reads the shaker motion level over BLE
- it blinks red based on the received intensity

## Which Pattern To Use

### Symmetric pattern

The symmetric pattern works best when the sensor becomes a stable event state, for example:

- proximity near / far
- button pressed / not pressed
- switches
- slow threshold-based sensors

Why it works:

- the signal changes relatively slowly
- the event often stays on long enough to be resent
- a missed BLE update is less noticeable

### One-way pattern

The one-way pattern works better when the input is brief, bursty, or noisy, for example:

- motion from an IMU
- shaking
- taps
- fast expressive gestures
- sensors where intensity changes quickly

Why it helps:

- one board only sends
- one board only receives and reacts
- the BLE transport becomes simpler
- students can focus on the output object without changing the sensing object

For the peacock chick / father project, the one-way pattern is the recommended one.

## Hardware Notes

- `ble_proximity_peer` was kept as the original symmetric example
- the split motion sketches are intended for Arduino Nano 33 BLE Sense Rev2 only
- the motion sketches use `Arduino_BMI270_BMM150`

## LED Meanings For The Motion Pair

### Shaker

- Blue: waiting for BLE connection
- Cyan: connected and idle
- Purple: motion is currently active enough to send
- Solid red: startup error

### Responder

- Blue: scanning for the shaker
- Green: connected and idle
- Blinking red: reacting to received motion
- Solid red: startup error

## Sensitivity Tuning For The Shaker

These settings are near the top of `ble_motion_shaker.ino`:

- `MOTION_START_THRESHOLD`
  Raise this if the shaker reacts too easily.
  Lower it if stronger handling is needed before anything happens.
- `MOTION_STOP_THRESHOLD`
  Raise this if the shaker stays active too long after movement stops.
  Lower it if the trigger turns off too quickly.
- `MOTION_FULL_SCALE`
  Lower this if strong shaking should reach the highest response more easily.
  Raise it if the strongest responses happen too often.
- `MOTION_WINDOW_MS`
  Lower this for quicker response with less averaging.
  Raise it for smoother response with more averaging.
- `MOTION_LEVEL_MAX`
  Changes how many motion steps are sent to the responder.

## Suggested Uploads

### Symmetric classroom demo

1. Upload `ble_proximity_peer.ino` to both boards.
2. Test the proximity event-action behavior.

### Peacock chick / father project

1. Upload `ble_motion_shaker.ino` to the chick board.
2. Upload `ble_motion_responder.ino` to the father board.
3. Power both boards.
4. Wait for the BLE link to form.
5. Shake the chick and watch the father react.

## Manual

The original student-facing manual is here:

- `ble_proximity_peer/MANUAL.md`

It now also includes guidance on when to choose a symmetric BLE example and when a one-way design is a better fit.
