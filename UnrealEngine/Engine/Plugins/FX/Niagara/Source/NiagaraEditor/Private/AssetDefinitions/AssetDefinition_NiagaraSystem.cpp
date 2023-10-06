// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_NiagaraSystem.h"
#include "NiagaraSystem.h"
#include "Toolkits/NiagaraSystemToolkit.h"
#include "NiagaraEditorStyle.h"

FLinearColor UAssetDefinition_NiagaraSystem::GetAssetColor() const
{
	return FNiagaraEditorStyle::Get().GetColor("NiagaraEditor.AssetColors.System");
}

EAssetCommandResult UAssetDefinition_NiagaraSystem::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	for (UNiagaraSystem* System : OpenArgs.LoadObjects<UNiagaraSystem>())
	{
		TSharedRef<FNiagaraSystemToolkit> NiagaraSystemToolkit = MakeShared<FNiagaraSystemToolkit>();
		NiagaraSystemToolkit->InitializeWithSystem(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, *System);
	}

	return EAssetCommandResult::Handled;
}
