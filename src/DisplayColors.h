#pragma once

#include <windows.h>
#include "CallsignLookup.h"

inline COLORREF ConfiguredColor(const char* key)
{
	const auto color = CCallsignLookup::Available && CCallsignLookup::Lookup != nullptr
		? CCallsignLookup::Lookup->getMarkerColor(key)
		: CCallsignLookup::getDefaultColor(key);
	return RGB(color.Red, color.Green, color.Blue);
}