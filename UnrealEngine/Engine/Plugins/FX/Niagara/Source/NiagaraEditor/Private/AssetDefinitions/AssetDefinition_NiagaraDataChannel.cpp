// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinitions/AssetDefinition_NiagaraDataChannel.h"

#include "NiagaraEditorStyle.h"
#include "NiagaraDataChannel.h"

#define LOCTEXT_NAMESPACE "UAssetDefinition_NiagaraDataChannel"

FLinearColor UAssetDefinition_NiagaraDataChannel::GetAssetColor() const
{
	return FNiagaraEditorStyle::Get().GetColor("NiagaraEditor.AssetColors.DataChannel");
}

TSoftClassPtr<> UAssetDefinition_NiagaraDataChannel::GetAssetClass() const
{
	return UNiagaraDataChannelAsset::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_NiagaraDataChannel::GetAssetCategories() const
{
	static FAssetCategoryPath AssetPaths[] = { FAssetCategoryPath(LOCTEXT("NiagaraAssetsCategory", "FX"), LOCTEXT("NiagaraDataChannel_SubCategory", "Advanced")) };
	return AssetPaths;
}

#undef LOCTEXT_NAMESPACE


