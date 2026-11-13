#include "stdafx.h"
#include "RadarScreen.h"
#include "FTDiPanel.h"
#include "SeparationMath.h"
#include "TurningPointMath.h"
#include "DisplayColors.h"
#include <cstdio>

namespace {
double SignedAngleDifference(double angle)
{
	return std::fmod(angle + 540.0, 360.0) - 180.0;
}

double TrueBearingDegrees(CPosition from, CPosition to)
{
	const double degreesToRadians = 3.14159265358979323846 / 180.0;
	const double latitudeFrom = from.m_Latitude * degreesToRadians;
	const double latitudeTo = to.m_Latitude * degreesToRadians;
	const double longitudeDifference = (to.m_Longitude - from.m_Longitude) * degreesToRadians;
	const double y = std::sin(longitudeDifference) * std::cos(latitudeTo);
	const double x = std::cos(latitudeFrom) * std::sin(latitudeTo) -
		std::sin(latitudeFrom) * std::cos(latitudeTo) * std::cos(longitudeDifference);
	const double bearing = std::atan2(y, x) / degreesToRadians;
	return std::fmod(bearing + 360.0, 360.0);
}

std::string UppercaseIdentifier(std::string value)
{
	std::transform(value.begin(), value.end(), value.begin(),
		[](unsigned char character) { return static_cast<char>(std::toupper(character)); });
	return value;
}

int MinimumAltitudeFeet()
{
	return CCallsignLookup::Available && CCallsignLookup::Lookup != nullptr
		? CCallsignLookup::Lookup->getMinimumAltitude()
		: 1000;
}

bool HasPassedRoutePoint(CPosition aircraftPosition, double trackHeading,
	CPosition routePoint, CPosition runwayThreshold)
{
	const double outboundHeading = TrueBearingDegrees(runwayThreshold, routePoint);
	const double inboundHeading = std::fmod(outboundHeading + 180.0, 360.0);
	const double relativeBearingRadians = SignedAngleDifference(
		TrueBearingDegrees(runwayThreshold, aircraftPosition) - outboundHeading) *
		3.14159265358979323846 / 180.0;
	const double distanceToThresholdNm = runwayThreshold.DistanceTo(aircraftPosition);
	const double alongTrackNm = distanceToThresholdNm * std::cos(relativeBearingRadians);
	const double crossTrackNm = std::abs(distanceToThresholdNm * std::sin(relativeBearingRadians));
	return std::abs(SignedAngleDifference(trackHeading - inboundHeading)) <= 30.0 &&
		crossTrackNm <= 0.5 && alongTrackNm >= 0.0 &&
		alongTrackNm <= runwayThreshold.DistanceTo(routePoint);
}

bool BuildAircraftLocalizerRoute(CPosition aircraftPosition, double trackHeading, double groundSpeedKts,
	CPosition joinPoint, CPosition runwayThreshold, TurningPointMath::TurnRoute& route, bool clearedForApproach = false)
{
	const double outboundHeading = TrueBearingDegrees(runwayThreshold, joinPoint);
	const double bearingRadians = SignedAngleDifference(
		TrueBearingDegrees(runwayThreshold, aircraftPosition) - outboundHeading) * TurningPointMath::Pi / 180.0;
	const double distanceNm = runwayThreshold.DistanceTo(aircraftPosition);
	return TurningPointMath::BuildRoute(distanceNm * std::cos(bearingRadians), distanceNm * std::sin(bearingRadians),
		SignedAngleDifference(trackHeading - outboundHeading) * TurningPointMath::Pi / 180.0,
		groundSpeedKts, 0.0, runwayThreshold.DistanceTo(joinPoint), route, clearedForApproach);
}

void DrawAircraftLocalizerRoute(CRadarScreen* screen, CDC& dc, CPosition aircraftPosition, double trackHeading,
	CPosition joinPoint, CPosition runwayThreshold, bool passedJoin, const TurningPointMath::TurnRoute* route)
{
	dc.MoveTo(screen->ConvertCoordFromPositionToPixel(aircraftPosition));
	if (!passedJoin) {
		if (route != nullptr) {
			const CPosition directStart = SeparationMath::Extrapolate(aircraftPosition,
				std::fmod(trackHeading + (route->StartDistanceNm < 0.0 ? 180.0 : 0.0), 360.0),
				std::abs(route->StartDistanceNm) * 1852.0);
			dc.LineTo(screen->ConvertCoordFromPositionToPixel(directStart));
			const double outboundHeading = TrueBearingDegrees(runwayThreshold, joinPoint);
			for (int segment = 1; segment < TurningPointMath::RouteCurveSegments; ++segment) {
				const auto point = TurningPointMath::RouteCurvePoint(*route,
					static_cast<double>(segment) / TurningPointMath::RouteCurveSegments);
				const double bearing = outboundHeading + std::atan2(point.CrossTrackNm, point.OutwardNm) *
					180.0 / TurningPointMath::Pi;
				const CPosition curvePosition = SeparationMath::Extrapolate(runwayThreshold, bearing,
					std::hypot(point.OutwardNm, point.CrossTrackNm) * 1852.0);
				dc.LineTo(screen->ConvertCoordFromPositionToPixel(curvePosition));
			}
		}
		dc.LineTo(screen->ConvertCoordFromPositionToPixel(joinPoint));
	}
	dc.LineTo(screen->ConvertCoordFromPositionToPixel(runwayThreshold));
}

double MinimumDistanceNm()
{
	return CCallsignLookup::Available && CCallsignLookup::Lookup != nullptr
		? CCallsignLookup::Lookup->getMinimumDistanceNm()
		: 3.0;
}

double FinalBufferNm()
{
	return CCallsignLookup::Available && CCallsignLookup::Lookup != nullptr
		? CCallsignLookup::Lookup->getFinalBufferNm()
		: 1.0;
}

double RequiredSpacingNm(char precedingWake, char succeedingWake)
{
	double baseDistanceNm = MinimumDistanceNm();
	if (CCallsignLookup::Available && CCallsignLookup::Lookup != nullptr) {
		double wakeDistanceNm = 0.0;
		if (CCallsignLookup::Lookup->getWakeSeparation(precedingWake, succeedingWake, wakeDistanceNm) &&
			wakeDistanceNm > baseDistanceNm)
			baseDistanceNm = wakeDistanceNm;
	}
	return baseDistanceNm + FinalBufferNm();
}

double PlannedDistanceFlownNm(
	double distanceToJoinNm, double distanceBeforeLongApproachNm,
	double longApproachGroundSpeedKts, double joinToThresholdNm,
	double actualGroundSpeedKts, double speedUntilReductionKts,
	double finalApproachSpeedKts, double reductionDistanceNm, double elapsedHours)
{
	if (distanceBeforeLongApproachNm > distanceToJoinNm)
		distanceBeforeLongApproachNm = distanceToJoinNm;
	const double distanceAtLongApproachSpeedNm = distanceToJoinNm - distanceBeforeLongApproachNm;
	const double timeBeforeLongApproach = distanceBeforeLongApproachNm / actualGroundSpeedKts;
	const double timeAtLongApproachSpeed = distanceAtLongApproachSpeedNm / longApproachGroundSpeedKts;
	const double timeToJoin = timeBeforeLongApproach + timeAtLongApproachSpeed;
	const double distanceBeforeReduction = joinToThresholdNm > reductionDistanceNm
		? joinToThresholdNm - reductionDistanceNm : 0.0;
	const double distanceAtFinalSpeed = joinToThresholdNm - distanceBeforeReduction;
	if (elapsedHours <= timeBeforeLongApproach)
		return actualGroundSpeedKts * elapsedHours;
	if (elapsedHours <= timeToJoin)
		return distanceBeforeLongApproachNm +
			longApproachGroundSpeedKts * (elapsedHours - timeBeforeLongApproach);

	const double elapsedAfterJoin = elapsedHours - timeToJoin;
	const double timeBeforeReduction = distanceBeforeReduction / speedUntilReductionKts;
	if (elapsedAfterJoin <= timeBeforeReduction)
		return distanceToJoinNm + speedUntilReductionKts * elapsedAfterJoin;

	const double flownDistance = distanceToJoinNm + distanceBeforeReduction +
		finalApproachSpeedKts * (elapsedAfterJoin - timeBeforeReduction);
	const double totalRouteDistance = distanceToJoinNm + joinToThresholdNm;
	return flownDistance < totalRouteDistance ? flownDistance : totalRouteDistance;
}

double ClearedActualSpeedBoundaryNm(CFlightPlan plan)
{
	if (!plan.IsValid() || !CCallsignLookup::Available || CCallsignLookup::Lookup == nullptr)
		return 0.0;
	const int clearedAltitude = plan.GetControllerAssignedData().GetClearedAltitude();
	const CFlightPlanData data = plan.GetFlightPlanData();
	const char* destination = data.GetDestination();
	const char* runway = data.GetArrivalRwy();
	double localizerPointNm = 8.0;
	if (destination != nullptr && runway != nullptr)
		CCallsignLookup::Lookup->getLocalizerPointDistanceNm(destination, runway, localizerPointNm);
	return TurningPointMath::ClearedSpeedBoundaryNm(clearedAltitude == 1 || clearedAltitude == 2,
		CCallsignLookup::Lookup->getReduceSpeedOnClearance(), localizerPointNm,
		CCallsignLookup::Lookup->getSpeedReductionDistanceNm());
}

double CalculateCompressionMarginNm(
	char precedingWake, char succeedingWake,
	int precedingFinalSpeedOverrideKts, int succeedingFinalSpeedOverrideKts,
	double precedingGroundSpeedKts, double precedingDistanceToJoinNm,
	double precedingJoinToThresholdNm, CPosition precedingPosition, double precedingTrack,
	double succeedingGroundSpeedKts, double succeedingDistanceToJoinNm,
	double succeedingJoinToThresholdNm, CPosition succeedingPosition, double succeedingTrack,
	double precedingActualSpeedUntilNm, double succeedingActualSpeedUntilNm,
	const AirportWeatherService* weatherService)
{
	const bool hasConfig = CCallsignLookup::Available && CCallsignLookup::Lookup != nullptr;
	const double reductionDistanceNm = hasConfig
		? CCallsignLookup::Lookup->getSpeedReductionDistanceNm() : 5.0;
	const double speedUntilReductionKts = hasConfig
		? CCallsignLookup::Lookup->getSpeedUntilReductionKts() : 170.0;
	const double longApproachNm = hasConfig
		? CCallsignLookup::Lookup->getLongApproachNm() : 0.0;
	const double longApproachSpeedKts = hasConfig
		? CCallsignLookup::Lookup->getLongApproachSpeedKts() : 250.0;
	double precedingFinalSpeedKts = precedingFinalSpeedOverrideKts > 0
		? precedingFinalSpeedOverrideKts
		: (hasConfig ? CCallsignLookup::Lookup->getFinalApproachSpeedKts(precedingWake) : 150.0);
	double succeedingFinalSpeedKts = succeedingFinalSpeedOverrideKts > 0
		? succeedingFinalSpeedOverrideKts
		: (hasConfig ? CCallsignLookup::Lookup->getFinalApproachSpeedKts(succeedingWake) : 150.0);
	double precedingCruiseSpeedKts = speedUntilReductionKts;
	double succeedingCruiseSpeedKts = speedUntilReductionKts;
	if (weatherService != nullptr) {
		precedingCruiseSpeedKts = weatherService->GetGroundSpeedKts(
			precedingPosition, precedingTrack, precedingCruiseSpeedKts);
		succeedingCruiseSpeedKts = weatherService->GetGroundSpeedKts(
			succeedingPosition, succeedingTrack, succeedingCruiseSpeedKts);
		precedingFinalSpeedKts = weatherService->GetGroundSpeedKts(
			precedingPosition, precedingTrack, precedingFinalSpeedKts);
		succeedingFinalSpeedKts = weatherService->GetGroundSpeedKts(
			succeedingPosition, succeedingTrack, succeedingFinalSpeedKts);
	}
	if (speedUntilReductionKts <= 0.0 || precedingFinalSpeedKts <= 0.0 || succeedingFinalSpeedKts <= 0.0 ||
		precedingJoinToThresholdNm <= 0.0 || succeedingJoinToThresholdNm <= 0.0)
		return 0.0;
	if (precedingGroundSpeedKts <= 0.0)
		precedingGroundSpeedKts = precedingCruiseSpeedKts;
	if (succeedingGroundSpeedKts <= 0.0)
		succeedingGroundSpeedKts = succeedingCruiseSpeedKts;
	if (precedingGroundSpeedKts <= 0.0 || succeedingGroundSpeedKts <= 0.0)
		return 0.0;
	TurningPointMath::SplitActualSpeedLegs(precedingDistanceToJoinNm, precedingJoinToThresholdNm,
		precedingActualSpeedUntilNm, reductionDistanceNm);
	TurningPointMath::SplitActualSpeedLegs(succeedingDistanceToJoinNm, succeedingJoinToThresholdNm,
		succeedingActualSpeedUntilNm, reductionDistanceNm);
	double precedingDistanceBeforeLongApproachNm = precedingDistanceToJoinNm;
	double succeedingDistanceBeforeLongApproachNm = succeedingDistanceToJoinNm;
	double precedingLongApproachGroundSpeedKts = precedingGroundSpeedKts;
	double succeedingLongApproachGroundSpeedKts = succeedingGroundSpeedKts;
	if (longApproachNm > 0.0) {
		const double precedingDistanceOutsideLongApproachNm =
			precedingDistanceToJoinNm + precedingJoinToThresholdNm - longApproachNm;
		const double succeedingDistanceOutsideLongApproachNm =
			succeedingDistanceToJoinNm + succeedingJoinToThresholdNm - longApproachNm;
		if (precedingDistanceOutsideLongApproachNm > 0.0 &&
			precedingDistanceBeforeLongApproachNm > precedingDistanceOutsideLongApproachNm)
			precedingDistanceBeforeLongApproachNm = precedingDistanceOutsideLongApproachNm;
		else if (precedingDistanceOutsideLongApproachNm <= 0.0)
			precedingDistanceBeforeLongApproachNm = 0.0;
		if (succeedingDistanceOutsideLongApproachNm > 0.0 &&
			succeedingDistanceBeforeLongApproachNm > succeedingDistanceOutsideLongApproachNm)
			succeedingDistanceBeforeLongApproachNm = succeedingDistanceOutsideLongApproachNm;
		else if (succeedingDistanceOutsideLongApproachNm <= 0.0)
			succeedingDistanceBeforeLongApproachNm = 0.0;
		precedingLongApproachGroundSpeedKts = weatherService != nullptr
			? weatherService->GetGroundSpeedKts(precedingPosition, precedingTrack, longApproachSpeedKts)
			: longApproachSpeedKts;
		succeedingLongApproachGroundSpeedKts = weatherService != nullptr
			? weatherService->GetGroundSpeedKts(succeedingPosition, succeedingTrack, longApproachSpeedKts)
			: longApproachSpeedKts;
		if (precedingGroundSpeedKts < precedingLongApproachGroundSpeedKts)
			precedingLongApproachGroundSpeedKts = precedingGroundSpeedKts;
		if (succeedingGroundSpeedKts < succeedingLongApproachGroundSpeedKts)
			succeedingLongApproachGroundSpeedKts = succeedingGroundSpeedKts;
	}

	if (precedingActualSpeedUntilNm > 0.0) {
		precedingDistanceBeforeLongApproachNm = precedingDistanceToJoinNm;
		precedingLongApproachGroundSpeedKts = precedingGroundSpeedKts;
	}
	if (succeedingActualSpeedUntilNm > 0.0) {
		succeedingDistanceBeforeLongApproachNm = succeedingDistanceToJoinNm;
		succeedingLongApproachGroundSpeedKts = succeedingGroundSpeedKts;
	}

	const double precedingPreReductionDistance = precedingJoinToThresholdNm > reductionDistanceNm
		? precedingJoinToThresholdNm - reductionDistanceNm : 0.0;
	const double succeedingPreReductionDistance = succeedingJoinToThresholdNm > reductionDistanceNm
		? succeedingJoinToThresholdNm - reductionDistanceNm : 0.0;
	const double precedingJoinTime = precedingDistanceBeforeLongApproachNm / precedingGroundSpeedKts +
		(precedingDistanceToJoinNm - precedingDistanceBeforeLongApproachNm) /
		precedingLongApproachGroundSpeedKts;
	const double succeedingJoinTime = succeedingDistanceBeforeLongApproachNm / succeedingGroundSpeedKts +
		(succeedingDistanceToJoinNm - succeedingDistanceBeforeLongApproachNm) /
		succeedingLongApproachGroundSpeedKts;
	const double precedingLongApproachStartTime =
		precedingDistanceBeforeLongApproachNm / precedingGroundSpeedKts;
	const double succeedingLongApproachStartTime =
		succeedingDistanceBeforeLongApproachNm / succeedingGroundSpeedKts;
	const double precedingReductionTime = precedingJoinTime + precedingPreReductionDistance / precedingCruiseSpeedKts;
	const double succeedingReductionTime = succeedingJoinTime + succeedingPreReductionDistance / succeedingCruiseSpeedKts;
	const double precedingTimeToTouchdown = precedingReductionTime +
		(precedingJoinToThresholdNm - precedingPreReductionDistance) / precedingFinalSpeedKts;
	const double succeedingTimeToTouchdown = succeedingReductionTime +
		(succeedingJoinToThresholdNm - succeedingPreReductionDistance) / succeedingFinalSpeedKts;

	double maximumCompressionNm = 0.0;
	const double checkTimes[] = {
		precedingLongApproachStartTime, succeedingLongApproachStartTime,
		precedingJoinTime, succeedingJoinTime, precedingReductionTime,
		succeedingReductionTime, precedingTimeToTouchdown, succeedingTimeToTouchdown };
	for (double elapsedHours : checkTimes) {
		if (elapsedHours < 0.0 || elapsedHours > precedingTimeToTouchdown)
			continue;
		const double precedingFlownNm = PlannedDistanceFlownNm(
			precedingDistanceToJoinNm, precedingDistanceBeforeLongApproachNm,
			precedingLongApproachGroundSpeedKts, precedingJoinToThresholdNm, precedingGroundSpeedKts,
			precedingCruiseSpeedKts, precedingFinalSpeedKts, reductionDistanceNm, elapsedHours);
		const double succeedingFlownNm = PlannedDistanceFlownNm(
			succeedingDistanceToJoinNm, succeedingDistanceBeforeLongApproachNm,
			succeedingLongApproachGroundSpeedKts, succeedingJoinToThresholdNm, succeedingGroundSpeedKts,
			succeedingCruiseSpeedKts, succeedingFinalSpeedKts, reductionDistanceNm, elapsedHours);
		const double compressionNm = succeedingFlownNm - precedingFlownNm;
		if (compressionNm > maximumCompressionNm)
			maximumCompressionNm = compressionNm;
	}
	return maximumCompressionNm;
}

TurningPointMath::SpeedProfile TurningSpeedProfile(CRadarTarget aircraft, CFlightPlan plan,
	double finalTrackHeading, const PredictionStability::Motion& motion,
	const FTDiSession& session, const AirportWeatherService* weatherService)
{
	const bool hasConfig = CCallsignLookup::Available && CCallsignLookup::Lookup != nullptr;
	const double currentSpeedKts = motion.GroundSpeedKts;
	const double longApproachNm = hasConfig ? CCallsignLookup::Lookup->getLongApproachNm() : 0.0;
	double beforeFinalKts = longApproachNm > 0.0 && hasConfig
		? CCallsignLookup::Lookup->getLongApproachSpeedKts() : currentSpeedKts;
	double cruiseKts = hasConfig ? CCallsignLookup::Lookup->getSpeedUntilReductionKts() : 170.0;
	const char wake = plan.GetFlightPlanData().GetAircraftWtc();
	const auto speedOverride = session.FinalApproachSpeedOverridesKts.find(aircraft.GetCallsign());
	double finalKts = speedOverride != session.FinalApproachSpeedOverridesKts.end()
		? speedOverride->second
		: (hasConfig ? CCallsignLookup::Lookup->getFinalApproachSpeedKts(wake) : 150.0);
	if (weatherService != nullptr) {
		const CPosition position = aircraft.GetPosition().GetPosition();
		if (longApproachNm > 0.0)
			beforeFinalKts = weatherService->GetGroundSpeedKts(position, motion.TrackDegrees, beforeFinalKts);
		cruiseKts = weatherService->GetGroundSpeedKts(position, finalTrackHeading, cruiseKts);
		finalKts = weatherService->GetGroundSpeedKts(position, finalTrackHeading, finalKts);
	}
	if (beforeFinalKts > currentSpeedKts)
		beforeFinalKts = currentSpeedKts;
	return { currentSpeedKts, beforeFinalKts, cruiseKts, finalKts,
		hasConfig ? CCallsignLookup::Lookup->getSpeedReductionDistanceNm() : 5.0, longApproachNm,
		ClearedActualSpeedBoundaryNm(plan) };
}

bool FindLocalizerMarkerPosition(
	CPlugIn* plugIn, const std::string& destination, const std::string& arrivalRunway,
	CPosition aircraftPosition, double aircraftTrackHeading, double requiredNm,
	CPosition& markerPosition, CPosition& localizerTrackPosition,
	CPosition& localizerJoinPoint, CPosition& runwayThreshold)
{
	UNREFERENCED_PARAMETER(aircraftTrackHeading);
	const double degreesToRadians = 3.14159265358979323846 / 180.0;
	const std::string normalizedDestination = UppercaseIdentifier(destination);
	const std::string normalizedArrivalRunway = UppercaseIdentifier(arrivalRunway);
	double closestCrossTrackNm = 1.0e100;
	double closestDestinationCrossTrackNm = 1.0e100;
	bool foundRunway = false;
	bool foundDestinationRunway = false;
	CPosition closestMarkerPosition;
	CPosition closestTrackPosition;
	CPosition destinationMarkerPosition;
	CPosition destinationTrackPosition;
	CPosition destinationJoinPoint;
	CPosition destinationThreshold;
	CSectorElement runway = plugIn->SectorFileElementSelectFirst(SECTOR_ELEMENT_RUNWAY);
	while (runway.IsValid()) {
		const char* runwayAirport = runway.GetAirportName();
		const bool destinationMatches = runwayAirport != nullptr &&
			UppercaseIdentifier(runwayAirport) == normalizedDestination;
		for (int index = 0; index < 2; ++index) {
			const char* sectorRunwayName = runway.GetRunwayName(index);
			if (sectorRunwayName == nullptr || UppercaseIdentifier(sectorRunwayName) != normalizedArrivalRunway)
				continue;

			CPosition threshold;
			CPosition oppositeEnd;
			if (!runway.GetPosition(&threshold, index) ||
				!runway.GetPosition(&oppositeEnd, 1 - index))
				continue;

			const double inboundTrueHeading = TrueBearingDegrees(threshold, oppositeEnd);
			double inboundHeading = inboundTrueHeading;
			double configuredMagneticCourse = 0.0;
			if (CCallsignLookup::Available && CCallsignLookup::Lookup != nullptr &&
				CCallsignLookup::Lookup->getLocalizerCourseDegrees(
					destination, arrivalRunway, configuredMagneticCourse)) {
				const double magneticDeviation = SignedAngleDifference(
					threshold.DirectionTo(oppositeEnd) - inboundTrueHeading);
				inboundHeading = std::fmod(configuredMagneticCourse - magneticDeviation + 360.0, 360.0);
			}
			const double outboundHeading = std::fmod(inboundHeading + 180.0, 360.0);
			const double distanceToThresholdNm = threshold.DistanceTo(aircraftPosition);
			const double relativeBearing = SignedAngleDifference(
				TrueBearingDegrees(threshold, aircraftPosition) - outboundHeading);
			const double relativeBearingRadians = relativeBearing * degreesToRadians;
			const double alongTrackNm = distanceToThresholdNm * std::cos(relativeBearingRadians);
			const double crossTrackNm = std::abs(distanceToThresholdNm * std::sin(relativeBearingRadians));
			double localizerPointDistanceNm = 8.0;
			if (CCallsignLookup::Available && CCallsignLookup::Lookup != nullptr)
				CCallsignLookup::Lookup->getLocalizerPointDistanceNm(
					destination, arrivalRunway, localizerPointDistanceNm);

			const double markerAlongTrackNm = alongTrackNm > 0.0 ? alongTrackNm : 0.0;

			CPosition candidateMarkerPosition = SeparationMath::Extrapolate(
				threshold, outboundHeading, (markerAlongTrackNm + requiredNm) * 1852.0);
			CPosition candidateTrackPosition = SeparationMath::Extrapolate(
				candidateMarkerPosition, inboundHeading, 1852.0);
			CPosition candidateJoinPoint = SeparationMath::Extrapolate(
				threshold, outboundHeading, localizerPointDistanceNm * 1852.0);
			if (crossTrackNm < closestCrossTrackNm) {
				closestCrossTrackNm = crossTrackNm;
				closestMarkerPosition = candidateMarkerPosition;
				closestTrackPosition = candidateTrackPosition;
				localizerJoinPoint = candidateJoinPoint;
				runwayThreshold = threshold;
				foundRunway = true;
			}
			if (destinationMatches && crossTrackNm < closestDestinationCrossTrackNm) {
				closestDestinationCrossTrackNm = crossTrackNm;
				destinationMarkerPosition = candidateMarkerPosition;
				destinationTrackPosition = candidateTrackPosition;
				destinationJoinPoint = candidateJoinPoint;
				destinationThreshold = threshold;
				foundDestinationRunway = true;
			}
		}
		runway = plugIn->SectorFileElementSelectNext(runway, SECTOR_ELEMENT_RUNWAY);
	}
	if (foundDestinationRunway) {
		markerPosition = destinationMarkerPosition;
		localizerTrackPosition = destinationTrackPosition;
		localizerJoinPoint = destinationJoinPoint;
		runwayThreshold = destinationThreshold;
	}
	else if (foundRunway) {
		markerPosition = closestMarkerPosition;
		localizerTrackPosition = closestTrackPosition;
	}
	return foundRunway;
}

int MarkerSizePixels(bool finalTarget)
{
	return CCallsignLookup::Available && CCallsignLookup::Lookup != nullptr
		? CCallsignLookup::Lookup->getMarkerSizePixels(finalTarget) : (finalTarget ? 26 : 22);
}

void DrawWakeSpacingMarker(
	CDC* dc, POINT center, POINT precedingPoint, POINT precedingTrackPoint,
	bool parallelDependent, bool highlighted)
{
	const COLORREF itdColor = ConfiguredColor("COLOR_ITD_TARGET");
	double directionX = static_cast<double>(precedingTrackPoint.x - precedingPoint.x);
	double directionY = static_cast<double>(precedingTrackPoint.y - precedingPoint.y);
	const double distance = std::sqrt(directionX * directionX + directionY * directionY);
	if (distance > 0.0) {
		directionX /= distance;
		directionY /= distance;
	}
	else {
		directionX = 1.0;
		directionY = 0.0;
	}
	const double crossX = -directionY;
	const double crossY = directionX;
	const int savedState = dc->SaveDC();
	const int targetPenStyle = (parallelDependent ? PS_DASH : PS_SOLID) | PS_ENDCAP_ROUND | PS_JOIN_ROUND;
	CPen targetPen(targetPenStyle, 2, itdColor);
	CPen highlightPen(PS_SOLID, 5, ConfiguredColor("COLOR_TURN_POINT"));
	const double arcHalfWidth = MarkerSizePixels(false) / 2.0;
	const double arcDepth = MarkerSizePixels(false) * 5.0 / 22.0;
	const auto drawArc = [dc, center, crossX, crossY, directionX, directionY, arcHalfWidth, arcDepth]() {
		for (int step = 0; step <= 12; ++step) {
			const double across = -1.0 + step / 6.0;
			const double recess = (1.0 - across * across) * arcDepth;
			const POINT arcPoint = {
				static_cast<LONG>(center.x + crossX * across * arcHalfWidth + directionX * recess),
				static_cast<LONG>(center.y + crossY * across * arcHalfWidth + directionY * recess) };
			if (step == 0)
				dc->MoveTo(arcPoint);
			else
				dc->LineTo(arcPoint);
		}
	};
	if (highlighted) {
		dc->SelectObject(&highlightPen);
		drawArc();
	}
	dc->SelectObject(&targetPen);
	drawArc();
	dc->RestoreDC(savedState);
}

void DrawFinalTargetMarker(
	CDC* dc, POINT center, POINT precedingPoint, POINT precedingTrackPoint,
	double requiredNm, double actualNm, bool parallelDependent, bool gapGoverning, bool showAlert,
	bool highlighted)
{
	const COLORREF color = ConfiguredColor(gapGoverning ? "COLOR_GAP_TARGET" : "COLOR_FTD_TARGET");
	double directionX = static_cast<double>(precedingTrackPoint.x - precedingPoint.x);
	double directionY = static_cast<double>(precedingTrackPoint.y - precedingPoint.y);
	const double directionLength = std::sqrt(directionX * directionX + directionY * directionY);
	if (directionLength > 0.0) {
		directionX /= directionLength;
		directionY /= directionLength;
	}
	else {
		directionX = 1.0;
		directionY = 0.0;
	}
	const double crossX = -directionY;
	const double crossY = directionX;
	const int savedState = dc->SaveDC();
	const int penStyle = (parallelDependent ? PS_DASH : PS_SOLID) | PS_ENDCAP_ROUND | PS_JOIN_ROUND;
	CPen targetPen(penStyle, 2, color);
	CPen highlightPen(PS_SOLID, 5, ConfiguredColor("COLOR_TURN_POINT"));
	const double halfWidth = MarkerSizePixels(true) / 2.0;
	const double depth = MarkerSizePixels(true) * 5.0 / 22.0;
	const auto drawShape = [dc, gapGoverning, center, crossX, crossY, directionX, directionY, halfWidth, depth]() {
	if (gapGoverning) {
		const POINT bracketTop = {
			static_cast<LONG>(center.x - crossX * halfWidth + directionX * depth * 0.8),
			static_cast<LONG>(center.y - crossY * halfWidth + directionY * depth * 0.8) };
		const POINT bracketBottom = {
			static_cast<LONG>(center.x + crossX * halfWidth + directionX * depth * 0.8),
			static_cast<LONG>(center.y + crossY * halfWidth + directionY * depth * 0.8) };
		const POINT topEnd = {
			static_cast<LONG>(bracketTop.x - directionX * depth * 1.8),
			static_cast<LONG>(bracketTop.y - directionY * depth * 1.8) };
		const POINT bottomEnd = {
			static_cast<LONG>(bracketBottom.x - directionX * depth * 1.8),
			static_cast<LONG>(bracketBottom.y - directionY * depth * 1.8) };
		dc->MoveTo(bracketTop);
		dc->LineTo(bracketBottom);
		dc->MoveTo(bracketTop);
		dc->LineTo(topEnd);
		dc->MoveTo(bracketBottom);
		dc->LineTo(bottomEnd);
	}
	else {
		for (int step = 0; step <= 12; ++step) {
			const double across = -1.0 + step / 6.0;
			const double recess = (1.0 - across * across) * depth;
			const POINT arcPoint = {
				static_cast<LONG>(center.x + crossX * across * halfWidth + directionX * recess),
				static_cast<LONG>(center.y + crossY * across * halfWidth + directionY * recess) };
			if (step == 0)
				dc->MoveTo(arcPoint);
			else
				dc->LineTo(arcPoint);
		}
	}
	};
	if (highlighted) {
		dc->SelectObject(&highlightPen);
		drawShape();
	}
	dc->SelectObject(&targetPen);
	drawShape();
	CFont font;
	font.CreatePointFont(75, "Arial", dc);
	dc->SelectObject(&font);
	dc->SetBkMode(TRANSPARENT);
	dc->SetTextColor(color);
	const int markerExtent = static_cast<int>(std::ceil(std::hypot(
		MarkerSizePixels(true) / 2.0, MarkerSizePixels(true) * 5.0 / 22.0))) + 4;
	LONG labelBottom = center.y + markerExtent;
	if (showAlert && actualNm < requiredNm) {
		const char* alertLabel = "ALERT";
		const CSize alertSize = dc->GetTextExtent(alertLabel);
		const int labelLeft = center.x - alertSize.cx / 2;
		const int alertY = labelBottom + 2;
		dc->SetTextColor(ConfiguredColor("COLOR_BREACH_ALERT"));
		dc->TextOut(labelLeft - 10, alertY, alertLabel);
		labelBottom = alertY + alertSize.cy;
	}
	dc->RestoreDC(savedState);
}
}

