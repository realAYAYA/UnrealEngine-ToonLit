// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_LensFile.h"

#include "AssetEditor/CameraCalibrationToolkit.h"
#include "LensFile.h"

#define LOCTEXT_NAMESPACE "AssetDefinition_LensFile"

TSoftClassPtr<UObject> UAssetDefinition_LensFile::GetAssetClass() const
{
	return ULensFile::StaticClass();
}

FText UAssetDefinition_LensFile::GetAssetDisplayName() const
{
	return LOCTEXT("AssetDisplayName", "Lens File");
}

FLinearColor UAssetDefinition_LensFile::GetAssetColor() const
{
	return FLinearColor(0.1f, 1.0f, 0.1f);
}

EAssetCommandResult UAssetDefinition_LensFile::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	const TArray<ULensFile*> ObjectsToOpen = OpenArgs.LoadObjects<ULensFile>();
	for (ULensFile* Object : ObjectsToOpen)
	{
		FCameraCalibrationToolkit::CreateEditor(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, Object);
	}

	return EAssetCommandResult::Handled;
}

#undef LOCTEXT_NAMESPACE
