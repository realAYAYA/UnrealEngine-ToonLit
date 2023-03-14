// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ContentBrowserItemData.h"
#include "AssetRegistry/AssetData.h"

class FAssetThumbnail;

class CONTENTBROWSERCLASSDATASOURCE_API FContentBrowserClassFolderItemDataPayload : public IContentBrowserItemDataPayload
{
public:
	explicit FContentBrowserClassFolderItemDataPayload(const FName InInternalPath)
		: InternalPath(InInternalPath)
	{
	}

	FName GetInternalPath() const
	{
		return InternalPath;
	}

	const FString& GetFilename() const;

private:
	FName InternalPath;

	mutable bool bHasCachedFilename = false;
	mutable FString CachedFilename;
};

class CONTENTBROWSERCLASSDATASOURCE_API FContentBrowserClassFileItemDataPayload : public IContentBrowserItemDataPayload
{
public:
	FContentBrowserClassFileItemDataPayload(const FName InInternalPath, UClass* InClass);

	FName GetInternalPath() const
	{
		return InternalPath;
	}

	UClass* GetClass() const
	{
		return Class.Get();
	}

	const FAssetData& GetAssetData() const
	{
		return AssetData;
	}

	const FString& GetFilename() const;

	void UpdateThumbnail(FAssetThumbnail& InThumbnail) const;

private:
	FName InternalPath;

	TWeakObjectPtr<UClass> Class;

	FAssetData AssetData;

	mutable bool bHasCachedFilename = false;
	mutable FString CachedFilename;
};
