// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_SubsurfaceProfile.h"
#include "Engine/SubsurfaceProfile.h"

UClass* FAssetTypeActions_SubsurfaceProfile::GetSupportedClass() const
{
	return USubsurfaceProfile::StaticClass();
}
