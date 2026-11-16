#pragma once

#include <cmath>
#include <algorithm>
#include <initializer_list>

namespace TurningPointMath {
	constexpr double Pi = 3.14159265358979323846;

	inline double GroundSpeedForTrackKts(double airspeedKts, double trackDegrees,
		double windFromDegrees, double windSpeedKts)
	{
		if (!std::isfinite(airspeedKts) || !std::isfinite(trackDegrees) ||
			!std::isfinite(windFromDegrees) || !std::isfinite(windSpeedKts) ||
			airspeedKts <= 0.0 || windSpeedKts < 0.0)
			return 0.0;
		const double windAngle = (windFromDegrees - trackDegrees) * Pi / 180.0;
		const double crosswindKts = windSpeedKts * std::sin(windAngle);
		if (std::abs(crosswindKts) > airspeedKts)
			return 0.0;
		const double alongAirKts = std::sqrt(airspeedKts * airspeedKts - crosswindKts * crosswindKts);
		return (std::max)(0.0, alongAirKts - windSpeedKts * std::cos(windAngle));
	}

	inline double RouteDistanceOnLocalizerNm(double remainingRouteNm, double aircraftAlongOwnLocalizerNm,
		double aircraftAlongTargetLocalizerNm)
	{
		return remainingRouteNm + aircraftAlongTargetLocalizerNm - aircraftAlongOwnLocalizerNm;
	}

	inline double MarkerDistanceNm(double remainingRouteNm, double leaderAlongOwnLocalizerNm,
		double leaderAlongLocalizerNm, double spacingNm)
	{
		const double commonRemainingNm = RouteDistanceOnLocalizerNm(remainingRouteNm,
			leaderAlongOwnLocalizerNm, leaderAlongLocalizerNm);
		const double referenceNm = (std::max)(0.0, (std::max)(commonRemainingNm, leaderAlongLocalizerNm));
		return referenceNm + (std::max)(0.0, spacingNm);
	}

	inline double LongitudinalComponentForDirectDistanceNm(double directDistanceNm, double crossTrackNm)
	{
		if (!std::isfinite(directDistanceNm) || !std::isfinite(crossTrackNm) || directDistanceNm <= 0.0)
			return 0.0;
		const double lateralDistanceNm = std::abs(crossTrackNm);
		if (lateralDistanceNm >= directDistanceNm)
			return 0.0;
		return std::sqrt(directDistanceNm * directDistanceNm - lateralDistanceNm * lateralDistanceNm);
	}

	inline double DependentTargetFloorNm(double referenceAlongNm, double referenceCrossTrackNm, double directDistanceNm)
	{
		return referenceAlongNm + LongitudinalComponentForDirectDistanceNm(directDistanceNm, referenceCrossTrackNm);
	}

	inline double DownstreamReferenceNm(double expectedAlongLocalizerNm, double ftdAlongLocalizerNm,
		bool clearedForApproach = false)
	{
		const double referenceNm = clearedForApproach || expectedAlongLocalizerNm > ftdAlongLocalizerNm
			? expectedAlongLocalizerNm : ftdAlongLocalizerNm;
		return referenceNm > 0.0 ? referenceNm : 0.0;
	}

	struct SpeedProfile {
		double CurrentKts;
		double BeforeFinalKts;
		double CruiseKts;
		double FinalKts;
		double ReductionNm;
		double LongApproachNm;
		double ActualSpeedUntilNm = 0.0;
	};

	inline double ClearedSpeedBoundaryNm(bool clearedForApproach, bool reduceOnClearance,
		double localizerPointNm, double reductionNm)
	{
		return clearedForApproach && !reduceOnClearance ? (std::max)(localizerPointNm, reductionNm) : 0.0;
	}

	inline void SplitActualSpeedLegs(double& beforeFinalNm, double& finalNm,
		double actualSpeedUntilNm, double reductionNm)
	{
		if (actualSpeedUntilNm <= 0.0)
			return;
		const double totalNm = beforeFinalNm + finalNm;
		finalNm = (std::min)(totalNm, (std::max)(actualSpeedUntilNm, reductionNm));
		beforeFinalNm = totalNm - finalNm;
	}

