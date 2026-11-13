#include "../src/DisplayColors.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>

void CheckColor(CCallsignLookup::RgbColor actual, int red, int green, int blue, const char* message)
{
	if (actual.Red != red || actual.Green != green || actual.Blue != blue) {
		std::cerr << "FAIL: " << message << '\n';
		std::exit(1);
	}
}

int main(int argumentCount, char** arguments)
{
	if (argumentCount != 2)
		return 1;
	const char* path = arguments[1];
	const char* keys[] = {
		"COLOR_FTD_TARGET", "COLOR_ITD_TARGET", "COLOR_FTD_ROUTE", "COLOR_ITD_ROUTE",
		"COLOR_GAP_TARGET", "COLOR_BREACH_ALERT", "COLOR_SELECTION_TEXT", "COLOR_TURN_POINT",
		"COLOR_TURN_PASSED", "COLOR_TOOLTIP_BACKGROUND", "COLOR_TOOLTIP_TEXT",
		"COLOR_PANEL_BACKGROUND", "COLOR_PANEL_SURFACE", "COLOR_PANEL_ROW", "COLOR_PANEL_DIVIDER",
		"COLOR_PANEL_TEXT", "COLOR_PANEL_HEADER_TEXT", "COLOR_PANEL_MUTED_TEXT", "COLOR_PANEL_ACCENT",
		"COLOR_PANEL_WARNING", "COLOR_PANEL_DISABLED", "COLOR_PANEL_REMOVE_BACKGROUND",
		"COLOR_PANEL_REMOVE_TEXT", "COLOR_PANEL_STATUS_WARNING", "COLOR_PANEL_CONTROL",
		"COLOR_PANEL_WAKE_L", "COLOR_PANEL_WAKE_M", "COLOR_PANEL_WAKE_H", "COLOR_PANEL_WAKE_S"
	};
	{
		std::ofstream config(path);
		config << "[colors]\n";
		for (const char* key : keys)
			config << (key + 6) << " = 12, 34, 56\n";
		config << "PANEL_BACKGROUND = -1, 0, 0\nTURN_POINT = 256, 0, 0\n";
		config << "TOOLTIP_TEXT = 1, 2\nPANEL_TEXT = 0, 255, 0\n";
	}
	CCallsignLookup configured(path);
	if (configured.getMarkerSizePixels(false) != 22 || configured.getMarkerSizePixels(true) != 26) {
		std::cerr << "FAIL: FTD should default slightly larger than ITD\n";
		return 1;
	}
	for (const char* key : keys) {
		if (std::string(key) == "COLOR_PANEL_TEXT")
			CheckColor(configured.getMarkerColor(key), 0, 255, 0, "last valid override");
		else
			CheckColor(configured.getMarkerColor(key), 12, 34, 56, key);
	}
	CCallsignLookup::Lookup = &configured;
	if (ConfiguredColor("COLOR_TURN_POINT") != RGB(12, 34, 56))
		return 1;
	{
		std::ofstream config(path);
		config << "[colors]\nFTD_TARGET = 1, 2, 3\nITD_TARGET = 4, 5, 6\n";
		config << "TURN_POINT = 300, 0, 0\n";
		config << "[general]\nftdMarkerSize = 10\nitdMarkerSize = 30\n";
	}
	CCallsignLookup legacy(path);
	if (legacy.getMarkerSizePixels(false) != 30 || legacy.getMarkerSizePixels(true) != 32) {
		std::cerr << "FAIL: custom sizes must keep FTD larger than ITD\n";
		return 1;
	}
	CheckColor(legacy.getMarkerColor("COLOR_FTD_ROUTE"), 1, 2, 3, "FTD route inherits target");
	CheckColor(legacy.getMarkerColor("COLOR_ITD_ROUTE"), 4, 5, 6, "ITD route inherits target");
	CheckColor(legacy.getMarkerColor("COLOR_TURN_POINT"), 65, 196, 230, "invalid value uses default");
	CheckColor(legacy.getMarkerColor("COLOR_PANEL_BACKGROUND"), 137, 140, 143, "missing value uses default");
	CCallsignLookup::Lookup = nullptr;
	CCallsignLookup::Available = false;
	if (ConfiguredColor("COLOR_PANEL_SURFACE") != RGB(73, 96, 116))
		return 1;
	CheckColor(CCallsignLookup::getDefaultColor("COLOR_FTD_ROUTE"), 255, 75, 75, "unloaded route default");
	CheckColor(CCallsignLookup::getDefaultColor("COLOR_PANEL_WAKE_L"), 38, 105, 73, "light category default");
	CheckColor(CCallsignLookup::getDefaultColor("COLOR_PANEL_WAKE_M"), 45, 99, 150, "medium category default");
	CheckColor(CCallsignLookup::getDefaultColor("COLOR_PANEL_WAKE_H"), 245, 179, 45, "heavy category default");
	CheckColor(CCallsignLookup::getDefaultColor("COLOR_PANEL_WAKE_S"), 183, 43, 53, "super category default");
	{
		std::ofstream config(path);
		config << "[general]\nftdMarkerSize = 999\nitdMarkerSize = -5\n";
	}
	CCallsignLookup bounded(path);
	if (bounded.getMarkerSizePixels(true) != 96 || bounded.getMarkerSizePixels(false) != 6)
		return 1;
	{
		std::ofstream config(path);
		config << "[general]\nftdMarkerSize = invalid\nitdMarkerSize = invalid\n";
	}
	CCallsignLookup invalid(path);
	if (invalid.getMarkerSizePixels(true) != 26 || invalid.getMarkerSizePixels(false) != 22)
		return 1;
	std::remove(path);
	std::cout << "All marker-size hierarchy/limits and 29 color configuration checks passed.\n";
}