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
	Check(BuildRoute(12.0, 5.0, 0.0, 0.0, 0.0, 14.0, route), "missing groundspeed uses a nominal turn radius");
	Check(route.BeforeFinalNm > std::hypot(2.0, 5.0) && route.FinalNm == 14.0,
		"turning adds distance before the final leg");
	Check(RouteCurvePoint(route, 0.0).OutwardNm == 12.0 && RouteCurvePoint(route, 0.0).CrossTrackNm == 5.0 &&
		RouteCurvePoint(route, 1.0).OutwardNm == 14.0 && RouteCurvePoint(route, 1.0).CrossTrackNm == 0.0,
		"route curve connects the aircraft and LOCALIZERPOINT");
	Check(route.RadiusNm > 0.0 && route.FirstTurnRadians < 2.0 * Pi && route.FinalTurnRadians < 2.0 * Pi,
		"route uses finite circular turns");
	const RoutePoint firstArcMiddle = RouteCurvePoint(route, 1.0 / 6.0);
	Check(std::abs(std::hypot(firstArcMiddle.OutwardNm - route.FirstTurnCenter.OutwardNm,
		firstArcMiddle.CrossTrackNm - route.FirstTurnCenter.CrossTrackNm) - route.RadiusNm) < 1.0e-9,
		"initial turn follows its constant-radius circle");
	const RoutePoint finalArcMiddle = RouteCurvePoint(route, 5.0 / 6.0);
	Check(std::abs(std::hypot(finalArcMiddle.OutwardNm - route.FinalTurnCenter.OutwardNm,
		finalArcMiddle.CrossTrackNm - route.FinalTurnCenter.CrossTrackNm) - route.RadiusNm) < 1.0e-9,
		"final turn follows its constant-radius circle");
	Check(std::abs(route.BeforeFinalNm - (route.RadiusNm * (route.FirstTurnRadians + route.FinalTurnRadians) +
		route.StraightDistanceNm)) < 1.0e-9, "modeled distance includes exact arc and straight lengths");
	TurnRoute fasterRoute;
	Check(BuildRoute(12.0, 5.0, 0.0, 220.0, 0.0, 14.0, fasterRoute) &&
		std::abs(fasterRoute.RadiusNm / route.RadiusNm - 220.0 * 220.0 / (180.0 * 180.0)) < 1.0e-9,
		"faster groundspeed increases radius for the same nominal bank angle");
	double sampledDistanceNm = 0.0;
	RoutePoint previousPoint = RouteCurvePoint(route, 0.0);
	for (int segment = 1; segment <= RouteCurveSegments; ++segment) {
		const RoutePoint point = RouteCurvePoint(route, static_cast<double>(segment) / RouteCurveSegments);
		sampledDistanceNm += std::hypot(point.OutwardNm - previousPoint.OutwardNm,
			point.CrossTrackNm - previousPoint.CrossTrackNm);
		previousPoint = point;
	}
	Check(sampledDistanceNm <= route.BeforeFinalNm + 1.0e-9 &&
		route.BeforeFinalNm - sampledDistanceNm < 0.1,
		"sampled drawing approximates the analytic arc lengths");
	TurnRoute straightRoute;
	Check(BuildRoute(20.0, 0.0, Pi, 0.0, 0.0, 14.0, straightRoute) &&
		std::abs(straightRoute.BeforeFinalNm - 6.0) < 1.0e-9,
		"an aircraft aligned with final adds no turn distance");
	TurnRoute oppositeRoute;
	Check(BuildRoute(20.0, 0.0, 0.0, 220.0, 0.0, 14.0, oppositeRoute) &&
		std::abs(RouteCurvePoint(oppositeRoute, 0.5).CrossTrackNm) > 0.5 &&
		oppositeRoute.BeforeFinalNm > oppositeRoute.DirectDistanceNm,
		"opposite-direction aircraft turns visibly instead of retracing the same line");
	Check(BuildRoute(10.0, 0.0, 0.0, 220.0, 0.0, 14.0, oppositeRoute) &&
		std::abs(RouteCurvePoint(oppositeRoute, 0.5).CrossTrackNm) > 0.5 &&
		oppositeRoute.BeforeFinalNm > oppositeRoute.DirectDistanceNm,
		"opposite-direction join bends visibly before reversing onto final");
	TurnRoute nearCenterline;
	Check(BuildRoute(20.0, -0.02, 0.0, 220.0, 0.0, 14.0, nearCenterline) &&
		RouteCurvePoint(nearCenterline, 0.5).CrossTrackNm > 0.5 &&
		BuildRoute(20.0, 0.02, 0.0, 220.0, 0.0, 14.0, nearCenterline) &&
		RouteCurvePoint(nearCenterline, 0.5).CrossTrackNm > 0.5,
		"small centerline position changes do not flip the reversal turn");
	for (int side : { -1, 1 }) {
		TurnRoute longReversal;
		Check(BuildRoute(-35.0, side * 20.0, 0.0, 220.0, 0.0, 14.0, longReversal),
			"long opposite-side approach builds a route");
		const RoutePoint lastSample = RouteCurvePoint(longReversal,
			static_cast<double>(RouteCurveSegments - 1) / RouteCurveSegments);
		const double finalSegmentAngle = std::atan2(std::abs(lastSample.CrossTrackNm),
			lastSample.OutwardNm - 14.0);
		Check(finalSegmentAngle < 10.0 * Pi / 180.0,
			"drawn curve joins final inbound without a sharp reversal on either side");
	}
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
	Check(BuildRoute(12.0, 5.0, 0.0, 180.0, 0.0, 14.0, route) &&
		std::abs(route.BeforeFinalNm + route.FinalNm - unclearedDistanceNm) < 1.0e-9,
		"removing CA/VA restores route through LOCALIZERPOINT");
	Check(BuildRoute(12.0, -5.0, 0.0, 180.0, 0.0, 14.0, route) && route.TurnAngleRadians > 0.0 &&
		std::abs(route.BeforeFinalNm + route.FinalNm - unclearedDistanceNm) < 1.0e-9,
		"opposite-side turn has mirrored geometry at the same groundspeed");
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
		"earliest point reaches required ITD spacing using the curved route");
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
			Check(route.BeforeFinalNm + 1.0e-9 >= expectedDistance && route.FinalNm == 18.0,
				"curved route is at least as long as the direct route");
			const RoutePoint firstStep = RouteCurvePoint(route, 1.0e-5);
			if (route.FirstTurnRadians > 1.0e-6)
				Check(std::abs(SignedAngle(std::atan2(firstStep.CrossTrackNm - route.CurveStart.CrossTrackNm,
					firstStep.OutwardNm - route.CurveStart.OutwardNm) - heading)) < 0.001,
					"initial arc starts tangent to the aircraft track");
			const RoutePoint lastStep = RouteCurvePoint(route, 1.0 - 1.0e-5);
			if (route.FinalTurnRadians > 1.0e-6)
				Check(std::abs(SignedAngle(std::atan2(-lastStep.CrossTrackNm,
					18.0 - lastStep.OutwardNm) - Pi)) < 0.001,
					"final arc ends tangent to the inbound localizer");
			for (double boundary : { 1.0 / 3.0, 2.0 / 3.0 }) {
				const RoutePoint before = RouteCurvePoint(route, boundary - 1.0e-7);
				const RoutePoint after = RouteCurvePoint(route, boundary + 1.0e-7);
				Check(std::hypot(after.OutwardNm - before.OutwardNm, after.CrossTrackNm - before.CrossTrackNm) < 0.001,
					"arc and straight segments connect without gaps");
			}
			++checkedRoutes;
		}
	}
	const SpeedProfile phases{ 220.0, 180.0, 170.0, 130.0, 5.0, 20.0 };
	Check(std::abs(TimeToThresholdHours(10.0, 15.0, phases) -
		(5.0 / 220.0 + 5.0 / 180.0 + 10.0 / 170.0 + 5.0 / 130.0)) < 1.0e-9, "configured speed phases remain unchanged");
	Check(DistanceFlownNm(10.0, 15.0, phases, 10.0) == 25.0, "leader movement stops at threshold");
	std::cout << "All curved-route, chevron-reference, earliest ITD, and speed-phase checks passed across "
		<< checkedRoutes << " heading/side combinations.\n";
}