# ble_proximity_peer

A simple two-device BLE prototype for the Arduino Nano 33 BLE Sense.

Two boards run the same sketch. When one board is shaken, the other board blinks red, with faster blinking for stronger motion.

## Why this project exists

This repository is designed as a beginner-friendly example for interactive prototyping.

It shows how to separate a prototype into three parts:

- input
- communication
- output

In this version:

- Input: motion sensing from the built-in IMU
- Communication: Bluetooth Low Energy
- Output: red LED blinking speed linked to motion intensity

## Quick Overview

- both boards run the same Arduino sketch
- one board senses a local event
- the event is sent over BLE
- the other board performs an action

Current prototype:

- Event: the board is moved or shaken hard enough
- Action: the other board blinks red, faster for stronger motion

## Repository Structure

- `ble_proximity_peer/ble_proximity_peer.ino`
  The main sketch to upload to both boards.
- `ble_proximity_peer/MANUAL.md`
  A student-facing guide for understanding and modifying the prototype.

## Getting Started

1. Connect two Arduino Nano 33 BLE Sense boards.
2. Open `ble_proximity_peer/ble_proximity_peer.ino` in the Arduino IDE.
3. Upload the same sketch to both boards.
4. Wait a few seconds for the BLE connection to form.
5. Move an object close to one board.
6. The other board should blink red.

## Teaching Use

This project works well as:

- a live demo
- a first BLE prototype
- a modification exercise
- a starting point for event-action assignments

Students can modify:

- the event logic
- the action logic
- sensor thresholds

without needing to redesign the BLE section.

## Documentation

For the full student guide, see:

- `ble_proximity_peer/MANUAL.md`
