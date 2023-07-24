// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_PoseCorrectives.h"
#include "PoseCorrectivesAsset.h"
#include "PoseCorrectivesEditorToolkit.h"

/* FAssetTypeActions_Base overrides
 *****************************************************************************/

uint32 FAssetTypeActions_PoseCorrectives::GetCategories()
{
	return EAssetTypeCategories::Animation;
}


FText FAssetTypeActions_PoseCorrectives::GetName() const
{
	return NSLOCTEXT("PoseCorrectives", "AssetTypeActions_PoseCorrectives", "Pose Correctives Asset");
}


UClass* FAssetTypeActions_PoseCorrectives::GetSupportedClass() const
{
	return UPoseCorrectivesAsset::StaticClass();
}


FColor FAssetTypeActions_PoseCorrectives::GetTypeColor() const
{
	return FColor::White;
}


void FAssetTypeActions_PoseCorrectives::OpenAssetEditor(
	const TArray<UObject*>& InObjects,
	TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	const EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;
    
	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		if (UPoseCorrectivesAsset* Asset = Cast<UPoseCorrectivesAsset>(*ObjIt))
		{
			TSharedRef<FPoseCorrectivesEditorToolkit> NewEditor(new FPoseCorrectivesEditorToolkit());
			NewEditor->InitAssetEditor(Mode, EditWithinLevelEditor, Asset);
		}
	}
}