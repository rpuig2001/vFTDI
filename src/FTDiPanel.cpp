#include "stdafx.h"
#include "FTDiPanel.h"
#include "CallsignLookup.h"
#include "DisplayColors.h"
#include "Constants.h"
#include "WakeShortcut.h"
#include <algorithm>
#include <cstdio>
#include <limits>

using namespace EuroScopePlugIn;

namespace {
constexpr int SequenceRowHeight = 38;

void DrawText(CDC& dc, int x, int y, const std::string& text, COLORREF color, int pointSize, bool bold = false)
{
	const int savedState = dc.SaveDC();
	CFont font;
	font.CreatePointFont(pointSize * 10, bold ? "Segoe UI Semibold" : "Segoe UI", &dc);
	dc.SelectObject(&font);
	dc.SetBkMode(TRANSPARENT);
	dc.SetTextColor(color);
	dc.TextOut(x, y, text.c_str());
	dc.RestoreDC(savedState);
}

void DrawFittedText(CDC& dc, const CRect& area, const std::string& text,
	COLORREF color, int pointSize, int minimumPointSize, bool bold = false)
{
	const int savedState = dc.SaveDC();
	dc.SetBkMode(TRANSPARENT);
	dc.SetTextColor(color);
	for (int size = pointSize; size >= minimumPointSize; --size) {
		CFont font;
		font.CreatePointFont(size * 10, bold ? "Segoe UI Semibold" : "Segoe UI", &dc);
		dc.SelectObject(&font);
		const CSize textSize = dc.GetTextExtent(text.c_str());
		if (textSize.cx <= area.Width()) {
			dc.TextOut(area.left, area.top + (area.Height() - textSize.cy) / 2, text.c_str());
			dc.RestoreDC(savedState);
			return;
		}
		dc.SelectStockObject(DEFAULT_GUI_FONT);
	}
	CFont font;
	font.CreatePointFont(minimumPointSize * 10, bold ? "Segoe UI Semibold" : "Segoe UI", &dc);
	dc.SelectObject(&font);
	CRect drawArea = area;
	dc.DrawText(text.c_str(), &drawArea, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
	dc.RestoreDC(savedState);
}

std::string ActiveAircraftCallsign(CRadarScreen* screen)
{
	if (screen == nullptr)
		return {};
	const CRadarTarget selected = screen->GetPlugIn()->RadarTargetSelectASEL();
	return selected.IsValid() ? selected.GetCallsign() : std::string();
}

void DrawButton(CDC& dc, const CRect& rect, const std::string& label, COLORREF fillColor, COLORREF textColor)
{
	const int savedState = dc.SaveDC();
	CBrush fillBrush(fillColor);
	CPen outlinePen(PS_SOLID, 1, ConfiguredColor("COLOR_PANEL_DIVIDER"));
	dc.SelectObject(&fillBrush);
	dc.SelectObject(&outlinePen);
	dc.Rectangle(rect);
	CFont font;
	font.CreatePointFont(80, "Segoe UI Semibold", &dc);
	dc.SelectObject(&font);
	dc.SetBkMode(TRANSPARENT);
	dc.SetTextColor(textColor);
	const CSize textSize = dc.GetTextExtent(label.c_str());
	dc.TextOut(rect.left + (rect.Width() - textSize.cx) / 2,
		rect.top + (rect.Height() - textSize.cy) / 2, label.c_str());
	dc.RestoreDC(savedState);
}

void DrawStatusBadge(CDC& dc, const CRect& area, const std::string& status)
{
	const bool turnNow = status.compare(0, 8, "TURN NOW") == 0;
	const bool turnSoon = status.compare(0, 9, "TURN SOON") == 0;
	const bool updating = status == "UPDATING";
	const bool countdown = status.compare(0, 3, "IN ") == 0;
	const bool noDelay = status == "NO DELAY";
	const bool cleared = status == "CLEARED";
	const bool leader = status == "LEADER";
	const char* fillKey = turnNow ? "COLOR_BREACH_ALERT"
		: (turnSoon ? "COLOR_GAP_TARGET"
			: (updating ? "COLOR_TURN_PASSED"
				: (countdown ? "COLOR_TURN_POINT"
					: (noDelay ? "COLOR_ITD_TARGET"
						: (cleared ? "COLOR_PANEL_DISABLED"
							: (leader ? "COLOR_PANEL_SURFACE" : "COLOR_PANEL_STATUS_WARNING"))))));
	const char* textKey = turnNow || leader || (!turnSoon && !updating && !countdown && !noDelay && !cleared)
		? "COLOR_PANEL_HEADER_TEXT" : "COLOR_PANEL_TEXT";
	const int savedState = dc.SaveDC();
	CFont font;
	font.CreatePointFont(80, "Segoe UI Semibold", &dc);
	dc.SelectObject(&font);
	const CSize textSize = dc.GetTextExtent(status.c_str());
	const int badgeWidth = (std::min)(static_cast<int>(area.Width()), static_cast<int>(textSize.cx) + 8);
	const CRect badge(area.left, area.top, area.left + badgeWidth, area.bottom);
	dc.FillSolidRect(badge, ConfiguredColor(fillKey));
	dc.SetBkMode(TRANSPARENT);
	dc.SetTextColor(ConfiguredColor(textKey));
	dc.TextOut(badge.left + 4, badge.top + (badge.Height() - textSize.cy) / 2, status.c_str());
	dc.RestoreDC(savedState);
}

void DrawTurnToggle(CRadarScreen* screen, CDC& dc, const CRect& rect, bool enabled)
{
	const int savedState = dc.SaveDC();
	dc.FillSolidRect(rect, ConfiguredColor("COLOR_PANEL_SURFACE"));
	CPen pen(PS_SOLID, 1, ConfiguredColor("COLOR_PANEL_HEADER_TEXT"));
	dc.SelectObject(&pen);
	dc.SelectStockObject(NULL_BRUSH);
	const int boxTop = rect.top + (rect.Height() - 10) / 2;
	dc.Rectangle(rect.left + 5, boxTop, rect.left + 15, boxTop + 10);
	if (enabled) {
		dc.MoveTo(rect.left + 7, boxTop + 5);
		dc.LineTo(rect.left + 10, boxTop + 8);
		dc.LineTo(rect.left + 14, boxTop + 2);
	}
	DrawText(dc, rect.left + 20, rect.top + (rect.Height() - 14) / 2,
		"TURN", ConfiguredColor("COLOR_PANEL_HEADER_TEXT"), 8, true);
	dc.RestoreDC(savedState);
	screen->AddScreenObject(SCREEN_PANEL_TURNS, "TURNS", rect, false,
		"Toggle recommendations for when to turn and capture final at LOCALIZERPOINT");
}

void DrawProfileControl(CRadarScreen* screen, CDC& dc, const CRect& rect,
	const std::vector<std::string>& profiles, const std::string& activeProfile)
{
	if (profiles.empty())
		return;
	const int profileCount = static_cast<int>(profiles.size());
	const int segmentWidth = rect.Width() / profileCount;
	for (int index = 0; index < profileCount; ++index) {
		const int left = rect.left + index * segmentWidth;
		const int right = index + 1 == profileCount ? rect.right : left + segmentWidth;
		const CRect segment(left + 1, rect.top, right - 1, rect.bottom);
		const bool active = profiles[index] == activeProfile;
		DrawButton(dc, segment, profiles[index],
			ConfiguredColor(active ? "COLOR_PANEL_ACCENT" : "COLOR_PANEL_SURFACE"),
			ConfiguredColor(active ? "COLOR_PANEL_TEXT" : "COLOR_PANEL_HEADER_TEXT"));
		const std::string tooltip = "Use runway-pair profile " + profiles[index];
		screen->AddScreenObject(SCREEN_PANEL_MODE, profiles[index].c_str(), segment, false, tooltip.c_str());
	}
}

std::string PairObjectId(const char* action, const WakeMeasurement& measurement)
{
	return std::string(action) + "|" + measurement.PrecedingCallsign + "|" + measurement.SucceedingCallsign;
}

void DrawSequenceRow(CRadarScreen* screen, CDC& dc, const CRect& row,
	const FTDiSession& session, int index, const std::string& aselCallsign)
{
	const std::string& callsign = session.ArrivalSequence[index];
	const bool isAsel = callsign == aselCallsign;
	const bool isRightClicked = callsign == session.RightClickedSequenceCallsign;
	const WakeMeasurement* measurement = nullptr;
	if (index > 0) {
		const std::string& precedingCallsign = session.ArrivalSequence[index - 1];
		const auto pair = std::find_if(session.WakeMeasurements.begin(), session.WakeMeasurements.end(),
				[&precedingCallsign, &callsign](const WakeMeasurement& candidate) {
				return candidate.PrecedingCallsign == precedingCallsign &&
					candidate.SucceedingCallsign == callsign;
			});
		if (pair != session.WakeMeasurements.end())
			measurement = &*pair;
	}
	dc.FillSolidRect(row, ConfiguredColor(isAsel ? "COLOR_GAP_TARGET" : "COLOR_PANEL_ROW"));
	dc.FillSolidRect(row.left, row.top, 2, row.Height(),
		ConfiguredColor(measurement != nullptr && measurement->ParallelIndependent
			? "COLOR_PANEL_DISABLED" : (measurement != nullptr && measurement->ParallelDependent
				? "COLOR_PANEL_WARNING" : "COLOR_PANEL_ACCENT")));
	if (measurement != nullptr)
		screen->AddScreenObject(SCREEN_PAIR_MODE, callsign.c_str(),
			CRect(row.left, row.top, row.left + 5, row.bottom), false,
			measurement->ParallelIndependent ? "Configured runway pair: INDEPENDENT; no inter-runway spacing target"
				: (measurement->ParallelDependent ? "Configured runway pair: DEPENDANT spacing" : "Alternating spacing"));
	screen->AddScreenObject(SCREEN_SEQUENCE_SELECT, callsign.c_str(), row, false,
		"");

	const std::string sequenceTitle = (index + 1 < 10 ? "0" : "") +
		std::to_string(index + 1) + "  " + callsign;
	const int controlsLeft = row.right - 102;
	const CRect callsignArea(row.left + 7, row.top + 2,
		controlsLeft - 3, row.top + 19);
	DrawFittedText(dc, callsignArea, sequenceTitle,
		ConfiguredColor(isRightClicked ? "COLOR_PANEL_STATUS_WARNING" : "COLOR_PANEL_TEXT"), 9, 7, true);

	const CRect moveButton(row.right - 64, row.top + 2, row.right - 22, row.top + 19);
	const CRect removeButton(row.right - 20, row.top + 2, row.right - 2, row.top + 19);
	const bool canMove = session.ArrivalSequence.size() > 1;
	const bool isMoveSource = session.PendingSequenceMoveCallsign == callsign;
	DrawButton(dc, moveButton, isMoveSource ? "PICK" : "MOVE",
		ConfiguredColor(!canMove ? "COLOR_PANEL_DISABLED" : (isMoveSource ? "COLOR_PANEL_WARNING" : "COLOR_PANEL_SURFACE")),
		ConfiguredColor(!canMove ? "COLOR_PANEL_MUTED_TEXT" : "COLOR_PANEL_HEADER_TEXT"));
	DrawButton(dc, removeButton, "x", ConfiguredColor("COLOR_PANEL_REMOVE_BACKGROUND"),
		ConfiguredColor("COLOR_PANEL_REMOVE_TEXT"));
	const std::string removeId = "SEQREMOVE|" + callsign;
	screen->AddScreenObject(SCREEN_SEQUENCE_MOVE, callsign.c_str(), moveButton, false,
		"Move this aircraft directly to a chosen position in the sequence");
	screen->AddScreenObject(SCREEN_PANEL_REMOVE, removeId.c_str(), removeButton, false,
		"Remove aircraft from the sequence");
	const CRect speedButton(row.right - 86, row.top + 21, row.right - 42, row.top + 36);
	const auto speedOverride = session.FinalApproachSpeedOverridesKts.find(callsign);
	char speedLabel[12] = {};
	if (speedOverride == session.FinalApproachSpeedOverridesKts.end())
		std::snprintf(speedLabel, sizeof(speedLabel), "FAS");
	else
		std::snprintf(speedLabel, sizeof(speedLabel), "%d", speedOverride->second);
	DrawButton(dc, speedButton, speedLabel,
		ConfiguredColor(speedOverride == session.FinalApproachSpeedOverridesKts.end()
			? "COLOR_PANEL_SURFACE" : "COLOR_PANEL_ACCENT"),
		ConfiguredColor(speedOverride == session.FinalApproachSpeedOverridesKts.end()
			? "COLOR_PANEL_HEADER_TEXT" : "COLOR_PANEL_TEXT"));
	const std::string speedTooltip = speedOverride == session.FinalApproachSpeedOverridesKts.end()
		? "Set final approach speed override in knots"
		: "Final approach speed override: " + std::to_string(speedOverride->second) + " kt";
	screen->AddScreenObject(SCREEN_PANEL_SPEED, callsign.c_str(), speedButton, false, speedTooltip.c_str());
	if (session.ShowTurningPoints || (measurement != nullptr && !measurement->HasCurrentSpacing)) {
		const std::string status = index == 0 ? "LEADER"
			: (measurement == nullptr || measurement->TurnStatus.empty() ? "NO PAIR" : measurement->TurnStatus);
		const std::string detail = index == 0 ? "Turn recommendations are calculated for followers, not the sequence leader"
			: (measurement == nullptr || measurement->TurnStatus.empty()
				? "No turn prediction is available for this aircraft pair yet" : measurement->TurnStatusDetail);
		const CRect statusArea(row.left + 7, row.top + 21, controlsLeft - 3, row.top + 36);
		const int statusState = dc.SaveDC();
		dc.IntersectClipRect(statusArea);
		const bool turnNow = status.compare(0, 8, "TURN NOW") == 0;
		if (!turnNow)
			DrawStatusBadge(dc, statusArea, status);
		dc.RestoreDC(statusState);
		if (!turnNow)
			screen->AddScreenObject(SCREEN_TURN_STATUS, callsign.c_str(), statusArea, false, detail.c_str());
	}
	if (measurement != nullptr && !measurement->ParallelIndependent) {
		const CRect gapButton(row.right - 40, row.top + 21, row.right - 2, row.top + 36);
		DrawButton(dc, gapButton, "GAP",
			ConfiguredColor(measurement->GapNm > 0.0 ? "COLOR_PANEL_WARNING" : "COLOR_PANEL_DISABLED"),
			ConfiguredColor(measurement->GapNm > 0.0 ? "COLOR_PANEL_TEXT" : "COLOR_PANEL_MUTED_TEXT"));
		const std::string gapId = PairObjectId("GAP", *measurement);
		const std::string gapTooltip = measurement->GapNm > 0.0
			? std::string("GAP ") + std::to_string(measurement->GapNm) + " NM"
			: "Set a distance-based GAP for this aircraft pair";
		screen->AddScreenObject(SCREEN_PANEL_GAP, gapId.c_str(), gapButton, false,
			gapTooltip.c_str());
	}
}
}

bool HandleFTDiPanelButton(
	CRadarScreen* screen, FTDiSession& session,
	int objectType, const char* objectId, int button, RECT area)
{
	if (objectId == nullptr)
		return false;
	std::string rowCallsign;
	if (objectType == SCREEN_SEQUENCE_SELECT || objectType == SCREEN_SEQUENCE_MOVE ||
		objectType == SCREEN_PANEL_SPEED || objectType == SCREEN_TURN_STATUS || objectType == SCREEN_PAIR_MODE)
		rowCallsign = objectId;
	else if (objectType == SCREEN_PANEL_REMOVE) {
		const std::string id(objectId);
		const std::string prefix = "SEQREMOVE|";
		if (id.compare(0, prefix.size(), prefix) == 0)
			rowCallsign = id.substr(prefix.size());
	}
	else if (objectType == SCREEN_PANEL_GAP) {
		const std::string id(objectId);
		const size_t firstSeparator = id.find('|');
		const size_t secondSeparator = firstSeparator == std::string::npos
			? std::string::npos : id.find('|', firstSeparator + 1);
		if (secondSeparator != std::string::npos && secondSeparator + 1 < id.size())
			rowCallsign = id.substr(secondSeparator + 1);
	}
	if (!rowCallsign.empty() && std::find(session.ArrivalSequence.begin(), session.ArrivalSequence.end(), rowCallsign) !=
		session.ArrivalSequence.end()) {
		if (button == BUTTON_LEFT && objectType == SCREEN_SEQUENCE_SELECT) {
			session.SelectedSequenceCallsign = rowCallsign;
			session.RightClickedSequenceCallsign.clear();
			if (screen != nullptr) {
				CPlugIn* plugIn = screen->GetPlugIn();
				const CFlightPlan flightPlan = plugIn->FlightPlanSelect(rowCallsign.c_str());
				if (flightPlan.IsValid())
					plugIn->SetASELAircraft(flightPlan);
				else {
					const CRadarTarget target = plugIn->RadarTargetSelect(rowCallsign.c_str());
					if (target.IsValid())
						plugIn->SetASELAircraft(target);
				}
			}
			if (!session.PendingSequenceMoveCallsign.empty()) {
				if (rowCallsign == session.PendingSequenceMoveCallsign) {
					session.PendingSequenceMoveCallsign.clear();
					return true;
				}
				session.PendingSequenceMoveTargetCallsign = rowCallsign;
				if (screen != nullptr) {
					CPlugIn* plugIn = screen->GetPlugIn();
					const std::string title = "POSITION " + session.PendingSequenceMoveCallsign +
						" RELATIVE TO " + rowCallsign;
					plugIn->OpenPopupList(area, title.c_str(), 1);
					const std::string above = "Above " + rowCallsign;
					const std::string below = "Below " + rowCallsign;
					plugIn->AddPopupListElement(above.c_str(), "", FUNCTION_SEQUENCE_MOVE_ABOVE);
					plugIn->AddPopupListElement(below.c_str(), "", FUNCTION_SEQUENCE_MOVE_BELOW);
				}
				return true;
			}
			return true;
		}
		if (button == BUTTON_RIGHT) {
			if (session.RightClickedSequenceCallsign == rowCallsign) {
				session.RightClickedSequenceCallsign.clear();
			}
			else {
				session.RightClickedSequenceCallsign = rowCallsign;
			}
			return true;
		}
		if (button == BUTTON_MIDDLE) {
			session.RoutePreviewCallsign = session.RoutePreviewCallsign == rowCallsign
				? std::string() : rowCallsign;
			return true;
		}
	}
	if (objectType == SCREEN_SEQUENCE_SELECT) {
		return false;
	}
	if (button != BUTTON_LEFT)
		return false;
	if (objectType == SCREEN_SEQUENCE_MOVE) {
		const auto aircraft = std::find(session.ArrivalSequence.begin(), session.ArrivalSequence.end(), objectId);
		if (aircraft == session.ArrivalSequence.end() || session.ArrivalSequence.size() < 2)
			return true;
		session.PendingSequenceMoveCallsign = session.PendingSequenceMoveCallsign == objectId
			? std::string() : objectId;
		session.PendingSequenceMoveTargetCallsign.clear();
		return true;
	}
	if (objectType == SCREEN_PANEL_GAP) {
		if (screen == nullptr)
			return true;
		const std::string id(objectId);
		const std::string prefix = "GAP|";
		if (id.compare(0, prefix.size(), prefix) != 0)
			return true;
		const size_t separator = id.find('|', prefix.size());
		if (separator == std::string::npos)
			return true;
		session.PendingGapPrecedingCallsign = id.substr(prefix.size(), separator - prefix.size());
		session.PendingGapSucceedingCallsign = id.substr(separator + 1);
		const auto measurement = std::find_if(session.WakeMeasurements.begin(), session.WakeMeasurements.end(),
			[&session](const WakeMeasurement& candidate) {
				return candidate.PrecedingCallsign == session.PendingGapPrecedingCallsign &&
					candidate.SucceedingCallsign == session.PendingGapSucceedingCallsign;
			});
		if (measurement == session.WakeMeasurements.end())
			return true;
		CPlugIn* plugIn = screen->GetPlugIn();
		plugIn->OpenPopupList(area, "GAP DISTANCE (NM)", 1);
		plugIn->AddPopupListElement("No GAP", "", FUNCTION_GAP_OFF, measurement->GapNm <= 0.0);
		plugIn->AddPopupListElement("3.0 NM", "", FUNCTION_GAP_3_NM, measurement->GapNm == 3.0);
		plugIn->AddPopupListElement("4.0 NM", "", FUNCTION_GAP_4_NM, measurement->GapNm == 4.0);
		plugIn->AddPopupListElement("5.0 NM", "", FUNCTION_GAP_5_NM, measurement->GapNm == 5.0);
		plugIn->AddPopupListElement("6.0 NM", "", FUNCTION_GAP_6_NM, measurement->GapNm == 6.0);
		plugIn->AddPopupListElement("Custom value...", "", FUNCTION_GAP_CUSTOM);
		return true;
	}
	if (objectType == SCREEN_PANEL_SPEED) {
		if (screen == nullptr)
			return true;
		session.PendingFinalApproachSpeedCallsign = objectId;
		const bool hasOverride = session.FinalApproachSpeedOverridesKts.find(objectId) !=
			session.FinalApproachSpeedOverridesKts.end();
		CPlugIn* plugIn = screen->GetPlugIn();
		plugIn->OpenPopupList(area, "FINAL APPROACH SPEED (KTS)", 1);
		if (hasOverride)
			plugIn->AddPopupListElement("Use aircraft type default", "", FUNCTION_SPEED_DEFAULT);
		plugIn->AddPopupListElement("Set custom speed...", "", FUNCTION_SPEED_CUSTOM);
		return true;
	}
	if (objectType == SCREEN_PANEL_ADD) {
		SequenceAddRequested.store(true);
		return true;
	}
	if (objectType == SCREEN_PANEL_ROUTE) {
		if (CCallsignLookup::Available && CCallsignLookup::Lookup != nullptr)
			CCallsignLookup::Lookup->toggleShowLocalizerRoute();
		return true;
	}
	if (objectType == SCREEN_PANEL_TURNS) {
		session.ShowTurningPoints = !session.ShowTurningPoints;
		return true;
	}
	if (objectType == SCREEN_PANEL_MODE) {
		const std::string profile(objectId);
		if (!CCallsignLookup::Available || CCallsignLookup::Lookup == nullptr)
			return false;
		const auto profiles = CCallsignLookup::Lookup->getProfileNames();
		if (std::find(profiles.begin(), profiles.end(), profile) == profiles.end())
			return false;
		if (session.ActiveProfile != profile) {
			session.ActiveProfile = profile;
			for (auto& measurement : session.WakeMeasurements)
				measurement.PlanningTurnTiming.Reset();
			session.SequenceDirty = true;
		}
		return true;
	}
	if (objectType == SCREEN_MANAGER_ROWS_DOWN) {
		if (session.ManagerVisibleRows > 1)
			--session.ManagerVisibleRows;
		return true;
	}
	if (objectType == SCREEN_MANAGER_ROWS_UP) {
		if (session.ManagerVisibleRows < (std::numeric_limits<int>::max)())
			++session.ManagerVisibleRows;
		return true;
	}
	if (objectType == SCREEN_SEQUENCE_SCROLL_UP) {
		if (session.SequenceScrollOffset > 0)
			--session.SequenceScrollOffset;
		return true;
	}
	if (objectType == SCREEN_SEQUENCE_SCROLL_DOWN) {
		++session.SequenceScrollOffset;
		return true;
	}
	if (objectType == SCREEN_PANEL_REMOVE) {
		const std::string prefix = "SEQREMOVE|";
		const std::string id(objectId);
		if (id.compare(0, prefix.size(), prefix) != 0)
			return false;
		const std::string callsign = id.substr(prefix.size());
		const auto aircraft = std::find(session.ArrivalSequence.begin(), session.ArrivalSequence.end(), callsign);
		if (aircraft == session.ArrivalSequence.end())
			return false;
		session.ArrivalSequence.erase(aircraft);
		if (session.SelectedSequenceCallsign == callsign) {
			session.SelectedSequenceCallsign.clear();
			session.RightClickedSequenceCallsign.clear();
		}
		if (session.RightClickedSequenceCallsign == callsign)
			session.RightClickedSequenceCallsign.clear();
		if (session.RoutePreviewCallsign == callsign)
			session.RoutePreviewCallsign.clear();
		if (session.PendingSequenceMoveCallsign == callsign)
			session.PendingSequenceMoveCallsign.clear();
		if (session.PendingSequenceMoveTargetCallsign == callsign)
			session.PendingSequenceMoveTargetCallsign.clear();
		session.SequenceDirty = true;
		return true;
	}
	return false;
}

void DrawFTDiRadarOverlay(
	CRadarScreen* screen, CDC& dc, RECT bounds, const std::shared_ptr<FTDiSession>& session)
{
	if (screen == nullptr || session == nullptr)
		return;

	const int availableWidth = bounds.right - bounds.left - 12;
	const int panelWidth = availableWidth < 228 ? availableWidth : 228;
	const int managerWidth = panelWidth;
	const int screenHeight = bounds.bottom - bounds.top;
	const int rowCapacity = screenHeight > 102 ? (screenHeight - 102) / SequenceRowHeight : 1;
	const int rowsLimit = session->ManagerVisibleRows < rowCapacity
		? session->ManagerVisibleRows : rowCapacity;
	const int count = static_cast<int>(session->ArrivalSequence.size());
	const int maximumScrollOffset = count > rowsLimit ? count - rowsLimit : 0;
	if (session->SequenceScrollOffset > maximumScrollOffset)
		session->SequenceScrollOffset = maximumScrollOffset;
	int visibleRows = count - session->SequenceScrollOffset;
	if (visibleRows < 0)
		visibleRows = 0;
	if (visibleRows > rowsLimit)
		visibleRows = rowsLimit;
	const int panelHeight = 68 + (visibleRows > 0 ? visibleRows : 1) * SequenceRowHeight;
	const int managerHeight = session->ManagerExpanded ? 26 + panelHeight : 24;
	if (!session->HasManagerPosition) {
		session->ManagerPosition = {
			bounds.left + ((bounds.right - bounds.left) - managerWidth) / 2,
			bounds.top + ((bounds.bottom - bounds.top) - managerHeight) / 2 };
		session->HasManagerPosition = true;
	}
	session->RadarBounds = bounds;
	session->ManagerWindowWidth = managerWidth;
	session->ManagerWindowHeight = managerHeight;
	const int leftMargin = bounds.left + 4;
	const int topMargin = bounds.top + 4;
	const int maximumLeft = bounds.right - managerWidth - 4;
	int maximumTop = bounds.bottom - managerHeight - 4;
	if (maximumTop < topMargin)
		maximumTop = topMargin;
	if (session->ManagerPosition.x < leftMargin)
		session->ManagerPosition.x = leftMargin;
	else if (maximumLeft >= leftMargin && session->ManagerPosition.x > maximumLeft)
		session->ManagerPosition.x = maximumLeft;
	if (session->ManagerPosition.y < topMargin)
		session->ManagerPosition.y = topMargin;
	else if (session->ManagerPosition.y > maximumTop)
		session->ManagerPosition.y = maximumTop;
	const int left = session->ManagerPosition.x;
	const int top = session->ManagerPosition.y;
	const CRect launcher(left, top, left + managerWidth, top + 24);
	dc.FillSolidRect(launcher, ConfiguredColor("COLOR_PANEL_SURFACE"));
	dc.FillSolidRect(launcher.left, launcher.top, 3, launcher.Height(), ConfiguredColor("COLOR_PANEL_WARNING"));
	DrawText(dc, launcher.left + 7, launcher.top + 4,
		"vFTDI   " + std::to_string(session->ArrivalSequence.size()), ConfiguredColor("COLOR_PANEL_HEADER_TEXT"), 8, true);
	const CRect toggleButton(launcher.right - 25, launcher.top + 3, launcher.right - 4, launcher.bottom - 3);
	DrawButton(dc, toggleButton, session->ManagerExpanded ? "-" : "+",
		ConfiguredColor("COLOR_PANEL_CONTROL"), ConfiguredColor("COLOR_PANEL_HEADER_TEXT"));
	const CRect moveHandle(launcher.left + 4, launcher.top,
		session->ManagerExpanded ? launcher.right - 77 : toggleButton.left - 4, launcher.bottom);
	screen->AddScreenObject(SCREEN_PANEL_MOVE, "MOVE", moveHandle, true,
		"Drag the vFTDI manager by this title area");
	screen->AddScreenObject(SCREEN_PANEL_TOGGLE, "TOGGLE", toggleButton, false,
		session->ManagerExpanded ? "Close the vFTDI arrival sequence" : "Open the vFTDI arrival sequence");
	if (!session->ManagerExpanded)
		return;
	const std::string aselCallsign = ActiveAircraftCallsign(screen);
	const CRect rowsDown(launcher.right - 73, launcher.top + 3, launcher.right - 52, launcher.bottom - 3);
	const CRect rowsUp(launcher.right - 49, launcher.top + 3, launcher.right - 28, launcher.bottom - 3);
	DrawButton(dc, rowsDown, "-", ConfiguredColor("COLOR_PANEL_CONTROL"), ConfiguredColor("COLOR_PANEL_HEADER_TEXT"));
	DrawButton(dc, rowsUp, "+", ConfiguredColor("COLOR_PANEL_CONTROL"), ConfiguredColor("COLOR_PANEL_HEADER_TEXT"));
	screen->AddScreenObject(SCREEN_MANAGER_ROWS_DOWN, "ROWS_DOWN", rowsDown, false, "Show fewer arrivals");
	screen->AddScreenObject(SCREEN_MANAGER_ROWS_UP, "ROWS_UP", rowsUp, false, "Show more arrivals");

	const int panelLeft = left;
	const int panelTop = launcher.bottom + 2;
	const CRect panel(panelLeft, panelTop, panelLeft + panelWidth, panelTop + panelHeight);
	dc.FillSolidRect(panel, ConfiguredColor("COLOR_PANEL_BACKGROUND"));
	CPen outline(PS_SOLID, 1, ConfiguredColor("COLOR_PANEL_DIVIDER"));
	CBrush panelBrush(ConfiguredColor("COLOR_PANEL_BACKGROUND"));
	const int savedState = dc.SaveDC();
	dc.SelectObject(&outline);
	dc.SelectObject(&panelBrush);
	dc.Rectangle(panel);
	dc.RestoreDC(savedState);

	const int actionWidth = (panel.Width() - 24) / 3;
	const CRect addButton(panel.left + 7, panel.top + 5,
		panel.left + 7 + actionWidth, panel.top + 25);
	const CRect routeButton(addButton.right + 5, panel.top + 5,
		addButton.right + 5 + actionWidth, panel.top + 25);
	const CRect turnButton(routeButton.right + 5, panel.top + 5,
		panel.right - 7, panel.top + 25);
	const bool routeVisible = !CCallsignLookup::Available || CCallsignLookup::Lookup == nullptr ||
		CCallsignLookup::Lookup->getShowLocalizerRoute();
	const bool selectingAircraft = SequenceSelectionActive.load();
	DrawButton(dc, addButton, selectingAircraft ? "PICK ACFT" : "+ ACFT",
		ConfiguredColor("COLOR_PANEL_ACCENT"), ConfiguredColor("COLOR_PANEL_TEXT"));
	DrawButton(dc, routeButton, routeVisible ? "RTE ON" : "RTE OFF",
		ConfiguredColor("COLOR_PANEL_SURFACE"), ConfiguredColor("COLOR_PANEL_HEADER_TEXT"));
	screen->AddScreenObject(SCREEN_PANEL_ADD, "ADD", addButton, false,
		selectingAircraft ? "Pick one aircraft to append; right-click to cancel"
			: "Start aircraft selection before clicking an aircraft to append");
	screen->AddScreenObject(SCREEN_PANEL_ROUTE, "ROUTE", routeButton, false, "Toggle the localizer route overlay");
	DrawTurnToggle(screen, dc, turnButton, session->ShowTurningPoints);
	const auto profiles = CCallsignLookup::Available && CCallsignLookup::Lookup != nullptr
		? CCallsignLookup::Lookup->getProfileNames() : std::vector<std::string>{ "ALT", "DEP" };
	DrawProfileControl(screen, dc, CRect(panel.left + 7, panel.top + 31,
		panel.right - 7, panel.top + 53), profiles, session->ActiveProfile);

	const int rowTop = panel.top + 62;
	const int rowViewportRows = visibleRows > 0 ? visibleRows : 1;
	const int rowViewportHeight = rowViewportRows * SequenceRowHeight - 1;
	const CRect scrollTrack(panel.right - 11, rowTop, panel.right - 4, rowTop + rowViewportHeight);
	if (count > rowsLimit) {
		const int thumbHeight = rowViewportHeight * rowsLimit / count;
		const int thumbTravel = rowViewportHeight - thumbHeight;
		const int thumbTop = scrollTrack.top + (maximumScrollOffset > 0
			? thumbTravel * session->SequenceScrollOffset / maximumScrollOffset : 0);
		dc.FillSolidRect(scrollTrack, ConfiguredColor("COLOR_PANEL_DIVIDER"));
		dc.FillSolidRect(scrollTrack.left, thumbTop, scrollTrack.Width(), thumbHeight, ConfiguredColor("COLOR_PANEL_WARNING"));
		screen->AddScreenObject(SCREEN_SEQUENCE_SCROLL_UP, "SCROLL_UP",
			CRect(scrollTrack.left - 4, scrollTrack.top, panel.right - 2,
				scrollTrack.top + rowViewportHeight / 2), false, "Scroll arrivals up");
		screen->AddScreenObject(SCREEN_SEQUENCE_SCROLL_DOWN, "SCROLL_DOWN",
			CRect(scrollTrack.left - 4, scrollTrack.top + rowViewportHeight / 2,
				panel.right - 2, scrollTrack.bottom), false, "Scroll arrivals down");
	}
	for (int index = 0; index < visibleRows; ++index) {
		const int thisRowTop = rowTop + index * SequenceRowHeight;
		const CRect row(panel.left + 5, thisRowTop, panel.right - 13, thisRowTop + SequenceRowHeight - 1);
		DrawSequenceRow(screen, dc, row, *session, session->SequenceScrollOffset + index, aselCallsign);
	}
	if (count == 0)
		DrawText(dc, panel.left + 10, rowTop + 12, "No arrivals queued", ConfiguredColor("COLOR_PANEL_MUTED_TEXT"), 8);
}

FTDiPanelScreen::FTDiPanelScreen(std::shared_ptr<FTDiSession> session)
	: Session(session)
{
}

void FTDiPanelScreen::OnRefresh(HDC hDC, int Phase)
{
	if (Phase != REFRESH_PHASE_BEFORE_TAGS)
		return;

	CDC dc;
	dc.Attach(hDC);
	RECT bounds = {};
	dc.GetClipBox(&bounds);
	dc.FillSolidRect(&bounds, ConfiguredColor("COLOR_PANEL_BACKGROUND"));
	dc.FillSolidRect(bounds.left, bounds.top, 4, bounds.bottom - bounds.top, ConfiguredColor("COLOR_PANEL_ACCENT"));

	DrawText(dc, bounds.left + 10, bounds.top + 6, "vFTDI  /  ARRIVALS", ConfiguredColor("COLOR_PANEL_TEXT"), 10, true);
	DrawText(dc, bounds.left + 17, bounds.top + 32,
		"FINAL APPROACH SEPARATION", ConfiguredColor("COLOR_PANEL_MUTED_TEXT"), 7);
	const bool selectingAircraft = SequenceSelectionActive.load();
	DrawText(dc, bounds.right - 102, bounds.top + 23,
		selectingAircraft ? "PICK ACFT" : "READY",
		ConfiguredColor(selectingAircraft ? "COLOR_PANEL_WARNING" : "COLOR_PANEL_ACCENT"), 8, true);

	const int buttonTop = bounds.top + 48;
	const int buttonHeight = 18;
	const int actionWidth = (bounds.right - bounds.left - 56) / 3;
	const CRect addButton(bounds.left + 20, buttonTop,
		bounds.left + 20 + actionWidth, buttonTop + buttonHeight);
	const bool routeVisible = !CCallsignLookup::Available || CCallsignLookup::Lookup == nullptr ||
		CCallsignLookup::Lookup->getShowLocalizerRoute();
	const CRect routeButton(addButton.right + 10, buttonTop,
		addButton.right + 10 + actionWidth, buttonTop + buttonHeight);
	const CRect turnButton(routeButton.right + 10, buttonTop,
		bounds.right - 16, buttonTop + buttonHeight);
	DrawButton(dc, addButton, "+ ACFT", ConfiguredColor("COLOR_PANEL_ACCENT"), ConfiguredColor("COLOR_PANEL_TEXT"));
	DrawButton(dc, routeButton, routeVisible ? "RTE ON" : "RTE OFF",
		ConfiguredColor("COLOR_PANEL_SURFACE"), ConfiguredColor("COLOR_PANEL_HEADER_TEXT"));
	AddScreenObject(SCREEN_PANEL_ADD, "ADD", addButton, false,
		"Select one aircraft to append it to the arrival sequence");
	AddScreenObject(SCREEN_PANEL_ROUTE, "ROUTE", routeButton, false, "Toggle the localizer route overlay");
	DrawTurnToggle(this, dc, turnButton, Session->ShowTurningPoints);
	const CRect modeControl(bounds.left + 20, buttonTop + buttonHeight + 4,
		bounds.right - 16, buttonTop + buttonHeight + 22);
	const auto profiles = CCallsignLookup::Available && CCallsignLookup::Lookup != nullptr
		? CCallsignLookup::Lookup->getProfileNames() : std::vector<std::string>{ "ALT", "DEP" };
	DrawProfileControl(this, dc, modeControl, profiles, Session->ActiveProfile);
	const std::string aselCallsign = ActiveAircraftCallsign(this);

	const int listTop = modeControl.bottom + 7;
	DrawText(dc, bounds.left + 16, listTop,
		"SEQUENCE  " + std::to_string(Session->ArrivalSequence.size()), ConfiguredColor("COLOR_PANEL_MUTED_TEXT"), 8, true);
	const int rowTop = listTop + 15;
	const int rowHeight = SequenceRowHeight;
	const int count = static_cast<int>(Session->ArrivalSequence.size());
	int maximumRows = (bounds.bottom - rowTop - 12) / rowHeight;
	if (maximumRows < 1)
		maximumRows = 1;
	const int rowLimit = Session->ManagerVisibleRows < maximumRows
		? Session->ManagerVisibleRows : maximumRows;
	const int maximumScrollOffset = count > rowLimit ? count - rowLimit : 0;
	if (Session->SequenceScrollOffset > maximumScrollOffset)
		Session->SequenceScrollOffset = maximumScrollOffset;
	int visibleRows = count - Session->SequenceScrollOffset;
	if (visibleRows < 0)
		visibleRows = 0;
	if (visibleRows > rowLimit)
		visibleRows = rowLimit;
	const CRect rowViewport(bounds.left + 8, rowTop, bounds.right - 22,
		bounds.bottom - 12);
	if (count > visibleRows && rowViewport.Height() > 0) {
		const int trackHeight = rowViewport.Height();
		int thumbHeight = trackHeight * visibleRows / count;
		if (thumbHeight < 18)
			thumbHeight = 18;
		const int thumbTravel = trackHeight - thumbHeight;
		const int thumbTop = rowViewport.top + (maximumScrollOffset > 0
			? thumbTravel * Session->SequenceScrollOffset / maximumScrollOffset : 0);
		dc.FillSolidRect(rowViewport.right + 3, rowViewport.top,
			6, trackHeight, ConfiguredColor("COLOR_PANEL_DIVIDER"));
		dc.FillSolidRect(rowViewport.right + 3, thumbTop, 6, thumbHeight, ConfiguredColor("COLOR_PANEL_WARNING"));
		AddScreenObject(SCREEN_SEQUENCE_SCROLL_UP, "SCROLL_UP",
			CRect(rowViewport.right, rowViewport.top, bounds.right - 6,
				rowViewport.top + trackHeight / 2), false, "Scroll arrivals up");
		AddScreenObject(SCREEN_SEQUENCE_SCROLL_DOWN, "SCROLL_DOWN",
			CRect(rowViewport.right, rowViewport.top + trackHeight / 2,
				bounds.right - 6, rowViewport.bottom), false, "Scroll arrivals down");
	}
	const int resizeTop = bounds.top + 10;
	const CRect rowsDown(bounds.right - 48, resizeTop, bounds.right - 29, resizeTop + 18);
	const CRect rowsUp(bounds.right - 25, resizeTop, bounds.right - 6, resizeTop + 18);
	DrawButton(dc, rowsDown, "-", ConfiguredColor("COLOR_PANEL_SURFACE"), ConfiguredColor("COLOR_PANEL_HEADER_TEXT"));
	DrawButton(dc, rowsUp, "+", ConfiguredColor("COLOR_PANEL_SURFACE"), ConfiguredColor("COLOR_PANEL_HEADER_TEXT"));
	AddScreenObject(SCREEN_MANAGER_ROWS_DOWN, "ROWS_DOWN", rowsDown, false, "Show fewer arrivals");
	AddScreenObject(SCREEN_MANAGER_ROWS_UP, "ROWS_UP", rowsUp, false, "Show more arrivals");
	for (int index = 0; index < visibleRows; ++index) {
		const int top = rowTop + index * rowHeight;
		const CRect row(bounds.left + 8, top, bounds.right - 22, top + rowHeight - 2);
		DrawSequenceRow(this, dc, row, *Session, Session->SequenceScrollOffset + index, aselCallsign);
	}

	if (count == 0)
		DrawText(dc, bounds.left + 16, rowTop + 4, "No arrivals queued", ConfiguredColor("COLOR_PANEL_MUTED_TEXT"), 9);

	dc.Detach();
}

void FTDiPanelScreen::OnButtonDownScreenObject(
	int ObjectType, const char* sObjectId, POINT Pt, RECT Area, int Button)
{
	UNREFERENCED_PARAMETER(Pt);
	UNREFERENCED_PARAMETER(Area);
	if (HandleFTDiPanelButton(this, *Session, ObjectType, sObjectId, Button, Area))
		RequestRefresh();
}