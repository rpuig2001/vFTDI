#pragma once

#include <cmath>
#include "EuroScopePlugIn.h"

namespace SeparationMath {
	constexpr double Pi = 3.14159265358979323846;
	inline bool Mode2LeaderEstablished(double alongNm, double crossNm, double headingDifferenceDegrees,
		double localizerToleranceNm, double headingToleranceDegrees)
	{
		return alongNm >= 0.0 && std::abs(crossNm) <= localizerToleranceNm &&
			std::abs(headingDifferenceDegrees) <= headingToleranceDegrees;
	}

	inline const char* Mode2ColorKey(bool clearedForApproach, double actualNm,
		double requiredNm, double warningNm, bool manual)
	{
		if (clearedForApproach && actualNm < requiredNm)
			return "COLOR_SEPARATION_BREACH";
		if (clearedForApproach && actualNm - requiredNm < warningNm)
			return "COLOR_SEPARATION_PREDICTION";
		return manual ? "COLOR_SEPARATION_MANUAL" : "COLOR_SEPARATION_NORMAL";
	}

	inline double DegToRad(double degrees)
	{
		return degrees / 180.0 * Pi;
	}

	inline double RadToDeg(double radians)
	{
		return radians / Pi * 180.0;
	}

	inline EuroScopePlugIn::CPosition Extrapolate(
		EuroScopePlugIn::CPosition initialPosition, double trackDegrees, double meters)
	{
		EuroScopePlugIn::CPosition newPosition;
		const double distance = (meters * 0.00053996) / 60.0 * Pi / 180.0;
		const double track = DegToRad(trackDegrees);
		const double latitude = DegToRad(initialPosition.m_Latitude);
		const double longitude = DegToRad(initialPosition.m_Longitude);
		const double newLatitude = std::asin(
			std::sin(latitude) * std::cos(distance) +
			std::cos(latitude) * std::sin(distance) * std::cos(track));
		const double newLongitude = std::cos(newLatitude) == 0
			? longitude
			: std::fmod(longitude + std::asin(std::sin(track) * std::sin(distance) /
				std::cos(newLatitude)) + Pi, 2.0 * Pi) - Pi;

		newPosition.m_Latitude = RadToDeg(newLatitude);
		newPosition.m_Longitude = RadToDeg(newLongitude);
		return newPosition;
	}
}