// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_NiagaraScript.h"
#include "NiagaraScript.h"
#include "NiagaraScriptToolkit.h"
#include "NiagaraEditorStyle.h"
#include "AssetRegistry/AssetData.h"
#include "NiagaraEditorUtilities.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "NiagaraScriptAssetTypeActions"

const FName FAssetTypeActions_NiagaraScriptFunctions::NiagaraFunctionScriptName = FName("Niagara Function Script");
const FName FAssetTypeActions_NiagaraScriptModules::NiagaraModuleScriptName = FName("Niagara Module Script");
const FName FAssetTypeActions_NiagaraScriptDynamicInputs::NiagaraDynamicInputScriptName = FName("Niagara Dynamic Input Script");


FColor FAssetTypeActions_NiagaraScript::GetTypeColor() const
{ 
	return FNiagaraEditorStyle::Get().GetColor("NiagaraEditor.AssetColors.Script").ToFColor(true); 
}

void FAssetTypeActions_NiagaraScript::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Script = Cast<UNiagaraScript>(*ObjIt);
		if (Script != NULL)
		{
			TSharedRef< FNiagaraScriptToolkit > NewNiagaraScriptToolkit(new FNiagaraScriptToolkit());
			NewNiagaraScriptToolkit->Initialize(Mode, EditWithinLevelEditor, Script);
		}
	}
}

FText FAssetTypeActions_NiagaraScript::GetDisplayNameFromAssetData(const FAssetData& AssetData) const
{
	static const FName NAME_Usage(TEXT("Usage"));
	const FAssetDataTagMapSharedView::FFindTagResult Usage = AssetData.TagsAndValues.FindTag(NAME_Usage);

	if (Usage.IsSet())
	{
		static const FString FunctionString(TEXT("Function"));
		static const FString ModuleString(TEXT("Module"));
		static const FString DynamicInputString(TEXT("DynamicInput"));
		if (Usage.GetValue() == FunctionString)
		{
			return FAssetTypeActions_NiagaraScriptFunctions::GetFormattedName();
		}
		else if (Usage.GetValue() == ModuleString)
		{
			return FAssetTypeActions_NiagaraScriptModules::GetFormattedName();
		}
		else if (Usage.GetValue() == DynamicInputString)
		{
			return FAssetTypeActions_NiagaraScriptDynamicInputs::GetFormattedName();
		}
	}

	return FAssetTypeActions_NiagaraScript::GetName();
}

void FAssetTypeActions_NiagaraScript::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
	Section.AddMenuEntry(
		"MarkDependentCompilableAssetsDirty",
		LOCTEXT("MarkDependentCompilableAssetsDirtyLabel", "Mark dependent compilable assets dirty"),
		LOCTEXT("MarkDependentCompilableAssetsDirtyToolTip", "Finds all niagara assets which depend on this asset either directly or indirectly,\n and marks them dirty so they can be saved with the latest version."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateStatic(&FNiagaraEditorUtilities::MarkDependentCompilableAssetsDirty, InObjects)));
}

UClass* FAssetTypeActions_NiagaraScript::GetSupportedClass() const
{ 
	return UNiagaraScript::StaticClass(); 
}

#undef LOCTEXT_NAMESPACE
