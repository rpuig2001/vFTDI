#pragma once

#include <atomic>
#include <ctime>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "EuroScopePlugIn.h"

class AirportWeatherService
{
public:
	explicit AirportWeatherService(std::vector<std::string> airportCodes);
	~AirportWeatherService();

	double GetGroundSpeedKts(
		EuroScopePlugIn::CPosition position, double trackHeading, double indicatedSpeedKts) const;

private:
	struct Observation {
		std::string Icao;
		double Latitude = 0.0;
		double Longitude = 0.0;
		double WindDirection = 0.0;
		double WindSpeedKts = 0.0;
		std::time_t ObservationTime = 0;
		bool HasWind = false;
	};

	void Run();
	bool Refresh();
	std::vector<std::string> AirportCodes;
	mutable std::mutex ObservationsMutex;
	std::vector<Observation> Observations;
	std::atomic<bool> Stop{ false };
	std::thread Worker;
};