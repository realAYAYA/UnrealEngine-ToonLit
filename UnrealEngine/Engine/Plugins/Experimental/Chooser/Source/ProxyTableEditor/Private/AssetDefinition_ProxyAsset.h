// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ProxyTable.h"
#include "AssetDefinitionDefault.h"

#include "AssetDefinition_ProxyAsset.generated.h"

UCLASS()
class UAssetDefinition_ProxyAsset : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	// UAssetDefinition Begin
	virtual FText GetAssetDisplayName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_ProxyAsset", "Proxy Asset"); }
	virtual FLinearColor GetAssetColor() const override { return FLinearColor(FColor(128,128,64)); }
	virtual TSoftClassPtr<UObject> GetAssetClass() const override { return UProxyAsset::StaticClass(); }
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static const auto Categories = { FAssetCategoryPath(EAssetCategoryPaths::Misc) };
		return Categories;
	}
	virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override;
	
	const FSlateBrush* GetIconBrush(const FAssetData& InAssetData, const FName InClassName) const override;
	const FSlateBrush* GetThumbnailBrush(const FAssetData& InAssetData, const FName InClassName) const override;
	// UAssetDefinition End
};
