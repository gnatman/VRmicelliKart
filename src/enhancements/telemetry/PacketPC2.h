#pragma once
#include <cstdint>

#pragma pack(push, 1)
struct PacketBase {
    uint32_t mPacketNumber;
    uint32_t mCategoryPacketNumber;
    uint8_t mPartialPacketIndex;
    uint8_t mPartialPacketNumber;
    uint8_t mPacketType;
    uint8_t mPacketVersion;
};

struct sParticipantInfo {
    int16_t sWorldPosition[3];
    int16_t sOrientation[3];
    uint16_t sCurrentLapDistance;
    uint8_t sRacePosition;
    uint8_t sLapsCompleted;
    uint8_t sCurrentLap;
    uint8_t sSector;
    uint8_t sLastSectorTime;
};

struct sTelemetryData {
    static const uint32_t sPacketSize = 556;
    PacketBase sBase;
    int8_t sViewedParticipantIndex;
    uint8_t sUnfilteredThrottle;
    uint8_t sUnfilteredBrake;
    int8_t sUnfilteredSteering;
    uint8_t sUnfilteredClutch;
    uint8_t sCarFlags;
    int16_t sOilTempCelsius;
    uint16_t sOilPressureKPa;
    int16_t sWaterTempCelsius;
    uint16_t sWaterPressureKpa;
    uint16_t sFuelPressureKpa;
    uint8_t sFuelLevelPct;
    uint16_t sVirtualRoadTemp;
    uint16_t sUnused1;
    float sFuelCapacity;
    int16_t sBrakeTempCelsius[4];
    uint16_t sTyrePressureKpa[4];
    uint8_t sTyreTemp[4];
    uint8_t sTyreInternalAirTemp[4];
    uint16_t sWheelSpeed[4];
    uint8_t sTyreTreadTemp[4];
    uint8_t sTyreLayerTemp[4];
    uint8_t sTyreCarcassTemp[4];
    uint8_t sTyreRimTemp[4];
    uint8_t sTyreInternalTemp[4];
    uint16_t sWheelLocalPositionY[4];
    uint16_t sRideHeight[4];
    uint16_t sSuspensionTravel[4];
    uint16_t sSuspensionVelocity[4];
    uint16_t sAirPressure[4];
    float sEngineSpeed;
    float sEngineTorque;
    int16_t sVelocity[3];
    int16_t sAcceleration[3];
    int16_t sAngularVelocity[3];
    int16_t sAngularAcceleration[3];
    int8_t sGear;
    uint8_t sNumGears;
    uint8_t sTyreCompound[4];
    uint8_t sUnused2;
    int16_t sSteering;
    sParticipantInfo sParticipantInfo[32];
};
#pragma pack(pop)