# BLE Peacock Pair

This branch now uses two separate Arduino sketches for a one-way BLE interaction on Arduino Nano 33 BLE Sense Rev2 boards.

- `ble_motion_shaker/ble_motion_shaker.ino`
- `ble_motion_responder/ble_motion_responder.ino`

The direction is fixed:

- the `SHAKER` board is the chick
- the `RESPONDER` board is the father

When the shaker board is handled roughly, it sends a motion level over BLE and the responder board blinks red. Stronger motion causes faster blinking.

## Why It Is Split

The earlier combined sketch tried to support both roles in one file. That made the BLE logic harder to reason about and harder for students to modify.

This version separates the responsibilities cleanly:

- the shaker sketch reads the IMU and advertises motion
- the responder sketch connects and reacts

That makes it easier to add new behavior such as:

- servos
- sound
- other outputs on the responder

## Hardware

This version is for:

- Arduino Nano 33 BLE Sense Rev2

It uses the built-in BMI270/BMM150 IMU through:

- `Arduino_BMI270_BMM150`

## LED Meanings

### Shaker

- Blue: waiting for a BLE connection
- Cyan: connected and idle
- Purple: motion is currently active enough to send
- Solid red: startup error

### Responder

- Blue: scanning for the shaker
- Green: connected and idle
- Blinking red: reacting to received motion
- Solid red: startup error

## Files

### `ble_motion_shaker/ble_motion_shaker.ino`

This sketch:

- reads acceleration
- computes a motion level from short RMS windows
- quantizes the level to `0..5`
- advertises that level over BLE

Students will mainly edit:

- thresholds
- motion mapping
- local shaker feedback

Sensitivity settings near the top of the shaker sketch:

- `MOTION_START_THRESHOLD`
  Raise this if the shaker reacts too easily.
  Lower it if stronger handling is required before anything happens.
- `MOTION_STOP_THRESHOLD`
  Raise this if the shaker stays active for too long after movement stops.
  Lower it if the trigger turns off too quickly.
- `MOTION_FULL_SCALE`
  Lower this if strong shaking should reach the highest response more easily.
  Raise it if the strongest responses happen too often.
- `MOTION_WINDOW_MS`
  Lower this for quicker response with less averaging.
  Raise it for smoother response with more averaging.
- `MOTION_LEVEL_MAX`
  Changes how many motion steps are sent to the responder.

### `ble_motion_responder/ble_motion_responder.ino`

This sketch:

- scans for the shaker
- connects to the shaker BLE service
- reads the shaker motion characteristic
- blinks the red LED based on received intensity

Students will mainly edit:

- the output behavior
- blink pattern
- servo or motor response

## Upload Order

1. Upload `ble_motion_shaker.ino` to the board that will go inside the chick.
2. Upload `ble_motion_responder.ino` to the board that will go inside the father.
3. Power both boards.
4. Wait for the BLE link to form.
5. Shake the shaker board and watch the responder react.

## Current Behavior

- the BLE direction is intentionally one-way
- the shaker always publishes
- the responder always listens
- there is no role negotiation in the application logic

That is deliberate because it is simpler and more reliable for this project.
