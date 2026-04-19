#ifndef PACKET_PC2_H
#define PACKET_PC2_H

#include <stdint.h>

#pragma pack(push, 1)

/**
 * @brief Base header for all Project CARS 2 UDP packets.
 * Total size: 12 bytes.
 */
struct sPacketBase {
    uint32_t mPacketNumber;
    uint32_t mCategoryPacketNumber;
    uint8_t mPartialPacketIndex;
    uint8_t mPartialPacketNumber;
    uint8_t mPacketType;
    uint8_t mPacketVersion;
};

/**
 * @brief PC2 Telemetry Data (Packet Type 0).
 * This is a simplified version containing the essential fields for motion telemetry.
 */
struct sTelemetryData {
    sPacketBase base;               // 0-11

    // Participation
    int8_t mViewedParticipantIndex; // 12

    // Input
    uint8_t mUnfilteredThrottle;    // 13
    uint8_t mUnfilteredBrake;       // 14
    int8_t mUnfilteredSteering;     // 15 - -127 to 127
    uint8_t mUnfilteredClutch;      // 16

    // Car State
    uint8_t mCarFlags;              // 17
    int16_t mOilTempCelsius;        // 18-19
    int16_t mWaterTempCelsius;      // 20-21
    uint16_t mRpm;                  // 22-23
    uint16_t mMaxRpm;               // 24-25
    float mSpeed;                   // 26-29 (m/s)
    uint8_t mGearNumGears;          // 30 (bits 0-3 gear, bits 4-7 num gears)

    // Motion
    float mOrientation[3];          // 31-42 (Yaw, Pitch, Roll)
    float mLocalVelocity[3];        // 43-54
    float mWorldPosition[3];        // 55-66
    float mLocalAcceleration[3];    // 67-78
};

#pragma pack(pop)

#endif // PACKET_PC2_H
