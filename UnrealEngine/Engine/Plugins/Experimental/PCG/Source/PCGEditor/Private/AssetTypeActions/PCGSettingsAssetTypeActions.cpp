// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGSettingsAssetTypeActions.h"
#include "PCGSettings.h"

FText FPCGSettingsAssetTypeActions::GetName() const
{
	return NSLOCTEXT("AssetTypeActions", "PCGSettingsAssetTypeActions", "PCG Settings");
}

UClass* FPCGSettingsAssetTypeActions::GetSupportedClass() const
{
	return UPCGSettings::StaticClass();
}
