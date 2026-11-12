#pragma once

#include <cmath>
#include <algorithm>

namespace TurningPointMath {
	constexpr double Pi = 3.14159265358979323846;

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
	};

	struct ItdTarget {
		double SlotDistanceNm;
		double LeaderBeforeFinalNm;
		double LeaderFinalNm;
		SpeedProfile LeaderSpeed;
		double ReportOffsetHours = 0.0;
	};

	struct TurnRoute {
		double StartDistanceNm = 0.0;
		double BeforeFinalNm = 0.0;
		double FinalNm = 0.0;
		double TurnAngleRadians = 0.0;
		double LocalizerPointNm = 0.0;
		double DirectDistanceNm = 0.0;
		double TimingExcessSeconds = 0.0;
		double ItdExcessNm = 0.0;
	};


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

	inline double TimeToThresholdHours(double beforeFinalNm, double finalNm,
		const SpeedProfile& speed)
	{
		if (speed.CurrentKts <= 0.0 || speed.BeforeFinalKts <= 0.0 ||
			speed.CruiseKts <= 0.0 || speed.FinalKts <= 0.0 ||
			beforeFinalNm < 0.0 || finalNm < 0.0 || speed.ReductionNm < 0.0)
			return -1.0;
		double currentSpeedDistanceNm = beforeFinalNm;
		if (speed.LongApproachNm > 0.0) {
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

	inline double DistanceFlownNm(double beforeFinalNm, double finalNm, const SpeedProfile& speed, double elapsedHours)
	{
		if (TimeToThresholdHours(beforeFinalNm, finalNm, speed) < 0.0 || !std::isfinite(elapsedHours))
			return 0.0;
		if (elapsedHours < 0.0)
			return elapsedHours * speed.CurrentKts;
		const double currentDistanceNm = speed.LongApproachNm > 0.0
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
		double, double startDistanceNm, double localizerPointNm, TurnRoute& route, bool clearedForApproach = false)
	{
		if (!clearedForApproach)
			return BuildDirectRoute(outwardNm, crossTrackNm, relativeTrackRadians, startDistanceNm, localizerPointNm, route);
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
					speed.CurrentKts, distanceNm, localizerPointNm, candidate)) {
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
							speed.CurrentKts, midpointNm, localizerPointNm, candidate)) {
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
				distanceNm, localizerPointNm, candidate) && ItdDeliveryErrorNm(candidate, speed, target) >= 0.0) {
				if (hasPrevious) {
					double lowerNm = previousDistanceNm;
					double upperNm = distanceNm;
					for (int iteration = 0; iteration < 35; ++iteration) {
						const double midpointNm = (lowerNm + upperNm) / 2.0;
						TurnRoute midpointRoute;
						if (BuildRoute(outwardNm, crossTrackNm, relativeTrackRadians, speed.CurrentKts,
							midpointNm, localizerPointNm, midpointRoute) && ItdDeliveryErrorNm(midpointRoute, speed, target) >= 0.0) {
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