// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cloner/CEClonerActorFactory.h"
#include "Cloner/CEClonerActor.h"
#include "EngineAnalytics.h"
#include "Subsystems/PlacementSubsystem.h"

UCEClonerActorFactory::UCEClonerActorFactory()
{
	NewActorClass = ACEClonerActor::StaticClass();
}

void UCEClonerActorFactory::PostPlaceAsset(TArrayView<const FTypedElementHandle> InHandle, const FAssetPlacementInfo& InPlacementInfo, const FPlacementOptions& InPlacementOptions)
{
	Super::PostPlaceAsset(InHandle, InPlacementInfo, InPlacementOptions);

	if (!InPlacementOptions.bIsCreatingPreviewElements && FEngineAnalytics::IsAvailable())
	{
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.ClonerEffector.PlaceCloner"));
	}
}
