// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/AssetDefinition_DataflowAsset.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "ContentBrowserModule.h"
#include "Dataflow/DataflowEditor.h"
#include "Dataflow/DataflowEditorModule.h"
#include "Dataflow/DataflowObject.h"
#include "Dialog/SMessageDialog.h"
#include "IContentBrowserSingleton.h"
#include "Math/Color.h"
#include "Misc/FileHelper.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"


#define LOCTEXT_NAMESPACE "AssetActions_DataflowAsset"

namespace DataflowAssetDefinitionHelpers
{
	// Return true if we should proceed, false if we should re-open the dialog
	bool CreateNewDataflowAsset(const UObject* Asset, UObject*& OutDataflowAsset)
	{
		const UClass* const DataflowClass = UDataflow::StaticClass();

		FSaveAssetDialogConfig NewDataflowAssetDialogConfig;
		{
			const FString PackageName = Asset->GetOutermost()->GetName();
			NewDataflowAssetDialogConfig.DefaultPath = FPackageName::GetLongPackagePath(PackageName);
			const FString AssetName = Asset->GetName();
			NewDataflowAssetDialogConfig.DefaultAssetName = AssetName + "_Dataflow";
			NewDataflowAssetDialogConfig.AssetClassNames.Add(DataflowClass->GetClassPathName());
			NewDataflowAssetDialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::Disallow;
			NewDataflowAssetDialogConfig.DialogTitleOverride = LOCTEXT("NewDataflowAssetDialogTitle", "Save Dataflow Asset As");
		}

		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

		FString NewPackageName;
		FText OutError;
		for (bool bFilenameValid = false; !bFilenameValid; bFilenameValid = FFileHelper::IsFilenameValidForSaving(NewPackageName, OutError))
		{
			const FString AssetSavePath = ContentBrowserModule.Get().CreateModalSaveAssetDialog(NewDataflowAssetDialogConfig);
			if (AssetSavePath.IsEmpty())
			{
				OutDataflowAsset = nullptr;
				return false;
			}
			NewPackageName = FPackageName::ObjectPathToPackageName(AssetSavePath);
		}

		const FName NewAssetName(FPackageName::GetLongPackageAssetName(NewPackageName));
		UPackage* const NewPackage = CreatePackage(*NewPackageName);
		UObject* const NewAsset = NewObject<UObject>(NewPackage, DataflowClass, NewAssetName, RF_Public | RF_Standalone | RF_Transactional);

		NewAsset->MarkPackageDirty();

		// Notify the asset registry
		FAssetRegistryModule::AssetCreated(NewAsset);

		OutDataflowAsset = NewAsset;
		return true;
	}


	// Return true if we should proceed, false if we should re-open the dialog
	bool OpenDataflowAsset(const UObject* Asset, UObject*& OutDataflowAsset)
	{
		const UClass* const DataflowClass = UDataflow::StaticClass();

		FOpenAssetDialogConfig NewDataflowAssetDialogConfig;
		{
			const FString PackageName = Asset->GetOutermost()->GetName();
			NewDataflowAssetDialogConfig.DefaultPath = FPackageName::GetLongPackagePath(PackageName);
			NewDataflowAssetDialogConfig.AssetClassNames.Add(DataflowClass->GetClassPathName());
			NewDataflowAssetDialogConfig.bAllowMultipleSelection = false;
			NewDataflowAssetDialogConfig.DialogTitleOverride = LOCTEXT("OpenDataflowAssetDialogTitle", "Open Dataflow Asset");
		}

		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		TArray<FAssetData> AssetData = ContentBrowserModule.Get().CreateModalOpenAssetDialog(NewDataflowAssetDialogConfig);

		if (AssetData.Num() == 1)
		{
			OutDataflowAsset = AssetData[0].GetAsset();
			return true;
		}

		return false;
	}

