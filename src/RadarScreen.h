#pragma once

#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "AirportWeather.h"
#include "CallsignLookup.h"
#include "Constants.h"
#include "EuroScopePlugIn.h"
#include "FTDiSession.h"
#include "WakeShortcut.h"

using namespace EuroScopePlugIn;

class RadarScreen : public CRadarScreen
{
public:
	RadarScreen(AirportWeatherService* weatherService, std::shared_ptr<FTDiSession> session);

	void OnRefresh(HDC hDC, int Phase);
	void OnButtonDownScreenObject(int ObjectType, const char * sObjectId, POINT Pt, RECT Area, int Button) override;
	void OnClickScreenObject(int ObjectType, const char* sObjectId, POINT Pt, RECT Area, int Button) override;
	void OnMoveScreenObject(int ObjectType, const char* sObjectId, POINT Pt, RECT Area, bool Released) override;
	void OnAsrContentToBeClosed(void) { if (Session->AutoPreviewRadarScreen == this) Session->AutoPreviewRadarScreen = nullptr; SequenceSelectionActive.store(false); SequencePickArmed.store(false); delete this; }

private:
	enum class SequenceSelectionStage { Idle, AwaitAircraft };

	std::shared_ptr<FTDiSession> Session;
	AirportWeatherService* WeatherService;
	SequenceSelectionStage SelectionStage = SequenceSelectionStage::Idle;

	void StartSequenceInsertion();
	void AppendAircraftToSequence(const std::string& callsign);
	void AddAutomaticArrivals();
	void UpdateAutomaticProposal(double nowSeconds);
	void SynchronizeSequenceMeasurements();
};

