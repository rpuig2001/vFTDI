#include "../src/DisplayColors.h"
#include "../src/SeparationMath.h"
#include "../src/FTDiSession.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>

void CheckColor(CCallsignLookup::RgbColor actual, int red, int green, int blue, const char* message)
{
	if (actual.Red != red || actual.Green != green || actual.Blue != blue) {
		std::cerr << "FAIL: " << message << '\n';
		std::exit(1);
	}
}

int main(int argumentCount, char** arguments)
{
	if (argumentCount != 2)
		return 1;
	const char* path = arguments[1];
	for (int mode : { 1, 2 }) {
		{
			std::ofstream config(path);
			config << "[general]\nmode = " << mode << "\nmaxDisplayDistanceNm = 25.5\n";
			config << "maxDisplayDistanceNm = -1\nmaxDisplayDistanceNm = invalid\n";
			config << "maxDisplayDistanceNm = 10 extra\nmaxDisplayDistanceNm = nan\n";
		}
		CCallsignLookup displayLimit(path);
		if (displayLimit.getMaximumDisplayDistanceNm() != 25.5 || displayLimit.getMaximumAutoDistanceNm() != 0.0 ||
			!SeparationMath::ShouldDisplayTargets(25.5, displayLimit.getMaximumDisplayDistanceNm()) ||
			SeparationMath::ShouldDisplayTargets(25.51, displayLimit.getMaximumDisplayDistanceNm()) ||
			SeparationMath::ShouldDisplayTargets(-1.0, displayLimit.getMaximumDisplayDistanceNm()))
			return 1;
	}
	if (!SeparationMath::ShouldDisplayTargets(999.0, 0.0) ||
		SeparationMath::ShouldDisplayTargets(std::numeric_limits<double>::quiet_NaN(), 25.0))
		return 1;
	{
		std::ofstream config(path);
		config << "[general]\nsequenceUrl = https://aman.vatsimspain.es/MAESTRO_sequence_data_LEPA.json\n";
	}
	CCallsignLookup sequenceUrlConfig(path);
	if (sequenceUrlConfig.getSequenceUrl() !=
		"https://aman.vatsimspain.es/MAESTRO_sequence_data_LEPA.json")
		return 1;
	{
		std::ofstream config(path);
		config << "[general]\nsequenceUrl = \n";
	}
	if (!CCallsignLookup(path).getSequenceUrl().empty())
		return 1;
	const std::pair<const char*, bool> proposalDefaults[] = {
		{ "true", true }, { "TRUE", true }, { "ON", true }, { "1", true },
		{ "false", false }, { "FALSE", false }, { "off", false }, { "0", false },
		{ "invalid", true }, { "", true }, { "false extra", true }
	};
	for (int mode : { 1, 2 }) {
		for (const auto& setting : proposalDefaults) {
			{
				std::ofstream config(path);
				config << "[general]\nmode = " << mode << "\nproposalsEnabled = " << setting.first << '\n';
			}
			CCallsignLookup startup(path);
			FTDiSession startupSession;
			startupSession.ProposalsEnabled = startup.getProposalsEnabled();
			if (startupSession.ProposalsEnabled != setting.second)
				return 1;
			startupSession.ToggleProposals();
			if (startupSession.ProposalsEnabled == setting.second)
				return 1;
		}
	}
	{
		std::ofstream config(path);
		config << "[general]\nproposalsEnabled = false\nproposalsEnabled = invalid\n";
	}
	if (CCallsignLookup(path).getProposalsEnabled())
		return 1;
	const char* keys[] = {
		"COLOR_FTD_TARGET", "COLOR_ITD_TARGET", "COLOR_FTD_ROUTE", "COLOR_ITD_ROUTE",
		"COLOR_GAP_TARGET", "COLOR_BREACH_ALERT", "COLOR_SELECTION_TEXT", "COLOR_TURN_POINT",
		"COLOR_TURN_PASSED", "COLOR_TOOLTIP_BACKGROUND", "COLOR_TOOLTIP_TEXT",
		"COLOR_PANEL_BACKGROUND", "COLOR_PANEL_SURFACE", "COLOR_PANEL_ROW", "COLOR_PANEL_DIVIDER",
		"COLOR_PANEL_TEXT", "COLOR_PANEL_HEADER_TEXT", "COLOR_PANEL_MUTED_TEXT", "COLOR_PANEL_ACCENT",
		"COLOR_PANEL_WARNING", "COLOR_PANEL_DISABLED", "COLOR_PANEL_REMOVE_BACKGROUND",
		"COLOR_PANEL_REMOVE_TEXT", "COLOR_PANEL_STATUS_WARNING", "COLOR_PANEL_CONTROL",
		"COLOR_PANEL_WAKE_L", "COLOR_PANEL_WAKE_M", "COLOR_PANEL_WAKE_H", "COLOR_PANEL_WAKE_S",
		"COLOR_TAG_SEQUENCE_NUMBER", "COLOR_TAG_SEQUENCE_SUGGESTION"
	};
	{
		std::ofstream config(path);
		config << "[colors.mode1]\n";
		for (const char* key : keys)
			config << (key + 6) << " = 12, 34, 56\n";
		config << "PANEL_BACKGROUND = -1, 0, 0\nTURN_POINT = 256, 0, 0\n";
		config << "TOOLTIP_TEXT = 1, 2\nPANEL_TEXT = 0, 255, 0\n";
	}
	CCallsignLookup configured(path);
	if (configured.getMaximumDisplayDistanceNm() != 0.0)
		return 1;
	if (!configured.getProposalsEnabled())
		return 1;
	if (configured.isMode2())
		return 1;
	if (configured.getMarkerSizePixels(false) != 22 || configured.getMarkerSizePixels(true) != 26) {
		std::cerr << "FAIL: FTD should default slightly larger than ITD\n";
		return 1;
	}
	for (const char* key : keys) {
		if (std::string(key) == "COLOR_PANEL_TEXT")
			CheckColor(configured.getMarkerColor(key), 0, 255, 0, "last valid override");
		else
			CheckColor(configured.getMarkerColor(key), 12, 34, 56, key);
	}
	CCallsignLookup::Lookup = &configured;
	if (ConfiguredColor("COLOR_TURN_POINT") != RGB(12, 34, 56))
		return 1;
	{
		std::ofstream config(path);
		config << "[colors.mode1]\nFTD_TARGET = 1, 2, 3\nITD_TARGET = 4, 5, 6\n";
		config << "TURN_POINT = 300, 0, 0\n";
		config << "[general]\nftdMarkerSize = 10\nitdMarkerSize = 30\n";
	}
	CCallsignLookup legacy(path);
	if (legacy.getMarkerSizePixels(false) != 30 || legacy.getMarkerSizePixels(true) != 32) {
		std::cerr << "FAIL: custom sizes must keep FTD larger than ITD\n";
		return 1;
	}
	CheckColor(legacy.getMarkerColor("COLOR_FTD_ROUTE"), 1, 2, 3, "FTD route inherits target");
	CheckColor(legacy.getMarkerColor("COLOR_ITD_ROUTE"), 4, 5, 6, "ITD route inherits target");
	CheckColor(legacy.getMarkerColor("COLOR_TURN_POINT"), 65, 196, 230, "invalid value uses default");
	CheckColor(legacy.getMarkerColor("COLOR_PANEL_BACKGROUND"), 137, 140, 143, "missing value uses default");
	CCallsignLookup::Lookup = nullptr;
	CCallsignLookup::Available = false;
	if (ConfiguredColor("COLOR_PANEL_SURFACE") != RGB(73, 96, 116))
		return 1;
	CheckColor(CCallsignLookup::getDefaultColor("COLOR_FTD_ROUTE"), 255, 75, 75, "unloaded route default");
	CheckColor(CCallsignLookup::getDefaultColor("COLOR_PANEL_WAKE_L"), 38, 105, 73, "light category default");
	CheckColor(CCallsignLookup::getDefaultColor("COLOR_PANEL_WAKE_M"), 45, 99, 150, "medium category default");
	CheckColor(CCallsignLookup::getDefaultColor("COLOR_PANEL_WAKE_H"), 245, 179, 45, "heavy category default");
	CheckColor(CCallsignLookup::getDefaultColor("COLOR_PANEL_WAKE_S"), 183, 43, 53, "super category default");
	CheckColor(CCallsignLookup::getDefaultColor("COLOR_TAG_SEQUENCE_NUMBER"), 205, 100, 0, "sequence TAG dark orange default");
	CheckColor(CCallsignLookup::getDefaultColor("COLOR_TAG_SEQUENCE_SUGGESTION"), 145, 55, 190, "suggestion TAG purple default");
	{
		std::ofstream config(path);
		config << "[general]\nftdMarkerSize = 999\nitdMarkerSize = -5\n";
	}
	CCallsignLookup bounded(path);
	if (bounded.getMarkerSizePixels(true) != 96 || bounded.getMarkerSizePixels(false) != 6)
		return 1;
	{
		std::ofstream config(path);
		config << "[general]\nftdMarkerSize = invalid\nitdMarkerSize = invalid\n";
	}
	CCallsignLookup invalid(path);
	if (invalid.getMarkerSizePixels(true) != 26 || invalid.getMarkerSizePixels(false) != 22)
		return 1;
	{
		std::ofstream config(path);
		config << "[general]\nmode = 2\n[mode2]\npredictionMarginNm = 0.75\n";
		config << "localizerToleranceNm = -1\nheadingToleranceDegrees = invalid\n";
		config << "[colors]\nPANEL_TEXT = 1, 2, 3\nPANEL_ACCENT = 1, 2, 3\n";
		config << "[colors.mode1]\nPANEL_ACCENT = 10, 20, 30\n";
		config << "[colors.mode2]\nSEPARATION_NORMAL = 40, 50, 60\nPANEL_TEXT = 7, 8, 9\n";
		config << "SEPARATION_BREACH = 999, 0, 0\n";
	}
	CCallsignLookup mode2(path);
	if (!mode2.isMode2() || mode2.getMode2Setting("predictionMarginNm", 0.5) != 0.75 ||
		mode2.getMode2Setting("localizerToleranceNm", 0.5) != 0.5 ||
		mode2.getMode2Setting("headingToleranceDegrees", 30.0) != 30.0)
		return 1;
	CheckColor(mode2.getMarkerColor("COLOR_SEPARATION_NORMAL"), 40, 50, 60, "mode 2 independent palette");
	CheckColor(mode2.getMarkerColor("COLOR_PANEL_TEXT"), 7, 8, 9, "mode 2 panel palette");
	CheckColor(mode2.getMarkerColor("COLOR_PANEL_ACCENT"), 105, 158, 181, "mode 2 ignores shared and mode 1 palettes");
	CheckColor(mode2.getMarkerColor("COLOR_SEPARATION_BREACH"), 255, 226, 62, "invalid mode color fallback");
	{
		std::ofstream config(path);
		config << "[general]\nmode = 1\nmode = 3\n[colors]\nPANEL_TEXT = 1, 2, 3\n";
		config << "[colors.mode1]\nFTD_TARGET = 10, 20, 30\n";
		config << "[colors.mode2]\nPANEL_TEXT = 40, 50, 60\n";
	}
	CCallsignLookup mode1(path);
	if (mode1.isMode2())
		return 1;
	CheckColor(mode1.getMarkerColor("COLOR_FTD_TARGET"), 10, 20, 30, "mode 1 independent palette");
	CheckColor(mode1.getMarkerColor("COLOR_FTD_ROUTE"), 10, 20, 30, "mode route inheritance");
	CheckColor(mode1.getMarkerColor("COLOR_PANEL_TEXT"), 29, 34, 37, "mode 1 ignores shared and mode 2 palettes");
	if (!SeparationMath::Mode2LeaderEstablished(10.0, 0.5, 30.0, 0.5, 30.0) ||
		SeparationMath::Mode2LeaderEstablished(10.0, 0.51, 0.0, 0.5, 30.0) ||
		SeparationMath::Mode2LeaderEstablished(10.0, 0.0, 31.0, 0.5, 30.0) ||
		SeparationMath::Mode2LeaderEstablished(-1.0, 0.0, 0.0, 0.5, 30.0))
		return 1;
	const auto checkState = [](bool established, double follower, const char* expected) {
		if (std::string(SeparationMath::Mode2ColorKey(established, follower, 14.0, 0.5, false)) != expected)
			std::exit(1);
	};
	checkState(true, 13.9, "COLOR_SEPARATION_BREACH");
	checkState(true, 14.0, "COLOR_SEPARATION_PREDICTION");
	checkState(true, 14.49, "COLOR_SEPARATION_PREDICTION");
	checkState(true, 14.5, "COLOR_SEPARATION_NORMAL");
	checkState(false, 13.9, "COLOR_SEPARATION_NORMAL");
	if (std::string(SeparationMath::Mode2ColorKey(false, 13.9, 14.0, 0.5, true)) != "COLOR_SEPARATION_MANUAL")
		return 1;
	for (int mode : { 1, 2 }) {
		for (const char* scheme : { "WTC", "RECAT-EU" }) {
			{
				std::ofstream config(path);
				config << "[general]\nmode = " << mode << "\n[separation]\nscheme = " << scheme << '\n';
				config << "minimumDistanceNm = 3.5\nfinalBufferNm = 0.5\n";
				config << "[recat]\nA = A388\nD = A320\n[recat.separation]\nA_D = 6\n";
				config << "[profile.DEP]\nEDDF.25L.25R = DEPENDANT\n[profile.IND]\nEDDF.25L.25R = INDEPENDENT\n";
			}
			CCallsignLookup shared(path);
			if (shared.getRequiredSpacingNm('H', 'M', "A388", "A320") != (std::string(scheme) == "WTC" ? 5.5 : 6.5) ||
				shared.getRequiredSpacingNm('M', 'M', "UNKNOWN", "UNKNOWN") != 4.0 ||
				shared.getRunwayPairMode("DEP", "EDDF", "25L", "EDDF", "25R") != CCallsignLookup::RunwayPairMode::Dependant ||
				shared.getRunwayPairMode("IND", "EDDF", "25L", "EDDF", "25R") != CCallsignLookup::RunwayPairMode::Independent)
				return 1;
		}
	}
	std::remove(path);
	FTDiSession session;
	session.ArrivalSequence = { "FIRST", "SECOND", "THIRD" };
	session.AutoSequenceAirport = "EDDF";
	const AutoSequenceProposal proposal{ session.ArrivalSequence, { "SECOND", "FIRST", "THIRD" } };
	session.AutoProposals.push_back(proposal);
	if (!session.ProposalsEnabled || FindCurrentAutoProposal(session, proposal.Order) == nullptr ||
		!HasCurrentAutoProposalInvolving(session, "SECOND"))
		return 1;
	session.AutoProposalMenuOrders.push_back(proposal.Order);
	session.PendingAutoDecisionBase = proposal.Base;
	session.PendingAutoDecisionOrder = proposal.Order;
	session.AutoPreviewBase = proposal.Base;
	session.AutoPreviewOrder = proposal.Order;
	session.AutoAcceptRequested = true;
	session.AutoNextEvaluationSeconds = 123.0;
	session.AutoLastEvaluationBase = proposal.Base;
	session.AutoRejectedBase = proposal.Base;
	session.AutoRejectedOrders.push_back(proposal.Order);
	session.ToggleProposals();
	if (session.ProposalsEnabled || !session.AutoProposals.empty() || !session.AutoProposalMenuOrders.empty() ||
		!session.PendingAutoDecisionBase.empty() || !session.PendingAutoDecisionOrder.empty() ||
		!session.AutoPreviewBase.empty() || !session.AutoPreviewOrder.empty() || session.AutoAcceptRequested ||
		session.AutoNextEvaluationSeconds != 0.0 || !session.AutoLastEvaluationBase.empty() ||
		session.AutoSequenceAirport != "EDDF" || session.ArrivalSequence != proposal.Base || session.AutoRejectedOrders.empty())
		return 1;
	session.AutoProposals.push_back(proposal);
	if (FindCurrentAutoProposal(session, proposal.Order) != nullptr || HasCurrentAutoProposalInvolving(session, "SECOND"))
		return 1;
	session.ToggleProposals();
	if (!session.ProposalsEnabled || !session.AutoProposals.empty() || session.AutoNextEvaluationSeconds != 0.0)
		return 1;
	session.SequenceFeedActive = true;
	session.ProposalsEnabled = false;
	session.ToggleProposals();
	if (session.ProposalsEnabled)
		return 1;
	std::cout << "All color, mode, spacing/alert, and proposal-toggle checks passed.\n";
}