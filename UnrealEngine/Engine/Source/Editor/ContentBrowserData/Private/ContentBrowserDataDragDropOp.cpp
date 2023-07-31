// Copyright Epic Games, Inc. All Rights Reserved.

#include "ContentBrowserDataDragDropOp.h"

#include "AssetRegistry/AssetData.h"
#include "AssetThumbnail.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "ContentBrowserDataFilter.h"
#include "ContentBrowserDataSubsystem.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "HAL/PlatformCrt.h"
#include "Templates/UnrealTemplate.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "UObject/NameTypes.h"

class UActorFactory;

TSharedRef<FContentBrowserDataDragDropOp> FContentBrowserDataDragDropOp::New(TArrayView<const FContentBrowserItem> InDraggedItems)
{
	TSharedRef<FContentBrowserDataDragDropOp> Operation = MakeShared<FContentBrowserDataDragDropOp>();

	Operation->Init(MoveTemp(InDraggedItems));

	Operation->Construct();
	return Operation;
}

TSharedRef<FContentBrowserDataDragDropOp> FContentBrowserDataDragDropOp::Legacy_New(TArrayView<const FAssetData> InAssetData, TArrayView<const FString> InAssetPaths, UActorFactory* InActorFactory)
{
	TSharedRef<FContentBrowserDataDragDropOp> Operation = MakeShared<FContentBrowserDataDragDropOp>();

	Operation->LegacyInit(InAssetData, InAssetPaths, InActorFactory);

	Operation->Construct();
	return Operation;
}

void FContentBrowserDataDragDropOp::Init(TArrayView<const FContentBrowserItem> InDraggedItems)
{
	DraggedItems.Append(InDraggedItems.GetData(), InDraggedItems.Num());

	TArray<FAssetData> DraggedAssets;
	TArray<FString> DraggedPackagePaths;

	for (const FContentBrowserItem& DraggedItem : DraggedItems)
	{
		if (DraggedItem.IsFile())
		{
			DraggedFiles.Add(DraggedItem);

			FAssetData ItemAssetData;
			if (DraggedItem.Legacy_TryGetAssetData(ItemAssetData) && !ItemAssetData.IsRedirector())
			{
				DraggedAssets.Add(MoveTemp(ItemAssetData));
			}
		}

		if (DraggedItem.IsFolder())
		{
			DraggedFolders.Add(DraggedItem);

			FName ItemPackagePath;
			if (DraggedItem.Legacy_TryGetPackagePath(ItemPackagePath))
			{
				DraggedPackagePaths.Add(ItemPackagePath.ToString());
			}
		}
	}

	FAssetDragDropOp::Init(MoveTemp(DraggedAssets), MoveTemp(DraggedPackagePaths), nullptr);
}

void FContentBrowserDataDragDropOp::LegacyInit(TArrayView<const FAssetData> InAssetData, TArrayView<const FString> InAssetPaths, UActorFactory* InActorFactory)
{
	UContentBrowserDataSubsystem* ContentBrowserData = GEditor->GetEditorSubsystem<UContentBrowserDataSubsystem>();

	for (const FAssetData& Asset : InAssetData)
	{
		TArray<FName, TInlineAllocator<2>> VirtualAssetPaths;
		ContentBrowserData->Legacy_TryConvertAssetDataToVirtualPaths(Asset, /*bUseFolderPaths*/false, [&VirtualAssetPaths](FName InPath)
		{
			VirtualAssetPaths.Add(InPath);
			return true;
		});

		for (const FName& VirtualAssetPath : VirtualAssetPaths)
		{
			FContentBrowserItem AssetItem = ContentBrowserData->GetItemAtPath(VirtualAssetPath, EContentBrowserItemTypeFilter::IncludeFiles);
			if (AssetItem.IsValid())
			{
				DraggedItems.Add(MoveTemp(AssetItem));
			}
		}
	}

	for (const FString& Path : InAssetPaths)
	{
		TArray<FName, TInlineAllocator<2>> VirtualFolderPaths;
		ContentBrowserData->Legacy_TryConvertPackagePathToVirtualPaths(*Path, [&VirtualFolderPaths](FName InPath)
		{
			VirtualFolderPaths.Add(InPath);
			return true;
		});

		for (const FName& VirtualFolderPath : VirtualFolderPaths)
		{
			FContentBrowserItem FolderItem = ContentBrowserData->GetItemAtPath(VirtualFolderPath, EContentBrowserItemTypeFilter::IncludeFolders);
			if (FolderItem.IsValid())
			{
				DraggedItems.Add(MoveTemp(FolderItem));
			}
		}
	}

	FAssetDragDropOp::Init(TArray<FAssetData>(InAssetData), TArray<FString>(InAssetPaths), InActorFactory);
}

void FContentBrowserDataDragDropOp::InitThumbnail()
{
	if (DraggedFiles.Num() > 0 && ThumbnailSize > 0)
	{
		// Create the thumbnail handle
		AssetThumbnail = MakeShared<FAssetThumbnail>(FAssetData(), ThumbnailSize, ThumbnailSize, UThumbnailManager::Get().GetSharedThumbnailPool());
		if (DraggedFiles[0].UpdateThumbnail(*AssetThumbnail))
		{
			// Request the texture then tick the pool once to render the thumbnail
			AssetThumbnail->GetViewportRenderTargetTexture();
		}
		else
		{
			AssetThumbnail.Reset();
		}
	}
}

bool FContentBrowserDataDragDropOp::HasFiles() const
{
	return DraggedFiles.Num() > 0;
}

bool FContentBrowserDataDragDropOp::HasFolders() const
{
	return DraggedFolders.Num() > 0;
}

int32 FContentBrowserDataDragDropOp::GetTotalCount() const
{
	return DraggedItems.Num();
}

FText FContentBrowserDataDragDropOp::GetFirstItemText() const
{
	if (DraggedFiles.Num() > 0)
	{
		return DraggedFiles[0].GetDisplayName();
	}

	if (DraggedFolders.Num() > 0)
	{
		return FText::FromName(DraggedFolders[0].GetVirtualPath());
	}

	return FText::GetEmpty();
}
