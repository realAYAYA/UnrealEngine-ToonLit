// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinitions/AssetDefinition_NiagaraDataChannelDefinitions.h"

#include "NiagaraEditorStyle.h"
#include "NiagaraDataChannelDefinitions.h"

#define LOCTEXT_NAMESPACE "UAssetDefinition_NiagaraDataChannelDefinitions"

FLinearColor UAssetDefinition_NiagaraDataChannelDefinitions::GetAssetColor() const
{
	return FNiagaraEditorStyle::Get().GetColor("NiagaraEditor.AssetColors.DataChannelDefinitions");
}

TSoftClassPtr<> UAssetDefinition_NiagaraDataChannelDefinitions::GetAssetClass() const
{
	return UNiagaraDataChannelDefinitions::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_NiagaraDataChannelDefinitions::GetAssetCategories() const
{
	static FAssetCategoryPath AssetPaths[] = { FAssetCategoryPath(LOCTEXT("NiagaraAssetsCategory", "FX"), LOCTEXT("NiagaraDataChannelDefinitions_SubCategory", "Advanced")) };
	return AssetPaths;
}

#undef LOCTEXT_NAMESPACE
