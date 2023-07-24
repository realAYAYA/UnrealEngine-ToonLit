// Copyright Epic Games, Inc. All Rights Reserved.
#include "AssetTypeActions_SoundModulationParameter.h"

#include "SoundModulationParameter.h"


#define LOCTEXT_NAMESPACE "AssetTypeActions"


UClass* FAssetTypeActions_SoundModulationParameter::GetSupportedClass() const
{
	return USoundModulationParameter::StaticClass();
}

const TArray<FText>& FAssetTypeActions_SoundModulationParameter::GetSubMenus() const
{
	static const TArray<FText> SubMenus
	{
		LOCTEXT("AssetSoundModulationSubMenu", "Modulation")
	};

	return SubMenus;
}
#undef LOCTEXT_NAMESPACE // AssetTypeActions
