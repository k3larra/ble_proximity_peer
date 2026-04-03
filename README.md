# ble-proximity_peer

Two Arduino Nano 33 BLE Sense boards running one shared BLE prototype.

Main sketch:
[ble_proximity_peer.ino](C:/Users/K3LARA/Box/Jobb%20(lars.holmberg@mah.se%203)/KD104B_IDF_VT26/agent3/ble_proximity_peer/ble_proximity_peer.ino)

Student guide:
[MANUAL.md](C:/Users/K3LARA/Box/Jobb%20(lars.holmberg@mah.se%203)/KD104B_IDF_VT26/agent3/ble_proximity_peer/MANUAL.md)

## What it does

- both boards run the same sketch
- one board detects a local event using the proximity sensor
- the event is sent over BLE
- the other board blinks red

## Current event and action

- Event: object close to proximity sensor
- Action: fast blinking red RGB LED on the other board

## Why this repo is structured this way

This repository keeps only one example so it is easier to teach and easier to modify.

Students can focus on:

- the event logic
- the action logic
- the relationship between two connected devices

## Files

- `ble_proximity_peer/ble_proximity_peer.ino`: main sketch
- `ble_proximity_peer/MANUAL.md`: student-facing manual
