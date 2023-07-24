// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetDefinitionDefault.h"
#include "Engine/UserDefinedEnum.h"

#include "AssetDefinition_UserDefinedEnum.generated.h"

UCLASS()
class UAssetDefinition_UserDefinedEnum : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	// UAssetDefinition Begin
	virtual FText GetAssetDisplayName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_Enum", "Enumeration"); }
	virtual FLinearColor GetAssetColor() const override { return FLinearColor(FColor(255, 200, 200)); }
	virtual TSoftClassPtr<UObject> GetAssetClass() const override { return UUserDefinedEnum::StaticClass(); }
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static const auto Categories = { EAssetCategoryPaths::Blueprint };
		return Categories;
	}
	virtual FAssetSupportResponse CanLocalize(const FAssetData& InAsset) const override
	{
		return FAssetSupportResponse::NotSupported();
	}
	virtual FText GetAssetDescription(const FAssetData& AssetData) const override;
	virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override;
	// UAssetDefinition End
};
