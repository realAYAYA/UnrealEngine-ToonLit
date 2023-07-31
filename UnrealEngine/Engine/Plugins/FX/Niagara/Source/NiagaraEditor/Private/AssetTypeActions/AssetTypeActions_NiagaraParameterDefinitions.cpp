// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_NiagaraParameterDefinitions.h"
#include "NiagaraEditorStyle.h"
#include "NiagaraParameterDefinitions.h"
#include "Toolkits/NiagaraParameterDefinitionsToolkit.h"

FColor FAssetTypeActions_NiagaraParameterDefinitions::GetTypeColor() const
{
	return FNiagaraEditorStyle::Get().GetColor("NiagaraEditor.AssetColors.ParameterDefinitions").ToFColor(true);
}

UClass* FAssetTypeActions_NiagaraParameterDefinitions::GetSupportedClass() const
{
	return UNiagaraParameterDefinitions::StaticClass();
}

void FAssetTypeActions_NiagaraParameterDefinitions::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor /*= TSharedPtr<IToolkitHost>()*/)
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto ParameterDefinitions = Cast<UNiagaraParameterDefinitions>(*ObjIt);
		if (ParameterDefinitions != nullptr)
		{
			TSharedRef< FNiagaraParameterDefinitionsToolkit > NewNiagaraParameterDefinitionsToolkit(new FNiagaraParameterDefinitionsToolkit());
			NewNiagaraParameterDefinitionsToolkit->Initialize(Mode, EditWithinLevelEditor, ParameterDefinitions);
		}
	}
}

