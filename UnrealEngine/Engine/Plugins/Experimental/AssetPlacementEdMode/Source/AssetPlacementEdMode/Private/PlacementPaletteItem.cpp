// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlacementPaletteItem.h"

#include "Factories/AssetFactoryInterface.h"
#include "Subsystems/PlacementSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PlacementPaletteItem)

void UPlacementPaletteClient::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UPlacementPaletteClient::PostLoad()
{
	Super::PostLoad();
}

void UPlacementPaletteClient::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
}

