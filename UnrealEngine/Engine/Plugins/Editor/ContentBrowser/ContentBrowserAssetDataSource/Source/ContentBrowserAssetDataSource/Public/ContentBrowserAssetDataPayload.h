// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ContentBrowserItemData.h"
#include "AssetRegistry/AssetData.h"
#include "UObject/GCObject.h"

class UAssetDefinition;
class IAssetTypeActions;
class FAssetThumbnail;
class UFactory;

class CONTENTBROWSERASSETDATASOURCE_API FContentBrowserAssetFolderItemDataPayload : public IContentBrowserItemDataPayload
{
public:
	explicit FContentBrowserAssetFolderItemDataPayload(const FName InInternalPath)
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

class CONTENTBROWSERASSETDATASOURCE_API FContentBrowserAssetFileItemDataPayload : public IContentBrowserItemDataPayload
{
public:
	explicit FContentBrowserAssetFileItemDataPayload(FAssetData&& InAssetData);
	explicit FContentBrowserAssetFileItemDataPayload(const FAssetData& InAssetData);

	const FAssetData& GetAssetData() const
	{
		return AssetData;
	}

	UPackage* GetPackage(const bool bTryRecacheIfNull = false) const;

	// LoadTags(optional) allows passing specific tags to the linker when loading the asset package (@see ULevel::LoadAllExternalObjectsTag for an example usage)
	UPackage* LoadPackage(TSet<FName> LoadTags = {}) const;

	UObject* GetAsset(const bool bTryRecacheIfNull = false) const;

	//  LoadTags (optional) allows passing specific tags to the linker when loading the asset (@see ULevel::LoadAllExternalObjectsTag for an example usage)
	UObject* LoadAsset(TSet<FName> LoadTags = {}) const;

	TSharedPtr<IAssetTypeActions> GetAssetTypeActions() const;
	
	const UAssetDefinition* GetAssetDefinition() const;

	const FString& GetFilename() const;

	void UpdateThumbnail(FAssetThumbnail& InThumbnail) const;

private:
	FAssetData AssetData;

	mutable bool bHasCachedPackagePtr = false;
	mutable TWeakObjectPtr<UPackage> CachedPackagePtr;

	mutable bool bHasCachedAssetPtr = false;
	mutable TWeakObjectPtr<UObject> CachedAssetPtr;

	mutable bool bHasCachedAssetTypeActionsPtr = false;
	mutable TWeakPtr<IAssetTypeActions> CachedAssetTypeActionsPtr;

	mutable bool bHasCachedAssetDefinitionPtr = false;
	mutable TWeakObjectPtr<const UAssetDefinition> CachedAssetDefinitionPtr;

	mutable bool bHasCachedFilename = false;
	mutable FString CachedFilename;

	void FlushCaches() const;
};

class CONTENTBROWSERASSETDATASOURCE_API FContentBrowserAssetFileItemDataPayload_Creation : public FContentBrowserAssetFileItemDataPayload, public FGCObject
{
public:
	FContentBrowserAssetFileItemDataPayload_Creation(FAssetData&& InAssetData, UClass* InAssetClass, UFactory* InFactory);

	//~ FGCObject interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		Collector.AddReferencedObject(AssetClass);
		Collector.AddReferencedObject(Factory);
	}
	virtual FString GetReferencerName() const override
	{
		return TEXT("FContentBrowserAssetFileItemDataPayload_Creation");
	}

	UClass* GetAssetClass() const
	{
		return AssetClass;
	}

	UFactory* GetFactory() const
	{
		return Factory;
	}

private:
	/** The class to use when creating the asset */
	TObjectPtr<UClass> AssetClass = nullptr;

	/** The factory to use when creating the asset. */
	TObjectPtr<UFactory> Factory = nullptr;
};

class CONTENTBROWSERASSETDATASOURCE_API FContentBrowserAssetFileItemDataPayload_Duplication : public FContentBrowserAssetFileItemDataPayload
{
public:
	FContentBrowserAssetFileItemDataPayload_Duplication(FAssetData&& InAssetData, TWeakObjectPtr<UObject> InSourceObject);

	UObject* GetSourceObject() const
	{
		return SourceObject.Get();
	}

private:
	/** The context to use when creating the asset. Used when initializing an asset with another related asset. */
	TWeakObjectPtr<UObject> SourceObject;
};

class CONTENTBROWSERASSETDATASOURCE_API FContentBrowserUnsupportedAssetFileItemDataPayload : public IContentBrowserItemDataPayload
{
public:
	// Unsupported asset file but it does have an asset data
	explicit FContentBrowserUnsupportedAssetFileItemDataPayload(FAssetData&& InAssetData);
	explicit FContentBrowserUnsupportedAssetFileItemDataPayload(const FAssetData& InAssetData);

	const FAssetData* GetAssetDataIfAvailable() const;

	const FString& GetFilename() const;

	UPackage* GetPackage() const;

private:
	void FlushCaches() const;


	TUniquePtr<FAssetData> OptionalAssetData;

	mutable bool bHasCachedPackagePtr = false;
	mutable TWeakObjectPtr<UPackage> CachedPackagePtr;

	mutable bool bHasCachedFilename = false;
	mutable FString CachedFilename;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
