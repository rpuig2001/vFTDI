#include "stdafx.h"
#include "CallsignLookup.h"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <fstream>
#include <sstream>

namespace {
std::string UppercaseIdentifier(std::string value)
{
	std::transform(value.begin(), value.end(), value.begin(),
		[](unsigned char character) { return static_cast<char>(std::toupper(character)); });
	return value;
}

std::string NormalizeRunway(std::string runway)
{
	runway = UppercaseIdentifier(runway);
	if (runway.size() == 1 || (runway.size() == 2 &&
		(runway[1] == 'L' || runway[1] == 'C' || runway[1] == 'R')))
		runway.insert(runway.begin(), '0');
	if ((runway.size() != 2 && runway.size() != 3) ||
		runway[0] < '0' || runway[0] > '9' || runway[1] < '0' || runway[1] > '9')
		return {};
	const int number = (runway[0] - '0') * 10 + runway[1] - '0';
	if (number < 1 || number > 36 || (runway.size() == 3 &&
		runway[2] != 'L' && runway[2] != 'C' && runway[2] != 'R'))
		return {};
	return runway;
}

std::string RunwayPairKey(std::string airport, std::string firstRunway, std::string secondRunway)
{
	airport = UppercaseIdentifier(airport);
	firstRunway = NormalizeRunway(firstRunway);
	secondRunway = NormalizeRunway(secondRunway);
	if (airport.size() != 4 || !std::all_of(airport.begin(), airport.end(),
		[](unsigned char character) { return std::isalnum(character) != 0; }) ||
		firstRunway.empty() || secondRunway.empty() || firstRunway == secondRunway)
		return {};
	if (firstRunway > secondRunway)
		std::swap(firstRunway, secondRunway);
	return airport + "|" + firstRunway + "|" + secondRunway;
}

bool IsProfileName(const std::string& value)
{
	return !value.empty() && value.size() <= 3 && std::all_of(value.begin(), value.end(),
		[](unsigned char character) { return std::isalnum(character) != 0; });
}

std::string Trim(std::string value)
{
	const size_t first = value.find_first_not_of(" \t\r\n");
	if (first == std::string::npos)
		return {};
	const size_t last = value.find_last_not_of(" \t\r\n");
	return value.substr(first, last - first + 1);
}

std::vector<std::string> Split(const std::string& value, char delimiter)
{
	std::vector<std::string> parts;
	std::istringstream parser(value);
	std::string part;
	while (std::getline(parser, part, delimiter))
		parts.push_back(Trim(part));
	return parts;
}

std::string IniDirective(const std::string& section, const std::string& key)
{
	const auto sectionParts = Split(section, '.');
	const std::string normalizedKey = UppercaseIdentifier(key);
	if (section == "GENERAL") {
		if (normalizedKey == "MINIMUMALTITUDE") return "MINALTITUDE";
		if (normalizedKey == "DEFAULTROWS") return "DEFAULTROWS";
		if (normalizedKey == "FTDMARKERSIZE") return "FTDMARKERSIZE";
		if (normalizedKey == "ITDMARKERSIZE") return "ITDMARKERSIZE";
		if (normalizedKey == "SHOWLOCALIZERROUTE") return "SHOWLOCALIZERROUTE";
		if (normalizedKey == "SEQUENCEMODE") return "SEQUENCEMODE";
	}
	if (section == "PROFILES" && normalizedKey == "DEFAULT")
		return "DEFAULTPROFILE";
	if (section == "AIRPORTS" && normalizedKey == "CODES")
		return "AIRPORTS";
	if (section == "SEPARATION") {
		if (normalizedKey == "MINIMUMDISTANCENM") return "MINDIST";
		if (normalizedKey == "MINIMUMPARALLELDEPENDENTDISTANCENM") return "MINPARALLELDEPENDENTDISTANCE";
		if (normalizedKey == "FINALBUFFERNM") return "FINALBUFFER";
	}
	if (section == "WAKE")
		return "WTC_" + normalizedKey;
	if (section == "SPEED") {
		if (normalizedKey == "LONGAPPROACHNM" || normalizedKey == "LONGAPPROACHSPEEDKTS" ||
			normalizedKey == "SPEEDUNTILREDUCTIONKTS" || normalizedKey == "SPEEDREDUCTIONNM" ||
			normalizedKey.compare(0, 17, "FINALAPPROACHSPEED_") == 0)
			return normalizedKey;
	}
	if (section == "COLORS")
		return "COLOR_" + normalizedKey;
	if (sectionParts.size() == 3 && sectionParts[0] == "RUNWAYS") {
		if (normalizedKey == "LOCALIZERPOINTNM")
			return "LOCALIZERPOINT_" + sectionParts[1] + "_" + sectionParts[2];
		if (normalizedKey == "LOCALIZERCOURSEDEGREES")
			return "LOCALIZERCOURSE_" + sectionParts[1] + "_" + sectionParts[2];
	}
	if (sectionParts.size() == 2 && sectionParts[0] == "PROFILE") {
		const auto pairParts = Split(key, '.');
		if (pairParts.size() == 3)
			return "RUNWAYPAIR " + sectionParts[1] + " " + pairParts[0] + " " +
				pairParts[1] + " " + pairParts[2];
	}
	return {};
}
}

