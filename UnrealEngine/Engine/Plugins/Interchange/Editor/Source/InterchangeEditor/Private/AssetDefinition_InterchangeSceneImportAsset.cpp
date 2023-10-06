// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_InterchangeSceneImportAsset.h"

#include "InterchangeAssetImportData.h"
#include "InterchangeEditorModule.h"
#include "InterchangeManager.h"
#include "InterchangeSceneImportAsset.h"

#include "ContentBrowserMenuContexts.h"
#include "Misc/App.h"
#include "Misc/DelayedAutoRegister.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"
#include "ToolMenus.h"
#include "ToolMenuSection.h"
#include "EditorReimportHandler.h"

#define LOCTEXT_NAMESPACE "AssetDefinition_InterchangeSceneImportAsset"

FAssetCategoryPath UAssetDefinition_InterchangeSceneImportAsset::Interchange(LOCTEXT("Interchange_Category_Path", "Interchange"));

EAssetCommandResult UAssetDefinition_InterchangeSceneImportAsset::OpenAssets(const FAssetOpenArgs& /*OpenArgs*/) const
{
	return EAssetCommandResult::Handled;
}

UThumbnailInfo* UAssetDefinition_InterchangeSceneImportAsset::LoadThumbnailInfo(const FAssetData& InAssetData) const
{
	return UE::Editor::FindOrCreateThumbnailInfo(InAssetData.GetAsset(), USceneThumbnailInfo::StaticClass());
}

namespace MenuExtension_InterchangeSceneImportAsset
{
	void ExecuteReimportOneAsset(UInterchangeSceneImportAsset* Asset, const FString& FilePath)
	{
		using namespace UE::Interchange;

		FScopedSourceData ScopedSourceData(FilePath);
		const UInterchangeSourceData* SourceData = ScopedSourceData.GetSourceData();

		UInterchangeManager& InterchangeManager = UInterchangeManager::GetInterchangeManager();
		if (InterchangeManager.CanTranslateSourceData(SourceData))
		{
			FImportAssetParameters ImportAssetParameters;
			ImportAssetParameters.bIsAutomated = GIsAutomationTesting || FApp::IsUnattended() || IsRunningCommandlet() || GIsRunningUnattendedScript;
			ImportAssetParameters.ReimportAsset = Asset;
			ImportAssetParameters.ReimportSourceIndex = INDEX_NONE;

			TTuple<FAssetImportResultRef, FSceneImportResultRef> ImportResult = InterchangeManager.ImportSceneAsync(FString(), SourceData, ImportAssetParameters);

			// TODO: Report if reimport failed
			//return ImportResult;
		}
	}

	void ExecuteReimport(const FToolMenuContext& InContext)
	{
		const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InContext);
		TArray<UInterchangeSceneImportAsset*> SceneImportAssets = Context->LoadSelectedObjects<UInterchangeSceneImportAsset>();

		for (UInterchangeSceneImportAsset* SceneImportAsset : SceneImportAssets)
		{
			if (SceneImportAsset)
			{
				ExecuteReimportOneAsset(SceneImportAsset, SceneImportAsset->AssetImportData->GetFirstFilename());
			}
		}
	}

	void ExecuteReimportWithFile(const FToolMenuContext& InContext)
	{
		const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InContext);
		TArray<UInterchangeSceneImportAsset*> SceneImportAssets = Context->LoadSelectedObjects<UInterchangeSceneImportAsset>();
		if (SceneImportAssets.Num() != 1 || !SceneImportAssets[0])
		{
			return;
		}
		
#if WITH_EDITORONLY_DATA
		UInterchangeSceneImportAsset* SceneImportAsset = SceneImportAssets[0];
		if (!SceneImportAsset->AssetImportData)
		{
			return;
		}

		TArray<FString> OpenFilenames = SceneImportAsset->AssetImportData->ExtractFilenames();
		FReimportManager::Instance()->GetNewReimportPath(SceneImportAsset, OpenFilenames);
		if (OpenFilenames.Num() == 1 && !OpenFilenames[0].IsEmpty())
		{
			ExecuteReimportOneAsset(SceneImportAsset, OpenFilenames[0]);
		}
#endif
	}

	bool CanExecuteReimport(const FToolMenuContext& InContext)
	{
		const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InContext);
		TArray<UInterchangeSceneImportAsset*> SceneImportAssets = Context->LoadSelectedObjects<UInterchangeSceneImportAsset>();

		for (UInterchangeSceneImportAsset* SceneImportAsset : SceneImportAssets)
		{
			if (SceneImportAsset && SceneImportAsset->AssetImportData)
			{
				// TODO: Check that at least one file exists before returning true
				if (SceneImportAsset->AssetImportData->ExtractFilenames().Num() > 0)
				{
					return true;
				}
			}
		}

		return false;
	}

	bool CanExecuteReimportWithFile(const FToolMenuContext& InContext)
	{
		const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InContext);
		TArray<UInterchangeSceneImportAsset*> SceneImportAssets = Context->LoadSelectedObjects<UInterchangeSceneImportAsset>();

		if (SceneImportAssets.Num() == 1)
		{
			const UInterchangeSceneImportAsset* SceneImportAsset = SceneImportAssets[0];
			if (SceneImportAsset && SceneImportAsset->AssetImportData)
			{
				return true;
			}
		}

		return false;
	}

	static FDelayedAutoRegisterHelper DelayedAutoRegister(EDelayedRegisterRunPhase::EndOfEngineInit, []{ 
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
		{
			FToolMenuOwnerScoped OwnerScoped(UE_MODULE_NAME);
			UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UInterchangeSceneImportAsset::StaticClass());
		
			if (!Menu->FindSection("Interchange"))
			{
				const FToolMenuInsert MenuInsert(NAME_None, EToolMenuInsertType::First);
				Menu->AddSection("Interchange", LOCTEXT("InterchangeSceneImportAsset_Section", "Interchange"), MenuInsert);
			}
			FToolMenuSection& Section = Menu->FindOrAddSection("Interchange");
			Section.AddDynamicEntry(NAME_None, FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
			{
				{
					const TAttribute<FText> Label = LOCTEXT("InterchangeSceneImportAsset_Reimport", "Reimport Scene");
					const TAttribute<FText> ToolTip = LOCTEXT("InterchangeSceneImportAsset_ReimportTooltip", "Reimport the scene associated with each selected InterchangeSceneImportAsset.");
					const FSlateIcon Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Import");

					FToolUIAction UIAction;
					UIAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&ExecuteReimport);
					UIAction.CanExecuteAction = FToolMenuCanExecuteAction::CreateStatic(&CanExecuteReimport);

					InSection.AddMenuEntry("InterchangeSceneImportAsset_Reimport", Label, ToolTip, Icon, UIAction);
				}
				{
					const TAttribute<FText> Label = LOCTEXT("InterchangeSceneImportAsset_ReimportWithFile", "Reimport Scene With File");
					const TAttribute<FText> ToolTip = LOCTEXT("InterchangeSceneImportAsset_ReimportWithFile_Tooltip", "Reimport the scene associated with the selected InterchangeSceneImportAsset using a new file.");
					const FSlateIcon Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Import");

					FToolUIAction UIAction;
					UIAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&ExecuteReimportWithFile);
					UIAction.CanExecuteAction = FToolMenuCanExecuteAction::CreateStatic(&CanExecuteReimportWithFile);

					InSection.AddMenuEntry("InterchangeSceneImportAsset_ReimportWithFile", Label, ToolTip, Icon, UIAction);
				}
			}));
		}));
	});
}

#undef LOCTEXT_NAMESPACE