	struct ItdTarget {
		double SlotDistanceNm;
		double LeaderBeforeFinalNm;
		double LeaderFinalNm;
		SpeedProfile LeaderSpeed;
		double ReportOffsetHours = 0.0;
	};

	struct RoutePoint {
		double OutwardNm = 0.0;
		double CrossTrackNm = 0.0;
	};

	constexpr int RouteCurveSegments = 96;

	struct TurnRoute {
		double StartDistanceNm = 0.0;
		double BeforeFinalNm = 0.0;
		double FinalNm = 0.0;
		double TurnAngleRadians = 0.0;
		double LocalizerPointNm = 0.0;
		double DirectDistanceNm = 0.0;
		double TimingExcessSeconds = 0.0;
		double ItdExcessNm = 0.0;
		RoutePoint CurveStart;
		RoutePoint CurveEnd;
		RoutePoint FirstTurnCenter;
		RoutePoint FinalTurnCenter;
		double RadiusNm = 0.0;
		double InitialTrackRadians = 0.0;
		double StraightTrackRadians = 0.0;
		double FirstTurnRadians = 0.0;
		double FinalTurnRadians = 0.0;
		double StraightDistanceNm = 0.0;
		int FirstTurnDirection = 1;
		int FinalTurnDirection = 1;
		bool StraightFallback = false;
	};

	inline bool IsPlausibleTurnRoute(const TurnRoute& route)
	{
		return std::isfinite(route.BeforeFinalNm) && std::isfinite(route.FinalNm) &&
			std::isfinite(route.FirstTurnRadians) && std::isfinite(route.FinalTurnRadians) &&
			route.BeforeFinalNm >= 0.0 && route.FinalNm >= 0.0 &&
			route.FirstTurnRadians >= 0.0 && route.FirstTurnRadians <= Pi &&
			route.FinalTurnRadians >= 0.0 && route.FinalTurnRadians <= Pi;
	}

	inline RoutePoint RouteCurvePoint(const TurnRoute& route, double fraction)
	{
		if (route.StraightFallback) {
			const double position = (std::max)(0.0, (std::min)(1.0, fraction));
			return { route.CurveStart.OutwardNm + (route.CurveEnd.OutwardNm - route.CurveStart.OutwardNm) * position,
				route.CurveStart.CrossTrackNm + (route.CurveEnd.CrossTrackNm - route.CurveStart.CrossTrackNm) * position };
		}
		if (fraction <= 0.0 || route.BeforeFinalNm == 0.0)
			return route.CurveStart;
		if (fraction >= 1.0)
			return route.CurveEnd;
		const double phase = fraction * 3.0;
		if (phase < 1.0) {
			const double heading = route.InitialTrackRadians + route.FirstTurnDirection * phase * route.FirstTurnRadians;
			return { route.FirstTurnCenter.OutwardNm + route.FirstTurnDirection * route.RadiusNm * std::sin(heading),
				route.FirstTurnCenter.CrossTrackNm - route.FirstTurnDirection * route.RadiusNm * std::cos(heading) };
		}
		const RoutePoint straightStart = {
			route.FirstTurnCenter.OutwardNm + route.FirstTurnDirection * route.RadiusNm * std::sin(route.StraightTrackRadians),
			route.FirstTurnCenter.CrossTrackNm - route.FirstTurnDirection * route.RadiusNm * std::cos(route.StraightTrackRadians) };
		if (phase < 2.0)
			return { straightStart.OutwardNm + (phase - 1.0) * route.StraightDistanceNm * std::cos(route.StraightTrackRadians),
				straightStart.CrossTrackNm + (phase - 1.0) * route.StraightDistanceNm * std::sin(route.StraightTrackRadians) };
		const double heading = route.StraightTrackRadians + route.FinalTurnDirection * (phase - 2.0) * route.FinalTurnRadians;
		return { route.FinalTurnCenter.OutwardNm + route.FinalTurnDirection * route.RadiusNm * std::sin(heading),
			route.FinalTurnCenter.CrossTrackNm - route.FinalTurnDirection * route.RadiusNm * std::cos(heading) };
	}


