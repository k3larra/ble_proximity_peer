# BLE Proximity Peer

This project uses two Arduino Nano 33 BLE Sense boards.

Both boards run the same sketch:
[ble_proximity_peer.ino](C:/Users/K3LARA/Box/Jobb%20(lars.holmberg@mah.se%203)/KD104B_IDF_VT26/agent3/ble_proximity_peer/ble_proximity_peer.ino)

## The Idea

This is a simple event-action prototype.

- An event happens on one board.
- The event is sent to the other board using BLE.
- The other board performs an action.

In this version:

- Event: an object comes close to the proximity sensor.
- Action: the other board blinks red quickly.

## What Students Should Learn From It

- how to read a sensor
- how to turn a sensor reading into an event
- how to send state between two devices
- how to separate input, communication, and output
- how to modify one part of a prototype without rewriting everything

## What the Current Prototype Does

- Each board reads its own proximity sensor.
- Each board runs the same code.
- The two boards create one BLE connection.
- One board becomes the BLE central.
- One board becomes the BLE peripheral.
- If board A detects something close, board B blinks red.
- If board B detects something close, board A blinks red.

## LED Meanings

- Blue: waiting for BLE connection
- Green: BLE connection is active
- Purple: this board's own event is active
- Blinking red: the other board's event is active
- Solid red: startup error

## Quick Start

> **Classroom note: choose a unique pair name before uploading.**
>
> If many students use this sketch in the same room, each pair of boards should use its own `DEVICE_NAME`.
> The two boards that should talk to each other must have identical names.
> Other pairs should use different names, for example `Nano33-Pair-01`, `Nano33-Pair-02`, and `Nano33-Pair-03`.

1. Connect both boards by USB.
2. Check the `DEVICE_NAME` near the top of the sketch.
3. Make sure both boards in your pair use the same `DEVICE_NAME`.
4. Upload the sketch to both boards.
5. Restart both boards if needed.
6. Wait a few seconds.
7. When the connection is working, both boards should usually be green when idle.
8. Move an object close to the sensor on one board.
9. That board should turn purple.
10. The other board should blink red.

## The Three Main Parts of the Code

Students should mainly understand these three parts:

### 1. Event

This part decides when "something happened" on the local board.

Main functions:

- `readEventSensorValue()`
- `shouldEventBeActive()`
- `updateLocalEvent()`

### 2. BLE Communication

This part sends the local event to the other board and receives the other board's event.

Students usually do not need to change this section.

### 3. Action

This part decides what the board should do when the remote event is active.

Main function:

- `runActionFromRemoteEvent()`

## Task Ideas for Students

Students can keep the BLE section unchanged and instead work with one of these tasks:

- replace the proximity sensor with a button
- replace the proximity sensor with another sensor
- change the blink pattern
- change the action from light to sound
- make the action happen only after a longer trigger
- make the event stronger or weaker by changing thresholds

## The Easiest Things to Change

### Change the Event

The easiest functions to edit are:

- `readEventSensorValue()`
- `shouldEventBeActive()`

Example:

- use a button instead of the proximity sensor
- return `digitalRead(buttonPin)` from `readEventSensorValue()`
- let `shouldEventBeActive()` return `sensorValue == HIGH`

### Change the Action

The easiest function to edit is:

- `runActionFromRemoteEvent()`

Right now the action is:

- blink the RGB LED red

This can be replaced with:

- another LED pattern
- a buzzer
- a motor
- serial messages
- a display output

## Important Settings Near the Top of the Sketch

- `DEVICE_NAME` - must be identical on the two boards in one pair, and different from other pairs in the room
- `EVENT_ON_THRESHOLD`
- `EVENT_OFF_THRESHOLD`
- `EVENT_IS_ON_WHEN_VALUE_IS_HIGH`
- `SENSOR_SAMPLE_MS`
- `BLINK_INTERVAL_MS`

What they control:

- when the event starts
- when the event stops
- whether high values or low values mean "active"
- how often the sensor is read
- how fast the red blinking is

## If the Sensor Feels Backwards

Try changing:

- `EVENT_IS_ON_WHEN_VALUE_IS_HIGH`

For example:

- change it from `false` to `true`

Then upload the sketch again and test both boards.

## Suggested Student Workflow

1. First make sure the original version works.
2. Change only one thing at a time.
3. Test after each change.
4. If something breaks, go back to the last working version.
5. Avoid changing the BLE section unless the goal is specifically to study communication between devices.

## Troubleshooting

- If both boards stay blue, the BLE connection has not formed yet.
- If the boards connect and then disconnect repeatedly, restart both boards and check that both have the same sketch version.
- If one board turns purple but the other board does not blink red, upload the same sketch again to both boards.
- If the event seems inverted, test `EVENT_IS_ON_WHEN_VALUE_IS_HIGH`.
- If a board stays solid red, BLE or the sensor failed to start.

## For Teachers

This example is suitable for beginner-level prototyping because it supports a clear event-action model:

- input on one board
- communication between boards
- output on the other board

It can be used as:

- a working demo
- a modification exercise
- a starting point for simple interactive design prototypes
