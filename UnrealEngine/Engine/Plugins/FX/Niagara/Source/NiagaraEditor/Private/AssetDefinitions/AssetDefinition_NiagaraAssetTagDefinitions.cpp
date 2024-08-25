// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_NiagaraAssetTagDefinitions.h"

#define LOCTEXT_NAMESPACE "UAssetDefinition_NiagaraAssetTagDefinitions"

UAssetDefinition_NiagaraAssetTagDefinitions::UAssetDefinition_NiagaraAssetTagDefinitions()
{
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_NiagaraAssetTagDefinitions::GetAssetCategories() const
{
	static const auto Categories = { EAssetCategoryPaths::FX / NSLOCTEXT("Niagara", "NiagaraAssetSubMenu_Advanced", "Advanced")};
	return Categories;
}

#undef LOCTEXT_NAMESPACE
