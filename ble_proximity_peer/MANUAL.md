# BLE Proximity Peer

This project uses two Arduino Nano 33 BLE Sense boards.

Both boards run the same sketch:
[ble_proximity_peer.ino](ble_proximity_peer.ino)

## The Idea

This is a simple event-action prototype.

- An event happens on one board.
- The event is sent to the other board using BLE.
- The other board performs an action.

In this version:

- Event: the board is moved or shaken.
- Action: the other board blinks red, with faster blinking for stronger motion.

## What Students Should Learn From It

- how to read a sensor
- how to turn a sensor reading into an event
- how to send state between two devices
- how to separate input, communication, and output
- how to modify one part of a prototype without rewriting everything

## What the Current Prototype Does

- Each board reads its own motion sensor.
- Each board runs the same code.
- The two boards create one BLE connection.
- One board becomes the BLE central.
- One board becomes the BLE peripheral.
- If board A is shaken, board B blinks red.
- If board B is shaken, board A blinks red.

## LED Meanings

- Blue: waiting for BLE connection
- Green: BLE connection is active
- Purple: this board is currently moving enough to trigger
- Blinking red: the other board is currently moving enough to trigger
- Solid red: startup error

## Quick Start

> **Classroom note: choose a unique pair name before uploading.**
>
> If many students use this sketch in the same room, each pair of boards should use its own `DEVICE_NAME`.
> The two boards that should talk to each other must have identical names.
> Other pairs should use different names, for example `Nano33-Pair-01`, `Nano33-Pair-02`, and `Nano33-Pair-03`.
>
> If you mix an older Nano 33 BLE Sense and a Rev2 board, upload separately:
> set `USE_REV2_IMU` to `0` before compiling for the older board, and set it to `1` before compiling for the Rev2 board.

1. Connect both boards by USB.
2. Check the `DEVICE_NAME` near the top of the sketch.
3. Make sure both boards in your pair use the same `DEVICE_NAME`.
4. Upload the sketch to both boards.
5. Restart both boards if needed.
6. Wait a few seconds. If one board starts much later than the other, connection can take up to about 10 seconds.
7. When the connection is working, both boards should usually be green when idle.
8. Pick up one board and shake or move it clearly.
9. That board should turn purple.
10. The other board should blink red.
11. More intense shaking should produce faster blinking.

## The Three Main Parts of the Code

Students should mainly understand these three parts:

### 1. Event

This part decides when "something happened" on the local board.

Main functions:

- `readMotionSample()`
- `quantizeMotionLevel()`
- `updateLocalMotion()`

### 2. BLE Communication

This part sends the local event to the other board and receives the other board's event.

Students usually do not need to change this section.

When no peer is connected, each board alternates between two search modes:

- advertising, where it waits for the other board to connect
- scanning, where it looks for the other board and tries to connect

This makes the pair recover if one board is started later, unplugged, or reset.

### 3. Action

This part decides what the board should do when the remote event is active.

Main function:

- `runActionFromRemoteEvent()`

## Task Ideas for Students

Students can keep the BLE section unchanged and instead work with one of these tasks:

- replace the motion sensor with a button
- replace the motion sensor with another sensor
- change the blink pattern
- change the action from light to sound
- make the action happen only after a longer trigger
- make the event stronger or weaker by changing thresholds

## The Easiest Things to Change

### Change the Event

The easiest functions to edit are:

- `readMotionSample()`
- `quantizeMotionLevel()`

Example:

- use a button instead of the proximity sensor
- return a fixed high motion value when `digitalRead(buttonPin) == HIGH`
- return `0` when the button is not pressed

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
- `USE_REV2_IMU` - `0` for the earlier Nano 33 BLE Sense, `1` for Rev2
- `MOTION_START_THRESHOLD`
- `MOTION_STOP_THRESHOLD`
- `MOTION_FULL_SCALE`
- `MOTION_LEVEL_STEPS`
- `MOTION_SMOOTHING`
- `SENSOR_SAMPLE_MS`
- `LINK_UPDATE_MS`
- `BLINK_INTERVAL_FAST_MS`
- `BLINK_INTERVAL_SLOW_MS`

What they control:

- when the event starts
- when the event stops
- how much motion counts as "very strong"
- how many motion steps are sent over BLE
- how much the motion reading is smoothed
- how often the sensor is read
- how often BLE updates are allowed
- the fastest and slowest blink speeds

## If the Motion Feels Too Sensitive or Not Sensitive Enough

Try changing:

- `MOTION_START_THRESHOLD`
- `MOTION_STOP_THRESHOLD`
- `MOTION_FULL_SCALE`

Examples:

- raise `MOTION_START_THRESHOLD` if careful handling still triggers
- lower `MOTION_START_THRESHOLD` if strong shaking is needed before anything happens
- lower `MOTION_FULL_SCALE` if you want violent movement to reach the fastest blink more easily

## Suggested Student Workflow

1. First make sure the original version works.
2. Change only one thing at a time.
3. Test after each change.
4. If something breaks, go back to the last working version.
5. Avoid changing the BLE section unless the goal is specifically to study communication between devices.

## Troubleshooting

- If both boards stay blue, the BLE connection has not formed yet.
- If one board is started later than the other, wait at least 10-15 seconds. The boards take turns advertising and scanning until they find each other.
- If one board has been unplugged or reset, the pair should reconnect automatically after a short wait.
- If the boards connect and then disconnect repeatedly, restart both boards and check that both have the same sketch version.
- If one board turns purple but the other board does not blink red, upload the same sketch again to both boards.
- If careful handling triggers the effect too easily, raise `MOTION_START_THRESHOLD`.
- If the LED changes too slowly for violent shaking, lower `BLINK_INTERVAL_FAST_MS`.
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