RadarScreen::RadarScreen(AirportWeatherService* weatherService, std::shared_ptr<FTDiSession> session)
	: Session(session), WeatherService(weatherService)
{
}

void RadarScreen::OnRefresh(HDC hDC, int Phase)
{
	if (Phase != REFRESH_PHASE_BEFORE_TAGS)
		return;

	if (SequenceAddRequested.exchange(false))
		StartSequenceInsertion();

	if (SequenceCancelRequested.exchange(false) && SelectionStage != SequenceSelectionStage::Idle) {
		SelectionStage = SequenceSelectionStage::Idle;
		SequenceSelectionActive.store(false);
		SequencePickArmed.store(false);
	}
	if (Session->ManagerExpanded)
		SequenceAddRequested.store(false);
	if (SequenceAircraftClickRequested.exchange(false) && SelectionStage == SequenceSelectionStage::AwaitAircraft) {
		const CRadarTarget selectedTarget = GetPlugIn()->RadarTargetSelectASEL();
		if (selectedTarget.IsValid())
			AppendAircraftToSequence(selectedTarget.GetCallsign());
		RequestRefresh();
	}

	CDC dc;
	dc.Attach(hDC);
	const int savedState = dc.SaveDC();
	dc.SetBkMode(TRANSPARENT);
	const int minimumAltitude = MinimumAltitudeFeet();
	for (auto aircraft = Session->ArrivalSequence.begin(); aircraft != Session->ArrivalSequence.end();) {
		const CRadarTarget target = GetPlugIn()->RadarTargetSelect(aircraft->c_str());
		if (target.IsValid() && target.GetPosition().GetPressureAltitude() < minimumAltitude) {
			aircraft = Session->ArrivalSequence.erase(aircraft);
			Session->SequenceDirty = true;
		}
		else {
			++aircraft;
		}
	}
	SynchronizeSequenceMeasurements();
	const double predictionNowSeconds = GetTickCount64() / 1000.0;
	for (auto motion = Session->PlanningMotion.begin(); motion != Session->PlanningMotion.end();) {
		if (std::find(Session->ArrivalSequence.begin(), Session->ArrivalSequence.end(), motion->first) == Session->ArrivalSequence.end())
			motion = Session->PlanningMotion.erase(motion);
		else
			++motion;
	}
	for (const std::string& callsign : Session->ArrivalSequence) {
		const CRadarTarget target = GetPlugIn()->RadarTargetSelect(callsign.c_str());
		if (!target.IsValid() || target.GetPosition().GetReceivedTime() > 30) {
			Session->PlanningMotion.erase(callsign);
			continue;
		}
		const CFlightPlan plan = target.GetCorrelatedFlightPlan();
		std::string context;
		if (plan.IsValid()) {
			const CFlightPlanData data = plan.GetFlightPlanData();
			context = std::string(data.GetDestination() != nullptr ? data.GetDestination() : "") + "|" +
				(data.GetArrivalRwy() != nullptr ? data.GetArrivalRwy() : "") + "|" + data.GetAircraftWtc() + "|" +
				std::to_string(plan.GetControllerAssignedData().GetClearedAltitude());
		}
		Session->PlanningMotion[callsign].Update(target.GetTrackHeading(), target.GetGS(), predictionNowSeconds, context);
	}
	const bool showLocalizerRoute = !CCallsignLookup::Available || CCallsignLookup::Lookup == nullptr ||
		CCallsignLookup::Lookup->getShowLocalizerRoute();
	const std::string& selectedSequenceCallsign = Session->SelectedSequenceCallsign;
	if (!selectedSequenceCallsign.empty() && std::find(Session->ArrivalSequence.begin(),
		Session->ArrivalSequence.end(), selectedSequenceCallsign) == Session->ArrivalSequence.end()) {
		Session->SelectedSequenceCallsign.clear();
		Session->RightClickedSequenceCallsign.clear();
	}
	if (!Session->RoutePreviewCallsign.empty() && std::find(Session->ArrivalSequence.begin(),
		Session->ArrivalSequence.end(), Session->RoutePreviewCallsign) == Session->ArrivalSequence.end())
		Session->RoutePreviewCallsign.clear();

	if (SelectionStage == SequenceSelectionStage::AwaitAircraft) {
		const int selectionState = dc.SaveDC();
		CFont selectionFont;
		selectionFont.CreatePointFont(100, "Segoe UI Semibold", &dc);
		dc.SelectObject(&selectionFont);
		dc.SetBkMode(TRANSPARENT);
		dc.SetTextColor(ConfiguredColor("COLOR_SELECTION_TEXT"));
		dc.TextOut(18, 18, "SELECT AIRCRAFT TO APPEND");
		dc.RestoreDC(selectionState);
	}

	GetPlugIn()->SelectScreenSectorfile(this);
	for (const std::string& leaderCallsign : Session->ArrivalSequence) {
		const auto follower = std::find_if(Session->WakeMeasurements.begin(), Session->WakeMeasurements.end(),
			[&leaderCallsign](const WakeMeasurement& measurement) {
				return measurement.SucceedingCallsign == leaderCallsign;
			});
		if (follower != Session->WakeMeasurements.end())
			continue;
		const CRadarTarget firstAircraft = GetPlugIn()->RadarTargetSelect(
			leaderCallsign.c_str());
		const bool previewFirstRoute = Session->RoutePreviewCallsign == leaderCallsign;
		if (firstAircraft.IsValid() && (showLocalizerRoute || previewFirstRoute)) {
			const CPosition aircraftPosition = firstAircraft.GetPosition().GetPosition();
			const CFlightPlan flightPlan = firstAircraft.GetCorrelatedFlightPlan();
			if (flightPlan.IsValid()) {
				const CFlightPlanData flightPlanData = flightPlan.GetFlightPlanData();
				const char* destination = flightPlanData.GetDestination();
				const char* arrivalRunway = flightPlanData.GetArrivalRwy();
				CPosition markerPosition;
				CPosition trackPosition;
				CPosition joinPoint;
				CPosition runwayThreshold;
				if (destination != nullptr && arrivalRunway != nullptr && arrivalRunway[0] != '\0' &&
					FindLocalizerMarkerPosition(GetPlugIn(), destination, arrivalRunway,
						aircraftPosition, firstAircraft.GetTrackHeading(), 0.0,
						markerPosition, trackPosition, joinPoint, runwayThreshold)) {
					const bool passedJoin = HasPassedRoutePoint(
						aircraftPosition, firstAircraft.GetTrackHeading(), joinPoint, runwayThreshold);
					TurningPointMath::TurnRoute route;
					const int clearedAltitude = flightPlan.GetControllerAssignedData().GetClearedAltitude();
					const bool clearedForApproach = clearedAltitude == 1 || clearedAltitude == 2;
					const bool hasTurnRoute = !passedJoin && BuildAircraftLocalizerRoute(aircraftPosition,
						firstAircraft.GetTrackHeading(), firstAircraft.GetGS(), joinPoint, runwayThreshold, route, clearedForApproach);
					CPen routePen(PS_DASH, 1, ConfiguredColor("COLOR_FTD_ROUTE"));
					const int routeState = dc.SaveDC();
					dc.SelectObject(&routePen);
					DrawAircraftLocalizerRoute(this, dc, aircraftPosition, firstAircraft.GetTrackHeading(),
						joinPoint, runwayThreshold, passedJoin || clearedForApproach, hasTurnRoute ? &route : nullptr);
					dc.RestoreDC(routeState);
				}
			}
		}
	}
	for (auto measurementIt = Session->WakeMeasurements.begin(); measurementIt != Session->WakeMeasurements.end();) {
		WakeMeasurement& measurement = *measurementIt;
		measurement.HasTargetSlotPosition = false;
		measurement.HasFinalTargetPosition = false;
		measurement.HasExpectedRoutePosition = false;
		measurement.HasCurrentSpacing = false;
		measurement.HasRouteBasedCurrentSpacing = false;
		measurement.TurnStatus = "NO DATA";
		measurement.TurnStatusDetail = "Turn prediction requires valid aircraft and correlated flight plans";
		CRadarTarget preceding = GetPlugIn()->RadarTargetSelect(measurement.PrecedingCallsign.c_str());
		CRadarTarget succeeding = GetPlugIn()->RadarTargetSelect(measurement.SucceedingCallsign.c_str());
		if (!preceding.IsValid() || !succeeding.IsValid()) {
			measurementIt = Session->WakeMeasurements.erase(measurementIt);
			continue;
		}
		if (preceding.GetPosition().GetPressureAltitude() < minimumAltitude ||
			succeeding.GetPosition().GetPressureAltitude() < minimumAltitude) {
			measurementIt = Session->WakeMeasurements.erase(measurementIt);
			continue;
		}

		const CPosition precedingPosition = preceding.GetPosition().GetPosition();
		const CPosition succeedingPosition = succeeding.GetPosition().GetPosition();
		const double directDistanceNm = precedingPosition.DistanceTo(succeedingPosition);
		const double relativeBearing = SignedAngleDifference(
			precedingPosition.DirectionTo(succeedingPosition) - preceding.GetTrackHeading());
		double actualNm = std::abs(directDistanceNm * std::cos(
			relativeBearing * 3.14159265358979323846 / 180.0));
		const CFlightPlan precedingPlan = preceding.GetCorrelatedFlightPlan();
		const CFlightPlan succeedingPlan = succeeding.GetCorrelatedFlightPlan();
		const auto& precedingMotion = Session->PlanningMotion[measurement.PrecedingCallsign];
		const auto& succeedingMotion = Session->PlanningMotion[measurement.SucceedingCallsign];
		const bool previouslyDependent = measurement.ParallelDependent;
		const bool previouslyIndependent = measurement.ParallelIndependent;
		measurement.ParallelDependent = false;
		measurement.ParallelIndependent = false;
		if (precedingPlan.IsValid() && succeedingPlan.IsValid() &&
			CCallsignLookup::Available && CCallsignLookup::Lookup != nullptr) {
			const CFlightPlanData precedingData = precedingPlan.GetFlightPlanData();
			const CFlightPlanData succeedingData = succeedingPlan.GetFlightPlanData();
			const char* precedingAirport = precedingData.GetDestination();
			const char* precedingRunway = precedingData.GetArrivalRwy();
			const char* succeedingAirport = succeedingData.GetDestination();
			const char* succeedingRunway = succeedingData.GetArrivalRwy();
			const auto pairMode = CCallsignLookup::Lookup->getRunwayPairMode(
				Session->ActiveProfile, precedingAirport != nullptr ? precedingAirport : "",
				precedingRunway != nullptr ? precedingRunway : "", succeedingAirport != nullptr ? succeedingAirport : "",
				succeedingRunway != nullptr ? succeedingRunway : "");
			measurement.ParallelDependent = pairMode == CCallsignLookup::RunwayPairMode::Dependant;
			measurement.ParallelIndependent = pairMode == CCallsignLookup::RunwayPairMode::Independent;
		}
		if (previouslyDependent != measurement.ParallelDependent ||
			previouslyIndependent != measurement.ParallelIndependent)
			measurement.PlanningTurnTiming.Reset();
		const int assignedClearedAltitude = succeedingPlan.IsValid()
			? succeedingPlan.GetControllerAssignedData().GetClearedAltitude() : 0;
		measurement.ClearedForApproach = assignedClearedAltitude == 1 || assignedClearedAltitude == 2;
		const int precedingClearedAltitude = precedingPlan.IsValid()
			? precedingPlan.GetControllerAssignedData().GetClearedAltitude() : 0;
		const bool precedingClearedForApproach = precedingClearedAltitude == 1 || precedingClearedAltitude == 2;
		CPosition precedingJoinPoint;
		CPosition precedingRunwayThreshold;
		CPosition succeedingJoinPoint;
		CPosition succeedingRunwayThreshold;
		CPosition routeMarkerPosition;
		CPosition routeTrackPosition;
		bool hasPrecedingRoute = false;
		bool hasSucceedingRoute = false;
		if (precedingPlan.IsValid()) {
			const CFlightPlanData flightPlanData = precedingPlan.GetFlightPlanData();
			const char* destination = flightPlanData.GetDestination();
			const char* arrivalRunway = flightPlanData.GetArrivalRwy();
			if (destination != nullptr && arrivalRunway != nullptr)
				hasPrecedingRoute = FindLocalizerMarkerPosition(GetPlugIn(), destination, arrivalRunway,
					precedingPosition, preceding.GetTrackHeading(), 0.0, routeMarkerPosition,
					routeTrackPosition, precedingJoinPoint, precedingRunwayThreshold);
		}
		if (succeedingPlan.IsValid()) {
			const CFlightPlanData flightPlanData = succeedingPlan.GetFlightPlanData();
			const char* destination = flightPlanData.GetDestination();
			const char* arrivalRunway = flightPlanData.GetArrivalRwy();
			if (destination != nullptr && arrivalRunway != nullptr)
				hasSucceedingRoute = FindLocalizerMarkerPosition(GetPlugIn(), destination, arrivalRunway,
					succeedingPosition, succeeding.GetTrackHeading(), 0.0, routeMarkerPosition,
					routeTrackPosition, succeedingJoinPoint, succeedingRunwayThreshold);
		}
		const bool precedingPassedJoin = hasPrecedingRoute &&
			HasPassedRoutePoint(precedingPosition, preceding.GetTrackHeading(),
				precedingJoinPoint, precedingRunwayThreshold);
		const bool succeedingPassedJoin = hasSucceedingRoute &&
			HasPassedRoutePoint(succeedingPosition, succeeding.GetTrackHeading(),
				succeedingJoinPoint, succeedingRunwayThreshold);
		TurningPointMath::TurnRoute precedingTurnRoute;
		TurningPointMath::TurnRoute succeedingTurnRoute;
		const bool hasPrecedingTurnRoute = hasPrecedingRoute && !precedingPassedJoin &&
			BuildAircraftLocalizerRoute(precedingPosition, preceding.GetTrackHeading(), preceding.GetGS(),
				precedingJoinPoint, precedingRunwayThreshold, precedingTurnRoute, precedingClearedForApproach);
		const bool hasSucceedingTurnRoute = hasSucceedingRoute && !succeedingPassedJoin &&
			BuildAircraftLocalizerRoute(succeedingPosition, succeeding.GetTrackHeading(), succeeding.GetGS(),
				succeedingJoinPoint, succeedingRunwayThreshold, succeedingTurnRoute, measurement.ClearedForApproach);
		const double precedingDistanceToJoinNm = hasPrecedingTurnRoute ? precedingTurnRoute.BeforeFinalNm
			: (hasPrecedingRoute && !precedingPassedJoin ? precedingPosition.DistanceTo(precedingJoinPoint) : 0.0);
		const double precedingPlanDistanceToDestinationNm = precedingPlan.IsValid()
			? precedingPlan.GetDistanceToDestination() : 0.0;
		const double precedingJoinToThresholdNm = hasPrecedingTurnRoute ? precedingTurnRoute.FinalNm : hasPrecedingRoute
			? (precedingPassedJoin ? precedingPosition.DistanceTo(precedingRunwayThreshold)
				: precedingJoinPoint.DistanceTo(precedingRunwayThreshold))
			: precedingPlanDistanceToDestinationNm;
		const double succeedingDistanceToJoinNm = hasSucceedingTurnRoute ? succeedingTurnRoute.BeforeFinalNm
			: (hasSucceedingRoute && !succeedingPassedJoin ? succeedingPosition.DistanceTo(succeedingJoinPoint) : 0.0);
		const double succeedingPlanDistanceToDestinationNm = succeedingPlan.IsValid()
			? succeedingPlan.GetDistanceToDestination() : 0.0;
		const double succeedingJoinToThresholdNm = hasSucceedingTurnRoute ? succeedingTurnRoute.FinalNm : hasSucceedingRoute
			? (succeedingPassedJoin ? succeedingPosition.DistanceTo(succeedingRunwayThreshold)
				: succeedingJoinPoint.DistanceTo(succeedingRunwayThreshold))
			: succeedingPlanDistanceToDestinationNm;
		TurningPointMath::TurnRoute precedingPlanningRoute;
		TurningPointMath::TurnRoute succeedingPlanningRoute;
		const bool hasPrecedingPlanningRoute = hasPrecedingRoute && !precedingPassedJoin &&
			BuildAircraftLocalizerRoute(precedingPosition, precedingMotion.TrackDegrees, precedingMotion.GroundSpeedKts,
				precedingJoinPoint, precedingRunwayThreshold, precedingPlanningRoute, precedingClearedForApproach);
		const bool hasSucceedingPlanningRoute = hasSucceedingRoute && !succeedingPassedJoin &&
			BuildAircraftLocalizerRoute(succeedingPosition, succeedingMotion.TrackDegrees, succeedingMotion.GroundSpeedKts,
				succeedingJoinPoint, succeedingRunwayThreshold, succeedingPlanningRoute, measurement.ClearedForApproach);
		const double precedingPlanningDistanceNm = hasPrecedingPlanningRoute
			? precedingPlanningRoute.BeforeFinalNm : precedingDistanceToJoinNm;
		const double succeedingPlanningDistanceNm = hasSucceedingPlanningRoute
			? succeedingPlanningRoute.BeforeFinalNm : succeedingDistanceToJoinNm;
		const double precedingPlanningFinalNm = hasPrecedingPlanningRoute
			? precedingPlanningRoute.FinalNm : precedingJoinToThresholdNm;
		const double succeedingPlanningFinalNm = hasSucceedingPlanningRoute
			? succeedingPlanningRoute.FinalNm : succeedingJoinToThresholdNm;
		if (hasSucceedingRoute) {
			const double remainingNm = succeedingPlanningDistanceNm + succeedingPlanningFinalNm;
			measurement.ExpectedRoutePosition = SeparationMath::Extrapolate(succeedingRunwayThreshold,
				TrueBearingDegrees(succeedingRunwayThreshold, succeedingJoinPoint),
				(std::max)(0.0, remainingNm) * 1852.0);
			measurement.HasExpectedRoutePosition = true;
		}
		if (measurement.ParallelIndependent) {
			measurement.TurnStatus = "INDEPENDENT";
			measurement.TurnStatusDetail = "This runway pair is configured Independent; no inter-runway spacing target is applied";
			measurement.PlanningTurnTiming.Reset();
			const bool previewSucceedingRoute = Session->RoutePreviewCallsign == measurement.SucceedingCallsign;
			if (hasSucceedingRoute && (showLocalizerRoute || previewSucceedingRoute))
				DrawAircraftLocalizerRoute(this, dc, succeedingPosition, succeeding.GetTrackHeading(),
					succeedingJoinPoint, succeedingRunwayThreshold,
					succeedingPassedJoin || measurement.ClearedForApproach,
					hasSucceedingTurnRoute ? &succeedingTurnRoute : nullptr);
			++measurementIt;
			continue;
		}
		if (hasPrecedingRoute && hasSucceedingRoute) {
			const double precedingRemainingRouteNm = precedingDistanceToJoinNm + precedingJoinToThresholdNm;
			const double succeedingRemainingRouteNm = succeedingDistanceToJoinNm + succeedingJoinToThresholdNm;
			const double ownOutwardHeading = TrueBearingDegrees(precedingRunwayThreshold, precedingJoinPoint);
			const double targetOutwardHeading = TrueBearingDegrees(succeedingRunwayThreshold, succeedingJoinPoint);
			const double ownBearingRadians = SignedAngleDifference(
				TrueBearingDegrees(precedingRunwayThreshold, precedingPosition) - ownOutwardHeading) * TurningPointMath::Pi / 180.0;
			const double targetBearingRadians = SignedAngleDifference(
				TrueBearingDegrees(succeedingRunwayThreshold, precedingPosition) - targetOutwardHeading) * TurningPointMath::Pi / 180.0;
			const double ownAlongNm = precedingRunwayThreshold.DistanceTo(precedingPosition) * std::cos(ownBearingRadians);
			const double targetAlongNm = succeedingRunwayThreshold.DistanceTo(precedingPosition) * std::cos(targetBearingRadians);
			actualNm = succeedingRemainingRouteNm - TurningPointMath::RouteDistanceOnLocalizerNm(
				precedingRemainingRouteNm, ownAlongNm, targetAlongNm);
			measurement.HasRouteBasedCurrentSpacing = true;
		}
		double finalTargetNm = measurement.RequiredNm;
		double compressionNm = 0.0;
		if (measurement.ParallelDependent) {
			const double minimumParallelNm = CCallsignLookup::Available && CCallsignLookup::Lookup != nullptr
				? CCallsignLookup::Lookup->getMinimumParallelDependentDistanceNm() : 3.0;
			const double finalBufferNm = CCallsignLookup::Available && CCallsignLookup::Lookup != nullptr
				? CCallsignLookup::Lookup->getFinalBufferNm() : 1.0;
			finalTargetNm = minimumParallelNm + finalBufferNm;
		}
		if (!precedingPlan.IsValid() || !succeedingPlan.IsValid()) {
			if (!measurement.ParallelDependent) {
				++measurementIt;
				continue;
			}
		}
		else {
			const char precedingWake = precedingPlan.GetFlightPlanData().GetAircraftWtc();
			const char succeedingWake = succeedingPlan.GetFlightPlanData().GetAircraftWtc();
			const auto precedingSpeedOverride = Session->FinalApproachSpeedOverridesKts.find(
				measurement.PrecedingCallsign);
			const auto succeedingSpeedOverride = Session->FinalApproachSpeedOverridesKts.find(
				measurement.SucceedingCallsign);
			compressionNm = CalculateCompressionMarginNm(
				precedingWake, succeedingWake,
				precedingSpeedOverride == Session->FinalApproachSpeedOverridesKts.end()
					? 0 : precedingSpeedOverride->second,
				succeedingSpeedOverride == Session->FinalApproachSpeedOverridesKts.end()
					? 0 : succeedingSpeedOverride->second,
				precedingMotion.GroundSpeedKts, precedingPlanningDistanceNm, precedingPlanningFinalNm,
				precedingPosition, precedingMotion.TrackDegrees,
				succeedingMotion.GroundSpeedKts, succeedingPlanningDistanceNm, succeedingPlanningFinalNm,
				succeedingPosition, succeedingMotion.TrackDegrees,
				ClearedActualSpeedBoundaryNm(precedingPlan), ClearedActualSpeedBoundaryNm(succeedingPlan), WeatherService);
		}
		const bool gapGoverning = measurement.GapNm > finalTargetNm;
		if (gapGoverning)
			finalTargetNm = measurement.GapNm;
		const double initialTargetNm = finalTargetNm + compressionNm;
		const double requiredNm = initialTargetNm;
		measurement.CurrentFTDNm = finalTargetNm;
		measurement.CurrentITDNm = initialTargetNm;
		measurement.CurrentCompressionNm = compressionNm;
		measurement.CurrentRequiredNm = requiredNm;
		measurement.CurrentActualNm = actualNm;
		measurement.HasCurrentSpacing = true;
		double markerHeading = preceding.GetTrackHeading() + 180.0;
		if (markerHeading >= 360.0)
			markerHeading -= 360.0;
		CPosition markerPosition = SeparationMath::Extrapolate(
			precedingPosition, markerHeading, requiredNm * 1852.0);
		CPosition finalMarkerPosition = SeparationMath::Extrapolate(
			precedingPosition, markerHeading, finalTargetNm * 1852.0);
		CPosition precedingTrackPosition = SeparationMath::Extrapolate(
			precedingPosition, preceding.GetTrackHeading(), 1852.0);
		CPosition finalTrackPosition = precedingTrackPosition;
		CPosition localizerJoinPoint = succeedingJoinPoint;
		CPosition runwayThreshold = succeedingRunwayThreshold;
		bool hasLocalizerMarkerPosition = hasSucceedingRoute;
		bool hasFinalTargetMarkerPosition = hasSucceedingRoute;
		if (hasLocalizerMarkerPosition && hasPrecedingRoute) {
			const double outboundHeading = TrueBearingDegrees(succeedingRunwayThreshold, succeedingJoinPoint);
			const double remainingNm = precedingPlanningDistanceNm + precedingPlanningFinalNm;
			const double leaderBearingRadians = SignedAngleDifference(
				TrueBearingDegrees(succeedingRunwayThreshold, precedingPosition) - outboundHeading) * TurningPointMath::Pi / 180.0;
			const double leaderAlongLocalizerNm = succeedingRunwayThreshold.DistanceTo(precedingPosition) * std::cos(leaderBearingRadians);
			const double ownBearingRadians = SignedAngleDifference(TrueBearingDegrees(precedingRunwayThreshold, precedingPosition) -
				TrueBearingDegrees(precedingRunwayThreshold, precedingJoinPoint)) * TurningPointMath::Pi / 180.0;
			const double leaderAlongOwnLocalizerNm = precedingRunwayThreshold.DistanceTo(precedingPosition) * std::cos(ownBearingRadians);
			markerPosition = SeparationMath::Extrapolate(succeedingRunwayThreshold,
				outboundHeading, TurningPointMath::MarkerDistanceNm(remainingNm, leaderAlongOwnLocalizerNm, leaderAlongLocalizerNm, requiredNm) * 1852.0);
			finalMarkerPosition = SeparationMath::Extrapolate(succeedingRunwayThreshold,
				outboundHeading, TurningPointMath::MarkerDistanceNm(remainingNm, leaderAlongOwnLocalizerNm, leaderAlongLocalizerNm, finalTargetNm) * 1852.0);
			precedingTrackPosition = SeparationMath::Extrapolate(markerPosition,
				std::fmod(outboundHeading + 180.0, 360.0), 1852.0);
			finalTrackPosition = SeparationMath::Extrapolate(finalMarkerPosition,
				std::fmod(outboundHeading + 180.0, 360.0), 1852.0);
		}
		else if (hasLocalizerMarkerPosition) {
			const CFlightPlanData flightPlanData = succeedingPlan.GetFlightPlanData();
			const char* destination = flightPlanData.GetDestination();
			const char* arrivalRunway = flightPlanData.GetArrivalRwy();
			if (destination != nullptr && arrivalRunway != nullptr && arrivalRunway[0] != '\0')
				hasLocalizerMarkerPosition = FindLocalizerMarkerPosition(GetPlugIn(),
					destination == nullptr ? "" : destination, arrivalRunway,
					precedingPosition, preceding.GetTrackHeading(), requiredNm,
					markerPosition, precedingTrackPosition, localizerJoinPoint, runwayThreshold);
			if (destination != nullptr && arrivalRunway != nullptr && arrivalRunway[0] != '\0') {
				CPosition finalJoinPoint;
				CPosition finalThreshold;
				hasFinalTargetMarkerPosition = FindLocalizerMarkerPosition(GetPlugIn(),
					destination, arrivalRunway,
					precedingPosition, preceding.GetTrackHeading(), finalTargetNm,
					finalMarkerPosition, finalTrackPosition, finalJoinPoint, finalThreshold);
			}
		}
		if (hasLocalizerMarkerPosition) {
			const auto upstreamMeasurement = std::find_if(Session->WakeMeasurements.begin(), Session->WakeMeasurements.end(),
				[&measurement](const WakeMeasurement& candidate) {
					return &candidate != &measurement &&
						candidate.SucceedingCallsign == measurement.PrecedingCallsign;
				});
			if (upstreamMeasurement != Session->WakeMeasurements.end()) {
				const double outboundHeading = TrueBearingDegrees(runwayThreshold, localizerJoinPoint);
				const auto alongLocalizerNm = [&runwayThreshold, outboundHeading](CPosition position) {
					const double distanceNm = runwayThreshold.DistanceTo(position);
					const double bearing = TrueBearingDegrees(runwayThreshold, position);
					const double relativeBearing = SignedAngleDifference(bearing - outboundHeading) *
						3.14159265358979323846 / 180.0;
					return distanceNm * std::cos(relativeBearing);
				};
				const double upstreamFtdAlongLocalizerNm = upstreamMeasurement->HasFinalTargetPosition
					? alongLocalizerNm(upstreamMeasurement->FinalTargetPosition)
					: (upstreamMeasurement->HasExpectedRoutePosition
						? alongLocalizerNm(upstreamMeasurement->ExpectedRoutePosition) : alongLocalizerNm(precedingPosition));
				const double upstreamExpectedAlongLocalizerNm = upstreamMeasurement->HasExpectedRoutePosition
					? alongLocalizerNm(upstreamMeasurement->ExpectedRoutePosition)
					: (upstreamMeasurement->ClearedForApproach ? alongLocalizerNm(precedingPosition) : upstreamFtdAlongLocalizerNm);
				const double upstreamReferenceAlongLocalizerNm = TurningPointMath::DownstreamReferenceNm(
					upstreamExpectedAlongLocalizerNm, upstreamFtdAlongLocalizerNm, upstreamMeasurement->ClearedForApproach);
				const double markerDistanceNm = runwayThreshold.DistanceTo(markerPosition);
				const double markerBearing = TrueBearingDegrees(runwayThreshold, markerPosition);
				const double markerRelativeBearing =
					SignedAngleDifference(markerBearing - outboundHeading) * 3.14159265358979323846 / 180.0;
				const double markerAlongLocalizerNm = markerDistanceNm * std::cos(markerRelativeBearing);
				const double minimumSlotDistanceNm = upstreamReferenceAlongLocalizerNm + requiredNm;
				if (markerAlongLocalizerNm < minimumSlotDistanceNm) {
					markerPosition = SeparationMath::Extrapolate(
						runwayThreshold, outboundHeading, minimumSlotDistanceNm * 1852.0);
					precedingTrackPosition = SeparationMath::Extrapolate(
						markerPosition, std::fmod(outboundHeading + 180.0, 360.0), 1852.0);
				}
				if (upstreamMeasurement->HasFinalTargetPosition) {
					const double finalMarkerDistanceNm = runwayThreshold.DistanceTo(finalMarkerPosition);
					const double finalMarkerBearing = TrueBearingDegrees(runwayThreshold, finalMarkerPosition);
					const double finalMarkerRelativeBearing =
						SignedAngleDifference(finalMarkerBearing - outboundHeading) * 3.14159265358979323846 / 180.0;
					const double finalMarkerAlongLocalizerNm =
						finalMarkerDistanceNm * std::cos(finalMarkerRelativeBearing);
					const double minimumFinalSlotDistanceNm = upstreamReferenceAlongLocalizerNm + finalTargetNm;
					if (finalMarkerAlongLocalizerNm < minimumFinalSlotDistanceNm)
						finalMarkerPosition = SeparationMath::Extrapolate(
							runwayThreshold, outboundHeading, minimumFinalSlotDistanceNm * 1852.0);
				}
			}
			measurement.TargetSlotPosition = markerPosition;
			measurement.HasTargetSlotPosition = true;
		}
		measurement.FinalTargetPosition = finalMarkerPosition;
		measurement.HasFinalTargetPosition = true;
		const POINT markerPoint = ConvertCoordFromPositionToPixel(markerPosition);
		const POINT finalTargetPoint = ConvertCoordFromPositionToPixel(finalMarkerPosition);
		const POINT precedingPoint = ConvertCoordFromPositionToPixel(
			hasLocalizerMarkerPosition ? markerPosition : precedingPosition);
		const POINT precedingTrackPoint = ConvertCoordFromPositionToPixel(precedingTrackPosition);
		const bool highlightPairMarkers =
			Session->RightClickedSequenceCallsign == measurement.SucceedingCallsign;
		const bool previewSucceedingRoute = Session->RoutePreviewCallsign == measurement.SucceedingCallsign;
		if (hasLocalizerMarkerPosition && (showLocalizerRoute || previewSucceedingRoute)) {
			CPen routePen(PS_DASH, 1, ConfiguredColor("COLOR_ITD_ROUTE"));
			const int routeState = dc.SaveDC();
			dc.SelectObject(&routePen);
			DrawAircraftLocalizerRoute(this, dc, succeedingPosition, succeeding.GetTrackHeading(),
				succeedingJoinPoint, succeedingRunwayThreshold, succeedingPassedJoin || measurement.ClearedForApproach,
				hasSucceedingTurnRoute ? &succeedingTurnRoute : nullptr);
			dc.RestoreDC(routeState);
		}
		if (measurement.ClearedForApproach) {
			measurement.PlanningTurnTiming.Reset();
			measurement.TurnStatus = "CLEARED";
			measurement.TurnStatusDetail = "Approach clearance set; turn recommendations are stopped";
		}
		else if (!hasPrecedingRoute || !hasSucceedingRoute) {
			measurement.PlanningTurnTiming.Reset();
			measurement.TurnStatus = "NO RWY";
			measurement.TurnStatusDetail = "Both aircraft need an assigned arrival runway available in the active sector file";
		}
		else if (succeedingPassedJoin) {
			measurement.PlanningTurnTiming.Reset();
			measurement.TurnStatus = "ON FINAL";
			measurement.TurnStatusDetail = "Aircraft is already established on final inside its localizer join point";
		}
		else if (PredictionStability::CanRecommendTurn(measurement.ClearedForApproach,
			succeedingPassedJoin) && precedingPlan.IsValid() && succeedingPlan.IsValid()) {
			const double outboundHeading = TrueBearingDegrees(succeedingRunwayThreshold, succeedingJoinPoint);
			const double localizerPointNm = succeedingRunwayThreshold.DistanceTo(succeedingJoinPoint);
			const double precedingFinalHeading = std::fmod(
				TrueBearingDegrees(precedingRunwayThreshold, precedingJoinPoint) + 180.0, 360.0);
			const auto leaderSpeed = TurningSpeedProfile(preceding, precedingPlan,
				precedingFinalHeading, precedingMotion, *Session, WeatherService);
			const auto followerSpeed = TurningSpeedProfile(succeeding, succeedingPlan,
				std::fmod(outboundHeading + 180.0, 360.0), succeedingMotion, *Session, WeatherService);
			const bool validLeaderProfile = TurningPointMath::TimeToThresholdHours(
				precedingPlanningDistanceNm, precedingPlanningFinalNm, leaderSpeed) >= 0.0;
			const double leaderReportAge = (std::max)(0, (std::min)(30, preceding.GetPosition().GetReceivedTime()));
			const double followerReportAge = (std::max)(0, (std::min)(30, succeeding.GetPosition().GetReceivedTime()));
			const double bearingRadians = SignedAngleDifference(
				TrueBearingDegrees(succeedingRunwayThreshold, succeedingPosition) - outboundHeading) *
				TurningPointMath::Pi / 180.0;
			const double thresholdDistanceNm = succeedingRunwayThreshold.DistanceTo(succeedingPosition);
			const double relativeTrackRadians = SignedAngleDifference(
				succeedingMotion.TrackDegrees - outboundHeading) * TurningPointMath::Pi / 180.0;
			measurement.TurnStatus = "NO MATCH";
			measurement.TurnStatusDetail = "Route remains modeled, but no turn point delivers to the ITD slot with LOCALIZERPOINT constraints within 10 minutes or 40 NM";
			if (!validLeaderProfile || followerSpeed.CurrentKts < 80.0 ||
				TurningPointMath::TimeToThresholdHours(0.0, 0.0, followerSpeed) < 0.0) {
				measurement.TurnStatus = "NO SPEED";
				measurement.TurnStatusDetail = "Turn prediction requires valid speed data and a follower groundspeed of at least 80 kt";
			}
			const auto precedingOverride = Session->FinalApproachSpeedOverridesKts.find(measurement.PrecedingCallsign);
			const auto succeedingOverride = Session->FinalApproachSpeedOverridesKts.find(measurement.SucceedingCallsign);
			const std::string timingContext = measurement.PrecedingCallsign + "|" + precedingMotion.Context + "|" +
				std::to_string(precedingMotion.Revision) + "|" + succeedingMotion.Context + "|" +
				std::to_string(succeedingMotion.Revision) + "|" + std::to_string(finalTargetNm) + "|" +
				Session->ActiveProfile + "|" +
				std::to_string(precedingOverride != Session->FinalApproachSpeedOverridesKts.end() ? precedingOverride->second : 0) + "|" +
				std::to_string(succeedingOverride != Session->FinalApproachSpeedOverridesKts.end() ? succeedingOverride->second : 0);
			const double itdBearingRadians = SignedAngleDifference(
				TrueBearingDegrees(succeedingRunwayThreshold, markerPosition) - outboundHeading) * TurningPointMath::Pi / 180.0;
			const double itdSlotDistanceNm = (std::max)(0.0,
				succeedingRunwayThreshold.DistanceTo(markerPosition) * std::cos(itdBearingRadians));
			const TurningPointMath::ItdTarget itdTarget{ itdSlotDistanceNm, precedingPlanningDistanceNm,
				precedingPlanningFinalNm, leaderSpeed, (leaderReportAge - followerReportAge) / 3600.0 };
			const bool hasUsablePredictionData = precedingMotion.Initialized && succeedingMotion.Initialized &&
				validLeaderProfile && followerSpeed.CurrentKts >= 80.0 && followerSpeed.FinalKts > 0.0 &&
				TurningPointMath::TimeToThresholdHours(0.0, 0.0, followerSpeed) >= 0.0;
			TurningPointMath::TurnRoute turnRoute;
			const bool freshSolution = hasUsablePredictionData &&
				TurningPointMath::FindEarliestItdTurnPoint(thresholdDistanceNm * std::cos(bearingRadians),
					thresholdDistanceNm * std::sin(bearingRadians), relativeTrackRadians, localizerPointNm, followerSpeed,
					itdTarget, turnRoute, followerReportAge * followerSpeed.CurrentKts / 3600.0);
			bool reusedSolution = false;
			if (!freshSolution && hasUsablePredictionData && measurement.PlanningTurnTiming.Initialized) {
				const double secondsToTurn = measurement.PlanningTurnTiming.DeadlineSeconds - predictionNowSeconds;
				if (TurningPointMath::BuildRoute(thresholdDistanceNm * std::cos(bearingRadians),
					thresholdDistanceNm * std::sin(bearingRadians), relativeTrackRadians, followerSpeed.CurrentKts,
					(secondsToTurn + followerReportAge) * followerSpeed.CurrentKts / 3600.0, localizerPointNm, turnRoute)) {
					const double itdErrorNm = TurningPointMath::ItdDeliveryErrorNm(turnRoute, followerSpeed, itdTarget);
					reusedSolution = measurement.PlanningTurnTiming.CanReuse(predictionNowSeconds, timingContext,
						itdErrorNm / followerSpeed.CurrentKts * 3600.0, secondsToTurn >= 0.0 && itdErrorNm >= 0.0);
				}
			}
			if (freshSolution || reusedSolution) {
				const bool largerGapSolution = freshSolution && turnRoute.ItdExcessNm > 0.02;
				const double rawSecondsToTurn = (std::max)(0.0,
					turnRoute.StartDistanceNm / followerSpeed.CurrentKts * 3600.0 - followerReportAge);
				if (freshSolution)
					measurement.PlanningTurnTiming.Reset();
				const double secondsToTurn = reusedSolution
					? measurement.PlanningTurnTiming.DeadlineSeconds - predictionNowSeconds
					: measurement.PlanningTurnTiming.Update(rawSecondsToTurn, predictionNowSeconds, timingContext);
				const double turnSeconds = (std::max)(0.0, secondsToTurn);
				if (turnSeconds <= 0.001) {
					measurement.PlanningTurnTiming.Reset();
					measurement.TurnStatus = "NO DELAY";
					measurement.TurnStatusDetail = "The aircraft has reached the earliest modeled turn point; no delay is needed";
				}
				else {
					const double turnDistanceNm = turnSeconds * followerSpeed.CurrentKts / 3600.0;
					if (Session->ShowTurningPoints) {
						const CPosition turnPosition = SeparationMath::Extrapolate(succeedingPosition,
							succeedingMotion.TrackDegrees, turnRoute.StartDistanceNm * 1852.0);
						const POINT turnPixel = ConvertCoordFromPositionToPixel(turnPosition);
						const int turnState = dc.SaveDC();
						const COLORREF turnColor = ConfiguredColor(reusedSolution ? "COLOR_TURN_PASSED" : "COLOR_TURN_POINT");
						CPen turnPen(reusedSolution ? PS_DOT : PS_DASH, 1, turnColor);
						dc.SelectObject(&turnPen);
						dc.MoveTo(ConvertCoordFromPositionToPixel(succeedingPosition));
						dc.LineTo(turnPixel);
						if (showLocalizerRoute || previewSucceedingRoute) {
							DrawAircraftLocalizerRoute(this, dc, succeedingPosition, succeedingMotion.TrackDegrees,
								succeedingJoinPoint, succeedingRunwayThreshold, false, &turnRoute);
						}
						CPen circlePen(PS_SOLID, 2, turnColor);
						dc.SelectObject(&circlePen);
						dc.SelectStockObject(NULL_BRUSH);
						dc.Ellipse(turnPixel.x - 6, turnPixel.y - 6, turnPixel.x + 7, turnPixel.y + 7);
						dc.RestoreDC(turnState);
					}
					const char* turnCommand = turnRoute.TurnAngleRadians < -1.0e-6 ? "turn LEFT"
						: (turnRoute.TurnAngleRadians > 1.0e-6 ? "turn RIGHT" : "proceed");
					char tooltip[240] = {};
					std::snprintf(tooltip, sizeof(tooltip),
						"%s: earliest modeled point to %s toward LOCALIZERPOINT (%.1f NM) with required ITD spacing: %.0f s / %.1f NM ahead",
						measurement.SucceedingCallsign.c_str(), turnCommand, localizerPointNm, turnSeconds, turnDistanceNm);
					if (largerGapSolution)
						std::snprintf(tooltip, sizeof(tooltip),
							"%s: earliest modeled point to %s toward LOCALIZERPOINT: %.0f s ahead; leaves %.1f NM extra behind ITD",
							measurement.SucceedingCallsign.c_str(), turnCommand, turnSeconds, turnRoute.ItdExcessNm);
					if (reusedSolution)
						std::snprintf(tooltip, sizeof(tooltip),
							"%s: UPDATING; previous turn point retained briefly while prediction recalculates; not a new turn instruction",
							measurement.SucceedingCallsign.c_str());
					char status[32] = {};
					std::snprintf(status, sizeof(status), largerGapSolution ? "TURN %.0fs+" : "TURN %.0fs", turnSeconds);
					const auto cue = PredictionStability::EarliestTurnCue(turnSeconds);
					measurement.TurnStatus = reusedSolution ? "UPDATING" :
						(cue == PredictionStability::TurnCue::Soon
							? (largerGapSolution ? "TURN SOON+" : "TURN SOON") : status);
					measurement.TurnStatusDetail = tooltip;
				}
			}
			else
				measurement.PlanningTurnTiming.Reset();
		}
		else
			measurement.PlanningTurnTiming.Reset();
		DrawFinalTargetMarker(&dc,
			finalTargetPoint,
			ConvertCoordFromPositionToPixel(hasFinalTargetMarkerPosition ? finalMarkerPosition : precedingPosition),
			ConvertCoordFromPositionToPixel(finalTrackPosition), finalTargetNm, actualNm,
			measurement.ParallelDependent, gapGoverning, measurement.ClearedForApproach,
			highlightPairMarkers);
		if (compressionNm > 0.05) {
			DrawWakeSpacingMarker(
				&dc, markerPoint, precedingPoint, precedingTrackPoint,
				measurement.ParallelDependent, highlightPairMarkers);
		}
		++measurementIt;
	}
	RECT screenBounds = {};
	dc.GetClipBox(&screenBounds);
	DrawFTDiRadarOverlay(this, dc, screenBounds, Session);

	dc.RestoreDC(savedState);
	dc.Detach();
	if (SelectionStage != SequenceSelectionStage::Idle)
		RequestRefresh();
}

