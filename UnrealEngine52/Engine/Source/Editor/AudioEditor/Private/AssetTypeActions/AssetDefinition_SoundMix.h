// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundMix.h"
#include "AssetDefinitionDefault.h"

#include "AssetDefinition_SoundMix.generated.h"

UCLASS()
class UAssetDefinition_SoundMix : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	// UAssetDefinition Begin
	virtual FText GetAssetDisplayName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_SoundMix", "Sound Class Mix"); }
	virtual FLinearColor GetAssetColor() const override { return FLinearColor(FColor(175, 0, 255)); }
	virtual TSoftClassPtr<UObject> GetAssetClass() const override { return USoundMix::StaticClass(); }
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static const auto Categories = { FAssetCategoryPath(EAssetCategoryPaths::Audio, NSLOCTEXT("AssetTypeActions", "AssetSoundClassSubMenu", "Classes")) };
		return Categories;
	}
	// UAssetDefinition End
};
