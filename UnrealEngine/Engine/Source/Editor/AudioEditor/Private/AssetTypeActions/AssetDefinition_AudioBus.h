// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Sound/AudioBus.h"
#include "AssetDefinitionDefault.h"

#include "AssetDefinition_AudioBus.generated.h"

UCLASS()
class UAssetDefinition_AudioBus : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	// UAssetDefinition Begin
	virtual FText GetAssetDisplayName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_AudioBus", "Audio Bus"); }
	virtual FLinearColor GetAssetColor() const override { return FLinearColor(FColor(97, 97, 85)); }
	virtual TSoftClassPtr<UObject> GetAssetClass() const override { return UAudioBus::StaticClass(); }
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static const auto Categories = { FAssetCategoryPath(EAssetCategoryPaths::Audio, NSLOCTEXT("AssetTypeActions", "AssetAudioBusMenu", "Mix")) };
		return Categories;
	}
	// UAssetDefinition End
};
