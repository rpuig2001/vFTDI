// dllmain.cpp : Defines the entry point for the DLL application.
#include "stdafx.h"
#include "EuroScopePlugIn.h"
#include "VFTDIPlugin.h"

//---EuroScopePlugInInit-----------------------------------------------

void __declspec (dllexport) EuroScopePlugInInit(EuroScopePlugIn::CPlugIn ** ppPlugInInstance)
{
	* ppPlugInInstance = (CPlugIn*)new VFTDIPlugin();
}

//---EuroScopePlugInExit-----------------------------------------------

void __declspec (dllexport) EuroScopePlugInExit(void)
{
}