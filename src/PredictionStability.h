#pragma once

#include <cmath>
#include <string>

namespace PredictionStability {
	enum class TurnCue { Countdown, Now, NoDelay };

	inline TurnCue EarliestTurnCue(double secondsToTurn)
	{
		return secondsToTurn > 10.0 ? TurnCue::Countdown :
			(secondsToTurn >= -10.0 ? TurnCue::Now : TurnCue::NoDelay);
	}

	struct TurnNowWindow {
		bool Initialized = false;
		double DeadlineSeconds = 0.0;
		double LastSeconds = 0.0;
		std::string Context;

		void Reset()
		{
			Initialized = false;
		}

		double Update(double secondsToTurn, double nowSeconds, const std::string& context)
		{
			if (!Initialized || context != Context || secondsToTurn > 0.001 || nowSeconds < LastSeconds)
				DeadlineSeconds = nowSeconds + secondsToTurn;
			else if (nowSeconds < DeadlineSeconds)
				DeadlineSeconds = nowSeconds;
			Context = context;
			LastSeconds = nowSeconds;
			Initialized = true;
			return DeadlineSeconds - nowSeconds;
		}
	};

	inline double HeadingDifference(double degrees)
	{
		return std::atan2(std::sin(degrees * 3.14159265358979323846 / 180.0),
			std::cos(degrees * 3.14159265358979323846 / 180.0)) * 180.0 / 3.14159265358979323846;
	}

	inline bool CanRecommendTurn(bool clearedForApproach, bool establishedOnFinal)
	{
		return !clearedForApproach && !establishedOnFinal;
	}

	struct Motion {
		bool Initialized = false;
		double TrackDegrees = 0.0;
		double GroundSpeedKts = 0.0;
		double LastSeconds = 0.0;
		unsigned int Revision = 0;
		std::string Context;

		void Update(double trackDegrees, double groundSpeedKts, double nowSeconds, const std::string& context)
		{
			if (!std::isfinite(trackDegrees) || !std::isfinite(groundSpeedKts) || groundSpeedKts <= 0.0) {
				Initialized = false;
				TrackDegrees = 0.0;
				GroundSpeedKts = 0.0;
				return;
			}
			const double elapsed = nowSeconds - LastSeconds;
			const double trackChange = HeadingDifference(trackDegrees - TrackDegrees);
			if (!Initialized || context != Context || elapsed < 0.0 || elapsed > 30.0 ||
				std::abs(trackChange) >= 15.0 || std::abs(groundSpeedKts - GroundSpeedKts) >= 25.0) {
				TrackDegrees = trackDegrees;
				GroundSpeedKts = groundSpeedKts;
				++Revision;
			}
			else {
				const double weight = 1.0 - std::exp(-elapsed / 8.0);
				TrackDegrees += weight * trackChange;
				GroundSpeedKts += weight * (groundSpeedKts - GroundSpeedKts);
			}
			TrackDegrees = std::fmod(TrackDegrees + 360.0, 360.0);
			Context = context;
			LastSeconds = nowSeconds;
			Initialized = true;
		}
	};

	struct TurnTiming {
		bool Initialized = false;
		double DeadlineSeconds = 0.0;
		double LastSeconds = 0.0;
		double LastSolutionSeconds = 0.0;
		std::string Context;

		void Reset()
		{
			Initialized = false;
		}

		bool CanReuse(double nowSeconds, const std::string& context, double timingErrorSeconds, bool eligible) const
		{
			return eligible && Initialized && context == Context &&
				nowSeconds >= LastSolutionSeconds && nowSeconds - LastSolutionSeconds <= 8.0 &&
				std::isfinite(timingErrorSeconds) && std::abs(timingErrorSeconds) <= 5.0;
		}

		double Update(double secondsToTurn, double nowSeconds, const std::string& context)
		{
			const double rawDeadline = nowSeconds + secondsToTurn;
			const double elapsed = nowSeconds - LastSeconds;
			const double change = rawDeadline - DeadlineSeconds;
			if (!Initialized || context != Context || elapsed < 0.0 || elapsed > 30.0 || std::abs(change) > 45.0)
				DeadlineSeconds = rawDeadline;
			else if (std::abs(change) > 2.0) {
				double adjustment = change * (1.0 - std::exp(-elapsed / 6.0));
				if (adjustment > elapsed)
					adjustment = elapsed;
				if (adjustment < -elapsed)
					adjustment = -elapsed;
				DeadlineSeconds += adjustment;
			}
			Context = context;
			LastSeconds = nowSeconds;
			LastSolutionSeconds = nowSeconds;
			Initialized = true;
			return DeadlineSeconds - nowSeconds;
		}
	};
}