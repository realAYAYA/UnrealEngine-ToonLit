// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_ClothAsset.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "ChaosClothAsset/ClothEditor.h"
#include "Dataflow/DataflowObject.h"
#include "Misc/MessageDialog.h"
#include "Misc/FileHelper.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "FileHelpers.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"
#include "Misc/WarnIfAssetsLoadedInScope.h"

#define LOCTEXT_NAMESPACE "AssetDefinition_ClothAsset"

namespace ClothAssetDefinitionHelpers
{
	// Create a new UDataflow if one doesn't already exist for the Cloth Asset
	UObject* CreateNewDataflowAsset(const UChaosClothAsset* ClothAsset)
	{
		if (FMessageDialog::Open(EAppMsgType::YesNo, LOCTEXT("MissingDataflow", "This Cloth asset currently has no Dataflow graph. Would you like to create a new one?")) == EAppReturnType::Yes)
		{
			const UClass* const DataflowClass = UDataflow::StaticClass();

			FSaveAssetDialogConfig NewDataflowAssetDialogConfig;
			{
				const FString PackageName = ClothAsset->GetOutermost()->GetName();
				NewDataflowAssetDialogConfig.DefaultPath = FPackageName::GetLongPackagePath(PackageName);
				NewDataflowAssetDialogConfig.AssetClassNames.Add(DataflowClass->GetClassPathName());
				NewDataflowAssetDialogConfig.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::AllowButWarn;
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
					return nullptr;
				}
				NewPackageName = FPackageName::ObjectPathToPackageName(AssetSavePath);
			}

			const FName NewAssetName(FPackageName::GetLongPackageAssetName(NewPackageName));
			UPackage* const NewPackage = CreatePackage(*NewPackageName);
			UObject* const NewAsset = NewObject<UObject>(NewPackage, DataflowClass, NewAssetName, RF_Public | RF_Standalone | RF_Transactional);

			NewAsset->MarkPackageDirty();

			// Notify the asset registry
			FAssetRegistryModule::AssetCreated(NewAsset);

			// Save the package
			TArray<UPackage*> PackagesToSave;
			PackagesToSave.Add(NewAsset->GetOutermost());
			constexpr bool bCheckDirty = false;
			constexpr bool bPromptToSave = false;
			FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, bCheckDirty, bPromptToSave);

			return NewAsset;
		}

		return nullptr;
	}
}


FText UAssetDefinition_ClothAsset::GetAssetDisplayName() const
{
	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_ClothAsset", "ClothAsset");
}

TSoftClassPtr<UObject> UAssetDefinition_ClothAsset::GetAssetClass() const
{
	return UChaosClothAsset::StaticClass();
}

FLinearColor UAssetDefinition_ClothAsset::GetAssetColor() const
{
	return FLinearColor(FColor(180, 120, 110));
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_ClothAsset::GetAssetCategories() const
{
	static const auto Categories = { EAssetCategoryPaths::Physics };
	return Categories;
}

UThumbnailInfo* UAssetDefinition_ClothAsset::LoadThumbnailInfo(const FAssetData& InAsset) const
{
	return UE::Editor::FindOrCreateThumbnailInfo(InAsset.GetAsset(), USceneThumbnailInfo::StaticClass());
}

EAssetCommandResult UAssetDefinition_ClothAsset::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	TArray<UChaosClothAsset*> ClothObjects = OpenArgs.LoadObjects<UChaosClothAsset>();

	// For now the cloth editor only works on one asset at a time
	ensure(ClothObjects.Num() == 0 || ClothObjects.Num() == 1);

	if (ClothObjects.Num() > 0)
	{
		UAssetEditorSubsystem* const AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
		UChaosClothAssetEditor* const AssetEditor = NewObject<UChaosClothAssetEditor>(AssetEditorSubsystem, NAME_None, RF_Transient);

		// Validate the asset
		UChaosClothAsset* const ClothAsset = CastChecked<UChaosClothAsset>(ClothObjects[0]);

		if (!ClothAsset->DataflowAsset)
		{
			if (UObject* const NewDataflowAsset = ClothAssetDefinitionHelpers::CreateNewDataflowAsset(ClothAsset))
			{
				ClothAsset->DataflowAsset = CastChecked<UDataflow>(NewDataflowAsset);
			}
		}

		TArray<TObjectPtr<UObject>> Objects;
		for (UChaosClothAsset* const ClothObject : ClothObjects)
		{
			Objects.Add(ClothObject);
		}
		AssetEditor->Initialize(Objects);

		return EAssetCommandResult::Handled;
	}

	return EAssetCommandResult::Unhandled;
}


#undef LOCTEXT_NAMESPACE
