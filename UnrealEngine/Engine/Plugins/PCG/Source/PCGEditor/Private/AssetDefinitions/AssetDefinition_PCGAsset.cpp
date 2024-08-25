// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_PCGAsset.h"

#include "PCGDataAsset.h"

#include "Misc/AssetCategoryPath.h"

#define LOCTEXT_NAMESPACE "AssetDefinition_PCGAsset"

FText UAssetDefinition_PCGAsset::GetAssetDisplayName() const
{
	return LOCTEXT("DisplayName", "PCG Data Asset");
}

FLinearColor UAssetDefinition_PCGAsset::GetAssetColor() const
{
	return FColor::Emerald;
}

TSoftClassPtr<UObject> UAssetDefinition_PCGAsset::GetAssetClass() const
{
	return UPCGDataAsset::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_PCGAsset::GetAssetCategories() const
{
	static const FAssetCategoryPath Categories[] = { FAssetCategoryPath(LOCTEXT("PCGCategory", "PCG")) };
	return Categories;
}

#undef LOCTEXT_NAMESPACE