void RadarScreen::StartSequenceInsertion()
{
	SelectionStage = SequenceSelectionStage::AwaitAircraft;
	SequenceSelectionActive.store(true);
	SequencePickArmed.store(false);
	SequenceAircraftClickRequested.store(false);
	SequenceCancelRequested.store(false);
	RequestRefresh();
}

void RadarScreen::AppendAircraftToSequence(const std::string& callsign)
{
	if (SelectionStage != SequenceSelectionStage::AwaitAircraft)
		return;
	const CRadarTarget target = GetPlugIn()->RadarTargetSelect(callsign.c_str());
	if (!target.IsValid() || target.GetPosition().GetPressureAltitude() < MinimumAltitudeFeet()) {
		return;
	}
	auto existing = std::find(Session->ArrivalSequence.begin(), Session->ArrivalSequence.end(), callsign);
	if (existing != Session->ArrivalSequence.end())
		Session->ArrivalSequence.erase(existing);
	Session->ArrivalSequence.push_back(callsign);
	Session->SequenceDirty = true;
	SelectionStage = SequenceSelectionStage::Idle;
	SequenceSelectionActive.store(false);
	SequencePickArmed.store(false);
	RequestRefresh();
}

void RadarScreen::SynchronizeSequenceMeasurements()
{
	std::vector<WakeMeasurement> measurements;
	if (Session->ArrivalSequence.size() > 1)
		measurements.reserve(Session->ArrivalSequence.size() - 1);

	for (size_t index = 1; index < Session->ArrivalSequence.size(); ++index) {
		const std::string& succeedingCallsign = Session->ArrivalSequence[index];
		const CRadarTarget succeeding = GetPlugIn()->RadarTargetSelect(succeedingCallsign.c_str());
		if (!succeeding.IsValid())
			continue;
		const CFlightPlan succeedingPlan = succeeding.GetCorrelatedFlightPlan();
		if (!succeedingPlan.IsValid())
			continue;
		const CFlightPlanData succeedingData = succeedingPlan.GetFlightPlanData();
		const char* succeedingAirport = succeedingData.GetDestination();
		const char* succeedingRunway = succeedingData.GetArrivalRwy();
		const size_t precedingIndex = FindSequencePredecessor(index, [&](size_t candidateIndex) {
			const CRadarTarget candidate = GetPlugIn()->RadarTargetSelect(Session->ArrivalSequence[candidateIndex].c_str());
			if (!candidate.IsValid() || !CCallsignLookup::Available || CCallsignLookup::Lookup == nullptr)
				return true;
			const CFlightPlan candidatePlan = candidate.GetCorrelatedFlightPlan();
			if (!candidatePlan.IsValid())
				return true;
			const CFlightPlanData candidateData = candidatePlan.GetFlightPlanData();
			const char* candidateAirport = candidateData.GetDestination();
			const char* candidateRunway = candidateData.GetArrivalRwy();
			return CCallsignLookup::Lookup->getRunwayPairMode(Session->ActiveProfile,
				candidateAirport != nullptr ? candidateAirport : "", candidateRunway != nullptr ? candidateRunway : "",
				succeedingAirport != nullptr ? succeedingAirport : "", succeedingRunway != nullptr ? succeedingRunway : "") !=
				CCallsignLookup::RunwayPairMode::Independent;
		});
		if (precedingIndex == index)
			continue;
		const std::string& precedingCallsign = Session->ArrivalSequence[precedingIndex];
		const CRadarTarget preceding = GetPlugIn()->RadarTargetSelect(precedingCallsign.c_str());
		if (!preceding.IsValid())
			continue;
		const CFlightPlan precedingPlan = preceding.GetCorrelatedFlightPlan();
		if (!precedingPlan.IsValid())
			continue;

		const char precedingWake = precedingPlan.GetFlightPlanData().GetAircraftWtc();
		const char succeedingWake = succeedingPlan.GetFlightPlanData().GetAircraftWtc();
		const auto previous = std::find_if(Session->WakeMeasurements.begin(), Session->WakeMeasurements.end(),
			[&precedingCallsign, &succeedingCallsign](const WakeMeasurement& candidate) {
				return candidate.PrecedingCallsign == precedingCallsign &&
					candidate.SucceedingCallsign == succeedingCallsign;
			});
		WakeMeasurement measurement = { precedingCallsign, succeedingCallsign,
			RequiredSpacingNm(precedingWake, succeedingWake) };
		if (previous != Session->WakeMeasurements.end())
			measurement = *previous;
		measurement.RequiredNm = RequiredSpacingNm(precedingWake, succeedingWake);
		measurements.push_back(measurement);
	}

	Session->WakeMeasurements.swap(measurements);
	Session->SequenceDirty = false;
}