	inline double SignedAngle(double radians)
	{
		return std::atan2(std::sin(radians), std::cos(radians));
	}

	inline bool BuildDirectRoute(double outwardNm, double crossTrackNm, double relativeTrackRadians,
		double startDistanceNm, double localizerPointNm, TurnRoute& route)
	{
		if (!std::isfinite(outwardNm) || !std::isfinite(crossTrackNm) || !std::isfinite(relativeTrackRadians) ||
			!std::isfinite(startDistanceNm) || !std::isfinite(localizerPointNm) || localizerPointNm < 0.0)
			return false;
		const double startOutwardNm = outwardNm + startDistanceNm * std::cos(relativeTrackRadians);
		const double startCrossTrackNm = crossTrackNm + startDistanceNm * std::sin(relativeTrackRadians);
		route = TurnRoute{};
		route.StartDistanceNm = startDistanceNm;
		route.BeforeFinalNm = std::hypot(localizerPointNm - startOutwardNm, startCrossTrackNm);
		route.FinalNm = localizerPointNm;
		route.LocalizerPointNm = localizerPointNm;
		route.DirectDistanceNm = route.BeforeFinalNm;
		route.TurnAngleRadians = route.BeforeFinalNm > 1.0e-9
			? SignedAngle(std::atan2(-startCrossTrackNm, localizerPointNm - startOutwardNm) - relativeTrackRadians) : 0.0;
		return true;
	}

	inline bool BuildCurvedRoute(double outwardNm, double crossTrackNm, double relativeTrackRadians,
		double startDistanceNm, double localizerPointNm, TurnRoute& route, double groundSpeedKts = 180.0)
	{
		if (!BuildDirectRoute(outwardNm, crossTrackNm, relativeTrackRadians, startDistanceNm, localizerPointNm, route))
			return false;
		const double speedMetersPerSecond = (std::isfinite(groundSpeedKts) && groundSpeedKts > 0.0
			? groundSpeedKts : 180.0) * 1852.0 / 3600.0;
		route.RadiusNm = speedMetersPerSecond * speedMetersPerSecond / (9.80665 * std::tan(25.0 * Pi / 180.0)) / 1852.0;
		route.InitialTrackRadians = relativeTrackRadians;
		route.CurveStart = { outwardNm + startDistanceNm * std::cos(relativeTrackRadians),
			crossTrackNm + startDistanceNm * std::sin(relativeTrackRadians) };
		route.CurveEnd = { localizerPointNm, 0.0 };
		if (route.DirectDistanceNm < 1.0e-9)
			return true;
		const auto positiveAngle = [](double angle) {
			const double normalized = std::fmod(angle + 4.0 * Pi, 2.0 * Pi);
			return normalized < 1.0e-9 || normalized > 2.0 * Pi - 1.0e-9 ? 0.0 : normalized;
		};
		double shortestNm = 1.0e100;
		const double tieToleranceNm = std::abs(route.CurveStart.CrossTrackNm) < 0.1 &&
			std::abs(std::sin(relativeTrackRadians)) < std::sin(5.0 * Pi / 180.0) ? 0.1 : 1.0e-8;
		for (int firstDirection : { 1, -1 }) {
			for (int finalDirection : { 1, -1 }) {
				const RoutePoint firstCenter = {
					route.CurveStart.OutwardNm - firstDirection * route.RadiusNm * std::sin(relativeTrackRadians),
					route.CurveStart.CrossTrackNm + firstDirection * route.RadiusNm * std::cos(relativeTrackRadians) };
				const RoutePoint finalCenter = { localizerPointNm, -finalDirection * route.RadiusNm };
				const double deltaOutward = finalCenter.OutwardNm - firstCenter.OutwardNm;
				const double deltaCross = finalCenter.CrossTrackNm - firstCenter.CrossTrackNm;
				const double centersDistanceNm = std::hypot(deltaOutward, deltaCross);
				const double offsetNm = (firstDirection - finalDirection) * route.RadiusNm;
				if (centersDistanceNm < 1.0e-9 || centersDistanceNm < std::abs(offsetNm))
					continue;
				const double straightHeading = std::atan2(deltaCross, deltaOutward) + std::asin(offsetNm / centersDistanceNm);
				const double firstAngle = positiveAngle(firstDirection * (straightHeading - relativeTrackRadians));
				const double finalAngle = positiveAngle(finalDirection * (Pi - straightHeading));
				const double straightNm = std::sqrt((std::max)(0.0,
					centersDistanceNm * centersDistanceNm - offsetNm * offsetNm));
				const double totalNm = route.RadiusNm * (firstAngle + finalAngle) + straightNm;
				if (totalNm >= shortestNm - tieToleranceNm)
					continue;
				shortestNm = totalNm;
				route.FirstTurnCenter = firstCenter;
				route.FinalTurnCenter = finalCenter;
				route.FirstTurnDirection = firstDirection;
				route.FinalTurnDirection = finalDirection;
				route.StraightTrackRadians = straightHeading;
				route.FirstTurnRadians = firstAngle;
				route.FinalTurnRadians = finalAngle;
				route.StraightDistanceNm = straightNm;
				route.TurnAngleRadians = firstDirection * firstAngle;
			}
		}
		route.BeforeFinalNm = shortestNm;
		return shortestNm < 1.0e99;
	}

