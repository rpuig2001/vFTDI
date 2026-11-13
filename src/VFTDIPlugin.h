#pragma once
#include <atomic>
#include <memory>
#include <thread>
#include "EuroScopePlugIn.h"
#include "AirportWeather.h"
#include "Constants.h"
#include "CallsignLookup.h"
#include "FTDiSession.h"
#include "WakeShortcut.h"
#include "RadarScreen.h"

using namespace std;
using namespace EuroScopePlugIn;

class VFTDIPlugin : CPlugIn
{
public:
	VFTDIPlugin();
	virtual ~VFTDIPlugin();

	CRadarScreen * OnRadarScreenCreated(const char * sDisplayName, bool NeedRadarContent, bool GeoReferenced, bool CanBeSaved, bool CanBeCreated);
	bool OnCompileCommand(const char * sCommandLine) override;
	void OnGetTagItem(CFlightPlan FlightPlan, CRadarTarget RadarTarget, int ItemCode, int TagData,
		char sItemString[16], int* pColorCode, COLORREF* pRGB, double* pFontSize) override;
	void OnFunctionCall(int FunctionId, const char* sItemString, POINT Pt, RECT Area) override;

protected:
	void RegisterPlugin();

private:
	std::string PendingTagMoveCallsign;
	std::atomic<bool> StopKeyPolling{ false };
	std::thread KeyPollingThread;
	std::unique_ptr<AirportWeatherService> WeatherService;
	std::shared_ptr<FTDiSession> Session = std::make_shared<FTDiSession>();
};