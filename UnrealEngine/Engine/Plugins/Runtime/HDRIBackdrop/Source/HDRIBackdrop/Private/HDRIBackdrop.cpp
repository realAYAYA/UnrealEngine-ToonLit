// Copyright Epic Games, Inc. All Rights Reserved.

#include "HDRIBackdrop.h"
#include "HDRIBackdropPlacement.h"
#include "HDRIBackdropStyle.h"

#define LOCTEXT_NAMESPACE "FHDRIBackdropModule"

void FHDRIBackdropModule::StartupModule()
{
	FHDRIBackdropStyle::Initialize();
	FHDRIBackdropPlacement::RegisterPlacement();
}

void FHDRIBackdropModule::ShutdownModule()
{
	FHDRIBackdropStyle::Shutdown();
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FHDRIBackdropModule, HDRIBackdrop)