	inline double TimeToThresholdHours(double beforeFinalNm, double finalNm,
		const SpeedProfile& speed)
	{
		if (speed.CurrentKts <= 0.0 || speed.BeforeFinalKts <= 0.0 ||
			speed.CruiseKts <= 0.0 || speed.FinalKts <= 0.0 ||
			beforeFinalNm < 0.0 || finalNm < 0.0 || speed.ReductionNm < 0.0)
			return -1.0;
		SplitActualSpeedLegs(beforeFinalNm, finalNm, speed.ActualSpeedUntilNm, speed.ReductionNm);
		double currentSpeedDistanceNm = beforeFinalNm;
		if (speed.LongApproachNm > 0.0 && speed.ActualSpeedUntilNm <= 0.0) {
			currentSpeedDistanceNm = beforeFinalNm + finalNm - speed.LongApproachNm;
			if (currentSpeedDistanceNm < 0.0)
				currentSpeedDistanceNm = 0.0;
			if (currentSpeedDistanceNm > beforeFinalNm)
				currentSpeedDistanceNm = beforeFinalNm;
		}
		const double cruiseDistanceNm = finalNm > speed.ReductionNm
			? finalNm - speed.ReductionNm : 0.0;
		return currentSpeedDistanceNm / speed.CurrentKts +
			(beforeFinalNm - currentSpeedDistanceNm) / speed.BeforeFinalKts +
			cruiseDistanceNm / speed.CruiseKts +
			(finalNm - cruiseDistanceNm) / speed.FinalKts;
	}

	inline double CurrentPhaseSpeedKts(double beforeFinalNm, double finalNm, const SpeedProfile& speed)
	{
		if (TimeToThresholdHours(beforeFinalNm, finalNm, speed) < 0.0)
			return -1.0;
		SplitActualSpeedLegs(beforeFinalNm, finalNm, speed.ActualSpeedUntilNm, speed.ReductionNm);
		const double currentDistanceNm = speed.LongApproachNm > 0.0 && speed.ActualSpeedUntilNm <= 0.0
			? (std::max)(0.0, (std::min)(beforeFinalNm, beforeFinalNm + finalNm - speed.LongApproachNm)) : beforeFinalNm;
		if (currentDistanceNm > 0.0)
			return speed.CurrentKts;
		if (beforeFinalNm > 0.0)
			return speed.BeforeFinalKts;
		return finalNm > speed.ReductionNm ? speed.CruiseKts : speed.FinalKts;
	}

