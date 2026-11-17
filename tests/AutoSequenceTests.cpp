#include "../src/SequenceOrder.h"
#include <cstdlib>
#include <iostream>

void Check(bool condition, const char* message)
{
	if (!condition) {
		std::cerr << "FAIL: " << message << '\n';
		std::exit(1);
	}
}

int main()
{
	const auto spacing = [](const std::string&, const std::string&) { return 60.0; };
	SuggestionMenuDismissDetector menu;
	Check(!menu.Update(true, true, true, false, false, false) && !menu.Update(true, true, false, false, false, false) &&
		menu.Update(true, true, true, false, false, false),
		"menu dismissal ignores its opening click and detects the next click");
	menu.Reset();
	Check(!menu.Update(true, true, false, false, false, false) && menu.Update(true, true, false, false, false, true),
		"Escape dismisses an armed suggestion menu");
	menu.Reset();
	Check(!menu.Update(true, true, false, false, false, false) && menu.Update(true, true, false, false, true, false),
		"middle click dismisses an armed suggestion menu");
	menu.Reset();
	Check(menu.Update(true, false, false, false, false, false) && !menu.Update(false, true, false, false, false, false),
		"loss of foreground dismisses the menu and inactive state resets the detector");
	Check(IsCurrentSuggestionMenuDismissal(5, 5, true) &&
		!IsCurrentSuggestionMenuDismissal(5, 6, true) &&
		!IsCurrentSuggestionMenuDismissal(0, 0, true) &&
		!IsCurrentSuggestionMenuDismissal(5, 5, false),
		"a delayed dismissal cannot close a different or inactive proposal menu");
	Check(IsAutomaticDistanceEligible(100.0, 100.0) && !IsAutomaticDistanceEligible(100.01, 100.0) &&
		IsAutomaticDistanceEligible(400.0, 0.0) && !IsAutomaticDistanceEligible(-1.0, 100.0) &&
		!IsAutomaticDistanceEligible(std::numeric_limits<double>::quiet_NaN(), 100.0),
		"auto distance limit is inclusive, rejects invalid distance, and zero disables it");
	std::vector<SequenceArrivalEstimate> insertion = {
		{ "FIRST", 60.0, false }, { "SECOND", 120.0, true }, { "THIRD", 240.0, false } };
	Check(AutomaticInsertionIndex(insertion, 30.0, false) == 2 &&
		AutomaticInsertionIndex(insertion, 180.0, false) == 2 &&
		AutomaticInsertionIndex(insertion, 300.0, false) == 3,
		"automatic insertion remains after locked arrivals and uses estimated arrival order");
	Check(AutomaticInsertionIndex(insertion, 30.0, true) == 3 &&
		AutomaticInsertionIndex(insertion, std::numeric_limits<double>::infinity(), false) == 3,
		"a GAP or missing estimate appends without changing existing pairs");
	Check(AutomaticInsertionIndex(insertion, 30.0, false, true) == insertion.size(),
		"a newly discovered CA aircraft is appended instead of being moved into an existing sequence");
	const std::vector<SequenceArrivalEstimate> caIntoCa = {
		{ "CA1", 120.0, false, true }, { "CA2", 0.0, false, true } };
	const std::vector<SequenceArrivalEstimate> caIntoUncleared = {
		{ "VECTOR", 120.0, false, false }, { "CA", 0.0, false, true } };
	Check(BestSequenceInsertionImprovement(caIntoCa, spacing).Order.empty() &&
		BestSequenceInsertionImprovement(caIntoUncleared, spacing).Order.empty(),
		"a CA aircraft cannot be moved before a CA or an uncleared aircraft");
	const std::vector<SequenceArrivalEstimate> unclearedIntoCa = {
		{ "CA1", 0.0, false, true }, { "CA2", 300.0, false, true }, { "VECTOR", 120.0, false, false } };
	const auto caGapProposal = BestSequenceInsertionImprovement(unclearedIntoCa, spacing);
	Check(caGapProposal.Order == std::vector<std::string>({ "CA1", "VECTOR", "CA2" }),
		"an uncleared aircraft can move into a gap between CA aircraft");
	const auto allowCaTest = [](const std::vector<std::string>&) { return true; };
	Check(!std::isfinite(SequenceProposalImprovementSeconds(caIntoUncleared, { "CA", "VECTOR" }, spacing, allowCaTest)) &&
		SequenceProposalImprovementSeconds(unclearedIntoCa, caGapProposal.Order, spacing, allowCaTest) > 0.0,
		"proposal revalidation rejects a newly CA-cleared moving aircraft but permits an uncleared insertion");
	auto newlyCleared = unclearedIntoCa;
	newlyCleared[2].ClearedForIls = true;
	Check(!std::isfinite(SequenceProposalImprovementSeconds(newlyCleared, caGapProposal.Order, spacing, allowCaTest)),
		"an existing suggestion becomes invalid if its moving aircraft receives CA");
	std::vector<SequenceArrivalEstimate> arrivals = {
		{ "FIRST", 120.0, false }, { "SECOND", 0.0, false }, { "THIRD", 240.0, false } };
	Check(SequenceTotalDelaySeconds(arrivals, spacing) == 180.0, "current order delay");
	const auto proposal = BestSequenceInsertionImprovement(arrivals, spacing);
	Check(proposal.Order == std::vector<std::string>({ "SECOND", "FIRST", "THIRD" }) &&
		proposal.AverageImprovementSeconds == 60.0, "best swap reduces average delay");
	const std::vector<std::string> base = { "FIRST", "SECOND", "THIRD" };
	Check(IsCurrentSequenceProposal(base, base, proposal.Order, proposal, 5.0),
		"matching beneficial proposal can be accepted");
	const auto allowed = [](const std::vector<std::string>&) { return true; };
	std::map<std::string, double> delayChanges;
	Check(SequenceProposalImprovementSeconds(arrivals, proposal.Order, spacing, allowed, &delayChanges) == 60.0 &&
		delayChanges["SECOND"] == -180.0 && delayChanges["FIRST"] == 0.0 && delayChanges["THIRD"] == 0.0 &&
		!std::isfinite(SequenceProposalImprovementSeconds(arrivals, { "SECOND", "SECOND", "THIRD" }, spacing, allowed)),
		"a proposal is rescored with per-aircraft changes and rejects duplicate aircraft");
	Check(IsConservativeSequenceProposal(base, proposal.Order, delayChanges),
		"a nearby single move with no significant extra delay is allowed");
	delayChanges["FIRST"] = 31.0;
	Check(!IsConservativeSequenceProposal(base, proposal.Order, delayChanges),
		"a proposal adding over thirty seconds to one aircraft is rejected");
	delayChanges["FIRST"] = 30.0;
	Check(IsConservativeSequenceProposal(base, proposal.Order, delayChanges),
		"the thirty-second individual delay limit is inclusive");
	std::vector<SequenceArrivalEstimate> downstream = {
		{ "FIRST", 120.0, false }, { "SECOND", 0.0, false }, { "THIRD", 150.0, false } };
	std::map<std::string, double> downstreamChanges;
	SequenceProposalImprovementSeconds(downstream, proposal.Order, spacing, allowed, &downstreamChanges);
	Check(downstreamChanges["THIRD"] == -60.0,
		"proposal scoring accounts for downstream aircraft whose position does not change");
	arrivals[2].Locked = true;
	Check(!std::isfinite(SequenceProposalImprovementSeconds(arrivals, proposal.Order, spacing, allowed)),
		"a proposal is invalid when a newly locked aircraft protects its earlier sequence");
	arrivals[2].Locked = false;
	const auto deny = [](const std::vector<std::string>&) { return false; };
	Check(!std::isfinite(SequenceProposalImprovementSeconds(arrivals, proposal.Order, spacing, deny)),
		"a proposal is invalid when its GAP constraints no longer hold");
	Check(!IsCurrentSequenceProposal(proposal.Order, base, proposal.Order, proposal, 5.0) &&
		!IsCurrentSequenceProposal(base, base, proposal.Order, proposal, 61.0) &&
		!IsCurrentSequenceProposal(base, base, base, proposal, 5.0),
		"changed order, stale proposal, and insufficient improvement cannot be accepted");
	Check(!ShouldEvaluateAutomaticSequence(110.0, 160.0, base, base, false) &&
		ShouldEvaluateAutomaticSequence(160.0, 160.0, base, base, false) &&
		ShouldEvaluateAutomaticSequence(110.0, 160.0, proposal.Order, base, false) &&
		ShouldEvaluateAutomaticSequence(110.0, 160.0, base, base, true),
		"decision cooldown pauses unchanged sequences but not a changed sequence or acceptance");
	std::vector<SequenceArrivalEstimate> reversed = {
		{ "FIRST", 100.0, false }, { "SECOND", 0.0, false },
		{ "THIRD", 500.0, false }, { "FOURTH", 400.0, false } };
	const auto firstChoice = BestSequenceInsertionImprovement(reversed, spacing);
	const auto secondChoice = BestSequenceInsertionImprovement(reversed, spacing, { firstChoice.Order });
	Check(!firstChoice.Order.empty() && !secondChoice.Order.empty() && firstChoice.Order != secondChoice.Order,
		"a declined proposal does not hide another beneficial swap");
	Check(firstChoice.Candidates.size() >= 2 &&
		std::any_of(firstChoice.Candidates.begin(), firstChoice.Candidates.end(), [&firstChoice](const auto& candidate) {
			return candidate.Order == firstChoice.Order;
		}), "optimization returns multiple individually beneficial proposals including its best move");
	const std::vector<std::string> proposalBase = { "A", "B", "C", "D", "E" };
	const std::vector<SequenceOptimization::Candidate> overlappingProposals = {
		{ { "C", "A", "B", "D", "E" }, 20.0 },
		{ { "A", "D", "B", "C", "E" }, 15.0 },
		{ { "B", "A", "C", "D", "E" }, 10.0 },
		{ { "E", "A", "B", "C", "D" }, 25.0 }
	};
	const auto selectedProposals = BestNonConflictingSequenceProposals(proposalBase, overlappingProposals, 5);
	std::set<std::string> proposalCallsigns;
	bool includesBestProposal = false;
	for (const auto& candidate : selectedProposals) {
		std::string movedCallsign;
		std::string beforeCallsign;
		Check(SequenceProposalMove(proposalBase, candidate.Order, movedCallsign, beforeCallsign),
			"selected concurrent proposal is a valid single move");
		Check(proposalCallsigns.insert(movedCallsign).second && proposalCallsigns.insert(beforeCallsign).second,
			"a flight appears in no more than one selected proposal");
		includesBestProposal = includesBestProposal || candidate.Order == overlappingProposals.back().Order;
	}
	Check(selectedProposals.size() == 2 && includesBestProposal,
		"proposal conflicts are resolved by keeping the best moves for distinct flights");
	arrivals[0].Locked = true;
	Check(BestSequenceInsertionImprovement(arrivals, spacing).Order.empty(), "locked aircraft cannot be moved");
	arrivals[0].Locked = false;
	arrivals[2].Locked = true;
	Check(BestSequenceInsertionImprovement(arrivals, spacing).Order.empty(),
		"an insertion before a locked aircraft cannot change its scheduled slot");
	arrivals[2].Locked = false;
	const auto independent = [](const std::string&, const std::string&) { return -1.0; };
	Check(SequenceTotalDelaySeconds(arrivals, independent) == 0.0 &&
		BestSequenceInsertionImprovement(arrivals, independent).Order.empty(), "independent approaches impose no spacing");
	const auto directional = [](const std::string& preceding, const std::string& succeeding) {
		if (preceding == "FIRST" && succeeding == "THIRD") return 120.0;
		return 0.0;
	};
	std::vector<SequenceArrivalEstimate> chain = {
		{ "FIRST", 0.0, false }, { "SECOND", 0.0, false }, { "THIRD", 0.0, false } };
	Check(SequenceTotalDelaySeconds(chain, directional) == 120.0,
		"each arrival respects constraints from earlier relevant aircraft, not just its neighbor");
	const auto invalidSpacing = [](const std::string&, const std::string&) {
		return std::numeric_limits<double>::infinity();
	};
	Check(BestSequenceInsertionImprovement(chain, invalidSpacing).Order.empty(),
		"unavailable pair spacing suppresses optimization");
	arrivals[1].ArrivalSeconds = -1.0;
	Check(BestSequenceInsertionImprovement(arrivals, spacing).Order.empty(), "missing prediction prevents proposals");
	const std::vector<SequenceArrivalEstimate> gap = {
		{ "A", 0.0, false }, { "B", 600.0, false },
		{ "C", 360.0, false }, { "D", 180.0, false } };
	const auto gapProposal = BestSequenceInsertionImprovement(gap, spacing);
	Check(gapProposal.Order == std::vector<std::string>({ "A", "D", "B", "C" }) &&
		gapProposal.AverageImprovementSeconds == 135.0,
		"a later aircraft can move across multiple positions to fill an empty gap");
	const std::vector<std::pair<std::string, std::string>> protectedGap = { { "B", "C" } };
	Check(PreservesSequenceGapPairs(gapProposal.Order, protectedGap, spacing) &&
		!PreservesSequenceGapPairs({ "A", "B", "D", "C" }, protectedGap, spacing),
		"optimization preserves a configured GAP's applicable predecessor");
	const auto independentGap = [](const std::string& preceding, const std::string& succeeding) {
		return preceding == "D" && succeeding == "C" ? -1.0 : 60.0;
	};
	Check(PreservesSequenceGapPairs({ "A", "B", "D", "C" }, protectedGap, independentGap),
		"an independent runway may remain between a protected GAP pair");
	Check(GapSafeInsertionIndex({ "A", "B", "C" }, "D", 2, protectedGap, spacing) == 3 &&
		GapSafeInsertionIndex({ "A", "B", "C" }, "D", 1, protectedGap, spacing) == 1 &&
		GapSafeInsertionIndex({ "A", "B", "C" }, "D", 2, protectedGap, independentGap) == 2,
		"new automatic arrivals may fill gaps without breaking an existing GAP pair");
	const auto protectedCandidate = [&protectedGap, &spacing](const std::vector<std::string>& order) {
		return PreservesSequenceGapPairs(order, protectedGap, spacing);
	};
	const auto gapWithProtection = BestSequenceInsertionImprovement(gap, spacing, {}, 25000000, protectedCandidate);
	Check(gapWithProtection.Order == gapProposal.Order,
		"a GAP does not block a beneficial insertion that preserves its pair");
	const std::vector<SequenceArrivalEstimate> example = {
		{ "DLH29C", 100.0, true }, { "DLH8CL", 800.0, false },
		{ "DLH4EY", 900.0, false }, { "DLH21VG", 450.0, false } };
	const auto exampleProposal = BestSequenceInsertionImprovement(example, spacing);
	Check(exampleProposal.Order == std::vector<std::string>({ "DLH29C", "DLH21VG", "DLH8CL", "DLH4EY" }),
		"a later unlocked aircraft can fill a modeled gap after an established aircraft");
	const auto coordinatedSpacing = [](const std::string& preceding, const std::string& succeeding) {
		return preceding == "A" || succeeding == "A" ? 20.0 : 0.0;
	};
	const std::vector<SequenceArrivalEstimate> coordinated = {
		{ "A", 0.0, false }, { "B", 0.0, false }, { "C", 0.0, false } };
	const auto coordinatedProposal = BestSequenceInsertionImprovement(coordinated, coordinatedSpacing);
	Check(coordinatedProposal.Order.empty(), "coordinated multi-move reorders are not proposed");
	Check(IsSingleForwardInsertion({ "A", "B", "C" }, { "C", "A", "B" }) &&
		!IsSingleForwardInsertion({ "A", "B", "C" }, { "B", "C", "A" }) &&
		!IsSingleForwardInsertion({ "A", "B", "C" }, { "A", "B", "C" }),
		"proposal presentation distinguishes a single insertion from a coordinated reorder");
	std::string moved;
	std::string before;
	Check(SequenceProposalMove({ "A", "B", "C" }, { "C", "A", "B" }, moved, before) &&
		moved == "C" && before == "A", "preview identifies the moving aircraft and its new successor");
	Check(!SequenceProposalMove({ "A", "B", "C" }, { "B", "C", "A" }, moved, before) &&
		moved.empty() && before.empty(), "ambiguous reorders do not produce a misleading two-aircraft preview");
	Check(SequenceProposalInvolves({ "A", "B", "C" }, { "A", "B", "C" }, { "C", "A", "B" }, "C") &&
		SequenceProposalInvolves({ "A", "B", "C" }, { "A", "B", "C" }, { "C", "A", "B" }, "A") &&
		!SequenceProposalInvolves({ "A", "B", "C" }, { "A", "B", "C" }, { "C", "A", "B" }, "B") &&
		!SequenceProposalInvolves({ "A", "C", "B" }, { "A", "B", "C" }, { "C", "A", "B" }, "C"),
		"suggestion TAGs mark only the two endpoints of a current proposal");
	Check(IsSameLiveSequenceProposal(base, base, proposal.Order, base, proposal.Order) &&
		!IsSameLiveSequenceProposal(proposal.Order, base, proposal.Order, base, proposal.Order) &&
		!IsSameLiveSequenceProposal(base, base, proposal.Order, base, base),
		"proposal menu decisions only apply to the exact still-current suggestion");
	std::vector<std::string> previewBase;
	std::vector<std::string> previewOrder;
	Check(BeginSequenceProposalPreview(base, base, proposal.Order, previewBase, previewOrder) &&
		previewBase == base && previewOrder == proposal.Order &&
		IsSameLiveSequenceProposal(base, base, proposal.Order, previewBase, previewOrder),
		"a click starts a preview of the current proposal");
	Check(!BeginSequenceProposalPreview(proposal.Order, base, proposal.Order, previewBase, previewOrder) &&
		!IsSameLiveSequenceProposal(proposal.Order, base, proposal.Order, previewBase, previewOrder),
		"a changed sequence invalidates the click preview");
	const std::vector<std::pair<std::string, std::string>> coordinatedGap = { { "B", "C" } };
	const auto coordinatedAllowed = [&coordinatedGap, &coordinatedSpacing](const std::vector<std::string>& order) {
		return PreservesSequenceGapPairs(order, coordinatedGap, coordinatedSpacing);
	};
	const auto protectedReorder = BestSequenceInsertionImprovement(coordinated, coordinatedSpacing, {}, 25000000, coordinatedAllowed);
	Check(protectedReorder.Order.empty(), "GAP protection does not enable coordinated reorders");
	const std::vector<std::string> longBase = { "A", "B", "C", "D" };
	const std::map<std::string, double> noDelayChanges = { { "A", 0.0 }, { "B", 0.0 }, { "C", 0.0 }, { "D", 0.0 } };
	Check(IsConservativeSequenceProposal(longBase, { "A", "D", "B", "C" }, noDelayChanges) &&
		!IsConservativeSequenceProposal(longBase, { "D", "A", "B", "C" }, noDelayChanges),
		"an aircraft may move at most two sequence positions forward");
	const std::vector<SequenceArrivalEstimate> unfair = {
		{ "A", 0.0, false }, { "B", 80.0, false }, { "C", 0.0, false } };
	const auto unfairSpacing = [](const std::string& preceding, const std::string& succeeding) {
		return preceding == "B" && succeeding == "C" ? 180.0 : 60.0;
	};
	const std::vector<std::string> unfairBase = { "A", "B", "C" };
	const auto fairOnly = [&unfair, &unfairSpacing, &unfairBase, &allowed](const std::vector<std::string>& order) {
		std::map<std::string, double> changes;
		const double saving = SequenceProposalImprovementSeconds(unfair, order, unfairSpacing, allowed, &changes);
		return saving >= AutoSequenceMinimumAverageSavingSeconds &&
			IsConservativeSequenceProposal(unfairBase, order, changes);
	};
	Check(BestSequenceInsertionImprovement(unfair, unfairSpacing).AverageImprovementSeconds > 5.0 &&
		BestSequenceInsertionImprovement(unfair, unfairSpacing, {}, 25000000, fairOnly).Order.empty(),
		"a big average saving is rejected when it adds too much delay to an individual");
	const auto noSpacing = [](const std::string&, const std::string&) { return 0.0; };
	const std::vector<SequenceArrivalEstimate> zeroDelayCandidate = {
		{ "FIRST", 0.5, false }, { "SECOND", 0.0, false } };
	const auto zeroDelay = BestSequenceInsertionImprovement(zeroDelayCandidate, noSpacing, {}, 2);
	Check(zeroDelay.SearchComplete && zeroDelay.Order == std::vector<std::string>({ "SECOND", "FIRST" }),
		"search stops when a candidate reaches zero delay even below the proposal threshold");
	std::vector<SequenceArrivalEstimate> largeSequence;
	for (int index = 0; index < 100; ++index)
		largeSequence.push_back({ "FLIGHT" + std::to_string(index), static_cast<double>((100 - index) * 60), false });
	Check(!BestSequenceInsertionImprovement(largeSequence, spacing).Order.empty(),
		"gap search supports a longer arrival sequence");
	const auto limitedSearch = BestSequenceInsertionImprovement(largeSequence, spacing, {}, 6000);
	Check(!limitedSearch.SearchComplete, "large searches stop at their work budget");
	Check(BestSequenceInsertionImprovement(largeSequence, spacing, {}, 1).Order.empty(),
		"an insufficient baseline budget cannot produce a proposal");
	std::cout << "Auto-sequence delay and proposal tests passed.\n";
}