// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaTextActorFactory.h"
#include "AvaTextActor.h"
#include "EngineAnalytics.h"
#include "Subsystems/PlacementSubsystem.h"

UAvaTextActorFactory::UAvaTextActorFactory()
{
	NewActorClass = AAvaTextActor::StaticClass();
}

void UAvaTextActorFactory::PostPlaceAsset(TArrayView<const FTypedElementHandle> InHandle, const FAssetPlacementInfo& InPlacementInfo, const FPlacementOptions& InPlacementOptions)
{
	Super::PostPlaceAsset(InHandle, InPlacementInfo, InPlacementOptions);
	if (!InPlacementOptions.bIsCreatingPreviewElements && FEngineAnalytics::IsAvailable())
	{
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.MotionDesign.PlaceText"));
	}
}
