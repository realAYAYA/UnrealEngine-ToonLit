// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaveformEditorWidgetsModule.h"

#include "Modules/ModuleManager.h"
#include "WaveformEditorStyle.h"

void FWaveformEditorWidgetsModule::StartupModule()
{
	// Initialize static style instance 
	FWaveformEditorStyle::Init();
}

void FWaveformEditorWidgetsModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FWaveformEditorWidgetsModule, WaveformEditorWidgets)
