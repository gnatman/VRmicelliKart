# Motion-Sim Telemetry Plan

## Overview
UDP telemetry output for motion simulators and external dashboards (SimHub).

## CVars

| CVar | Type | Default | Purpose |
|---|---|---|---|
| `gTelemetry.Enabled` | int 0/1 | 0 | Master switch |
| `gTelemetry.Format` | int | 0 | 0=PC2, 1=DR2, 2=both |
| `gTelemetry.Host` | string | "127.0.0.1" | Destination (SimHub runs on loopback by default) |
| `gTelemetry.PortPC2` | int | 5606 | PC2 default |
| `gTelemetry.PortDR2` | int | 20777 | DR2 default |
| `gTelemetry.Rate` | int Hz | 60 | 30–120 range |
| `gTelemetry.PlayerIndex` | int | 0 | Which player in `gPlayers[]` |
| `gTelemetry.SpeedScale` | float | *empirical* | N64 units → m/s |
| `gTelemetry.AccelScale` | float | *empirical* | N64 units/tick² → m/s² |
| `gTelemetry.RPMMin` | int | 900 | Fake engine idle |
| `gTelemetry.RPMMax` | int | 9500 | Fake engine redline |
| `gTelemetry.MaxGears` | int | 4 | Faked gear count |

## Packet Formats

### Project CARS 2 UDP v2 (Recommended)
Default format, richest support in SimHub. Includes:
- Participant Info
- Telemetry Data (Speed, RPM, Gear, Pos, Velocity, Accel/G-force, Tyres, Surface)

### DiRT Rally 2.0
Optional "Extra Data 3" format.

## SimHub Integration
SimHub should be configured to listen on the specified port for the selected format. The "Project CARS 2" or "DiRT Rally 2.0" game plugin should be enabled.
