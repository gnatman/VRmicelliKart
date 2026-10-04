#pragma once

#include <cstdint>

#pragma pack(push, 1)
struct TelemetryPacket {
    // Header (54 bytes)
    uint32_t GameSignature;                 // 0x00 (0xE1E56A8F)
    uint32_t TelemetrySignature;            // 0x04 (0x386A90C1)
    uint16_t LayoutMajorVersion;            // 0x08 (1)
    uint16_t LayoutMinorVersion;            // 0x0A (0)
    uint64_t EmitterInstanceId;             // 0x0C
    uint8_t  PacketId;                      // 0x14 (0)
    uint64_t PacketsCounter;                // 0x15
    uint8_t  IsSessionRunning;              // 0x1D
    uint8_t  IsSessionPaused;               // 0x1E
    uint64_t SessionId;                     // 0x1F
    uint8_t  IsReplay;                      // 0x27
    uint8_t  IsUserInControl;               // 0x28
    uint8_t  IsAIInControl;                 // 0x29
    uint8_t  IsSpectator;                   // 0x2A
    double   SessionTimeSeconds;            // 0x2B
    uint32_t PhysicsDiscontinuityCounter;   // 0x33

    // Standard Fields (158 bytes)
    float    YawDegrees;                    // 0x37
    float    PitchDegrees;                  // 0x3B (from slopeAccel)
    float    RollDegrees;                   // 0x3F (from unk_206)
    float    YawRateDegreesPerSecond;       // 0x43 (dYaw/dt)
    float    PitchRateDegreesPerSecond;     // 0x47 (dPitch/dt)
    float    RollRateDegreesPerSecond;      // 0x4B (dRoll/dt)

    float    LocalSurgeMs2;                 // 0x4F (Longitudinal accel)
    float    LocalSwayMs2;                  // 0x53 (Lateral accel)
    float    LocalHeaveMs2;                 // 0x57 (Vertical accel / landing shock)

    float    LocalVelocityForwardMps;       // 0x5B
    float    LocalVelocityLateralMps;       // 0x5F
    float    LocalVelocityUpMps;            // 0x63

    double   VehiclePositionNorth;          // 0x67
    double   VehiclePositionEast;           // 0x6F
    double   VehiclePositionUp;             // 0x77

    uint8_t  FlightIsOnGround;              // 0x7F (1=ground, 0=air)
    float    GroundSpeedKmh;                // 0x80

    float    Throttle;                      // 0x84 (0.0 - 1.0)
    float    Brake;                         // 0x88 (0.0 - 1.0)
    char     Gear[8];                       // 0x8C (UTF8: "1", "2", "N", "R")
    float    EngineRpm;                     // 0x94
    float    EngineMaxRpm;                  // 0x98

    float    WheelSlipFrontLeft;            // 0x9C
    float    WheelSlipFrontRight;           // 0xA0
    float    WheelSlipRearLeft;             // 0xA4
    float    WheelSlipRearRight;            // 0xA8

    float    SuspensionVelocityFrontLeftMps;  // 0xAC
    float    SuspensionVelocityFrontRightMps; // 0xB0
    float    SuspensionVelocityRearLeftMps;   // 0xB4
    float    SuspensionVelocityRearRightMps;  // 0xB8

    uint16_t TyreContactSurfaceFrontLeft;   // 0xBC (1=Primary, 2=Rumble, 4=Grass, 8=Gravel)
    uint16_t TyreContactSurfaceFrontRight;  // 0xBE
    uint16_t TyreContactSurfaceRearLeft;    // 0xC0
    uint16_t TyreContactSurfaceRearRight;   // 0xC2

    uint32_t CompletedLaps;                 // 0xC4
    int32_t  RacePosition;                  // 0xC8
    double   CurrentLapTime;                // 0xCC

    // Custom Fields (7 bytes)
    int8_t   IsRaceActive;                  // 0xD4
    int32_t  EventFlags;                    // 0xD5
    int16_t  SurfaceType;                   // 0xD9
};
#pragma pack(pop)

static_assert(sizeof(TelemetryPacket) == 219, "TelemetryPacket size mismatch: must be exactly 219 bytes");

// SimHub Constants
#define TELEMETRY_GAME_SIGNATURE 0xE1E56A8F
#define TELEMETRY_PROTOCOL_SIGNATURE 0x386A90C1

// SimHub Tyre Contact Surface bitmask values
#define SIMHUB_SURFACE_NONE         0
#define SIMHUB_SURFACE_PRIMARY      1
#define SIMHUB_SURFACE_RUMBLESTRIPS 2
#define SIMHUB_SURFACE_GRASS        4
#define SIMHUB_SURFACE_GRAVEL       8

// Event Flags (Bitmask)
#define TELEMETRY_EVENT_JUMPING    (1 << 0)
#define TELEMETRY_EVENT_BOOSTING   (1 << 1)
#define TELEMETRY_EVENT_HIT        (1 << 2)
#define TELEMETRY_EVENT_SPINOUT    (1 << 3)
#define TELEMETRY_EVENT_WALL_HIT   (1 << 4)
#define TELEMETRY_EVENT_LANDING    (1 << 5)
