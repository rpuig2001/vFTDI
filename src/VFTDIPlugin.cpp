#include "stdafx.h"
#include "VFTDIPlugin.h"
#include "CallsignLookup.h"
#include "FTDiPanel.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <sstream>

std::atomic<bool> SequenceAddRequested{ false };
std::atomic<bool> SequenceSelectionActive{ false };
std::atomic<bool> SequencePickArmed{ false };
std::atomic<bool> SequenceAircraftClickRequested{ false };
std::atomic<bool> SequenceCancelRequested{ false };

VFTDIPlugin::VFTDIPlugin()
    : CPlugIn(COMPATIBILITY_CODE,
              PLUGIN_NAME.c_str(),
              PLUGIN_VERSION.c_str(),
              PLUGIN_AUTHOR.c_str(),
              PLUGIN_COPY.c_str())
{
    RegisterPlugin();
    const char* savedManagerPosition = GetDataFromSettings("VFTDI_MANAGER_POSITION");
    if (savedManagerPosition != nullptr) {
        std::istringstream savedPosition(savedManagerPosition);
        int left = 0;
        int top = 0;
    char separator = 0;
        if (savedPosition >> left >> separator >> top && separator == ',') {
            Session->ManagerPosition = { left, top };
            Session->HasManagerPosition = true;
        }
    }

    char modulePath[_MAX_PATH] = {};
    GetModuleFileNameA(HINSTANCE(&__ImageBase), modulePath, sizeof(modulePath));
    std::string configPath = modulePath;
    const size_t directoryEnd = configPath.find_last_of("\\/");
    if (directoryEnd != std::string::npos)
        configPath.resize(directoryEnd + 1);
    CCallsignLookup::Lookup = new CCallsignLookup(configPath + "config.ini");
    Session->ActiveProfile = CCallsignLookup::Lookup->getDefaultProfile();
    Session->ManagerVisibleRows = CCallsignLookup::Lookup->getDefaultVisibleRows();
        WeatherService.reset(new AirportWeatherService(CCallsignLookup::Lookup->getAirportCodes()));

    KeyPollingThread = std::thread([this]() {
        bool ctrlWWasDown = false;
        bool leftWasDown = false;
        bool rightWasDown = false;
        while (!StopKeyPolling.load()) {
            DWORD foregroundProcess = 0;
            const HWND foregroundWindow = GetForegroundWindow();
            if (foregroundWindow != nullptr)
                GetWindowThreadProcessId(foregroundWindow, &foregroundProcess);

            const bool ctrlWDown = foregroundProcess == GetCurrentProcessId() &&
                (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0 &&
                (GetAsyncKeyState('W') & 0x8000) != 0;
            if (ctrlWDown && !ctrlWWasDown)
                SequenceAddRequested.store(true);
            ctrlWWasDown = ctrlWDown;

            const bool isForeground = foregroundProcess == GetCurrentProcessId();
            const bool selectionActive = SequenceSelectionActive.load();
                const bool leftDown = isForeground &&
				(GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
            const bool rightDown = isForeground && selectionActive &&
                (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
            if (!selectionActive)
                SequencePickArmed.store(false);
            else if (!leftDown)
                SequencePickArmed.store(true);
            if (leftDown && !leftWasDown && selectionActive && SequencePickArmed.exchange(false))
                SequenceAircraftClickRequested.store(true);
            if (rightDown && !rightWasDown)
                SequenceCancelRequested.store(true);
            leftWasDown = leftDown;
            rightWasDown = rightDown;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    });
}


VFTDIPlugin::~VFTDIPlugin()
{
    StopKeyPolling.store(true);
    if (KeyPollingThread.joinable())
        KeyPollingThread.join();
    WeatherService.reset();
    delete CCallsignLookup::Lookup;
    CCallsignLookup::Lookup = nullptr;
}

CRadarScreen * VFTDIPlugin::OnRadarScreenCreated(const char * sDisplayName, bool NeedRadarContent, bool GeoReferenced, bool CanBeSaved, bool CanBeCreated)
{
    if (!strcmp(sDisplayName, VFTDI_RADAR_SCREEN_VIEW))
        return new RadarScreen(WeatherService.get(), Session);
    if (!strcmp(sDisplayName, VFTDI_PANEL_SCREEN_VIEW))
        return new FTDiPanelScreen(Session);

	return nullptr;
}

bool VFTDIPlugin::OnCompileCommand(const char * sCommandLine)
{
    if (sCommandLine == nullptr || CCallsignLookup::Lookup == nullptr)
        return false;

    std::string command(sCommandLine);
    const size_t first = command.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return false;
    const size_t last = command.find_last_not_of(" \t\r\n");
    command = command.substr(first, last - first + 1);
    std::transform(command.begin(), command.end(), command.begin(),
        [](unsigned char value) { return static_cast<char>(std::toupper(value)); });
    if (!command.empty() && command[0] == '.')
        command.erase(0, 1);
    if (command != "SHOWLOCALIZERROUTE")
        return false;

    const bool visible = CCallsignLookup::Lookup->toggleShowLocalizerRoute();
    const char* message = visible ? "Localizer route shown" : "Localizer route hidden";
    DisplayUserMessage(PLUGIN_NAME.c_str(), "", message, true, false, false, false, false);
    return true;
}

void VFTDIPlugin::OnFunctionCall(int FunctionId, const char* sItemString, POINT Pt, RECT Area)
{
    UNREFERENCED_PARAMETER(Pt);
    if (FunctionId == FUNCTION_SEQUENCE_MOVE_ABOVE || FunctionId == FUNCTION_SEQUENCE_MOVE_BELOW) {
        if (MoveSequenceRelative(Session->ArrivalSequence, Session->PendingSequenceMoveCallsign,
            Session->PendingSequenceMoveTargetCallsign, FunctionId == FUNCTION_SEQUENCE_MOVE_BELOW))
            Session->SequenceDirty = true;
        Session->PendingSequenceMoveCallsign.clear();
        Session->PendingSequenceMoveTargetCallsign.clear();
        return;
    }
    if (FunctionId == FUNCTION_SPEED_CUSTOM) {
        const auto speedOverride = Session->FinalApproachSpeedOverridesKts.find(
            Session->PendingFinalApproachSpeedCallsign);
        const std::string initialValue = speedOverride != Session->FinalApproachSpeedOverridesKts.end()
            ? std::to_string(speedOverride->second) : std::string();
        OpenPopupEdit(Area, FUNCTION_SPEED_CUSTOM_VALUE, initialValue.c_str());
        return;
    }
    if (FunctionId == FUNCTION_SPEED_DEFAULT) {
        Session->FinalApproachSpeedOverridesKts.erase(Session->PendingFinalApproachSpeedCallsign);
        Session->SequenceDirty = true;
        Session->PendingFinalApproachSpeedCallsign.clear();
        return;
    }
    if (FunctionId == FUNCTION_SPEED_CUSTOM_VALUE) {
        if (sItemString == nullptr)
            return;
        char* end = nullptr;
        const long speedKts = std::strtol(sItemString, &end, 10);
        while (end != nullptr && std::isspace(static_cast<unsigned char>(*end)))
            ++end;
        if (end == sItemString || end == nullptr || *end != '\0' || speedKts < 30 || speedKts > 350)
            return;
        Session->FinalApproachSpeedOverridesKts[Session->PendingFinalApproachSpeedCallsign] =
            static_cast<int>(speedKts);
        Session->SequenceDirty = true;
        Session->PendingFinalApproachSpeedCallsign.clear();
        return;
    }
    if (FunctionId == FUNCTION_GAP_CUSTOM) {
        auto measurement = std::find_if(Session->WakeMeasurements.begin(), Session->WakeMeasurements.end(),
            [this](const WakeMeasurement& candidate) {
                return candidate.PrecedingCallsign == Session->PendingGapPrecedingCallsign &&
                    candidate.SucceedingCallsign == Session->PendingGapSucceedingCallsign;
            });
        const std::string initialValue = measurement != Session->WakeMeasurements.end() && measurement->GapNm > 0.0
            ? std::to_string(measurement->GapNm) : std::string();
        OpenPopupEdit(Area, FUNCTION_GAP_CUSTOM_VALUE, initialValue.c_str());
        return;
    }

    double gapNm = -1.0;
    switch (FunctionId) {
    case FUNCTION_GAP_OFF: gapNm = 0.0; break;
    case FUNCTION_GAP_3_NM: gapNm = 3.0; break;
    case FUNCTION_GAP_4_NM: gapNm = 4.0; break;
    case FUNCTION_GAP_5_NM: gapNm = 5.0; break;
    case FUNCTION_GAP_6_NM: gapNm = 6.0; break;
    case FUNCTION_GAP_CUSTOM_VALUE:
        if (sItemString == nullptr)
            return;
        {
            char* end = nullptr;
            gapNm = std::strtod(sItemString, &end);
            while (end != nullptr && std::isspace(static_cast<unsigned char>(*end)))
                ++end;
            if (end == sItemString || end == nullptr || *end != '\0' ||
                !std::isfinite(gapNm) || gapNm < 0.0 || gapNm > 20.0)
                return;
        }
        break;
    default:
        return;
    }

    auto measurement = std::find_if(Session->WakeMeasurements.begin(), Session->WakeMeasurements.end(),
        [this](const WakeMeasurement& candidate) {
            return candidate.PrecedingCallsign == Session->PendingGapPrecedingCallsign &&
                candidate.SucceedingCallsign == Session->PendingGapSucceedingCallsign;
        });
    if (measurement == Session->WakeMeasurements.end())
        return;
    measurement->GapNm = gapNm;
    Session->SequenceDirty = true;
    Session->PendingGapPrecedingCallsign.clear();
    Session->PendingGapSucceedingCallsign.clear();
}

void VFTDIPlugin::RegisterPlugin() {
    RegisterDisplayType(VFTDI_RADAR_SCREEN_VIEW, true, true, true, true);
    RegisterDisplayType(VFTDI_PANEL_SCREEN_VIEW, false, false, true, true);
}