	inline double DistanceFlownNm(double beforeFinalNm, double finalNm, const SpeedProfile& speed, double elapsedHours)
	{
		if (TimeToThresholdHours(beforeFinalNm, finalNm, speed) < 0.0 || !std::isfinite(elapsedHours))
			return 0.0;
		if (elapsedHours < 0.0)
			return elapsedHours * speed.CurrentKts;
		SplitActualSpeedLegs(beforeFinalNm, finalNm, speed.ActualSpeedUntilNm, speed.ReductionNm);
		const double currentDistanceNm = speed.LongApproachNm > 0.0 && speed.ActualSpeedUntilNm <= 0.0
			? (std::max)(0.0, (std::min)(beforeFinalNm, beforeFinalNm + finalNm - speed.LongApproachNm)) : beforeFinalNm;
		const double cruiseDistanceNm = (std::max)(0.0, finalNm - speed.ReductionNm);
		const double distances[] = { currentDistanceNm, beforeFinalNm - currentDistanceNm,
			cruiseDistanceNm, finalNm - cruiseDistanceNm };
		const double speeds[] = { speed.CurrentKts, speed.BeforeFinalKts, speed.CruiseKts, speed.FinalKts };
		double flownNm = 0.0;
		for (int phase = 0; phase < 4; ++phase) {
			const double phaseHours = distances[phase] / speeds[phase];
			if (elapsedHours <= phaseHours)
				return flownNm + elapsedHours * speeds[phase];
			flownNm += distances[phase];
			elapsedHours -= phaseHours;
		}
		return beforeFinalNm + finalNm;
	}

	inline double ItdDeliveryErrorNm(const TurnRoute& route, const SpeedProfile& followerSpeed, const ItdTarget& target)
	{
		const double elapsedHours = route.StartDistanceNm / followerSpeed.CurrentKts + target.ReportOffsetHours;
		const double slotAtTurnNm = target.SlotDistanceNm - DistanceFlownNm(target.LeaderBeforeFinalNm,
			target.LeaderFinalNm, target.LeaderSpeed, elapsedHours);
		const double errorNm = route.BeforeFinalNm + route.FinalNm - slotAtTurnNm;
		return std::abs(errorNm) < 1.0e-10 ? 0.0 : errorNm;
	}

	inline bool BuildRoute(double outwardNm, double crossTrackNm, double relativeTrackRadians,
		double groundSpeedKts, double startDistanceNm, double localizerPointNm, TurnRoute& route, bool clearedForApproach = false)
	{
		if (!clearedForApproach) {
			if (BuildCurvedRoute(outwardNm, crossTrackNm, relativeTrackRadians, startDistanceNm, localizerPointNm, route, groundSpeedKts) &&
				IsPlausibleTurnRoute(route))
				return true;
			if (!BuildDirectRoute(outwardNm, crossTrackNm, relativeTrackRadians, startDistanceNm, localizerPointNm, route))
				return false;
			route.CurveStart = { outwardNm + startDistanceNm * std::cos(relativeTrackRadians),
				crossTrackNm + startDistanceNm * std::sin(relativeTrackRadians) };
			route.CurveEnd = { localizerPointNm, 0.0 };
			route.StraightFallback = true;
			return true;
		}
		if (!BuildDirectRoute(outwardNm, crossTrackNm, relativeTrackRadians, startDistanceNm, 0.0, route))
			return false;
		route.FinalNm = route.BeforeFinalNm;
		route.BeforeFinalNm = 0.0;
		return true;
	}

