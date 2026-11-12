#include "../src/TurningPointMath.h"
#include <cstdlib>
#include <iostream>
#include <limits>

void Check(bool condition, const char* message)
{
	if (!condition) {
		std::cerr << "FAIL: " << message << '\n';
		std::exit(1);
	}
}

int main()
{
	using namespace TurningPointMath;
	Check(MarkerDistanceNm(10.0, 10.0, 12.0, 1.5) == 13.5, "FTD stays behind stagger-corrected leader");
	Check(MarkerDistanceNm(16.0, 12.0, 12.0, 4.0) == 20.0, "longer modeled route remains target reference");
	Check(MarkerDistanceNm(10.0, 10.0, 12.0, 6.0) >= MarkerDistanceNm(10.0, 10.0, 12.0, 4.0),
		"ITD remains behind or coincident with FTD");
	Check(RouteDistanceOnLocalizerNm(10.0, 10.0, 12.0) == 12.0, "route uses follower threshold coordinates");
	Check(DownstreamReferenceNm(14.0, 10.0) == 14.0, "expected behind FTD governs downstream spacing");
	Check(DownstreamReferenceNm(8.0, 10.0) == 10.0, "uncleared expected position ahead of FTD uses FTD");
	Check(DownstreamReferenceNm(8.0, 10.0, true) == 8.0, "CA/VA uses only expected position");
	Check(DownstreamReferenceNm(-2.0, 10.0, true) == 0.0, "cleared reference clamps at threshold");
	const double firstFollowerFtd = DownstreamReferenceNm(12.0, 10.0) + 4.0;
	Check(firstFollowerFtd == 16.0 && DownstreamReferenceNm(14.0, firstFollowerFtd) + 4.0 == 20.0,
		"downstream FTD floor propagates through successive aircraft");
	TurnRoute route;
	Check(BuildRoute(12.0, 5.0, 0.0, 0.0, 0.0, 14.0, route), "geometry needs no groundspeed or bank radius");
	Check(std::abs(route.BeforeFinalNm - std::hypot(2.0, 5.0)) < 1.0e-9 && route.FinalNm == 14.0,
		"route length is direct to LOCALIZERPOINT plus final leg");
	Check(route.TurnAngleRadians < 0.0, "left direct-to-point direction");
	const double unclearedDistanceNm = route.BeforeFinalNm + route.FinalNm;
	TurnRoute clearedRoute;
	Check(BuildRoute(12.0, 5.0, 0.0, 220.0, 0.0, 14.0, clearedRoute, true), "CA/VA route builds direct to runway");
	Check(clearedRoute.BeforeFinalNm == 0.0 && std::abs(clearedRoute.FinalNm - 13.0) < 1.0e-9,
		"cleared route is present-position to threshold only, with no LOCALIZERPOINT leg");
	Check(clearedRoute.LocalizerPointNm == 0.0 && clearedRoute.FinalNm < unclearedDistanceNm,
		"cleared model does not retain localizer-point detour distance");
	TurnRoute differentLocalizer;
	Check(BuildRoute(12.0, 5.0, 0.0, 220.0, 0.0, 25.0, differentLocalizer, true) &&
		differentLocalizer.FinalNm == clearedRoute.FinalNm, "CA/VA route is independent of configured LOCALIZERPOINT distance");
	Check(BuildRoute(12.0, 5.0, 0.0, 220.0, 0.0, 14.0, route) &&
		std::abs(route.BeforeFinalNm + route.FinalNm - unclearedDistanceNm) < 1.0e-9,
		"removing CA/VA restores route through LOCALIZERPOINT");
	Check(BuildRoute(12.0, -5.0, 0.0, 350.0, 0.0, 14.0, route) && route.TurnAngleRadians > 0.0,
		"right direct-to-point direction");
	Check(BuildRoute(14.0, 0.0, 0.0, 0.0, 0.0, 14.0, route) && route.BeforeFinalNm == 0.0,
		"at LOCALIZERPOINT route continues to runway");
	Check(BuildRoute(14.0, 0.0, 0.0, 0.0, 0.0, 0.0, route) && route.FinalNm == 0.0,
		"point at threshold does not cause arc-related rejection");
	Check(!BuildRoute(std::numeric_limits<double>::quiet_NaN(), 5.0, 0.0, 180.0, 0.0, 14.0, route),
		"invalid coordinates are not modeled");
	const SpeedProfile speed{ 220.0, 220.0, 170.0, 130.0, 5.0, 999.0 };
	const SpeedProfile leaderSpeed{ 200.0, 200.0, 170.0, 150.0, 5.0, 999.0 };
	TurnRoute reference;
	Check(BuildRoute(12.0, 5.0, 0.0, speed.CurrentKts, 3.0, 18.0, reference), "future direct route");
	const ItdTarget target{ reference.BeforeFinalNm + reference.FinalNm +
		DistanceFlownNm(10.0, 18.0, leaderSpeed, 3.0 / speed.CurrentKts), 10.0, 18.0, leaderSpeed, 0.0 };
	TurnRoute earliest;
	Check(FindEarliestItdTurnPoint(12.0, 5.0, 0.0, 18.0, speed, target, earliest), "earliest direct-to-point recommendation");
	Check(std::abs(earliest.StartDistanceNm - 3.0) < 1.0e-5 && ItdDeliveryErrorNm(earliest, speed, target) >= 0.0,
		"earliest point reaches required ITD spacing without arc assumptions");
	ItdTarget largerTarget = target;
	largerTarget.SlotDistanceNm += 2.0;
	TurnRoute later;
	Check(FindEarliestItdTurnPoint(12.0, 5.0, 0.0, 18.0, speed, largerTarget, later) &&
		later.StartDistanceNm > earliest.StartDistanceNm, "larger ITD moves earliest point later");
	const SpeedProfile slowerLeader{ 120.0, 120.0, 120.0, 120.0, 5.0, 999.0 };
	TurnRoute inwardReference;
	Check(BuildRoute(30.0, 5.0, Pi, 220.0, 3.0, 18.0, inwardReference), "inward direct route");
	const ItdTarget permittedTarget{ inwardReference.BeforeFinalNm + inwardReference.FinalNm +
		DistanceFlownNm(50.0, 18.0, slowerLeader, 3.0 / speed.CurrentKts), 50.0, 18.0, slowerLeader, 0.0 };
	Check(FindEarliestItdTurnPoint(30.0, 5.0, Pi, 18.0, speed, permittedTarget, earliest) &&
		earliest.StartDistanceNm == 0.0, "already permitted turn is shown now instead of waiting for exact match");
	Check(FindEarliestItdTurnPoint(30.0, 5.0, Pi, 18.0, speed, permittedTarget, earliest, 0.1) &&
		earliest.StartDistanceNm >= 0.1, "earliest point respects report-age travel");
	ItdTarget impossible = target;
	impossible.SlotDistanceNm = 10000.0;
	Check(!FindEarliestItdTurnPoint(12.0, 5.0, 0.0, 18.0, speed, impossible, earliest),
		"insufficient ITD spacing never produces a recommendation");
	ItdTarget agedTarget = target;
	agedTarget.ReportOffsetHours = 2.0 / 3600.0;
	agedTarget.SlotDistanceNm = reference.BeforeFinalNm + reference.FinalNm +
		DistanceFlownNm(10.0, 18.0, leaderSpeed, 3.0 / speed.CurrentKts + agedTarget.ReportOffsetHours);
	Check(FindEarliestItdTurnPoint(12.0, 5.0, 0.0, 18.0, speed, agedTarget, earliest) &&
		std::abs(earliest.StartDistanceNm - 3.0) < 1.0e-5, "ITD report-age correction remains consistent");
	const double horizon = (std::min)(40.0, speed.CurrentKts / 6.0);
	TurnRoute horizonReference;
	Check(BuildRoute(12.0, 5.0, 0.0, speed.CurrentKts, horizon, 18.0, horizonReference), "route at search horizon");
	ItdTarget horizonTarget = target;
	horizonTarget.SlotDistanceNm = horizonReference.BeforeFinalNm + horizonReference.FinalNm +
		DistanceFlownNm(10.0, 18.0, leaderSpeed, horizon / speed.CurrentKts);
	Check(FindEarliestItdTurnPoint(12.0, 5.0, 0.0, 18.0, speed, horizonTarget, earliest) &&
		std::abs(earliest.StartDistanceNm - horizon) < 1.0e-5, "exact search horizon is included");
	int checkedRoutes = 0;
	for (int headingDegrees = -180; headingDegrees <= 180; headingDegrees += 15) {
		for (int side = -1; side <= 1; side += 2) {
			const double heading = headingDegrees * Pi / 180.0;
			Check(BuildRoute(12.0, side * 5.0, heading, 350.0, 2.0, 18.0, route),
				"opposite and acute headings never reject direct route geometry");
			const double expectedDistance = std::hypot(18.0 - 12.0 - 2.0 * std::cos(heading),
				side * 5.0 + 2.0 * std::sin(heading));
			Check(std::abs(route.BeforeFinalNm - expectedDistance) < 1.0e-9 && route.FinalNm == 18.0,
				"all headings use the same two straight legs");
			++checkedRoutes;
		}
	}
	const SpeedProfile phases{ 220.0, 180.0, 170.0, 130.0, 5.0, 20.0 };
	Check(std::abs(TimeToThresholdHours(10.0, 15.0, phases) -
		(5.0 / 220.0 + 5.0 / 180.0 + 10.0 / 170.0 + 5.0 / 130.0)) < 1.0e-9, "configured speed phases remain unchanged");
	Check(DistanceFlownNm(10.0, 15.0, phases, 10.0) == 25.0, "leader movement stops at threshold");
	std::cout << "All direct-route, chevron-reference, earliest ITD, and speed-phase checks passed across "
		<< checkedRoutes << " heading/side combinations.\n";
}