void RadarScreen::OnButtonDownScreenObject(int ObjectType, const char * sObjectId, POINT Pt, RECT Area, int Button)
{
	UNREFERENCED_PARAMETER(Area);
	if (ObjectType == SCREEN_PANEL_MOVE && Button == BUTTON_LEFT) {
		Session->ManagerDragging = true;
		Session->ManagerDragOffset.x = Pt.x - Session->ManagerPosition.x;
		Session->ManagerDragOffset.y = Pt.y - Session->ManagerPosition.y;
		return;
	}
	if (ObjectType == SCREEN_PANEL_TOGGLE && Button == BUTTON_LEFT) {
		Session->ManagerExpanded = !Session->ManagerExpanded;
		RequestRefresh();
		return;
	}
	if (HandleFTDiPanelButton(this, *Session, ObjectType, sObjectId, Button, Area)) {
		RequestRefresh();
		return;
	}

	if (SelectionStage != SequenceSelectionStage::Idle && Button == BUTTON_RIGHT) {
		SelectionStage = SequenceSelectionStage::Idle;
		SequenceSelectionActive.store(false);
		SequencePickArmed.store(false);
		SequenceAircraftClickRequested.store(false);
		SequenceCancelRequested.store(false);
		RequestRefresh();
		return;
	}
}

