// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_DatasmithScene.h"

#include "DatasmithAssetImportData.h"
#include "DatasmithContentEditorModule.h"
#include "DatasmithContentEditorStyle.h"
#include "DatasmithScene.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ToolMenuSection.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions_DatasmithScene"

uint32 FAssetTypeActions_DatasmithScene::GetCategories()
{
	return IDatasmithContentEditorModule::DatasmithAssetCategoryBit;
}

FText FAssetTypeActions_DatasmithScene::GetName() const
{
	return LOCTEXT("AssetTypeActions_DatasmithScene_Name", "Datasmith Scene");
}

UClass* FAssetTypeActions_DatasmithScene::GetSupportedClass() const
{
	return UDatasmithScene::StaticClass();
}

void FAssetTypeActions_DatasmithScene::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
	TArray<TWeakObjectPtr<UDatasmithScene>> Scenes = GetTypedWeakObjectPtrs<UDatasmithScene>(InObjects);

	FilterByDirectLinkAutoReimportSupport(Scenes);

	const bool bHasCanExecuteAutoReimport = Scenes.Num() > 0;
	
	bool bHasAutoReimportEnabled = false;
	const IDatasmithContentEditorModule& DatasmithContentEditorModule = IDatasmithContentEditorModule::Get();
	for (const TWeakObjectPtr<UDatasmithScene>& Scene : Scenes)
	{
		if (Scene.IsValid())
		{
			bHasAutoReimportEnabled = DatasmithContentEditorModule.IsAssetAutoReimportEnabled(Scene.Get()).Get(false);
			if (bHasAutoReimportEnabled)
			{
				break;
			}
		}
	}

	const FText AutoReimportText = LOCTEXT("DatasmithScene_ToggleDirectLinkAutoReimport", "Direct Link Auto-Reimport");
	const FText AutoReimportTooltip = LOCTEXT("DatasmithScene_ToggleDirectLinkAutoReimportTooltip", "Toggle Direct Link Auto-Reimport for all selected Datasmith Scenes.");

	// #ueent_todo DatasmithContent should not know about DirectLink, this should be made modular, or we should directly depend on DirectLinkExtension module.
	Section.AddMenuEntry(
		"DatasmithScene_ToggleDirectLinkAutoReimport",
		AutoReimportText,
		AutoReimportTooltip,
		FSlateIcon(FDatasmithContentEditorStyle::GetStyleSetName(), "DatasmithContent.AutoReimportGrayscale"),
		FUIAction(
			FExecuteAction::CreateLambda([Scenes = MoveTemp(Scenes), bHasAutoReimportEnabled]() { FAssetTypeActions_DatasmithScene::ExecuteToggleDirectLinkAutoReimport(Scenes, !bHasAutoReimportEnabled); }),
			FCanExecuteAction::CreateLambda([bHasCanExecuteAutoReimport]() { return bHasCanExecuteAutoReimport; }),
			FGetActionCheckState::CreateLambda([bHasAutoReimportEnabled]() { return bHasAutoReimportEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
		)
		, EUserInterfaceActionType::Check
	);
}

void FAssetTypeActions_DatasmithScene::GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const
{
	for ( const UObject* Asset : TypeAssets )
	{
		const UDatasmithScene* DatasmithScene = CastChecked< UDatasmithScene >( Asset );

		if ( DatasmithScene && DatasmithScene->AssetImportData )
		{
			DatasmithScene->AssetImportData->ExtractFilenames( OutSourceFilePaths );
		}
	}
}

void FAssetTypeActions_DatasmithScene::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor)
{
	if (InObjects.Num() == 0)
	{
		return;
	}

	FOnCreateDatasmithSceneEditor DatasmithSceneEditorHandler = IDatasmithContentEditorModule::Get().GetDatasmithSceneEditorHandler();

	if (DatasmithSceneEditorHandler.IsBound() == false)
	{
		FSimpleAssetEditor::CreateEditor(EToolkitMode::Standalone, EditWithinLevelEditor, InObjects);
		return;
	}

	for (UObject* Object : InObjects)
	{
		UDatasmithScene* DatasmithScene = Cast<UDatasmithScene>(Object);
		if (DatasmithScene != nullptr)
		{
			DatasmithSceneEditorHandler.ExecuteIfBound(EToolkitMode::Standalone, EditWithinLevelEditor, DatasmithScene);
		}
	}
}

void FAssetTypeActions_DatasmithScene::ExecuteToggleDirectLinkAutoReimport(const TArray<TWeakObjectPtr<UDatasmithScene>>& Scenes, bool bEnabled)
{
	const IDatasmithContentEditorModule& DatasmithContentEditorModule = IDatasmithContentEditorModule::Get();

	for (const TWeakObjectPtr<class UDatasmithScene>& Scene : Scenes)
	{
		if (Scene.IsValid())
		{
			DatasmithContentEditorModule.SetAssetAutoReimport(Scene.Get(), bEnabled);
		}
	}
}

void FAssetTypeActions_DatasmithScene::FilterByDirectLinkAutoReimportSupport(TArray<TWeakObjectPtr<UDatasmithScene>>& Scenes)
{
	const IDatasmithContentEditorModule& DatasmithContentEditorModule = IDatasmithContentEditorModule::Get();

	for (int32 SceneIndex = Scenes.Num() - 1; SceneIndex >= 0; --SceneIndex)
	{
		if (Scenes[SceneIndex].IsValid())
		{
			const bool bIsReimportAvailable = DatasmithContentEditorModule.IsAssetAutoReimportAvailable(Scenes[SceneIndex].Get()).Get(false);
			if (bIsReimportAvailable)
			{
				continue;
			}
		}
		Scenes.RemoveAtSwap(SceneIndex);
	}
}


#undef LOCTEXT_NAMESPACE
