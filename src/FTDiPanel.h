#pragma once

#include <memory>
#include "EuroScopePlugIn.h"
#include "FTDiSession.h"

class SequenceFeedService;

class FTDiPanelScreen : public EuroScopePlugIn::CRadarScreen
{
public:
	FTDiPanelScreen(std::shared_ptr<FTDiSession> session, SequenceFeedService* sequenceFeed);

	void OnRefresh(HDC hDC, int Phase);
	void OnClickScreenObject(int ObjectType, const char* sObjectId, POINT Pt, RECT Area, int Button) override;
	void OnAsrContentToBeClosed(void) { delete this; }

private:
	std::shared_ptr<FTDiSession> Session;
	SequenceFeedService* SequenceFeed;
};

void DrawFTDiRadarOverlay(
	EuroScopePlugIn::CRadarScreen* screen,
	CDC& dc,
	RECT bounds,
	const std::shared_ptr<FTDiSession>& session);

bool HandleFTDiPanelButton(
	EuroScopePlugIn::CRadarScreen* screen, FTDiSession& session,
	int objectType, const char* objectId, int button, RECT area);

bool OpenAutoSuggestionMenu(EuroScopePlugIn::CPlugIn* plugIn, FTDiSession& session, RECT area,
	const std::string& callsign = {});
bool OpenAutoSuggestionDecisionMenu(EuroScopePlugIn::CPlugIn* plugIn, FTDiSession& session,
	const std::vector<std::string>& order, RECT area);
