#pragma once

#include <memory>
#include "EuroScopePlugIn.h"
#include "FTDiSession.h"

class FTDiPanelScreen : public EuroScopePlugIn::CRadarScreen
{
public:
	explicit FTDiPanelScreen(std::shared_ptr<FTDiSession> session);

	void OnRefresh(HDC hDC, int Phase);
	void OnButtonDownScreenObject(int ObjectType, const char* sObjectId, POINT Pt, RECT Area, int Button) override;
	void OnAsrContentToBeClosed(void) { delete this; }

private:
	std::shared_ptr<FTDiSession> Session;
};

void DrawFTDiRadarOverlay(
	EuroScopePlugIn::CRadarScreen* screen,
	CDC& dc,
	RECT bounds,
	const std::shared_ptr<FTDiSession>& session);

bool HandleFTDiPanelButton(
	EuroScopePlugIn::CRadarScreen* screen, FTDiSession& session,
	int objectType, const char* objectId, int button, RECT area);

bool OpenAutoSuggestionMenu(EuroScopePlugIn::CPlugIn* plugIn, FTDiSession& session, RECT area);
