// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaveformEditorWidgetsModule.h"

#include "Modules/ModuleManager.h"
#include "WaveformEditorStyle.h"
#include "WaveformTransformationRendererMapper.h"

void FWaveformEditorWidgetsModule::StartupModule()
{
	// Initialize static style instance 
	FWaveformEditorStyle::Init();
	FWaveformTransformationRendererMapper::Init();
}

void FWaveformEditorWidgetsModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FWaveformEditorWidgetsModule, WaveformEditorWidgets)
