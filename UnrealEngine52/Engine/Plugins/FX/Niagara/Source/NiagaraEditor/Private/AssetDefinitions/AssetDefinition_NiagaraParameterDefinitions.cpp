// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_NiagaraParameterDefinitions.h"
#include "NiagaraEditorStyle.h"
#include "Toolkits/NiagaraParameterDefinitionsToolkit.h"

FLinearColor UAssetDefinition_NiagaraParameterDefinitions::GetAssetColor() const
{
	return FNiagaraEditorStyle::Get().GetColor("NiagaraEditor.AssetColors.ParameterDefinitions");
}

EAssetCommandResult UAssetDefinition_NiagaraParameterDefinitions::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	for (UNiagaraParameterDefinitions* ParameterDefinitions : OpenArgs.LoadObjects<UNiagaraParameterDefinitions>())
   	{
		TSharedRef<FNiagaraParameterDefinitionsToolkit> NewNiagaraParameterDefinitionsToolkit(new FNiagaraParameterDefinitionsToolkit());
		NewNiagaraParameterDefinitionsToolkit->Initialize(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, ParameterDefinitions);
	}

	return EAssetCommandResult::Handled;
}
