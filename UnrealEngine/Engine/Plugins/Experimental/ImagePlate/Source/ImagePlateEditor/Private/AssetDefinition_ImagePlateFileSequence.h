// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ImagePlateFileSequence.h"
#include "AssetDefinitionDefault.h"

#include "AssetDefinition_ImagePlateFileSequence.generated.h"

UCLASS()
class UAssetDefinition_ImagePlateFileSequence : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	// UAssetDefinition Begin
	virtual FText GetAssetDisplayName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_ImagePlateFileSequence", "Image Plate File Sequence"); }
	virtual FLinearColor GetAssetColor() const override { return FLinearColor(FColor(105, 0, 60)); }
	virtual TSoftClassPtr<UObject> GetAssetClass() const override { return UImagePlateFileSequence::StaticClass(); }
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static const auto Categories = { EAssetCategoryPaths::Media };
		return Categories;
	}
	// UAssetDefinition End
};
