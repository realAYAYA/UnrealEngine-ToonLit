// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_ConsoleVariables.h"

#include "ConsoleVariablesAsset.h"
#include "ConsoleVariablesEditorStyle.h"
#include "ConsoleVariablesEditorModule.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"

#define LOCTEXT_NAMESPACE "ConsoleVariablesEditor"

FText FAssetTypeActions_ConsoleVariables::GetName() const
{
	return LOCTEXT("AssetTypeActions_ConsoleVariable_Name", "Console Variable Collection");
}

UClass* FAssetTypeActions_ConsoleVariables::GetSupportedClass() const
{
	return UConsoleVariablesAsset::StaticClass();
}

void FAssetTypeActions_ConsoleVariables::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder)
{
	FAssetTypeActions_Base::GetActions(InObjects, MenuBuilder);

	const TArray<TWeakObjectPtr<UConsoleVariablesAsset>> ConsoleVariableAssets = GetTypedWeakObjectPtrs<UConsoleVariablesAsset>(InObjects);
	const int32 ConsoleVariablesAssetCount = ConsoleVariableAssets.Num();

	MenuBuilder.AddMenuEntry(
		LOCTEXT("AssetTypeActions_OpenVariableCollection", "Open Variable Collection in Editor"),
		LOCTEXT("AssetTypeActions_OpenVariableCollectionToolTip", "Open this console variable collection in the Console Variables Editor. Select only one asset at a time."),
		FSlateIcon(FAppStyle::Get().GetStyleSetName(), "SystemWideCommands.SummonOpenAssetDialog"),
		FUIAction(
			FExecuteAction::CreateLambda([this, InObjects] {
					OpenAssetEditor(InObjects, TSharedPtr<IToolkitHost>());
			}),
			FCanExecuteAction::CreateLambda([ConsoleVariablesAssetCount] {
					// We only want to open a single Variable Collection asset at a time, so let's ensure
					// the number of selected assets is exactly one.
					return ConsoleVariablesAssetCount == 1;
				}
			)
		)
	);
	
	MenuBuilder.AddMenuEntry(
		LOCTEXT("AssetTypeActions_ExecuteVariableCollection", "Execute Variable Collection"),
		LOCTEXT("AssetTypeActions_ExecuteVariableCollectionToolTip", "Executes all commands and variables saved to the selected assets."),
		FSlateIcon(FConsoleVariablesEditorStyle::Get().GetStyleSetName(), "ConsoleVariables.ToolbarButton.Small"),
		FUIAction(
			FExecuteAction::CreateLambda([ConsoleVariableAssets] {
				UWorld* CurrentWorld = nullptr;
				check (GIsEditor);
				{
					CurrentWorld = GEditor->GetEditorWorldContext().World();
				}
				
				for (TWeakObjectPtr<UConsoleVariablesAsset> Asset : ConsoleVariableAssets)
				{
					if (Asset.IsValid())
					{
						Asset->ExecuteSavedCommands(CurrentWorld);
					}
				}
			}),
			FCanExecuteAction::CreateLambda([ConsoleVariablesAssetCount] {
					// We only want to open a single Variable Collection asset at a time, so let's ensure
					// the number of selected assets is exactly one.
					return ConsoleVariablesAssetCount > 0;
				}
			)
		)
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("AssetTypeActions_ExecuteOnlyCheckedInVariableCollection", "Execute Variable Collection (Only Checked)"),
		LOCTEXT("AssetTypeActions_ExecuteOnlyCheckedInVariableCollectionToolTip", "Executes commands and variables with a Checked checkstate saved to the selected assets. Checkstates can be edited in the Console Variables Editor UI."),
		FSlateIcon(FConsoleVariablesEditorStyle::Get().GetStyleSetName(), "ConsoleVariables.ToolbarButton.Small"),
		FUIAction(
			FExecuteAction::CreateLambda([ConsoleVariableAssets] {
				UWorld* CurrentWorld = nullptr;
				check (GIsEditor)
				{
					CurrentWorld = GEditor->GetEditorWorldContext().World();
				}
				
				for (TWeakObjectPtr<UConsoleVariablesAsset> Asset : ConsoleVariableAssets)
				{
					if (Asset.IsValid())
					{
						Asset->ExecuteSavedCommands(CurrentWorld, true);
					}
				}
			}),
			FCanExecuteAction::CreateLambda([ConsoleVariablesAssetCount] {
					// We only want to open a single Variable Collection asset at a time, so let's ensure
					// the number of selected assets is exactly one.
					return ConsoleVariablesAssetCount > 0;
				}
			)
		)
	);
}

void FAssetTypeActions_ConsoleVariables::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	if (InObjects.Num() && InObjects[0])
	{
		FConsoleVariablesEditorModule& Module = FConsoleVariablesEditorModule::Get();
					
		Module.OpenConsoleVariablesDialogWithAssetSelected(FAssetData(InObjects[0]));
	}
}

#undef LOCTEXT_NAMESPACE
