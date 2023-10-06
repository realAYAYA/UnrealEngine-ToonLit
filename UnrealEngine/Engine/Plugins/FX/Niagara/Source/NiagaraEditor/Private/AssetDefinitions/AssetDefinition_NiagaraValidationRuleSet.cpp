// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_NiagaraValidationRuleSet.h"
#include "NiagaraEditorStyle.h"

FLinearColor UAssetDefinition_NiagaraValidationRuleSet::GetAssetColor() const
{
	return FNiagaraEditorStyle::Get().GetColor("NiagaraEditor.AssetColors.ValidationRuleSet");
}
