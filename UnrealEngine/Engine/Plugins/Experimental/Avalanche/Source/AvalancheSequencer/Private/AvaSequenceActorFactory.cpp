// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaSequenceActorFactory.h"
#include "AvaSequence.h"
#include "LevelSequenceActor.h"

#define LOCTEXT_NAMESPACE "AvaSequenceActorFactory"

UAvaSequenceActorFactory::UAvaSequenceActorFactory()
{
	DisplayName = LOCTEXT("DisplayName", "Motion Design Sequence");
	NewActorClass = UAvaSequence::StaticClass();
}

bool UAvaSequenceActorFactory::CanCreateActorFrom(const FAssetData& InAssetData, FText& OutErrorMessage)
{
	return false;
}

AActor* UAvaSequenceActorFactory::SpawnActor(UObject* InAsset, ULevel* InLevel, const FTransform& InTransform, const FActorSpawnParameters& InSpawnParams)
{
	return nullptr;
}

bool UAvaSequenceActorFactory::CanPlaceElementsFromAssetData(const FAssetData& InAssetData)
{
	return false;
}

#undef LOCTEXT_NAMESPACE
