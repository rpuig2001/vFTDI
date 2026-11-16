#pragma once

#include <map>
#include <string>
#include <vector>
#include "EuroScopePlugIn.h"
#include "PredictionStability.h"
#include "SequenceOrder.h"

struct WakeMeasurement {
	std::string PrecedingCallsign;
	std::string SucceedingCallsign;
	double RequiredNm;
	double GapNm = 0.0;
	bool ParallelDependent = false;
	bool ParallelIndependent = false;
	bool ClearedForApproach = false;
	EuroScopePlugIn::CPosition ExpectedRoutePosition;
	bool HasExpectedRoutePosition = false;
	EuroScopePlugIn::CPosition TargetSlotPosition;
	bool HasTargetSlotPosition = false;
	double CurrentActualNm = 0.0;
	double CurrentRequiredNm = 0.0;
	double CurrentFTDNm = 0.0;
	double CurrentITDNm = 0.0;
	double CurrentCompressionNm = 0.0;
	bool HasCurrentSpacing = false;
	bool HasRouteBasedCurrentSpacing = false;
	EuroScopePlugIn::CPosition FinalTargetPosition;
	bool HasFinalTargetPosition = false;
	std::string TurnStatus;
	std::string TurnStatusDetail;
	PredictionStability::TurnTiming PlanningTurnTiming;
	PredictionStability::TurnNowWindow PlanningTurnNowWindow;
};

struct FTDiSession {
	std::vector<std::string> ArrivalSequence;
	std::map<std::string, int> FinalApproachSpeedOverridesKts;
	std::vector<WakeMeasurement> WakeMeasurements;
	std::map<std::string, PredictionStability::Motion> PlanningMotion;
	std::map<std::string, double> ModeledGroundSpeedKts;
	std::map<std::string, bool> ModeledSpeedHasWind;
	std::string AutoSequenceAirport;
	std::string AutoSequenceStatus;
	bool AutoDistanceReferenceAvailable = true;
	bool AutoRunwayReferenceAvailable = true;
	std::vector<std::string> AutoAddedCallsigns;
	std::vector<std::string> AutoProposalOrder;
	std::vector<std::string> AutoProposalBase;
	std::vector<std::string> PendingAutoDecisionBase;
	std::vector<std::string> PendingAutoDecisionOrder;
	std::vector<std::string> AutoPreviewBase;
	std::vector<std::string> AutoPreviewOrder;
	EuroScopePlugIn::CRadarScreen* AutoPreviewRadarScreen = nullptr;
	std::vector<std::vector<std::string>> AutoRejectedOrders;
	std::vector<std::string> AutoRejectedBase;
	double AutoProposalSinceSeconds = 0.0;
	double AutoNextEvaluationSeconds = 0.0;
	std::vector<std::string> AutoLastEvaluationBase;
	bool AutoAcceptRequested = false;
	bool SequenceDirty = false;
	bool ManagerExpanded = false;
	bool ShowTurningPoints = false;
	std::string ActiveProfile = "ALT";
	std::string SelectedSequenceCallsign;
	std::string RightClickedSequenceCallsign;
	std::string RoutePreviewCallsign;
	bool HasManagerPosition = false;
	bool ManagerDragging = false;
	POINT ManagerPosition{};
	POINT ManagerDragOffset{};
	int ManagerWindowWidth = 0;
	int ManagerWindowHeight = 0;
	int ManagerVisibleRows = 3;
	int SequenceScrollOffset = 0;
	RECT RadarBounds{};
	std::string PendingGapPrecedingCallsign;
	std::string PendingGapSucceedingCallsign;
	std::string PendingFinalApproachSpeedCallsign;
	std::string PendingSequenceMoveCallsign;
	std::string PendingSequenceMoveTargetCallsign;
};