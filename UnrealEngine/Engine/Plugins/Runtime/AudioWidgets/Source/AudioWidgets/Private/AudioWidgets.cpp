// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioWidgets.h"
#include "AdvancedWidgetsModule.h"

#define LOCTEXT_NAMESPACE "FAudioWidgetsModule"

void FAudioWidgetsModule::StartupModule()
{
	// Required to load so the AudioWidget plugin content can reference widgets
	// defined in AdvancedWidgets (ex. RadialSlider for UMG-defined knobs)
	FModuleManager::Get().LoadModuleChecked<FAdvancedWidgetsModule>("AdvancedWidgets");
	// Initialize static style instance 
	FAudioWidgetsStyle::Get();
}

void FAudioWidgetsModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FAudioWidgetsModule, AudioWidgets)