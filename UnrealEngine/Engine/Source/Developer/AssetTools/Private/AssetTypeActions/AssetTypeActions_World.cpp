// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_World.h"
#include "Misc/PackageName.h"
#include "Misc/MessageDialog.h"
#include "ThumbnailRendering/WorldThumbnailInfo.h"
#include "FileHelpers.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

namespace AssetTypeActions_World
{
	bool CanRenameOrDuplicate(const FAssetData& InAsset, FText* OutErrorMsg)
	{
		if (ULevel::GetIsLevelPartitionedFromAsset(InAsset))
		{
			for (const FWorldContext& WorldContext : GEditor->GetWorldContexts())
			{
				if (const UWorld* World = WorldContext.World(); WorldContext.World() && InAsset.PackageName == World->GetPackage()->GetFName())
				{
					if (OutErrorMsg)
					{
						*OutErrorMsg = LOCTEXT("Error_CannotDuplicatRenameWorldPartitionWhileInUse", "Cannot duplicate / rename a partition world while it is used.");
					}
					return false;
				}
			}
		}

		return true;
	}
}

void FAssetTypeActions_World::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor )
{
	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		UWorld* World = Cast<UWorld>(*ObjIt);
		if (World != nullptr && 
			ensureMsgf(World->GetPackage(), TEXT("World(%s) is not in a package and cannot be opened"), *World->GetFullName()) && 
			ensureMsgf(!World->GetPackage()->HasAnyPackageFlags(PKG_NewlyCreated), TEXT("World(%s) is unsaved and cannot be opened")))
		{
			const FString FileToOpen = FPackageName::LongPackageNameToFilename(World->GetOutermost()->GetName(), FPackageName::GetMapPackageExtension());
			const bool bLoadAsTemplate = false;
			const bool bShowProgress = true;
			FEditorFileUtils::LoadMap(FileToOpen, bLoadAsTemplate, bShowProgress);

			// We can only edit one world at a time... so just break after the first valid world to load
			break;
		}
	}
}

UThumbnailInfo* FAssetTypeActions_World::GetThumbnailInfo(UObject* Asset) const
{
	UWorld* World = CastChecked<UWorld>(Asset);
	UThumbnailInfo* ThumbnailInfo = World->ThumbnailInfo;
	if (ThumbnailInfo == NULL)
	{
		ThumbnailInfo = NewObject<UWorldThumbnailInfo>(World, NAME_None, RF_Transactional);
		World->ThumbnailInfo = ThumbnailInfo;
	}

	return ThumbnailInfo;
}

TArray<FAssetData> FAssetTypeActions_World::GetValidAssetsForPreviewOrEdit(TArrayView<const FAssetData> InAssetDatas, bool bIsPreview)
{
	TArray<FAssetData> AssetsToOpen;
	if (InAssetDatas.Num())
	{
		const FAssetData& AssetData = InAssetDatas[0];

		// If there are any unsaved changes to the current level, see if the user wants to save those first
		// If they do not wish to save, then we will bail out of opening this asset.
		constexpr bool bPromptUserToSave = true;
		constexpr bool bSaveMapPackages = true;
		constexpr bool bSaveContentPackages = true;
		if (FEditorFileUtils::SaveDirtyPackages(bPromptUserToSave, bSaveMapPackages, bSaveContentPackages))
		{
			// Validate that Asset was saved or isn't loaded meaning it can be loaded
			const bool bLoad = false;
			if (UWorld* World = Cast<UWorld>(AssetData.FastGetAsset(bLoad)); !World || !World->GetPackage()->HasAnyPackageFlags(PKG_NewlyCreated))
			{
				AssetsToOpen.Add(AssetData);
			}
			else
			{
				FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("CannotOpenNewlyCreatedMapWithoutSaving", "The level you are trying to open needs to be saved first."));
			}
		}
	}

	return MoveTemp(AssetsToOpen);
}

bool FAssetTypeActions_World::CanRename(const FAssetData& InAsset, FText* OutErrorMsg) const
{
	return AssetTypeActions_World::CanRenameOrDuplicate(InAsset, OutErrorMsg);
}

bool FAssetTypeActions_World::CanDuplicate(const FAssetData& InAsset, FText* OutErrorMsg) const
{
	return AssetTypeActions_World::CanRenameOrDuplicate(InAsset, OutErrorMsg);
}

#undef LOCTEXT_NAMESPACE