	inline bool FindTurnPoint(double outwardNm, double crossTrackNm, double relativeTrackRadians,
		double localizerPointNm, const SpeedProfile& speed, double targetArrivalHours,
		TurnRoute& result, bool includePassed = false, bool allowLargerGap = false,
		double minimumFutureDistanceNm = 0.0, const ItdTarget* itdTarget = nullptr)
	{
		if (!std::isfinite(outwardNm) || !std::isfinite(crossTrackNm) ||
			!std::isfinite(relativeTrackRadians) ||
			!std::isfinite(localizerPointNm) || localizerPointNm <= 0.0 ||
			(itdTarget == nullptr && (!std::isfinite(targetArrivalHours) || targetArrivalHours <= 0.0)) ||
			(itdTarget != nullptr && (!std::isfinite(itdTarget->SlotDistanceNm) || itdTarget->SlotDistanceNm < 0.0 ||
				!std::isfinite(itdTarget->ReportOffsetHours) || TimeToThresholdHours(itdTarget->LeaderBeforeFinalNm,
					itdTarget->LeaderFinalNm, itdTarget->LeaderSpeed) < 0.0)) || speed.CurrentKts < 80.0 ||
			TimeToThresholdHours(0.0, 0.0, speed) < 0.0)
			return false;
		const double horizonNm = speed.CurrentKts / 6.0 < 40.0 ? speed.CurrentKts / 6.0 : 40.0;
		for (int pass = 0; pass < (includePassed ? 2 : 1); ++pass) {
			bool hasLaterArrival = false;
			double smallestExcessHours = 1.0e100;
			TurnRoute laterArrivalRoute;
			bool hasPrevious = false;
			double previousDistanceNm = 0.0;
			double previousError = 0.0;
			for (double stepNm = 0.0; stepNm <= horizonNm; stepNm += 0.25) {
				const double distanceNm = pass == 0 ? stepNm : -stepNm;
				TurnRoute candidate;
				if (!BuildRoute(outwardNm, crossTrackNm, relativeTrackRadians,
					speed.CurrentKts, distanceNm, localizerPointNm, candidate) || !IsPlausibleTurnRoute(candidate)) {
					hasPrevious = false;
					continue;
				}
				const double error = itdTarget != nullptr ? ItdDeliveryErrorNm(candidate, speed, *itdTarget)
					: distanceNm / speed.CurrentKts + TimeToThresholdHours(candidate.BeforeFinalNm, candidate.FinalNm, speed) - targetArrivalHours;
				if (std::abs(error) < 1.0e-8) {
					result = candidate;
					return true;
				}
				if (pass == 0 && allowLargerGap && distanceNm >= minimumFutureDistanceNm &&
					error > 0.0 && error < smallestExcessHours) {
					hasLaterArrival = true;
					smallestExcessHours = error;
					laterArrivalRoute = candidate;
					laterArrivalRoute.TimingExcessSeconds = itdTarget != nullptr ? error / speed.CurrentKts * 3600.0 : error * 3600.0;
					laterArrivalRoute.ItdExcessNm = itdTarget != nullptr ? error : 0.0;
				}
				if (hasPrevious && error * previousError < 0.0) {
					double lowerNm = previousDistanceNm;
					double upperNm = distanceNm;
					bool validBracket = true;
					for (int iteration = 0; iteration < 40; ++iteration) {
						const double midpointNm = (lowerNm + upperNm) / 2.0;
						if (!BuildRoute(outwardNm, crossTrackNm, relativeTrackRadians,
							speed.CurrentKts, midpointNm, localizerPointNm, candidate) || !IsPlausibleTurnRoute(candidate)) {
							validBracket = false;
							break;
						}
						const double midpointError = itdTarget != nullptr ? ItdDeliveryErrorNm(candidate, speed, *itdTarget)
							: midpointNm / speed.CurrentKts + TimeToThresholdHours(candidate.BeforeFinalNm, candidate.FinalNm, speed) - targetArrivalHours;
						if (midpointError * previousError > 0.0)
							lowerNm = midpointNm;
						else
							upperNm = midpointNm;
					}
					const double residualHours = itdTarget != nullptr ? ItdDeliveryErrorNm(candidate, speed, *itdTarget)
						: candidate.StartDistanceNm / speed.CurrentKts + TimeToThresholdHours(candidate.BeforeFinalNm, candidate.FinalNm, speed) - targetArrivalHours;
					if (validBracket && std::abs(residualHours) < 1.0e-7) {
						result = candidate;
						return true;
					}
				}
				hasPrevious = true;
				previousDistanceNm = distanceNm;
				previousError = error;
			}
			if (pass == 0 && hasLaterArrival) {
				result = laterArrivalRoute;
				return true;
			}
		}
		return false;
	}

