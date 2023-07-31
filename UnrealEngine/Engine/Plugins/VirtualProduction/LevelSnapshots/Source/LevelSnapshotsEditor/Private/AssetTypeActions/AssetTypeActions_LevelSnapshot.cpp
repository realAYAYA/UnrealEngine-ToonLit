// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_LevelSnapshot.h"

#include "LevelSnapshot.h"
#include "LevelSnapshotsEditorModule.h"
#include "Data/LevelSnapshotsEditorData.h"
#include "LevelSnapshotsEditorFunctionLibrary.h"
#include "LevelSnapshotsEditorStyle.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"

#define LOCTEXT_NAMESPACE "LevelSnapshotsEditor"

FText FAssetTypeActions_LevelSnapshot::GetName() const
{
	return LOCTEXT("AssetTypeActions_LevelSnapshot_Name", "Level Snapshot");
}

UClass* FAssetTypeActions_LevelSnapshot::GetSupportedClass() const
{
	return ULevelSnapshot::StaticClass();
}

void FAssetTypeActions_LevelSnapshot::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder)
{
	FAssetTypeActions_Base::GetActions(InObjects, MenuBuilder);

	const TArray<TWeakObjectPtr<ULevelSnapshot>> LevelSnapshotAssets = GetTypedWeakObjectPtrs<ULevelSnapshot>(InObjects);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("AssetTypeActions_LevelSnapshot_UpdateSnapshotData", "Update Snapshot Data"),
		LOCTEXT("AssetTypeActions_LevelSnapshot_UpdateSnapshotDataToolTip", "Record a snapshot of the current map to this snapshot asset and update the thumbnail. Equivalent to 'Take Snapshot'. Select only one Level Snapshot asset at a time."),
		FSlateIcon(FLevelSnapshotsEditorStyle::GetStyleSetName(), "LevelSnapshots.ToolbarButton.Small"),
		FUIAction(
			FExecuteAction::CreateLambda([=] {
				for (const TWeakObjectPtr<ULevelSnapshot>& LevelSnapshotAsset : LevelSnapshotAssets)
				{
					if (LevelSnapshotAsset.IsValid())
					{
						if (UWorld* World = ULevelSnapshotsEditorData::GetEditorWorld())
						{
							LevelSnapshotAsset->SnapshotWorld(World);
							ULevelSnapshotsEditorFunctionLibrary::GenerateThumbnailForSnapshotAsset(LevelSnapshotAsset.Get());
							LevelSnapshotAsset->MarkPackageDirty();
						}
					}
				}
			}),
			FCanExecuteAction::CreateLambda([=] {
					// We only want to save a snapshot to a single asset at a time, so let's ensure
					// the number of selected assets is exactly one.
					return LevelSnapshotAssets.Num() == 1;
				})
			)
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("AssetTypeActions_LevelSnapshot_CaptureThumbnails", "Capture Thumbnails"),
		LOCTEXT("AssetTypeActions_LevelSnapshot_CaptureThumbnailsToolTip", "Capture and update thumbnails only for the selected snapshot assets."),
		FSlateIcon(FLevelSnapshotsEditorStyle::GetStyleSetName(), "LevelSnapshots.ToolbarButton.Small"),
		FUIAction(
			FExecuteAction::CreateLambda([=] {
				for (const TWeakObjectPtr<ULevelSnapshot>& LevelSnapshotAsset : LevelSnapshotAssets)
				{
					if (LevelSnapshotAsset.IsValid())
					{
						ULevelSnapshotsEditorFunctionLibrary::GenerateThumbnailForSnapshotAsset(LevelSnapshotAsset.Get());
						LevelSnapshotAsset->MarkPackageDirty();
					}
				}
			}),
			FCanExecuteAction::CreateLambda([=] {
					return true;
				})
			)
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("AssetTypeActions_LevelSnapshot_OpenSnapshot", "Open Snapshot in Editor"),
		LOCTEXT("AssetTypeActions_LevelSnapshot_OpenSnapshotToolTip", "Open this snapshot in the Level Snapshots Editor. Select only one Level Snapshot asset at a time."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "SystemWideCommands.SummonOpenAssetDialog"),
		FUIAction(
			FExecuteAction::CreateLambda([=] {
					OpenAssetWithLevelSnapshotsEditor(InObjects);
			}),
			FCanExecuteAction::CreateLambda([=] {
					// We only want to save a snapshot to a single asset at a time, so let's ensure
					// the number of selected assets is exactly one.
					return LevelSnapshotAssets.Num() == 1;
				})
			)
	);
}

void FAssetTypeActions_LevelSnapshot::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	OpenAssetWithLevelSnapshotsEditor(InObjects);
}

void FAssetTypeActions_LevelSnapshot::OpenAssetWithLevelSnapshotsEditor(const TArray<UObject*>& InObjects)
{
	if (InObjects.Num() && InObjects[0])
	{
		FLevelSnapshotsEditorModule& Module = FLevelSnapshotsEditorModule::Get();
					
		Module.OpenLevelSnapshotsDialogWithAssetSelected(FAssetData(InObjects[0]));
	}
}

#undef LOCTEXT_NAMESPACE