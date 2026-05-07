# Wheel & Force Feedback Module

## Purpose
Adds robust racing wheel support, complete with configurable steering linearity, degrees of rotation (DOR), and advanced Force Feedback (FFB).

## How It Fits
- **Input:** Replaces standard joystick axis mapping with a curve-adjusted `WheelAxisMapping` and `WheelPedalMapping` via libultraship's SDL2 ControlDeck.
- **FFB:** Bypasses SDL2's haptic API in favor of **DirectInput8** for full control over centering springs, periodic rumbles, and collision constants.
- **FFBEffects:** Translates game state (surface type, kart hop velocity, wall collisions) into physical wheel torque.
