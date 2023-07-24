// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Slate/SlateBrushAsset.h"
#include "AssetDefinitionDefault.h"

#include "AssetDefinition_SlateBrush.generated.h"

UCLASS()
class UAssetDefinition_SlateBrush : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	// UAssetDefinition Begin
	virtual FText GetAssetDisplayName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_SlateBrush", "Slate Brush"); }
	virtual FLinearColor GetAssetColor() const override { return FLinearColor(FColor(105, 165, 60)); }
	virtual TSoftClassPtr<UObject> GetAssetClass() const override { return USlateBrushAsset::StaticClass(); }
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static const auto Categories = { EAssetCategoryPaths::UI };
		return Categories;
	}
};