	inline bool FindItdTurnPoint(double outwardNm, double crossTrackNm, double relativeTrackRadians,
		double localizerPointNm, const SpeedProfile& speed, const ItdTarget& target,
		TurnRoute& result, bool includePassed = false, bool allowLargerGap = false, double minimumFutureDistanceNm = 0.0)
	{
		return FindTurnPoint(outwardNm, crossTrackNm, relativeTrackRadians, localizerPointNm, speed,
			0.0, result, includePassed, allowLargerGap, minimumFutureDistanceNm, &target);
	}

	inline bool FindEarliestItdTurnPoint(double outwardNm, double crossTrackNm, double relativeTrackRadians,
		double localizerPointNm, const SpeedProfile& speed, const ItdTarget& target,
		TurnRoute& result, double minimumFutureDistanceNm = 0.0)
	{
		const double leaderTime = TimeToThresholdHours(target.LeaderBeforeFinalNm, target.LeaderFinalNm, target.LeaderSpeed);
		if (!std::isfinite(outwardNm) || !std::isfinite(crossTrackNm) || !std::isfinite(relativeTrackRadians) ||
			!std::isfinite(localizerPointNm) || localizerPointNm <= 0.0 || !std::isfinite(speed.CurrentKts) || speed.CurrentKts < 80.0 ||
			!std::isfinite(target.SlotDistanceNm) || target.SlotDistanceNm < 0.0 || !std::isfinite(target.ReportOffsetHours) ||
			!std::isfinite(minimumFutureDistanceNm) || !std::isfinite(leaderTime) || leaderTime < 0.0)
			return false;
		const double firstDistanceNm = (std::max)(0.0, minimumFutureDistanceNm);
		const double horizonNm = (std::min)(40.0, speed.CurrentKts / 6.0);
		double previousDistanceNm = firstDistanceNm;
		bool hasPrevious = false;
		for (double distanceNm = firstDistanceNm; distanceNm <= horizonNm;
			distanceNm = (std::min)(horizonNm, distanceNm + 0.05)) {
			TurnRoute candidate;
			if (BuildRoute(outwardNm, crossTrackNm, relativeTrackRadians, speed.CurrentKts,
				distanceNm, localizerPointNm, candidate) && IsPlausibleTurnRoute(candidate) &&
				ItdDeliveryErrorNm(candidate, speed, target) >= 0.0) {
				if (hasPrevious) {
					double lowerNm = previousDistanceNm;
					double upperNm = distanceNm;
					for (int iteration = 0; iteration < 35; ++iteration) {
						const double midpointNm = (lowerNm + upperNm) / 2.0;
						TurnRoute midpointRoute;
						if (BuildRoute(outwardNm, crossTrackNm, relativeTrackRadians, speed.CurrentKts,
							midpointNm, localizerPointNm, midpointRoute) && IsPlausibleTurnRoute(midpointRoute) &&
							ItdDeliveryErrorNm(midpointRoute, speed, target) >= 0.0) {
							upperNm = midpointNm;
							candidate = midpointRoute;
						}
						else
							lowerNm = midpointNm;
					}
				}
				candidate.ItdExcessNm = ItdDeliveryErrorNm(candidate, speed, target);
				candidate.TimingExcessSeconds = candidate.ItdExcessNm / speed.CurrentKts * 3600.0;
				result = candidate;
				return true;
			}
			previousDistanceNm = distanceNm;
			hasPrevious = true;
			if (distanceNm >= horizonNm)
				break;
		}
		return false;
	}
}