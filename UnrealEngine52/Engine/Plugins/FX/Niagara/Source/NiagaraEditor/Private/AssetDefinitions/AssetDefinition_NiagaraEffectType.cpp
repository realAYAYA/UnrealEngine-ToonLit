// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_NiagaraEffectType.h"
#include "NiagaraEditorStyle.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions_NiagaraEffectType"

FLinearColor UAssetDefinition_NiagaraEffectType::GetAssetColor() const
{
	return FNiagaraEditorStyle::Get().GetColor("NiagaraEditor.AssetColors.EffectType");
}

#undef LOCTEXT_NAMESPACE
