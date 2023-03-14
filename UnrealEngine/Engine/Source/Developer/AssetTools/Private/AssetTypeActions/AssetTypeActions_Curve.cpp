// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_Curve.h"
#include "EditorFramework/AssetImportData.h"
#include "ICurveAssetEditor.h"
#include "CurveAssetEditorModule.h"
#include "DesktopPlatformModule.h"
#include "ToolMenuSection.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

void FAssetTypeActions_Curve::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto Curve = Cast<UCurveBase>(*ObjIt);
		if (Curve != nullptr)
		{
			FCurveAssetEditorModule& CurveAssetEditorModule = FModuleManager::LoadModuleChecked<FCurveAssetEditorModule>( "CurveAssetEditor" );
			TSharedRef< ICurveAssetEditor > NewCurveAssetEditor = CurveAssetEditorModule.CreateCurveAssetEditor( Mode, EditWithinLevelEditor, Curve );
		}
	}
}

void FAssetTypeActions_Curve::GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const
{
	for (auto& Asset : TypeAssets)
	{
		const auto Curve = CastChecked<UCurveBase>(Asset);
		if (Curve->AssetImportData)
		{
			Curve->AssetImportData->ExtractFilenames(OutSourceFilePaths);
		}
	}
}

void FAssetTypeActions_Curve::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
	// Don't show the import/export options if even one of the assets doesn't support the option
	bool bShowOptions = true;
	for (const UObject* Object : InObjects)
	{
		bShowOptions &= (Cast<UCurveBase>(Object) != nullptr);
	}

	if (bShowOptions)
	{
		const TArray<TWeakObjectPtr<UObject>> Curves = GetTypedWeakObjectPtrs<UObject>(InObjects);
	
		Section.AddMenuEntry(
		"Curve_ImportFromJSON",
		LOCTEXT("Curve_ImportFromJSON", "Import from JSON"),
		LOCTEXT("Curve_ImportFromJSONTooltip", "Import the curve from a file containing JSON data."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_Curve::ExecuteImportFromJSON, Curves),
			FCanExecuteAction())
		);
		
		Section.AddMenuEntry(
		"Curve_ExportAsJSON",
		LOCTEXT("Curve_ExportAsJSON", "Export as JSON"),
		LOCTEXT("Curve_ExportAsJSONTooltip", "Export the curve as a file containing JSON data."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_Curve::ExecuteExportAsJSON, Curves),
			FCanExecuteAction())
		);
	}
}

void FAssetTypeActions_Curve::PerformAssetDiff(UObject* OldAsset, UObject* NewAsset, const FRevisionInfo& OldRevision, const FRevisionInfo& NewRevision) const
{
	UCurveBase* OldCurve = Cast<UCurveBase>(OldAsset);
	UCurveBase* NewCurve = Cast<UCurveBase>(NewAsset);

	if (!ensure(OldCurve != nullptr && NewCurve != nullptr))
	{
		return;
	}

	// Build names for temp json files
	const FString RelOldTempFileName = FString::Printf(TEXT("%sTemp%s-%s.json"), *FPaths::DiffDir(), *OldAsset->GetName(), *OldRevision.Revision);
	const FString AbsoluteOldTempFileName = FPaths::ConvertRelativePathToFull(RelOldTempFileName);
	const FString RelNewTempFileName = FString::Printf(TEXT("%sTemp%s-%s.json"), *FPaths::DiffDir(), *NewAsset->GetName(), *NewRevision.Revision);
	const FString AbsoluteNewTempFileName = FPaths::ConvertRelativePathToFull(RelNewTempFileName);

	// save temp files
	const bool OldResult = FFileHelper::SaveStringToFile(OldCurve->ExportAsJSONString(), *AbsoluteOldTempFileName);
	const bool NewResult = FFileHelper::SaveStringToFile(NewCurve->ExportAsJSONString(), *AbsoluteNewTempFileName);

	if (OldResult && NewResult)
	{
		const FString DiffCommand = GetDefault<UEditorLoadingSavingSettings>()->TextDiffToolPath.FilePath;

		FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
		AssetToolsModule.Get().CreateDiffProcess(DiffCommand, AbsoluteOldTempFileName, AbsoluteNewTempFileName);
	}
	else
	{
		FAssetTypeActions_Base::PerformAssetDiff(OldAsset, NewAsset, OldRevision, NewRevision);
	}
}

