// Copyright Epic Games, Inc. All Rights Reserved.
#include "AssetTypeActions_NiagaraEffectType.h"
#include "NiagaraEffectType.h"
#include "NiagaraEffectTypeFactoryNew.h"
#include "NiagaraEditorStyle.h"

#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"

#include "ToolMenus.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions_NiagaraEffectType"

FColor FAssetTypeActions_NiagaraEffectType::GetTypeColor() const
{
	return FNiagaraEditorStyle::Get().GetColor("NiagaraEditor.AssetColors.EffectType").ToFColor(true);
}

// void FAssetTypeActions_NiagaraEffectType::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
// {
// 	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;
// 
// 	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
// 	{
// 		auto Instance = Cast<UNiagaraParameterCollectionInstance>(*ObjIt);
// 		if (Instance != NULL)
// 		{
// 			TSharedRef< FNiagaraParameterCollectionToolkit > NewNiagaraNPCToolkit(new FNiagaraParameterCollectionToolkit());
// 			NewNiagaraNPCToolkit->Initialize(Mode, EditWithinLevelEditor, Instance);
// 		}
// 	}
// }

UClass* FAssetTypeActions_NiagaraEffectType::GetSupportedClass() const
{
	return UNiagaraEffectType::StaticClass();
}

#undef LOCTEXT_NAMESPACE
