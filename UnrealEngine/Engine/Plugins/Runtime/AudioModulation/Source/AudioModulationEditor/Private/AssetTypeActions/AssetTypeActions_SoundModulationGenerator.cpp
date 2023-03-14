// Copyright Epic Games, Inc. All Rights Reserved.
#include "AssetTypeActions_SoundModulationGenerator.h"

#include "SoundModulationGenerator.h"


#define LOCTEXT_NAMESPACE "AssetTypeActions"
UClass* FAssetTypeActions_SoundModulationGenerator::GetSupportedClass() const
{
	return USoundModulationGenerator::StaticClass();
}

const TArray<FText>& FAssetTypeActions_SoundModulationGenerator::GetSubMenus() const
{
	static const TArray<FText> SubMenus
	{
		LOCTEXT("AssetSoundModulationSubMenu", "Modulation")
	};

	return SubMenus;
}
#undef LOCTEXT_NAMESPACE // AssetTypeActions