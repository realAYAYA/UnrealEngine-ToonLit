// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_PhysicalMaterialMask.h"
#include "PhysicalMaterials/PhysicalMaterialMask.h"
#include "PhysicalMaterialMaskImport.h"
#include "EditorFramework/AssetImportData.h"
#include "Editor.h"
#include "Modules/ModuleManager.h"
#include "Engine/Texture.h"
#include "PhysicalMaterialMaskImport.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/FileManager.h"
#include "ToolMenus.h"
#include "Misc/MessageDialog.h"
#include "Misc/FeedbackContext.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Framework/Application/SlateApplication.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

DEFINE_LOG_CATEGORY_STATIC(LogAssetTypeActionsPhysicalMaterialMask, Log, All);

UClass* FAssetTypeActions_PhysicalMaterialMask::GetSupportedClass() const
{
	return UPhysicalMaterialMask::StaticClass();
}

void FAssetTypeActions_PhysicalMaterialMask::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
	TArray<TWeakObjectPtr<UPhysicalMaterialMask>> SelectedMasks = GetTypedWeakObjectPtrs<UPhysicalMaterialMask>(InObjects);

	TArray<FString> ResolvedFilePaths;
	GetResolvedSourceFilePaths(InObjects, ResolvedFilePaths);

	if (SelectedMasks.Num() == 1 && ResolvedFilePaths.Num() == 1 && ResolvedFilePaths[0].IsEmpty())
	{
		Section.AddMenuEntry(
			"PhysicalMaterialMask_Import",
			LOCTEXT("PhysicalMaterialMask_ImportMaskTexture", "Import Mask Texture"),
			LOCTEXT("PhysicalMaterialMask_ImportMaskTextureTooltip", "Imports a texture to use as a physical material mask."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.Material"),
			FUIAction(
				FExecuteAction::CreateSP(this, &FAssetTypeActions_PhysicalMaterialMask::ExecuteImport, SelectedMasks[0]),
				FCanExecuteAction()
			)
		);
	}
}

void FAssetTypeActions_PhysicalMaterialMask::GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const
{
	for (auto& Asset : TypeAssets)
	{
		TArray<FString> ExtractedFilenames;
		UPhysicalMaterialMask* PhysMatMask = CastChecked<UPhysicalMaterialMask>(Asset);
		if (UAssetImportData* AssetImportData = PhysMatMask->AssetImportData)
		{
			if (AssetImportData->GetSourceFileCount() > 0)
			{
				if (!AssetImportData->GetSourceData().SourceFiles[0].RelativeFilename.IsEmpty())
				{
					PhysMatMask->AssetImportData->ExtractFilenames(ExtractedFilenames);
				}
			}
		}

		if (ExtractedFilenames.Num() > 0)
		{
			OutSourceFilePaths.Emplace(ExtractedFilenames[0]);
		}
		else
		{
			OutSourceFilePaths.Emplace(FString());
		}
	}
}

bool FAssetTypeActions_PhysicalMaterialMask::CanExecuteImportedAssetActions(const TArray<FString> ResolvedFilePaths) const
{
	// Verify that all the file paths are legitimate
	for (const auto& SourceFilePath : ResolvedFilePaths)
	{
		if (!SourceFilePath.Len() || IFileManager::Get().FileSize(*SourceFilePath) == INDEX_NONE)
		{
			return false;
		}
	}

	return true;
}

void FAssetTypeActions_PhysicalMaterialMask::ExecuteImport(TWeakObjectPtr<UPhysicalMaterialMask> SelectedMask)
{
	if (UPhysicalMaterialMask* PhysMatMask = SelectedMask.Get())
	{
		FPhysicalMaterialMaskImport::ImportMaskTexture(PhysMatMask);
	}
}

#undef LOCTEXT_NAMESPACE
