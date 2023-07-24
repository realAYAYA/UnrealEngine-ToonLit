// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Chooser.h"
#include "AssetDefinitionDefault.h"

#include "AssetDefinition_ChooserTable.generated.h"

UCLASS()
class UAssetDefinition_ChooserTable : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	// UAssetDefinition Begin
	virtual FText GetAssetDisplayName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_ChooserTable", "Chooser Table"); }
	virtual FLinearColor GetAssetColor() const override { return FLinearColor(FColor(128,128,64)); }
	virtual TSoftClassPtr<UObject> GetAssetClass() const override { return UChooserTable::StaticClass(); }
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static const auto Categories = { FAssetCategoryPath(EAssetCategoryPaths::Animation, NSLOCTEXT("AssetTypeActions", "DataInterfacesSubMenu", "Data Interfaces")) };
		
		return Categories;
	}
	virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override;
	// UAssetDefinition End
};
