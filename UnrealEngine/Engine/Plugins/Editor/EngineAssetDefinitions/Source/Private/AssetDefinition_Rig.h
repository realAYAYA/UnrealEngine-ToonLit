// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/Rig.h"
#include "AssetDefinitionDefault.h"

#include "AssetDefinition_Rig.generated.h"

UCLASS()
class UAssetDefinition_Rig : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	// UAssetDefinition Begin
	virtual FText GetAssetDisplayName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_Rig", "Rig"); }
	virtual FLinearColor GetAssetColor() const override { return FLinearColor(FColor(255,201,14)); }
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	virtual TSoftClassPtr<UObject> GetAssetClass() const override { return URig::StaticClass(); }
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static const auto Categories = { EAssetCategoryPaths::Animation };
		return Categories;
	}
	// UAssetDefinition End
};
