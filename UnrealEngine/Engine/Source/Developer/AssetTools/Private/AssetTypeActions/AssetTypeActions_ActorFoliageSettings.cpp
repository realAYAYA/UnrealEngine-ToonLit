// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_ActorFoliageSettings.h"
#include "FoliageType_Actor.h"

UClass* FAssetTypeActions_ActorFoliageSettings::GetSupportedClass() const
{
	return UFoliageType_Actor::StaticClass();
}
