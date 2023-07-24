// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Curves/CurveLinearColorAtlas.h"
#include "AssetDefinition_Texture.h"

#include "AssetDefinition_CurveLinearColorAtlas.generated.h"

UCLASS()
class UAssetDefinition_CurveLinearColorAtlas : public UAssetDefinition_Texture
{
	GENERATED_BODY()

public:
	// UAssetDefinition Begin
	virtual FText GetAssetDisplayName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_CurveLinearColorAtlas", "Curve Atlas"); }
	virtual FLinearColor GetAssetColor() const override { return FLinearColor(FColor(128,64,64)); }
	virtual TSoftClassPtr<UObject> GetAssetClass() const override { return UCurveLinearColorAtlas::StaticClass(); }
	virtual bool CanImport() const override { return false; }
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static const auto Categories ={ EAssetCategoryPaths::Misc };
		return Categories;
	}
	// UAssetDefinition End
};