	// Return true if we should proceed, false if we should re-open the dialog
	bool NewOrOpenDialog(const UObject* Asset, UObject*& OutDataflowAsset)
	{
		TSharedRef<SMessageDialog> ConfirmDialog = SNew(SMessageDialog)
			.Title(FText(LOCTEXT("Dataflow_WindowTitle", "Create or Open Dataflow graph?")))
			.Message(LOCTEXT("Dataflow_WindowText", "This Asset currently has no Dataflow graph"))
			.Buttons({
				SMessageDialog::FButton(LOCTEXT("Dataflow_NewText", "Create new Dataflow")),
				SMessageDialog::FButton(LOCTEXT("Dataflow_OpenText", "Open existing Dataflow")),
				SMessageDialog::FButton(LOCTEXT("Dataflow_ContinueText", "Continue without Dataflow")),
					 });

		const int32 ResultButtonIdx = ConfirmDialog->ShowModal();
		switch (ResultButtonIdx)
		{
		case 0:
			return CreateNewDataflowAsset(Asset, OutDataflowAsset);
			break;
		case 1:
			return OpenDataflowAsset(Asset, OutDataflowAsset);
			break;
		default:
			break;
		}

		return true;
	}

	// Create a new UDataflow if one doesn't already exist for the Asset
	UObject* NewOrOpenDataflowAsset(const UObject* Asset)
	{
		UObject* DataflowAsset = nullptr;
		bool bDialogDone = false;
		while (!bDialogDone)
		{
			bDialogDone = NewOrOpenDialog(Asset, DataflowAsset);
		}

		return DataflowAsset;
	}
}



namespace UE::Dataflow::DataflowAsset
{
	struct FColorScheme
	{
		static inline const FLinearColor Asset = FColor(180, 120, 110);
		static inline const FLinearColor NodeHeader = FColor(180, 120, 110);
		static inline const FLinearColor NodeBody = FColor(18, 12, 11, 127);
	};
}

FText UAssetDefinition_DataflowAsset::GetAssetDisplayName() const
{
	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_DataflowAsset", "DataflowAsset");
}

TSoftClassPtr<UObject> UAssetDefinition_DataflowAsset::GetAssetClass() const
{
	return UDataflow::StaticClass();
}

FLinearColor UAssetDefinition_DataflowAsset::GetAssetColor() const
{
	return UE::Dataflow::DataflowAsset::FColorScheme::Asset;
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_DataflowAsset::GetAssetCategories() const
{
	static const auto Categories = { EAssetCategoryPaths::Physics };
	return Categories;
}

UThumbnailInfo* UAssetDefinition_DataflowAsset::LoadThumbnailInfo(const FAssetData& InAsset) const
{
	return UE::Editor::FindOrCreateThumbnailInfo(InAsset.GetAsset(), USceneThumbnailInfo::StaticClass());
}

EAssetCommandResult UAssetDefinition_DataflowAsset::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	TArray<UDataflow*> DataflowObjects = OpenArgs.LoadObjects<UDataflow>();

	// For now the dataflow editor only works on one asset at a time
	ensure(DataflowObjects.Num() == 0 || DataflowObjects.Num() == 1);

	if (DataflowObjects.Num() == 1)
	{
		UAssetEditorSubsystem* const AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
		UDataflowEditor* const AssetEditor = NewObject<UDataflowEditor>(AssetEditorSubsystem, NAME_None, RF_Transient);

		// Validate the asset
		if (UDataflow* const DataflowAsset = CastChecked<UDataflow>(DataflowObjects[0]))
		{
			AssetEditor->InitializeContent(nullptr, DataflowAsset);
			return EAssetCommandResult::Handled;
		}
	}

	return EAssetCommandResult::Unhandled;
}

FString FDataflowConnectionData::GetNode(const FString InConnection)
{
	FString Left, Right;

	if (InConnection.Split(TEXT(":"), &Left, nullptr, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
	{
		if (Left.Split(TEXT("/"), nullptr, &Right, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
		{
			return Right;
		}
	}

	return FString("");
}

FString FDataflowConnectionData::GetProperty(const FString InConnection)
{
	FString Right;

	if (InConnection.Split(TEXT(":"), nullptr, &Right, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
	{
		return Right;
	}

	return FString("");
}

#undef LOCTEXT_NAMESPACE
