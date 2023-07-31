// Copyright Epic Games, Inc. All Rights Reserved.

#include "SunPositionModule.h"
#include "SunPositionPlacement.h"
#include "SunPositionStyle.h"

void FSunPositionModule::StartupModule()
{
	FSunPositionStyle::Initialize();
	FSunPositionPlacement::RegisterPlacement();
}

void FSunPositionModule::ShutdownModule()
{
	FSunPositionStyle::Shutdown();
}

IMPLEMENT_MODULE(FSunPositionModule, SunPosition)