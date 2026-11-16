#include "stdafx.h"
#include "VFTDIPlugin.h"
#include "CallsignLookup.h"
#include "FTDiPanel.h"
#include "DisplayColors.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <sstream>

std::atomic<bool> SequenceAddRequested{ false };
std::atomic<bool> SequenceSelectionActive{ false };
std::atomic<bool> SequencePickArmed{ false };
std::atomic<bool> SequenceAircraftClickRequested{ false };
std::atomic<bool> SequenceCancelRequested{ false };
std::atomic<bool> SuggestionMenuActive{ false };
std::atomic<unsigned long long> SuggestionMenuGeneration{ 0 };
std::atomic<unsigned long long> SuggestionMenuDismissedGeneration{ 0 };

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
        bool altWWasDown = false;
        bool leftWasDown = false;
        bool rightWasDown = false;
        SuggestionMenuDismissDetector suggestionDismissal;
        unsigned long long observedSuggestionGeneration = 0;
        unsigned long long reportedDismissalGeneration = 0;
        while (!StopKeyPolling.load()) {
            DWORD foregroundProcess = 0;
            const HWND foregroundWindow = GetForegroundWindow();
            if (foregroundWindow != nullptr)
                GetWindowThreadProcessId(foregroundWindow, &foregroundProcess);

            const bool altWDown = foregroundProcess == GetCurrentProcessId() &&
                (GetAsyncKeyState(VK_MENU) & 0x8000) != 0 &&
                (GetAsyncKeyState('W') & 0x8000) != 0;
            if (altWDown && !altWWasDown)
                SequenceAddRequested.store(true);
            altWWasDown = altWDown;

            const bool isForeground = foregroundProcess == GetCurrentProcessId();
            const bool selectionActive = SequenceSelectionActive.load();
                const bool leftDown = isForeground &&
				(GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
            const bool rightDown = isForeground && selectionActive &&
                (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
            const bool suggestionActive = SuggestionMenuActive.load();
            const unsigned long long suggestionGeneration = SuggestionMenuGeneration.load();
            if (suggestionGeneration != observedSuggestionGeneration) {
                suggestionDismissal.Reset();
                observedSuggestionGeneration = suggestionGeneration;
            }
            const bool suggestionRightDown = isForeground && (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
            const bool suggestionMiddleDown = isForeground && (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
            const bool escapeDown = isForeground && (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
            if (suggestionDismissal.Update(suggestionActive, isForeground, leftDown, suggestionRightDown, suggestionMiddleDown, escapeDown) &&
                reportedDismissalGeneration != suggestionGeneration && SuggestionMenuGeneration.load() == suggestionGeneration) {
                SuggestionMenuDismissedGeneration.store(suggestionGeneration);
                reportedDismissalGeneration = suggestionGeneration;
            }
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

void VFTDIPlugin::OnGetTagItem(CFlightPlan FlightPlan, CRadarTarget RadarTarget, int ItemCode, int TagData,
    char sItemString[16], int* pColorCode, COLORREF* pRGB, double* pFontSize)
{
    UNREFERENCED_PARAMETER(TagData);
    UNREFERENCED_PARAMETER(pFontSize);
    if (ItemCode != TAG_ITEM_SEQUENCE_NUMBER && ItemCode != TAG_ITEM_SEQUENCE_SUGGESTION)
        return;
    sItemString[0] = '\0';
    *pColorCode = TAG_COLOR_RGB_DEFINED;
    *pRGB = ConfiguredColor(ItemCode == TAG_ITEM_SEQUENCE_NUMBER ? "COLOR_TAG_SEQUENCE_NUMBER" : "COLOR_TAG_SEQUENCE_SUGGESTION");
    const char* callsign = FlightPlan.IsValid() ? FlightPlan.GetCallsign()
        : (RadarTarget.IsValid() ? RadarTarget.GetCallsign() : nullptr);
    if (callsign == nullptr)
        return;
    if (ItemCode == TAG_ITEM_SEQUENCE_SUGGESTION) {
        if (SequenceProposalInvolves(Session->ArrivalSequence, Session->AutoProposalBase,
            Session->AutoProposalOrder, callsign))
            std::snprintf(sItemString, 16, "P");
        return;
    }
    const size_t number = SequenceNumber(Session->ArrivalSequence, callsign);
    if (number != 0)
        std::snprintf(sItemString, 16, "%02zu", number);
}

void VFTDIPlugin::OnFunctionCall(int FunctionId, const char* sItemString, POINT Pt, RECT Area)
{
    UNREFERENCED_PARAMETER(Pt);
    if (FunctionId == FUNCTION_SEQUENCE_SUGGESTION_TAG) {
        const CFlightPlan flightPlan = FlightPlanSelectASEL();
        const CRadarTarget radarTarget = RadarTargetSelectASEL();
        const char* callsign = flightPlan.IsValid() ? flightPlan.GetCallsign()
            : (radarTarget.IsValid() ? radarTarget.GetCallsign() : nullptr);
        if (callsign != nullptr && SequenceProposalInvolves(Session->ArrivalSequence,
            Session->AutoProposalBase, Session->AutoProposalOrder, callsign))
            OpenAutoSuggestionMenu(this, *Session, Area);
        return;
    }
    if (FunctionId == FUNCTION_AUTO_ACCEPT || FunctionId == FUNCTION_AUTO_DECLINE) {
        const bool current = IsSameLiveSequenceProposal(Session->ArrivalSequence,
            Session->AutoProposalBase, Session->AutoProposalOrder,
            Session->PendingAutoDecisionBase, Session->PendingAutoDecisionOrder);
        if (!current) {
            Session->PendingAutoDecisionBase.clear();
            Session->PendingAutoDecisionOrder.clear();
            return;
        }
        SuggestionMenuActive.store(false);
        SuggestionMenuDismissedGeneration.store(0);
        Session->AutoPreviewBase.clear();
        Session->AutoPreviewOrder.clear();
        if (Session->AutoPreviewRadarScreen != nullptr)
            Session->AutoPreviewRadarScreen->RequestRefresh();
        if (FunctionId == FUNCTION_AUTO_ACCEPT) {
            Session->AutoAcceptRequested = true;
        }
        else {
            Session->AutoRejectedBase = Session->AutoProposalBase;
            Session->AutoRejectedOrders.push_back(Session->AutoProposalOrder);
            Session->AutoProposalOrder.clear();
            Session->AutoProposalBase.clear();
            Session->AutoAcceptRequested = false;
            Session->AutoNextEvaluationSeconds = GetTickCount64() / 1000.0 + AutoSequenceDecisionCooldownSeconds;
            Session->AutoLastEvaluationBase = Session->ArrivalSequence;
            Session->AutoSequenceStatus = "Paused after declined suggestion";
            Session->PendingAutoDecisionBase.clear();
            Session->PendingAutoDecisionOrder.clear();
        }
        return;
    }
    if (FunctionId == FUNCTION_AUTO_CHANGE) {
        OpenPopupEdit(Area, FUNCTION_AUTO_AIRPORT, Session->AutoSequenceAirport.c_str());
        return;
    }
    if (FunctionId == FUNCTION_AUTO_DISABLE) {
        SuggestionMenuActive.store(false);
        Session->AutoSequenceAirport.clear();
        Session->AutoSequenceStatus.clear();
        Session->AutoAddedCallsigns.clear();
        Session->AutoProposalOrder.clear();
        Session->AutoProposalBase.clear();
        Session->PendingAutoDecisionBase.clear();
        Session->PendingAutoDecisionOrder.clear();
        Session->AutoPreviewBase.clear();
        Session->AutoPreviewOrder.clear();
        Session->AutoRejectedOrders.clear();
        Session->AutoRejectedBase.clear();
        Session->AutoAcceptRequested = false;
        return;
    }
    if (FunctionId == FUNCTION_AUTO_AIRPORT) {
        if (sItemString == nullptr)
            return;
        std::string airport(sItemString);
        const size_t first = airport.find_first_not_of(" \t\r\n");
        const size_t last = airport.find_last_not_of(" \t\r\n");
        if (first == std::string::npos)
            return;
        airport = airport.substr(first, last - first + 1);
        std::transform(airport.begin(), airport.end(), airport.begin(),
            [](unsigned char value) { return static_cast<char>(std::toupper(value)); });
        if (airport.size() != 4 || !std::all_of(airport.begin(), airport.end(),
            [](unsigned char value) { return value >= 'A' && value <= 'Z'; })) {
            DisplayUserMessage(PLUGIN_NAME.c_str(), "Sequence", "Enter a four-letter airport ICAO code", true, false, false, false, false);
            return;
        }
        Session->AutoSequenceAirport = airport;
        SuggestionMenuActive.store(false);
        Session->AutoSequenceStatus = "Scanning arrivals";
        Session->AutoNextEvaluationSeconds = 0.0;
        Session->AutoLastEvaluationBase.clear();
        Session->AutoProposalOrder.clear();
        Session->AutoProposalBase.clear();
        Session->PendingAutoDecisionBase.clear();
        Session->PendingAutoDecisionOrder.clear();
        Session->AutoPreviewBase.clear();
        Session->AutoPreviewOrder.clear();
        Session->AutoRejectedOrders.clear();
        Session->AutoRejectedBase.clear();
        Session->AutoAcceptRequested = false;
        return;
    }
    if (FunctionId == FUNCTION_SEQUENCE_MOVE_CANCEL) {
        PendingTagMoveCallsign.clear();
        Session->PendingSequenceMoveCallsign.clear();
        Session->PendingSequenceMoveTargetCallsign.clear();
        return;
    }
    if (FunctionId == FUNCTION_SEQUENCE_TAG_START_MOVE) {
        if (SequenceNumber(Session->ArrivalSequence, PendingTagMoveCallsign) != 0) {
            Session->PendingSequenceMoveCallsign = PendingTagMoveCallsign;
            Session->PendingSequenceMoveTargetCallsign.clear();
            const std::string message = "Move " + PendingTagMoveCallsign +
                ": click another sequence number or panel row to choose its position";
            DisplayUserMessage(PLUGIN_NAME.c_str(), "Sequence", message.c_str(), true, false, false, false, false);
        }
        PendingTagMoveCallsign.clear();
        return;
    }
    if (FunctionId == FUNCTION_SEQUENCE_TAG_MOVE_PICK || FunctionId == FUNCTION_SEQUENCE_TAG_TOGGLE_TARGETS) {
        const CFlightPlan flightPlan = FlightPlanSelectASEL();
        const CRadarTarget radarTarget = RadarTargetSelectASEL();
        const char* selectedCallsign = flightPlan.IsValid() ? flightPlan.GetCallsign()
            : (radarTarget.IsValid() ? radarTarget.GetCallsign() : nullptr);
        if (selectedCallsign == nullptr || SequenceNumber(Session->ArrivalSequence, selectedCallsign) == 0)
            return;
        const std::string callsign(selectedCallsign);
        if (FunctionId == FUNCTION_SEQUENCE_TAG_TOGGLE_TARGETS) {
            Session->RightClickedSequenceCallsign = Session->RightClickedSequenceCallsign == callsign
                ? std::string() : callsign;
            return;
        }
        if (Session->PendingSequenceMoveCallsign == callsign) {
            OpenPopupList(Area, callsign.c_str(), 1);
            AddPopupListElement("X", "", FUNCTION_SEQUENCE_MOVE_CANCEL);
            return;
        }
        if (SequenceNumber(Session->ArrivalSequence, Session->PendingSequenceMoveCallsign) == 0) {
            PendingTagMoveCallsign = callsign;
            OpenPopupList(Area, callsign.c_str(), 1);
            AddPopupListElement("Move", "", FUNCTION_SEQUENCE_TAG_START_MOVE);
            return;
        }
        Session->PendingSequenceMoveTargetCallsign = callsign;
        const std::string title = "POSITION " + Session->PendingSequenceMoveCallsign + " RELATIVE TO " + callsign;
        OpenPopupList(Area, title.c_str(), 1);
        AddPopupListElement("-1", "", FUNCTION_SEQUENCE_MOVE_ABOVE);
        AddPopupListElement("+1", "", FUNCTION_SEQUENCE_MOVE_BELOW);
        AddPopupListElement("X", "", FUNCTION_SEQUENCE_MOVE_CANCEL);
        return;
    }
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
    RegisterTagItemType("Sequence Number", TAG_ITEM_SEQUENCE_NUMBER);
    RegisterTagItemType("Sequence Suggestion", TAG_ITEM_SEQUENCE_SUGGESTION);
    RegisterTagItemFunction("Sequence Move/Pick", FUNCTION_SEQUENCE_TAG_MOVE_PICK);
    RegisterTagItemFunction("Cancel Sequence Move", FUNCTION_SEQUENCE_MOVE_CANCEL);
    RegisterTagItemFunction("Toggle Targets", FUNCTION_SEQUENCE_TAG_TOGGLE_TARGETS);
    RegisterTagItemFunction("Sequence Suggestion", FUNCTION_SEQUENCE_SUGGESTION_TAG);
}