void FAssetTypeActions_Curve::ExecuteExportAsJSON(const TArray<TWeakObjectPtr<UObject>> Objects) const
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();

	const void* ParentWindowWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);

	for (const TWeakObjectPtr<UObject>& Object : Objects)
	{
		UCurveBase* Curve = Cast<UCurveBase>(Object);
		if (Curve != nullptr)
		{
			const FText Title = FText::Format(LOCTEXT("Curve_ExportJSONDialogTitle", "Export '{0}' as JSON..."), FText::FromString(*Curve->GetName()));
			const FString CurrentFilename = Curve->AssetImportData->GetFirstFilename();
			const FString FileTypes = TEXT("Curve JSON (*.json)|*.json");

			TArray<FString> OutFilenames;
			DesktopPlatform->SaveFileDialog(
				ParentWindowWindowHandle,
				Title.ToString(),
				(CurrentFilename.IsEmpty()) ? TEXT("") : FPaths::GetPath(CurrentFilename),
				(CurrentFilename.IsEmpty()) ? TEXT("") : FPaths::GetBaseFilename(CurrentFilename) + TEXT(".json"),
				FileTypes,
				EFileDialogFlags::None,
				OutFilenames
				);

			if (OutFilenames.Num() > 0)
			{
				FFileHelper::SaveStringToFile(Curve->ExportAsJSONString(), *OutFilenames[0]);
			}
		}
	}
}

void FAssetTypeActions_Curve::ExecuteImportFromJSON(const TArray<TWeakObjectPtr<UObject>> Objects) const
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();

	const void* ParentWindowWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);

	for (const TWeakObjectPtr<UObject>& Object : Objects)
	{
		UCurveBase* Curve = Cast<UCurveBase>(Object);
		if (Curve != nullptr)
		{
			const FText Title = FText::Format(LOCTEXT("Curve_ImportJSONDialogTitle", "Import '{0}' from JSON..."), FText::FromString(*Curve->GetName()));
			const FString CurrentFilename = Curve->AssetImportData->GetFirstFilename();
			const FString FileTypes = TEXT("Curve JSON (*.json)|*.json");

			TArray<FString> OutFilenames;
			DesktopPlatform->OpenFileDialog(
				ParentWindowWindowHandle,
				Title.ToString(),
				(CurrentFilename.IsEmpty()) ? TEXT("") : FPaths::GetPath(CurrentFilename),
				(CurrentFilename.IsEmpty()) ? TEXT("") : FPaths::GetBaseFilename(CurrentFilename) + TEXT(".json"),
				FileTypes,
				EFileDialogFlags::None,
				OutFilenames
				);

			if (OutFilenames.Num() > 0)
			{
				FString FileContent;
				FFileHelper::LoadFileToString(FileContent, *OutFilenames[0]);

				// If the JSON file contains the quoted string format, parse it into a valid JSON string, else use it as is
				FString JSONString;
				if (!FParse::QuotedString(*FileContent, JSONString))
				{
					JSONString = FileContent;
				}

				TArray<FString> Problems;
				Curve->ImportFromJSONString(JSONString, Problems);
				if (Problems.Num() > 0)
				{
					FString BigProblem;
					for (const FString& Problem :  Problems)
					{
						BigProblem.Append(Problem).Append(FString(TEXT("\n")));
					}
					FText WarningText = FText::FromString(FString::Format(TEXT("Couldn't import file {0} into {1}:\n\n{2}"), { OutFilenames[0], Curve->GetName(), BigProblem }));
					FMessageDialog::Open(EAppMsgType::Ok, WarningText);
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
