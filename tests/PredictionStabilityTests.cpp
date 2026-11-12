#include "../src/PredictionStability.h"
#include "../src/TurningPointMath.h"
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
	using namespace PredictionStability;
	Check(EarliestTurnCue(16.0) == TurnCue::Countdown, "countdown before earliest-turn warning window");
	Check(EarliestTurnCue(15.0) == TurnCue::Soon, "fifteen-second cue warns without saying turn is permitted now");
	Check(EarliestTurnCue(1.0) == TurnCue::Soon, "no TURN NOW before the earliest permitted point");
	Check(EarliestTurnCue(0.0) == TurnCue::Now, "TURN NOW only at the earliest permitted point");
	Check(CanRecommendTurn(true, false, false), "uncleared follower may receive recommendations");
	Check(!CanRecommendTurn(true, true, false), "approach clearance immediately suppresses recommendations");
	Check(!CanRecommendTurn(true, true, true), "clearance suppression applies on final too");
	Check(!CanRecommendTurn(false, false, false), "disabled overlay stays disabled");
	Check(!CanRecommendTurn(true, false, true), "established aircraft needs no recommendation");
	TurningPointMath::TurnRoute directRoute;
	Check(TurningPointMath::BuildDirectRoute(12.0, 5.0, 0.0, 0.0, 14.0, directRoute),
		"straight route is independent of banked-turn feasibility");
	Check(std::abs(directRoute.BeforeFinalNm - std::hypot(2.0, 5.0)) < 1.0e-9 && directRoute.FinalNm == 14.0,
		"straight route counts only aircraft-to-localizer and localizer-to-threshold legs");
	for (int headingDegrees = -180; headingDegrees <= 180; headingDegrees += 30)
		Check(TurningPointMath::BuildDirectRoute(12.0, 5.0, headingDegrees * TurningPointMath::Pi / 180.0,
			0.0, 14.0, directRoute), "all headings retain the direct route without arc rejection");
	Check(TurningPointMath::BuildDirectRoute(14.0, 0.0, 0.0, 0.0, 14.0, directRoute) && directRoute.BeforeFinalNm == 0.0,
		"aircraft at LOCALIZERPOINT retains its final route");
	Motion motion;
	motion.Update(90.0, 180.0, 100.0, "EDDF/25L");
	for (int step = 1; step <= 20; ++step) {
		motion.Update(step % 2 == 0 ? 92.0 : 88.0, step % 2 == 0 ? 183.0 : 177.0,
			100.0 + step, "EDDF/25L");
		Check(std::abs(motion.TrackDegrees - 90.0) < 0.5, "small heading jitter is damped");
		Check(std::abs(motion.GroundSpeedKts - 180.0) < 0.6, "small speed jitter is damped");
	}
	Motion slowRefresh;
	Motion fastRefresh;
	slowRefresh.Update(90.0, 180.0, 100.0, "same");
	fastRefresh.Update(90.0, 180.0, 100.0, "same");
	slowRefresh.Update(94.0, 184.0, 104.0, "same");
	for (int step = 1; step <= 40; ++step)
		fastRefresh.Update(94.0, 184.0, 100.0 + step / 10.0, "same");
	Check(std::abs(slowRefresh.TrackDegrees - fastRefresh.TrackDegrees) < 1.0e-8, "heading filter is redraw-rate independent");
	Check(std::abs(slowRefresh.GroundSpeedKts - fastRefresh.GroundSpeedKts) < 1.0e-8, "speed filter is redraw-rate independent");
	motion.Update(120.0, 180.0, 121.0, "EDDF/25L");
	Check(motion.TrackDegrees == 120.0, "significant heading change applies immediately");
	motion.Update(120.0, 220.0, 122.0, "EDDF/25L");
	Check(motion.GroundSpeedKts == 220.0, "significant speed change applies immediately");
	motion.Update(122.0, 219.0, 123.0, "EDDF/25R");
	Check(motion.TrackDegrees == 122.0, "runway change resets motion filter");
	motion.Update(124.0, 217.0, 160.0, "EDDF/25R");
	Check(motion.TrackDegrees == 124.0, "long data gap resets filter");
	Motion north;
	north.Update(359.0, 180.0, 100.0, "same");
	north.Update(1.0, 180.0, 104.0, "same");
	Check(std::abs(HeadingDifference(north.TrackDegrees)) < 2.0, "heading filtering wraps through north");
	TurnTiming timing;
	Check(timing.Update(60.0, 100.0, "pair") == 60.0, "initial turn countdown");
	Check(timing.Update(60.0, 101.0, "pair") == 59.0, "countdown advances despite small prediction jitter");
	const double previousDeadline = timing.DeadlineSeconds;
	timing.Update(75.0, 102.0, "pair");
	Check(std::abs(timing.DeadlineSeconds - previousDeadline) <= 1.0, "moderate deadline correction is bounded");
	const double sameFrameDeadline = timing.DeadlineSeconds;
	timing.Update(74.0, 102.0, "pair");
	Check(timing.DeadlineSeconds == sameFrameDeadline, "repeated redraw does not move deadline");
	Check(timing.Update(30.0, 103.0, "new GAP") == 30.0, "GAP or sequence context change resets immediately");
	Check(timing.Update(100.0, 104.0, "new GAP") == 100.0, "major timing change resets immediately");
	timing.Reset();
	Check(!timing.Initialized, "clearance or invalid route clears prior turn prediction");
	Check(timing.Update(10.0, 105.0, "new GAP") == 10.0, "prediction after reset starts fresh");
	TurnTiming transientFailure;
	transientFailure.Update(60.0, 100.0, "same pair");
	Check(transientFailure.CanReuse(101.0, "same pair", 1.0, true), "single missed solve can retain a validated prior solution");
	Check(transientFailure.CanReuse(108.0, "same pair", 5.0, true), "retention is bounded to eight seconds and five-second timing error");
	Check(transientFailure.LastSolutionSeconds == 100.0, "retaining a circle cannot extend its validity window");
	Check(!transientFailure.CanReuse(108.01, "same pair", 1.0, true), "expired solution is hidden");
	Check(!transientFailure.CanReuse(101.0, "same pair", 5.01, true), "materially wrong prior timing is hidden");
	Check(!transientFailure.CanReuse(101.0, "new runway", 1.0, true), "runway or sequence context change invalidates prior solution");
	Check(!transientFailure.CanReuse(101.0, "same pair", 1.0, CanRecommendTurn(true, true, false)),
		"clearance cannot retain a previous turn circle");
	Check(!transientFailure.CanReuse(101.0, "same pair", 1.0, false), "missing data or infeasible geometry cannot be retained");
	transientFailure.Reset();
	Check(!transientFailure.CanReuse(101.0, "same pair", 1.0, true), "reset cannot revive old recommendation");
	TurnTiming reportTiming;
	for (int age = 0; age <= 10; ++age)
		Check(std::abs(reportTiming.Update(60.0 - age, 100.0 + age, "same report") - (60.0 - age)) < 1.0e-8,
			"report-age correction keeps a steady countdown between radar reports");
	Motion nearParallel;
	nearParallel.Update(357.1, 180.0, 100.0, "EDDF/25L");
	TurningPointMath::TurnRoute referenceRoute;
	Check(TurningPointMath::BuildRoute(20.0, 5.0, nearParallel.TrackDegrees * TurningPointMath::Pi / 180.0,
		nearParallel.GroundSpeedKts, 0.0, 14.0, referenceRoute), "near-parallel reference route");
	const double referenceSlotNm = referenceRoute.BeforeFinalNm + referenceRoute.FinalNm + 4.0;
	for (int step = 1; step <= 40; ++step) {
		nearParallel.Update(step % 2 == 0 ? 357.2 : 357.0, 180.0, 100.0 + step, "EDDF/25L");
		TurningPointMath::TurnRoute route;
		Check(TurningPointMath::BuildRoute(20.0, 5.0, nearParallel.TrackDegrees * TurningPointMath::Pi / 180.0,
			nearParallel.GroundSpeedKts, 0.0, 14.0, route), "near-parallel jitter retains a valid route");
		const double slotNm = route.BeforeFinalNm + route.FinalNm + 4.0;
		Check(std::abs(slotNm - referenceSlotNm) < 0.05,
			"remaining-route target avoids far-intersection jumps under near-parallel heading jitter");
	}
	std::cout << "All clearance suppression, motion filtering, timing stability, and reset checks passed.\n";
}