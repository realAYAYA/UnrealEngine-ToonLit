// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_NiagaraParameterCollectionInstance.h"
#include "NiagaraParameterCollection.h"
#include "Toolkits/NiagaraParameterCollectionToolkit.h"
#include "NiagaraEditorStyle.h"

#define LOCTEXT_NAMESPACE "UAssetDefinition_NiagaraParameterCollectionInstance"

FLinearColor UAssetDefinition_NiagaraParameterCollectionInstance::GetAssetColor() const
{ 
	return FNiagaraEditorStyle::Get().GetColor("NiagaraEditor.AssetColors.ParameterCollectionInstance"); 
}

EAssetCommandResult UAssetDefinition_NiagaraParameterCollectionInstance::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	for (UNiagaraParameterCollectionInstance* Instance : OpenArgs.LoadObjects<UNiagaraParameterCollectionInstance>())
	{
		TSharedRef< FNiagaraParameterCollectionToolkit > NewNiagaraNPCToolkit(new FNiagaraParameterCollectionToolkit());
		NewNiagaraNPCToolkit->Initialize(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, Instance);
	}

	return EAssetCommandResult::Handled;
}

#undef LOCTEXT_NAMESPACE