bool CCallsignLookup::Available = false;
CCallsignLookup* CCallsignLookup::Lookup = nullptr;

CCallsignLookup::CCallsignLookup(const std::string& fileName)
{
	std::ifstream file(fileName);
	if (!file.is_open())
		return;

	std::vector<std::string> directives;
	std::string section;
	std::string line;
	while (std::getline(file, line)) {
		line = Trim(line);
		if (line.empty() || line[0] == ';' || line[0] == '#')
			continue;
		if (line.front() == '[' && line.back() == ']') {
			section = UppercaseIdentifier(Trim(line.substr(1, line.size() - 2)));
			const auto sectionParts = Split(section, '.');
			if (sectionParts.size() == 2 && sectionParts[0] == "PROFILE")
				directives.push_back("#PROFILE " + sectionParts[1]);
			continue;
		}
		const size_t separator = line.find('=');
		if (separator == std::string::npos)
			continue;
		const std::string key = Trim(line.substr(0, separator));
		std::string value = Trim(line.substr(separator + 1));
		const std::string directive = IniDirective(section, key);
		if (directive.empty())
			continue;
		std::replace(value.begin(), value.end(), ',', ' ');
		directives.push_back("#" + directive + " " + value);
	}

	std::string requestedDefaultProfile;
	for (const std::string& line : directives) {
		std::istringstream parser(line.substr(1));
		std::string key;
		if (!(parser >> key))
			continue;
		if (key == "PROFILE") {
			std::string profile;
			if (parser >> profile) {
				profile = UppercaseIdentifier(profile);
				if (IsProfileName(profile) &&
					std::find(profileNames.begin(), profileNames.end(), profile) == profileNames.end() &&
					profileNames.size() < 5)
					profileNames.push_back(profile);
			}
			continue;
		}
		if (key == "DEFAULTPROFILE") {
			if (parser >> requestedDefaultProfile)
				requestedDefaultProfile = UppercaseIdentifier(requestedDefaultProfile);
			continue;
		}
		if (key == "SEQUENCEMODE") {
			std::string mode;
			if (parser >> mode) {
				mode = UppercaseIdentifier(mode);
				if (mode == "ALTERNATING")
					dependentMode = false;
				else if (mode == "DEPENDANT" || mode == "DEPENDENT")
					dependentMode = true;
			}
			continue;
		}
		if (key == "RUNWAYPAIR") {
			std::vector<std::string> arguments;
			std::string argument;
			while (parser >> argument)
				arguments.push_back(argument);
			std::string profile, airport, firstRunway, secondRunway, mode;
			if (arguments.size() == 4) {
				profile = "DEP";
				airport = arguments[0];
				firstRunway = arguments[1];
				secondRunway = arguments[2];
				mode = arguments[3];
			}
			else if (arguments.size() == 5) {
				profile = UppercaseIdentifier(arguments[0]);
				airport = arguments[1];
				firstRunway = arguments[2];
				secondRunway = arguments[3];
				mode = arguments[4];
			}
			if (!profile.empty()) {
				const std::string pairKey = RunwayPairKey(airport, firstRunway, secondRunway);
				mode = UppercaseIdentifier(mode);
				if (!pairKey.empty()) {
					RunwayPairMode pairMode;
					bool validMode = true;
					if (mode == "DEPENDANT" || mode == "DEPENDENT" || mode == "IN-TRAIL" || mode == "INTRAIL")
						pairMode = RunwayPairMode::Dependant;
					else if (mode == "ALTERNATING")
						pairMode = RunwayPairMode::Alternating;
					else if (mode == "INDEPENDENT")
						pairMode = RunwayPairMode::Independent;
					else
						validMode = false;
					if (validMode) {
						if (arguments.size() == 4)
							legacyRunwayPairModes[pairKey] = pairMode;
						else if (IsProfileName(profile) &&
							(std::find(profileNames.begin(), profileNames.end(), profile) != profileNames.end() ||
								profileNames.size() < 5)) {
							if (std::find(profileNames.begin(), profileNames.end(), profile) == profileNames.end())
								profileNames.push_back(profile);
							profileRunwayPairModes[profile][pairKey] = pairMode;
						}
					}
				}
			}
			continue;
		}
		if (key.compare(0, 15, "LOCALIZERPOINT_") == 0) {
			double value;
			if (parser >> value && std::isfinite(value) && value >= 0.0)
				localizerPointDistancesNm[key.substr(15)] = value;
			continue;
		}
		if (key.compare(0, 16, "LOCALIZERCOURSE_") == 0) {
			double value;
			if (parser >> value && std::isfinite(value) && value >= 0.0 && value < 360.0)
				localizerCoursesDegrees[key.substr(16)] = value;
			continue;
		}
		if (key.compare(0, 6, "COLOR_") == 0) {
			RgbColor color;
			if (parser >> color.Red >> color.Green >> color.Blue &&
				color.Red >= 0 && color.Red <= 255 &&
				color.Green >= 0 && color.Green <= 255 &&
				color.Blue >= 0 && color.Blue <= 255)
				markerColors[key] = color;
			continue;
		}
		if (key == "AIRPORTS") {
			std::string airport;
			while (parser >> airport) {
				std::transform(airport.begin(), airport.end(), airport.begin(),
					[](unsigned char value) { return static_cast<char>(std::toupper(value)); });
				if (airport.size() == 4 &&
					std::find(airportCodes.begin(), airportCodes.end(), airport) == airportCodes.end())
					airportCodes.push_back(airport);
			}
			continue;
		}
		if (key == "MINDIST") {
		double value;
		if (parser >> value && std::isfinite(value) && value >= 0.0)
			minimumDistanceNm = value;
		continue;
		}
		if (key == "MINPARALLELDEPENDENTDISTANCE") {
			double value;
			if (parser >> value && std::isfinite(value) && value >= 0.0)
				minimumParallelDependentDistanceNm = value;
			continue;
		}
		if (key == "FINALBUFFER") {
			double value;
			if (parser >> value && std::isfinite(value) && value >= 0.0)
				finalBufferNm = value;
			continue;
		}
		if (key == "SHOWLOCALIZERROUTE") {
			int value;
			if (parser >> value && (value == 0 || value == 1))
				showLocalizerRoute = value == 1;
			continue;
		}
		if (key == "LONGAPPROACHNM") {
			double value;
			if (parser >> value && std::isfinite(value) && value >= 0.0)
				longApproachNm = value;
			continue;
		}
		if (key == "LONGAPPROACHSPEEDKTS") {
			double value;
			if (parser >> value && std::isfinite(value) && value > 0.0)
				longApproachSpeedKts = value;
			continue;
		}
		if (key.compare(0, 4, "WTC_") == 0) {
		double value;
		if (parser >> value && std::isfinite(value) && value >= 0.0)
			wakeSeparations[key] = value;
		continue;
		}

		int value;
		if (parser >> value)
			configValues[key] = value;
	}
	if (profileNames.empty()) {
		profileNames = { "ALT", "DEP" };
		for (const auto& pair : legacyRunwayPairModes)
			profileRunwayPairModes["DEP"][pair.first] = pair.second;
		defaultProfile = dependentMode ? "DEP" : "ALT";
	}
	else {
		for (const auto& pair : legacyRunwayPairModes) {
			const std::string legacyProfile = std::find(profileNames.begin(), profileNames.end(), "DEP") != profileNames.end()
				? "DEP" : profileNames.front();
			profileRunwayPairModes[legacyProfile][pair.first] = pair.second;
		}
		defaultProfile = IsProfileName(requestedDefaultProfile) &&
			std::find(profileNames.begin(), profileNames.end(), requestedDefaultProfile) != profileNames.end()
			? requestedDefaultProfile : profileNames.front();
	}

	Available = true;
}

