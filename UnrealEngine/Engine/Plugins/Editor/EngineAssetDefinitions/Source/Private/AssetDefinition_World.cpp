// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_World.h"

#include "FileHelpers.h"
#include "Misc/PackageName.h"
#include "ThumbnailRendering/WorldThumbnailInfo.h"
#include "Misc/MessageDialog.h"
#include "Engine/Level.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "UAssetDefinition_World"

TConstArrayView<FAssetCategoryPath> UAssetDefinition_World::GetAssetCategories() const
{
	static const auto Categories = { EAssetCategoryPaths::Basic };
	return Categories;
}

TArray<FAssetData> UAssetDefinition_World::PrepareToActivateAssets(const FAssetActivateArgs& ActivateArgs) const
{
	TArray<FAssetData> AssetsToOpen;
	if (ActivateArgs.Assets.Num())
	{
		const FAssetData& AssetData = ActivateArgs.Assets[0];

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

EAssetCommandResult UAssetDefinition_World::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	for (const FAssetData& WorldAsset : OpenArgs.Assets)
	{
		UWorld* World = Cast<UWorld>(WorldAsset.GetAsset());
		if (World != nullptr && 
			ensureMsgf(World->GetPackage(), TEXT("World(%s) is not in a package and cannot be opened"), *World->GetFullName()) && 
			ensureMsgf(!World->GetPackage()->HasAnyPackageFlags(PKG_NewlyCreated), TEXT("World(%s) is unsaved and cannot be opened"), *World->GetFullName()))
		{
			const FString FileToOpen = FPackageName::LongPackageNameToFilename(World->GetOutermost()->GetName(), FPackageName::GetMapPackageExtension());
			const bool bLoadAsTemplate = false;
			const bool bShowProgress = true;
			FEditorFileUtils::LoadMap(FileToOpen, bLoadAsTemplate, bShowProgress);

			// We can only edit one world at a time... so just break after the first valid world to load
			return EAssetCommandResult::Handled;
		}
	}

	return EAssetCommandResult::Unhandled;
}

UThumbnailInfo* UAssetDefinition_World::LoadThumbnailInfo(const FAssetData& InAsset) const
{
	return UE::Editor::FindOrCreateThumbnailInfo(InAsset.GetAsset(), UWorldThumbnailInfo::StaticClass());
}

FAssetSupportResponse UAssetDefinition_World::CanRename(const FAssetData& InAsset) const
{
	if (IsPartitionWorldInUse(InAsset))
	{
		return FAssetSupportResponse::Error(LOCTEXT("CanNotRenameWorldInUse", "Cannot rename a partition world while it is used."));
	}
	
	return FAssetSupportResponse::Supported();
}

FAssetSupportResponse UAssetDefinition_World::CanDuplicate(const FAssetData& InAsset) const
{
	if (IsPartitionWorldInUse(InAsset))
	{
		return FAssetSupportResponse::Error(LOCTEXT("CanNotDuplicateWorldInUse", "Cannot duplicate a partition world while it is used."));
	}
	
	return FAssetSupportResponse::Supported();
}

bool UAssetDefinition_World::IsPartitionWorldInUse(const FAssetData& InAsset) const
{
	if (ULevel::GetIsLevelPartitionedFromAsset(InAsset))
	{
		for (const FWorldContext& WorldContext : GEditor->GetWorldContexts())
		{
			if (const UWorld* World = WorldContext.World(); WorldContext.World() && InAsset.PackageName == World->GetPackage()->GetFName())
			{
				return true;
			}
		}
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
