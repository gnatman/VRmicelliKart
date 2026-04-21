#include "TelemetryMenu.h"
#include "enhancements/telemetry/Telemetry.h"
#include "PortMenu.h"
#include <libultraship.h>
#include <string>
#include <spdlog/fmt/fmt.h>

namespace GameUI {

extern std::shared_ptr<PortMenu> mPortMenu;

void AddTelemetrySettings() {
    // Add Telemetry menu entry
    mPortMenu->AddMenuEntry("Telemetry", "gTelemetry.Menu.TelemetrySidebarSection");

    // General Telemetry Settings
    mPortMenu->AddSidebarEntry("Telemetry", "General", 2);
    WidgetPath path = { "Telemetry", "General", SECTION_COLUMN_1 };

    mPortMenu->AddWidget(path, "Enable Telemetry", WIDGET_CVAR_CHECKBOX)
        .CVar("gTelemetryEnabled")
        .Options(UIWidgets::CheckboxOptions().Tooltip("Enable or disable UDP telemetry output."));

    mPortMenu->AddWidget(path, "Telemetry Host", WIDGET_CUSTOM)
        .CustomFunction([](WidgetInfo& info) {
            char buf[256];
            strncpy(buf, CVarGetString("gTelemetryHost", "127.0.0.1"), sizeof(buf));
            if (ImGui::InputText("Host", buf, sizeof(buf))) {
                CVarSetString("gTelemetryHost", buf);
                Ship::Context::GetInstance()->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("The IP address or hostname to send telemetry to (default: 127.0.0.1).");
            }
        });

    mPortMenu->AddWidget(path, "Telemetry Port", WIDGET_CVAR_SLIDER_INT)
        .CVar("gTelemetryPort")
        .Options(UIWidgets::IntSliderOptions()
            .Min(1)
            .Max(65535)
            .DefaultValue(5606)
            .Tooltip("The UDP port to send telemetry to (default: 5606)."));

    mPortMenu->AddWidget(path, "Speed Scale: %.2f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gTelemetrySpeedScale")
        .Options(UIWidgets::FloatSliderOptions()
            .Format("%.2f")
            .Min(0.1f)
            .Max(10.0f)
            .DefaultValue(1.0f)
            .Step(0.1f)
            .Tooltip("Adjust the speed scale in the telemetry data."));

    mPortMenu->AddWidget(path, "Acceleration Scale: %.2f", WIDGET_CVAR_SLIDER_FLOAT)
        .CVar("gTelemetryAccelScale")
        .Options(UIWidgets::FloatSliderOptions()
            .Format("%.2f")
            .Min(0.1f)
            .Max(10.0f)
            .DefaultValue(1.0f)
            .Step(0.1f)
            .Tooltip("Adjust the acceleration scale in the telemetry data."));

    path.column = SECTION_COLUMN_2;
    
    // Live Readout Header
    mPortMenu->AddWidget(path, "Live Readout", WIDGET_SEPARATOR_TEXT);
    
    // Live Readout Text
    mPortMenu->AddWidget(path, "Status", WIDGET_TEXT)
        .PreFunc([](WidgetInfo& info) {
            if (Telemetry::Instance) {
                sTelemetryData packet = Telemetry::Instance->GetPacket();
                float speedKph = std::abs(packet.mSpeed * 3.6f);
                int gear = (packet.mGearNumGears & 0x0F);
                info.name = fmt::format("Speed: {:.1f} km/h\nRPM: {}\nGear: {}\n\nAccel: {:.2f}, {:.2f}, {:.2f}", 
                    speedKph, (int)packet.mRpm, gear, 
                    packet.mLocalAcceleration[0], packet.mLocalAcceleration[1], packet.mLocalAcceleration[2]);
            } else {
                info.name = "Telemetry not initialized";
            }
        });
}

}