int CCallsignLookup::getValue(const std::string& key) const
{
	const auto value = configValues.find(key);
	return value == configValues.end() ? 0 : value->second;
}

int CCallsignLookup::getMinimumAltitude() const
{
	const auto value = configValues.find("MINALTITUDE");
	return value == configValues.end() ? 1000 : value->second;
}

int CCallsignLookup::getDefaultVisibleRows() const
{
	const auto value = configValues.find("DEFAULTROWS");
	if (value == configValues.end())
		return 3;
	return (std::max)(1, value->second);
}

int CCallsignLookup::getMarkerSizePixels(bool finalTarget) const
{
	const auto value = configValues.find(finalTarget ? "FTDMARKERSIZE" : "ITDMARKERSIZE");
	const int configured = value != configValues.end() ? value->second : (finalTarget ? 26 : 22);
	const int bounded = (std::max)(finalTarget ? 8 : 6, (std::min)(finalTarget ? 96 : 64, configured));
	return finalTarget ? (std::max)(bounded, getMarkerSizePixels(false) + 2) : bounded;
}

double CCallsignLookup::getMinimumDistanceNm() const
{
	return minimumDistanceNm;
}

double CCallsignLookup::getMinimumParallelDependentDistanceNm() const
{
	return minimumParallelDependentDistanceNm;
}

