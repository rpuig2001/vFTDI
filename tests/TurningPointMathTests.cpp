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
	TurnRoute loopCheck;
	loopCheck.BeforeFinalNm = 10.0;
	loopCheck.FinalNm = 12.0;
	loopCheck.FirstTurnRadians = Pi;
	loopCheck.FinalTurnRadians = Pi;
	Check(IsPlausibleTurnRoute(loopCheck), "turns up to 180 degrees are within the modeled limit");
	loopCheck.FirstTurnRadians = Pi + 0.01;
	Check(!IsPlausibleTurnRoute(loopCheck), "a loop over 180 degrees is not a usable planning route");
	loopCheck.FirstTurnRadians = 0.0;
	loopCheck.FinalTurnRadians = 2.0 * Pi - 0.01;
	Check(!IsPlausibleTurnRoute(loopCheck), "a near-full-circle final turn is not a usable planning route");
	bool foundGeneratedLoop = false;
	for (double outward : { 10.0, 12.0, 14.0, 16.0, 20.0 }) {
		for (double crossTrack : { -5.0, -2.0, 0.0, 2.0, 5.0 }) {
			for (int headingDegrees = -180; headingDegrees <= 180; headingDegrees += 30) {
				TurnRoute candidate;
				if (BuildCurvedRoute(outward, crossTrack, headingDegrees * Pi / 180.0, 0.0, 14.0, candidate, 220.0) &&
					(candidate.FirstTurnRadians > Pi || candidate.FinalTurnRadians > Pi)) {
					TurnRoute fallback;
					Check(!IsPlausibleTurnRoute(candidate) &&
						BuildRoute(outward, crossTrack, headingDegrees * Pi / 180.0, 220.0, 0.0, 14.0, fallback) &&
						fallback.StraightFallback && std::abs(fallback.BeforeFinalNm - fallback.DirectDistanceNm) < 1.0e-9 &&
						std::abs(RouteCurvePoint(fallback, 0.5).OutwardNm - (outward + 14.0) / 2.0) < 1.0e-9 &&
						std::abs(RouteCurvePoint(fallback, 0.5).CrossTrackNm - crossTrack / 2.0) < 1.0e-9,
						"a generated looping route uses a straight path and consistent distance");
					foundGeneratedLoop = true;
				}
			}
		}
	}
	Check(foundGeneratedLoop, "representative localizer geometries include a generated looping route");
	Check(std::abs(GroundSpeedForTrackKts(200.0, 0.0, 0.0, 20.0) - 180.0) < 1.0e-9 &&
		std::abs(GroundSpeedForTrackKts(200.0, 0.0, 180.0, 20.0) - 220.0) < 1.0e-9,
		"headwind and tailwind adjust the modeled groundspeed");
	Check(std::abs(GroundSpeedForTrackKts(200.0, 0.0, 90.0, 40.0) - std::sqrt(200.0 * 200.0 - 40.0 * 40.0)) < 1.0e-9 &&
		std::abs(GroundSpeedForTrackKts(200.0, 0.0, 270.0, 200.0)) < 1.0e-9 &&
		GroundSpeedForTrackKts(200.0, 0.0, 270.0, 201.0) == 0.0,
		"crosswind correction accounts for track holding and rejects impossible winds");
	Check(GroundSpeedForTrackKts(200.0, 0.0, 0.0, 0.0) == 200.0 &&
		GroundSpeedForTrackKts(200.0, 0.0, 0.0, -1.0) == 0.0,
		"calm and invalid wind inputs are handled explicitly");
	const double dependentAlongTrackNm = LongitudinalComponentForDirectDistanceNm(5.0, 3.0);
	Check(std::abs(std::hypot(dependentAlongTrackNm, 3.0) - 5.0) < 1.0e-9,
		"parallel dependent target uses the configured direct distance");
	Check(LongitudinalComponentForDirectDistanceNm(5.0, 6.0) == 0.0,
		"cross-track distance alone can satisfy a smaller direct separation minimum");
	const double dependentFloorNm = DependentTargetFloorNm(12.0, 1.0, 2.0);
	Check(std::abs(std::hypot(dependentFloorNm - 12.0, 1.0) - 2.0) < 1.0e-9,
		"dependent FTD floor stays the required direct distance behind the upstream ITD marker");
	Check(DependentTargetFloorNm(12.0, -3.0, 2.0) == 12.0 &&
		DependentTargetFloorNm(12.0, -1.0, 3.0) > dependentFloorNm,
		"dependent floors account for marker cross-track distance and increased ITD spacing");
	const double expectedPositionFloorNm = DependentTargetFloorNm(20.0, 0.8, 2.0);
	Check(expectedPositionFloorNm > 20.0 &&
		std::abs(std::hypot(expectedPositionFloorNm - 20.0, 0.8) - 2.0) < 1.0e-9 &&
		DependentTargetFloorNm(20.0, 10.8, 2.0) == 20.0,
		"cleared dependent spacing uses the expected marker's localizer offset, not the distant aircraft offset");
	const double expectedFtdNm = DependentTargetFloorNm(20.0, 0.8, 2.0);
	const double expectedItdNm = DependentTargetFloorNm(20.0, 0.8, 2.5);
	Check(std::hypot(expectedFtdNm - 20.0, 0.8) >= 2.0 - 1.0e-9 &&
		std::hypot(expectedItdNm - 20.0, 0.8) >= 2.5 - 1.0e-9 && expectedItdNm >= expectedFtdNm,
		"dependent FTD and ITD respect spacing from the expected marker with compression");
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
	Check(BuildCurvedRoute(20.0, 0.0, 0.0, 0.0, 14.0, oppositeRoute, 220.0) &&
		std::abs(RouteCurvePoint(oppositeRoute, 0.5).CrossTrackNm) > 0.5 &&
		oppositeRoute.BeforeFinalNm > oppositeRoute.DirectDistanceNm,
		"opposite-direction aircraft turns visibly instead of retracing the same line");
	Check(BuildCurvedRoute(10.0, 0.0, 0.0, 0.0, 14.0, oppositeRoute, 220.0) &&
		std::abs(RouteCurvePoint(oppositeRoute, 0.5).CrossTrackNm) > 0.5 &&
		oppositeRoute.BeforeFinalNm > oppositeRoute.DirectDistanceNm,
		"opposite-direction join bends visibly before reversing onto final");
	TurnRoute nearCenterline;
	Check(BuildCurvedRoute(20.0, -0.02, 0.0, 0.0, 14.0, nearCenterline, 220.0) &&
		RouteCurvePoint(nearCenterline, 0.5).CrossTrackNm > 0.5 &&
		BuildCurvedRoute(20.0, 0.02, 0.0, 0.0, 14.0, nearCenterline, 220.0) &&
		RouteCurvePoint(nearCenterline, 0.5).CrossTrackNm > 0.5,
		"small centerline position changes do not flip the reversal turn");
	for (int side : { -1, 1 }) {
		TurnRoute longReversal;
		Check(BuildCurvedRoute(-35.0, side * 20.0, 0.0, 0.0, 14.0, longReversal, 220.0),
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
	const ItdTarget straightTarget{ 0.0, 0.0, 14.0, leaderSpeed, 0.0 };
	TurnRoute straightRecommendation;
	Check(FindEarliestItdTurnPoint(20.0, 0.0, 0.0, 14.0, speed, straightTarget, straightRecommendation) &&
		straightRecommendation.StraightFallback && straightRecommendation.StartDistanceNm == 0.0 &&
		std::abs(straightRecommendation.BeforeFinalNm - 6.0) < 1.0e-9,
		"turn timing uses the straight fallback instead of suppressing a looping route");
	TurnRoute reference;
	Check(BuildRoute(12.0, 5.0, 0.0, speed.CurrentKts, 3.0, 18.0, reference), "future direct route");
	const ItdTarget target{ reference.BeforeFinalNm + reference.FinalNm +
		DistanceFlownNm(10.0, 18.0, leaderSpeed, 3.0 / speed.CurrentKts), 10.0, 18.0, leaderSpeed, 0.0 };
	TurnRoute earliest;
	Check(FindEarliestItdTurnPoint(12.0, 5.0, 0.0, 18.0, speed, target, earliest), "earliest direct-to-point recommendation");
	Check(std::abs(earliest.StartDistanceNm - 3.0) < 1.0e-5 && ItdDeliveryErrorNm(earliest, speed, target) >= 0.0,
		"earliest point reaches required ITD spacing using the curved route");
	Check(IsPlausibleTurnRoute(earliest), "turn recommendations do not use looping routes");
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
	const SpeedProfile delayedReduction{ 240.0, 220.0, 170.0, 130.0, 5.0, 999.0, 12.0 };
	Check(CurrentPhaseSpeedKts(4.0, 14.0, delayedReduction) == 240.0 &&
		CurrentPhaseSpeedKts(0.0, 12.0, delayedReduction) == 170.0 &&
		CurrentPhaseSpeedKts(0.0, 5.0, delayedReduction) == 130.0,
		"current modeled speed follows the cleared speed boundaries");
	const SpeedProfile phaseSpeeds{ 240.0, 220.0, 170.0, 130.0, 5.0, 20.0 };
	Check(CurrentPhaseSpeedKts(10.0, 14.0, phaseSpeeds) == 240.0 &&
		CurrentPhaseSpeedKts(6.0, 14.0, phaseSpeeds) == 220.0 &&
		CurrentPhaseSpeedKts(0.0, 14.0, phaseSpeeds) == 170.0 &&
		CurrentPhaseSpeedKts(0.0, 5.0, phaseSpeeds) == 130.0 &&
		CurrentPhaseSpeedKts(-1.0, 5.0, phaseSpeeds) < 0.0,
		"current modeled speed follows each route phase and rejects invalid geometry");
	Check(ClearedSpeedBoundaryNm(true, false, 12.0, 5.0) == 12.0 &&
		ClearedSpeedBoundaryNm(true, true, 12.0, 5.0) == 0.0 &&
		ClearedSpeedBoundaryNm(false, false, 12.0, 5.0) == 0.0,
		"delayed reduction applies only to cleared aircraft when enabled by configuration");
	Check(std::abs(TimeToThresholdHours(0.0, 20.0, delayedReduction) -
		(8.0 / 240.0 + 7.0 / 170.0 + 5.0 / 130.0)) < 1.0e-9,
		"cleared aircraft retains actual GS until localizer distance despite long-approach settings");
	Check(std::abs(DistanceFlownNm(0.0, 20.0, delayedReduction, 4.0 / 240.0) - 4.0) < 1.0e-9 &&
		std::abs(DistanceFlownNm(0.0, 20.0, delayedReduction, 8.0 / 240.0 + 2.0 / 170.0) - 10.0) < 1.0e-9,
		"distance prediction transitions from observed GS to configured approach speed at the boundary");
	Check(std::abs(TimeToThresholdHours(0.0, 10.0, delayedReduction) -
		(5.0 / 170.0 + 5.0 / 130.0)) < 1.0e-9 &&
		std::abs(TimeToThresholdHours(0.0, 4.0, delayedReduction) - 4.0 / 130.0) < 1.0e-9,
		"inside localizer and reduction boundaries the appropriate reduced speed applies immediately");
	Check(std::abs(TimeToThresholdHours(0.0, 12.0, delayedReduction) -
		(7.0 / 170.0 + 5.0 / 130.0)) < 1.0e-9 &&
		std::abs(TimeToThresholdHours(0.0, 5.0, delayedReduction) - 5.0 / 130.0) < 1.0e-9,
		"exact localizer and final-speed boundaries enter the next speed phase");
	SpeedProfile shortLocalizer = delayedReduction;
	shortLocalizer.ActualSpeedUntilNm = ClearedSpeedBoundaryNm(true, false, 3.0, 5.0);
	Check(std::abs(TimeToThresholdHours(0.0, 10.0, shortLocalizer) -
		(5.0 / 240.0 + 5.0 / 130.0)) < 1.0e-9,
		"final-speed reduction boundary takes precedence over a closer localizer point");
	SpeedProfile immediateReduction = delayedReduction;
	immediateReduction.ActualSpeedUntilNm = 0.0;
	Check(std::abs(TimeToThresholdHours(0.0, 20.0, immediateReduction) -
		(15.0 / 170.0 + 5.0 / 130.0)) < 1.0e-9,
		"immediate-clearance reduction preserves the previous speed model");
	SpeedProfile withWind = delayedReduction;
	withWind.BeforeFinalKts = 195.0;
	withWind.CruiseKts = 145.0;
	withWind.FinalKts = 105.0;
	Check(std::abs(DistanceFlownNm(0.0, 20.0, withWind, 4.0 / 240.0) - 4.0) < 1.0e-9,
		"wind-adjusted configured speeds do not alter the observed-GS phase");
	const SpeedProfile phases{ 220.0, 180.0, 170.0, 130.0, 5.0, 20.0 };
	Check(std::abs(TimeToThresholdHours(10.0, 15.0, phases) -
		(5.0 / 220.0 + 5.0 / 180.0 + 10.0 / 170.0 + 5.0 / 130.0)) < 1.0e-9, "configured speed phases remain unchanged");
	Check(DistanceFlownNm(10.0, 15.0, phases, 10.0) == 25.0, "leader movement stops at threshold");
	std::cout << "All curved-route, chevron-reference, earliest ITD, and speed-phase checks passed across "
		<< checkedRoutes << " heading/side combinations.\n";
}