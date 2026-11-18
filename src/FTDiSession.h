#pragma once

#include <map>
#include <set>
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
	bool Mode2SequenceInvalid = false;
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

struct AutoSequenceProposal {
	std::vector<std::string> Base;
	std::vector<std::string> Order;
};

struct FTDiSession {
	std::vector<std::string> ArrivalSequence;
	std::map<std::string, int> FinalApproachSpeedOverridesKts;
	std::vector<WakeMeasurement> WakeMeasurements;
	std::set<std::string> Mode2UnsequencedArrivals;
	std::map<std::string, PredictionStability::Motion> PlanningMotion;
	std::map<std::string, double> ModeledGroundSpeedKts;
	std::map<std::string, bool> ModeledSpeedHasWind;
	std::string AutoSequenceAirport;
	bool SequenceFeedActive = false;
	unsigned long long SequenceFeedRevision = 0;
	bool AutoDistanceReferenceAvailable = true;
	bool AutoRunwayReferenceAvailable = true;
	std::vector<std::string> AutoAddedCallsigns;
	std::vector<AutoSequenceProposal> AutoProposals;
	std::vector<std::vector<std::string>> AutoProposalMenuOrders;
	std::vector<std::string> PendingAutoDecisionBase;
	std::vector<std::string> PendingAutoDecisionOrder;
	std::vector<std::string> AutoPreviewBase;
	std::vector<std::string> AutoPreviewOrder;
	EuroScopePlugIn::CRadarScreen* AutoPreviewRadarScreen = nullptr;
	std::vector<std::vector<std::string>> AutoRejectedOrders;
	std::vector<std::string> AutoRejectedBase;
	double AutoNextEvaluationSeconds = 0.0;
	std::vector<std::string> AutoLastEvaluationBase;
	bool AutoAcceptRequested = false;
	bool ProposalsEnabled = true;
	void ToggleProposals()
	{
		if (SequenceFeedActive)
			return;
		ProposalsEnabled = !ProposalsEnabled;
		AutoProposals.clear();
		AutoProposalMenuOrders.clear();
		PendingAutoDecisionBase.clear();
		PendingAutoDecisionOrder.clear();
		AutoPreviewBase.clear();
		AutoPreviewOrder.clear();
		AutoAcceptRequested = false;
		AutoNextEvaluationSeconds = 0.0;
		AutoLastEvaluationBase.clear();
	}
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

inline const AutoSequenceProposal* FindCurrentAutoProposal(const FTDiSession& session,
	const std::vector<std::string>& order)
{
	if (!session.ProposalsEnabled)
		return nullptr;
	const auto proposal = std::find_if(session.AutoProposals.begin(), session.AutoProposals.end(),
		[&session, &order](const AutoSequenceProposal& candidate) {
			return candidate.Base == session.ArrivalSequence && candidate.Order == order;
		});
	return proposal == session.AutoProposals.end() ? nullptr : &*proposal;
}

inline bool HasCurrentAutoProposalInvolving(const FTDiSession& session, const std::string& callsign)
{
	return session.ProposalsEnabled && std::any_of(session.AutoProposals.begin(), session.AutoProposals.end(),
		[&session, &callsign](const AutoSequenceProposal& proposal) {
			return SequenceProposalInvolves(session.ArrivalSequence, proposal.Base, proposal.Order, callsign);
		});
}