double CCallsignLookup::getFinalBufferNm() const
{
	return finalBufferNm;
}

bool CCallsignLookup::getShowLocalizerRoute() const
{
	return showLocalizerRoute;
}

bool CCallsignLookup::toggleShowLocalizerRoute()
{
	showLocalizerRoute = !showLocalizerRoute;
	return showLocalizerRoute;
}

bool CCallsignLookup::getDependentMode() const
{
	return dependentMode;
}

std::vector<std::string> CCallsignLookup::getProfileNames() const
{
	return profileNames.empty() ? std::vector<std::string>{ "ALT", "DEP" } : profileNames;
}

std::string CCallsignLookup::getDefaultProfile() const
{
	return defaultProfile.empty() ? "ALT" : defaultProfile;
}

CCallsignLookup::RunwayPairMode CCallsignLookup::getRunwayPairMode(const std::string& profile,
	const std::string& precedingAirport, const std::string& precedingRunway,
	const std::string& succeedingAirport, const std::string& succeedingRunway) const
{
	if (UppercaseIdentifier(precedingAirport) != UppercaseIdentifier(succeedingAirport))
		return RunwayPairMode::Alternating;
	const std::string key = RunwayPairKey(precedingAirport, precedingRunway, succeedingRunway);
	const auto profileRules = profileRunwayPairModes.find(UppercaseIdentifier(profile));
	if (profileRules == profileRunwayPairModes.end())
		return RunwayPairMode::Alternating;
	const auto rule = profileRules->second.find(key);
	return rule == profileRules->second.end() ? RunwayPairMode::Alternating : rule->second;
}

bool CCallsignLookup::usesDependentSpacing(bool enabled,
	const std::string& precedingAirport, const std::string& precedingRunway,
	const std::string& succeedingAirport, const std::string& succeedingRunway) const
{
	return getRunwayPairMode(std::string(enabled ? "DEP" : "ALT"), precedingAirport, precedingRunway,
		succeedingAirport, succeedingRunway) == RunwayPairMode::Dependant;
}

bool CCallsignLookup::getWakeSeparation(char preceding, char succeeding, double& separationNm) const
{
	const std::string key = std::string("WTC_") + preceding + "_" + succeeding;
	const auto value = wakeSeparations.find(key);
	if (value == wakeSeparations.end())
		return false;
	separationNm = value->second;
	return true;
}

int CCallsignLookup::getSpeedReductionDistanceNm() const
{
	const auto value = configValues.find("SPEEDREDUCTIONNM");
	return value == configValues.end() ? 5 : value->second;
}

int CCallsignLookup::getSpeedUntilReductionKts() const
{
	const auto value = configValues.find("SPEEDUNTILREDUCTIONKTS");
	return value == configValues.end() ? 170 : value->second;
}

double CCallsignLookup::getLongApproachNm() const
{
	return longApproachNm;
}

double CCallsignLookup::getLongApproachSpeedKts() const
{
	return longApproachSpeedKts;
}

int CCallsignLookup::getFinalApproachSpeedKts(char wakeCategory) const
{
	if (wakeCategory == 'J')
		wakeCategory = 'S';

	const std::string key = std::string("FINALAPPROACHSPEED_") + wakeCategory;
	const auto value = configValues.find(key);
	return value == configValues.end() ? 150 : value->second;
}

