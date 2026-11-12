#include "stdafx.h"
#include "AirportWeather.h"
#include "SeparationMath.h"
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <utility>
#include <wininet.h>

namespace {
bool ReadJsonString(const std::string& object, const std::string& key, std::string& value)
{
	const std::string property = "\"" + key + "\"";
	const size_t keyPosition = object.find(property);
	if (keyPosition == std::string::npos)
		return false;
	const size_t colon = object.find(':', keyPosition + property.size());
	if (colon == std::string::npos)
		return false;
	const size_t quote = object.find('"', colon + 1);
	if (quote == std::string::npos)
		return false;
	const size_t endQuote = object.find('"', quote + 1);
	if (endQuote == std::string::npos)
		return false;
	value = object.substr(quote + 1, endQuote - quote - 1);
	return true;
}

bool ReadJsonNumber(const std::string& object, const std::string& key, double& value)
{
	const std::string property = "\"" + key + "\"";
	const size_t keyPosition = object.find(property);
	if (keyPosition == std::string::npos)
		return false;
	const size_t colon = object.find(':', keyPosition + property.size());
	if (colon == std::string::npos)
		return false;
	const char* start = object.c_str() + colon + 1;
	while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n')
		++start;
	char* end = nullptr;
	value = std::strtod(start, &end);
	return end != start && std::isfinite(value);
}

std::vector<std::string> JsonObjects(const std::string& json)
{
	std::vector<std::string> objects;
	size_t objectStart = std::string::npos;
	int depth = 0;
	bool inString = false;
	bool escaped = false;
	for (size_t index = 0; index < json.size(); ++index) {
		const char character = json[index];
		if (inString) {
			if (escaped)
				escaped = false;
			else if (character == '\\')
				escaped = true;
			else if (character == '"')
				inString = false;
			continue;
		}
		if (character == '"') {
			inString = true;
		}
		else if (character == '{') {
			if (depth == 0)
				objectStart = index;
			++depth;
		}
		else if (character == '}' && depth > 0) {
			--depth;
			if (depth == 0 && objectStart != std::string::npos) {
				objects.push_back(json.substr(objectStart, index - objectStart + 1));
				objectStart = std::string::npos;
			}
		}
	}
	return objects;
}
}

AirportWeatherService::AirportWeatherService(std::vector<std::string> airportCodes)
	: AirportCodes(std::move(airportCodes))
{
	if (!AirportCodes.empty())
		Worker = std::thread(&AirportWeatherService::Run, this);
}

AirportWeatherService::~AirportWeatherService()
{
	Stop.store(true);
	if (Worker.joinable())
		Worker.join();
}

void AirportWeatherService::Run()
{
	while (!Stop.load()) {
		Refresh();
		for (int second = 0; second < 300 && !Stop.load(); ++second)
			std::this_thread::sleep_for(std::chrono::seconds(1));
	}
}

bool AirportWeatherService::Refresh()
{
	if (AirportCodes.empty())
		return false;

	std::string url = "https://aviationweather.gov/api/data/metar?ids=";
	for (size_t index = 0; index < AirportCodes.size(); ++index) {
		if (index != 0)
			url += ",";
		url += AirportCodes[index];
	}
	url += "&format=json";

	HINTERNET session = InternetOpenA(
		"vFTDI", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
	if (session == nullptr)
		return false;
	DWORD timeoutMs = 5000;
	InternetSetOptionA(session, INTERNET_OPTION_CONNECT_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
	InternetSetOptionA(session, INTERNET_OPTION_SEND_TIMEOUT, &timeoutMs, sizeof(timeoutMs));
	InternetSetOptionA(session, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeoutMs, sizeof(timeoutMs));

	HINTERNET request = InternetOpenUrlA(session, url.c_str(), nullptr, 0,
		INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_SECURE, 0);
	if (request == nullptr) {
		InternetCloseHandle(session);
		return false;
	}

	std::string response;
	char buffer[4096];
	DWORD bytesRead = 0;
	while (InternetReadFile(request, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
		response.append(buffer, bytesRead);
		if (response.size() > 1024 * 1024)
			break;
	}
	InternetCloseHandle(request);
	InternetCloseHandle(session);
	if (response.empty() || response.size() > 1024 * 1024)
		return false;

	std::vector<Observation> freshObservations;
	for (const std::string& object : JsonObjects(response)) {
		Observation observation;
		if (!ReadJsonString(object, "icaoId", observation.Icao) ||
			!ReadJsonNumber(object, "lat", observation.Latitude) ||
			!ReadJsonNumber(object, "lon", observation.Longitude))
			continue;

		double observationTime = 0.0;
		double windDirection = 0.0;
		double windSpeed = 0.0;
		if (ReadJsonNumber(object, "obsTime", observationTime) && observationTime > 0.0)
			observation.ObservationTime = static_cast<std::time_t>(observationTime);
		observation.HasWind = ReadJsonNumber(object, "wdir", windDirection) &&
			ReadJsonNumber(object, "wspd", windSpeed) && windSpeed >= 0.0;
		if (observation.HasWind) {
			observation.WindDirection = windDirection;
			observation.WindSpeedKts = windSpeed;
		}
		freshObservations.push_back(observation);
	}

	if (freshObservations.empty())
		return false;
	std::lock_guard<std::mutex> lock(ObservationsMutex);
	Observations = std::move(freshObservations);
	return true;
}

double AirportWeatherService::GetGroundSpeedKts(
	EuroScopePlugIn::CPosition position, double trackHeading, double indicatedSpeedKts) const
{
	const std::time_t now = std::time(nullptr);
	double nearestDistanceNm = HUGE_VAL;
	const Observation* nearestObservation = nullptr;
	std::lock_guard<std::mutex> lock(ObservationsMutex);
	for (const Observation& observation : Observations) {
		if (!observation.HasWind || observation.ObservationTime == 0 ||
			now - observation.ObservationTime > 90 * 60)
			continue;

		EuroScopePlugIn::CPosition airportPosition;
		airportPosition.m_Latitude = observation.Latitude;
		airportPosition.m_Longitude = observation.Longitude;
		const double distanceNm = position.DistanceTo(airportPosition);
		if (distanceNm < nearestDistanceNm) {
			nearestDistanceNm = distanceNm;
			nearestObservation = &observation;
		}
	}

	if (nearestObservation == nullptr)
		return indicatedSpeedKts;

	const double windAngle = SeparationMath::DegToRad(
		nearestObservation->WindDirection - trackHeading);
	const double tailwindKts = -nearestObservation->WindSpeedKts * std::cos(windAngle);
	const double groundSpeedKts = indicatedSpeedKts + tailwindKts;
	return groundSpeedKts > 0.0 ? groundSpeedKts : 0.0;
}