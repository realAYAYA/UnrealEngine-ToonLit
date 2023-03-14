// Copyright Epic Games, Inc. All Rights Reserved.

#include "CompositePlane.h"
#include "CompositePlanePlacement.h"

#define LOCTEXT_NAMESPACE "FCompositePlaneModule"

void FCompositePlaneModule::StartupModule()
{
	FCompositePlanePlacement::RegisterPlacement();
}

void FCompositePlaneModule::ShutdownModule()
{
	FCompositePlanePlacement::UnregisterPlacement();
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FCompositePlaneModule, CompositePlane)