// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundAttenuation.h"
#include "AssetDefinitionDefault.h"
#include "AudioEditorSettings.h"

#include "AssetDefinition_SoundAttenuation.generated.h"

UCLASS()
class UAssetDefinition_SoundAttenuation : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	// UAssetDefinition Begin
	virtual FText GetAssetDisplayName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_SoundAttenuation", "Sound Attenuation"); }
	virtual FLinearColor GetAssetColor() const override { return FLinearColor(FColor(77, 120, 239)); }
	virtual TSoftClassPtr<UObject> GetAssetClass() const override { return USoundAttenuation::StaticClass(); }
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static const auto PinnedCategories = { EAssetCategoryPaths::Audio };
		static const auto Categories = { FAssetCategoryPath(EAssetCategoryPaths::Audio, NSLOCTEXT("AssetTypeActions", "AssetSoundAttenuationSubMenu", "Spatialization")) };
		
		if (GetDefault<UAudioEditorSettings>()->bPinSoundAttenuationInAssetMenu)
		{
			return PinnedCategories;
		}
		return Categories;
	}
	// UAssetDefinition End
};
