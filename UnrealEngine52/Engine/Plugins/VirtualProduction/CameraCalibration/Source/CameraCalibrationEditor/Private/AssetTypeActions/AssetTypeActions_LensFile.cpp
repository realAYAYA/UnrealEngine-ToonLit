// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_LensFile.h"

#include "AssetEditor/CameraCalibrationToolkit.h"
#include "EditorFramework/AssetImportData.h"
#include "LensFile.h"

#define LOCTEXT_NAMESPACE "LensFileTypeActions"

FText FAssetTypeActions_LensFile::GetName() const
{
	return LOCTEXT("AssetTypeActions_LensFile", "Lens File");
}

UClass* FAssetTypeActions_LensFile::GetSupportedClass() const
{
	return ULensFile::StaticClass();
}

void FAssetTypeActions_LensFile::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	const EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (UObject* Object : InObjects)
	{
		if (ULensFile* Asset = Cast<ULensFile>(Object))
		{
			FCameraCalibrationToolkit::CreateEditor(Mode, EditWithinLevelEditor, Asset);
		}
	}
}

void FAssetTypeActions_LensFile::GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const
{
	for (UObject* Asset : TypeAssets)
	{
		if (const ULensFile* LensFile = CastChecked<ULensFile>(Asset))
		{
			if (LensFile->AssetImportData)
			{
				LensFile->AssetImportData->ExtractFilenames(OutSourceFilePaths);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
