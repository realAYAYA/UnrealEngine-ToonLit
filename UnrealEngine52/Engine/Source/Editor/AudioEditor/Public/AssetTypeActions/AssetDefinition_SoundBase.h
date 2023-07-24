// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundBase.h"
#include "AssetDefinitionDefault.h"

#include "AssetDefinition_SoundBase.generated.h"

namespace UE::AudioEditor
{
	AUDIOEDITOR_API void StopSound();
	
	AUDIOEDITOR_API void PlaySound(USoundBase* Sound);
	
	AUDIOEDITOR_API bool IsSoundPlaying(USoundBase* Sound);
	AUDIOEDITOR_API bool IsSoundPlaying(const FAssetData& AssetData);
}

UCLASS()
class AUDIOEDITOR_API UAssetDefinition_SoundBase : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	// UAssetDefinition Begin
	virtual FText GetAssetDisplayName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_SoundBase", "Sound Base"); }
	virtual FLinearColor GetAssetColor() const override { return FLinearColor(FColor(97, 85, 212)); }
	virtual TSoftClassPtr<UObject> GetAssetClass() const override { return USoundBase::StaticClass(); }
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static const auto Categories = { EAssetCategoryPaths::Audio };
		return Categories;
	}
	virtual TSharedPtr<SWidget> GetThumbnailOverlay(const FAssetData& InAssetData) const override;
	virtual EAssetCommandResult ActivateAssets(const FAssetActivateArgs& ActivateArgs) const override;
	// UAssetDefinition End
};
