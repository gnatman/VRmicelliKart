#pragma once

#include <cstdint>

#pragma pack(push, 1)
struct TelemetryPacket {
    // Header (54 bytes)
    uint32_t GameSignature;                 // 0x00
    uint32_t TelemetrySignature;            // 0x04
    uint16_t LayoutMajorVersion;            // 0x08
    uint16_t LayoutMinorVersion;            // 0x0A
    uint64_t EmitterInstanceId;             // 0x0C
    uint8_t  PacketId;                      // 0x14
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

    // Fields (86 bytes)
    float  YawDegrees;                      // 0x37
    float  PitchDegrees;                    // 0x3B
    float  RollDegrees;                     // 0x3F
    float  LocalSurgeMs2;                   // 0x43
    float  LocalSwayMs2;                    // 0x47
    float  LocalHeaveMs2;                   // 0x4B
    float  LocalVelocityForwardMps;         // 0x4F
    float  LocalVelocityLateralMps;         // 0x53
    float  LocalVelocityUpMps;              // 0x57
    double VehiclePositionNorth;            // 0x5B
    double VehiclePositionEast;             // 0x63
    double VehiclePositionUp;               // 0x6B
    float  GroundSpeedKmh;                  // 0x73
    uint32_t CompletedLaps;                 // 0x77
    int32_t RacePosition;                   // 0x7B
    double CurrentLapTime;                  // 0x7F
    int8_t IsRaceActive;                    // 0x87
    int32_t EventFlags;                     // 0x88 (End at 0x8C / 140 bytes)
};
#pragma pack(pop)

// SimHub Constants
#define TELEMETRY_GAME_SIGNATURE 0xE1E56A8F
#define TELEMETRY_PROTOCOL_SIGNATURE 0x10633BA4

// Event Flags (Bitmask)
#define TELEMETRY_EVENT_JUMPING  (1 << 0)
#define TELEMETRY_EVENT_BOOSTING (1 << 1)
#define TELEMETRY_EVENT_HIT      (1 << 2)
#define TELEMETRY_EVENT_SPINOUT  (1 << 3)