bool CCallsignLookup::getLocalizerPointDistanceNm(
	const std::string& airportCode, const std::string& runwayName, double& distanceNm) const
{
	std::string key = "LOCALIZERPOINT_" + airportCode + "_" + runwayName;
	std::transform(key.begin(), key.end(), key.begin(),
		[](unsigned char value) { return static_cast<char>(std::toupper(value)); });
	const auto value = localizerPointDistancesNm.find(key.substr(15));
	if (value == localizerPointDistancesNm.end())
		return false;
	distanceNm = value->second;
	return true;
}

bool CCallsignLookup::getLocalizerCourseDegrees(
	const std::string& airportCode, const std::string& runwayName, double& courseDegrees) const
{
	std::string key = "LOCALIZERCOURSE_" + airportCode + "_" + runwayName;
	std::transform(key.begin(), key.end(), key.begin(),
		[](unsigned char value) { return static_cast<char>(std::toupper(value)); });
	const auto value = localizerCoursesDegrees.find(key.substr(16));
	if (value == localizerCoursesDegrees.end())
		return false;
	courseDegrees = value->second;
	return true;
}

CCallsignLookup::RgbColor CCallsignLookup::getMarkerColor(const std::string& key) const
{
	const auto value = markerColors.find(key);
	if (value != markerColors.end())
		return value->second;
	if (key == "COLOR_FTD_ROUTE")
		return getMarkerColor("COLOR_FTD_TARGET");
	if (key == "COLOR_ITD_ROUTE")
		return getMarkerColor("COLOR_ITD_TARGET");
	return getDefaultColor(key);
}

CCallsignLookup::RgbColor CCallsignLookup::getDefaultColor(const std::string& key)
{
	static const std::map<std::string, RgbColor> defaults{
		{ "COLOR_FTD_TARGET", { 255, 75, 75 } },
		{ "COLOR_ITD_TARGET", { 0, 120, 255 } },
		{ "COLOR_GAP_TARGET", { 255, 226, 62 } },
		{ "COLOR_BREACH_ALERT", { 255, 75, 75 } },
		{ "COLOR_SELECTION_TEXT", { 255, 190, 90 } },
		{ "COLOR_TURN_POINT", { 65, 196, 230 } },
		{ "COLOR_TURN_PASSED", { 239, 190, 54 } },
		{ "COLOR_TOOLTIP_BACKGROUND", { 0, 0, 0 } },
		{ "COLOR_TOOLTIP_TEXT", { 255, 255, 255 } },
		{ "COLOR_PANEL_BACKGROUND", { 137, 140, 143 } },
		{ "COLOR_PANEL_SURFACE", { 73, 96, 116 } },
		{ "COLOR_PANEL_ROW", { 190, 193, 194 } },
		{ "COLOR_PANEL_DIVIDER", { 72, 79, 84 } },
		{ "COLOR_PANEL_TEXT", { 29, 34, 37 } },
		{ "COLOR_PANEL_HEADER_TEXT", { 244, 244, 236 } },
		{ "COLOR_PANEL_MUTED_TEXT", { 75, 82, 86 } },
		{ "COLOR_PANEL_ACCENT", { 105, 158, 181 } },
		{ "COLOR_PANEL_WARNING", { 239, 190, 54 } },
		{ "COLOR_PANEL_DISABLED", { 158, 161, 161 } },
		{ "COLOR_PANEL_REMOVE_BACKGROUND", { 74, 43, 37 } },
		{ "COLOR_PANEL_REMOVE_TEXT", { 255, 190, 160 } },
		{ "COLOR_PANEL_STATUS_WARNING", { 110, 50, 20 } },
		{ "COLOR_PANEL_CONTROL", { 58, 79, 97 } },
		{ "COLOR_PANEL_WAKE_L", { 38, 105, 73 } },
		{ "COLOR_PANEL_WAKE_M", { 45, 99, 150 } },
		{ "COLOR_PANEL_WAKE_H", { 245, 179, 45 } },
		{ "COLOR_PANEL_WAKE_S", { 183, 43, 53 } }
	};
	if (key == "COLOR_FTD_ROUTE")
		return getDefaultColor("COLOR_FTD_TARGET");
	if (key == "COLOR_ITD_ROUTE")
		return getDefaultColor("COLOR_ITD_TARGET");
	const auto value = defaults.find(key);
	return value != defaults.end() ? value->second : RgbColor{ 0, 120, 255 };
}

std::vector<std::string> CCallsignLookup::getAirportCodes() const
{
	return airportCodes;
}
