// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_ReverbEffect.h"

#include "Sound/ReverbEffect.h"

class UClass;

#define LOCTEXT_NAMESPACE "AssetTypeActions"

UClass* FAssetTypeActions_ReverbEffect::GetSupportedClass() const
{
	return UReverbEffect::StaticClass();
}

const TArray<FText>& FAssetTypeActions_ReverbEffect::GetSubMenus() const
{
	static const TArray<FText> SubMenus
	{
		FText(LOCTEXT("AssetEffectSubMenu", "Effects"))
	};

	return SubMenus;
}

#undef LOCTEXT_NAMESPACE