// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterColorGradingModule.h"

#include "DisplayClusterColorGradingCommands.h"
#include "DataModelGenerators/DisplayClusterColorGradingGenerator_RootActor.h"
#include "DataModelGenerators/DisplayClusterColorGradingGenerator_PostProcessVolume.h"
#include "DataModelGenerators/DisplayClusterColorGradingGenerator_ColorCorrectRegion.h"
#include "Drawer/DisplayClusterColorGradingDrawerSingleton.h"
#include "Drawer/SDisplayClusterColorGradingDrawer.h"

#include "DisplayClusterRootActor.h"
#include "Components/DisplayClusterICVFXCameraComponent.h"

#include "ColorCorrectRegion.h"
#include "Engine/PostProcessVolume.h"

#define LOCTEXT_NAMESPACE "DisplayClusterColorGrading"

void FDisplayClusterColorGradingModule::StartupModule()
{
	ColorGradingDrawerSingleton = MakeUnique<FDisplayClusterColorGradingDrawerSingleton>();

	FDisplayClusterColorGradingDataModel::RegisterColorGradingDataModelGenerator<ADisplayClusterRootActor>(
		FGetColorGradingDataModelGenerator::CreateStatic(&FDisplayClusterColorGradingGenerator_RootActor::MakeInstance));

	FDisplayClusterColorGradingDataModel::RegisterColorGradingDataModelGenerator<UDisplayClusterICVFXCameraComponent>(
		FGetColorGradingDataModelGenerator::CreateStatic(&FDisplayClusterColorGradingGenerator_ICVFXCamera::MakeInstance));

	FDisplayClusterColorGradingDataModel::RegisterColorGradingDataModelGenerator<APostProcessVolume>(
		FGetColorGradingDataModelGenerator::CreateStatic(&FDisplayClusterColorGradingGenerator_PostProcessVolume::MakeInstance));

	FDisplayClusterColorGradingDataModel::RegisterColorGradingDataModelGenerator<AColorCorrectRegion>(
		FGetColorGradingDataModelGenerator::CreateStatic(&FDisplayClusterColorGradingGenerator_ColorCorrectRegion::MakeInstance));

	FDisplayClusterColorGradingCommands::Register();
}

void FDisplayClusterColorGradingModule::ShutdownModule()
{
	ColorGradingDrawerSingleton.Reset();
}

IDisplayClusterColorGradingDrawerSingleton& FDisplayClusterColorGradingModule::GetColorGradingDrawerSingleton() const
{
	return *ColorGradingDrawerSingleton.Get();
}

IMPLEMENT_MODULE(FDisplayClusterColorGradingModule, DisplayClusterColorGrading);

#undef LOCTEXT_NAMESPACE
