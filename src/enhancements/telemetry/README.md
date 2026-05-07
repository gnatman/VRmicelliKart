# Telemetry Module

## Purpose
Broadcasts live game state over UDP for motion simulators, bass shakers, and dashboards (e.g., SimHub, Yaw, DOF Reality).

## How It Fits
Hooks into the game loop (`GameEngine::StartFrame()`) to read `Player` and `Camera` state once per frame. It translates N64 arbitrary units into real-world units (meters, km/h, G-forces) and synthesizes missing data (RPM, gears).

## Architecture
- **Lockless SPSC Ring:** Data is collected on the main 30 Hz game thread and passed safely to a background worker thread.
- **Worker Thread:** Sends UDP packets non-blocking at a configurable rate (default 60 Hz).
- **Formats:** Defaults to Project CARS 2 UDP v2 packet layout.
