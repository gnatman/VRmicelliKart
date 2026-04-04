#include "Telemetry.h"
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <spdlog/spdlog.h>

static int sTelemetrySocket = -1;
static struct sockaddr_in sDestAddr;
static bool sTelemetryEnabled = false;

void Telemetry_Init() {
    sTelemetrySocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sTelemetrySocket < 0) {
        SPDLOG_ERROR("Telemetry: Failed to create socket");
        return;
    }

    memset(&sDestAddr, 0, sizeof(sDestAddr));
    sDestAddr.sin_family = AF_INET;
    sDestAddr.sin_port = htons(20777); // Standard telemetry port
    sDestAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    sTelemetryEnabled = true;
    SPDLOG_INFO("Telemetry: Initialized on 127.0.0.1:20777");
}

void Telemetry_Update(Player* player) {
    if (!sTelemetryEnabled || !player) return;

    TelemetryPacket packet;
    packet.magic = 0x4D4B3634; // 'MK64'

    std::memcpy(packet.pos, player->pos, sizeof(float) * 3);
    std::memcpy(packet.velocity, player->velocity, sizeof(float) * 3);
    packet.speed = player->speed;
    packet.currentSpeed = player->currentSpeed;
    std::memcpy(packet.rotation, player->rotation, sizeof(int16_t) * 3);

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            packet.orientationMatrix[i][j] = player->orientationMatrix[i][j];
        }
    }

    sendto(sTelemetrySocket, &packet, sizeof(packet), 0, (struct sockaddr*)&sDestAddr, sizeof(sDestAddr));
}

void Telemetry_Terminate() {
    if (sTelemetrySocket >= 0) {
        close(sTelemetrySocket);
        sTelemetrySocket = -1;
    }
    sTelemetryEnabled = false;
    SPDLOG_INFO("Telemetry: Terminated");
}