void RadarScreen::OnMoveScreenObject(
	int ObjectType, const char* sObjectId, POINT Pt, RECT Area, bool Released)
{
	if (ObjectType != SCREEN_PANEL_MOVE || sObjectId == nullptr ||
		std::strcmp(sObjectId, "MOVE") != 0)
		return;
	if (!Session->ManagerDragging) {
		Session->ManagerDragging = true;
		Session->ManagerDragOffset.x = Pt.x - Session->ManagerPosition.x;
		Session->ManagerDragOffset.y = Pt.y - Session->ManagerPosition.y;
	}

	const RECT bounds = Session->RadarBounds;
	const int leftMargin = bounds.left + 4;
	const int topMargin = bounds.top + 4;
	const int managerWidth = Session->ManagerWindowWidth > 0
		? Session->ManagerWindowWidth : (Session->ManagerExpanded ? 228 : 178);
	int maximumLeft = bounds.right - managerWidth - 4;
	int maximumTop = bounds.bottom - Session->ManagerWindowHeight - 4;
	if (maximumLeft < leftMargin)
		maximumLeft = leftMargin;
	if (maximumTop < topMargin)
		maximumTop = topMargin;
	int left = Pt.x - Session->ManagerDragOffset.x;
	int top = Pt.y - Session->ManagerDragOffset.y;
	if (left < leftMargin)
		left = leftMargin;
	else if (left > maximumLeft)
		left = maximumLeft;
	if (top < topMargin)
		top = topMargin;
	else if (top > maximumTop)
		top = maximumTop;
	Session->ManagerPosition = { left, top };
	if (Released) {
		Session->ManagerDragging = false;
		const std::string position = std::to_string(left) + "," + std::to_string(top);
		GetPlugIn()->SaveDataToSettings("VFTDI_MANAGER_POSITION",
			"vFTDI sequence manager position", position.c_str());
	}
	RequestRefresh();
}

