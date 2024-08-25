// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Rundown/AvaRundownManagedInstance.h"

class IAvaMediaSyncProvider;
class UPackage;

/**
 * Cache for the managed Motion Design instances.
 *
 * This is LRU cache. 
 */
class AVALANCHEMEDIA_API FAvaRundownManagedInstanceCache
{
public:
	FAvaRundownManagedInstanceCache();
	virtual ~FAvaRundownManagedInstanceCache();

	/**
	 *	Retrieves elements from the cache or load a new one if not present.
	 *	If the cache size is exceeded, the oldest elements are going to be flushed.
	 */
	TSharedPtr<FAvaRundownManagedInstance> GetOrLoadInstance(const FSoftObjectPath& InAssetPath);

	/**
	 * Invalidates a cached entry without deleting it immediately.
	 * It will be deleted and reloaded on the next access query.
	 */
	void InvalidateNoDelete(const FSoftObjectPath& InAssetPath);

	/**
	 * Invalidates a cached entry. The entry may be deleted as a result.
	 */
	void Invalidate(const FSoftObjectPath& InAssetPath);

	/**
	 *	Delegate called when an entry is invalidated. Reason for an entry to be invalidated is if the source
	 *	has been modified.
	 */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnEntryInvalidated, const FSoftObjectPath& /*InAssetPath*/);
	FOnEntryInvalidated OnEntryInvalidated;
	
	/**
	 * Get the maximum size of the cache beyond witch it will start flushing elements.
	 */
	int32 GetMaximumCacheSize() const;
	
	/**
	 *	Flush specified entry from the cache.
	 */
	void Flush(const FSoftObjectPath& InAssetPath);

	/**
	 * Flush all unused entries from the cache.
	 */
	void Flush();

	/**
	 * Trim the cache elements that exceed the cache capacity according to LRU replacement policy.
	 */
	void TrimCache();

	/**
	 *	Perform any pending actions, such has removing invalidated entries. This may lead to
	 *	objects being deleted.
	 */
	void FinishPendingActions();

private:
	void RemovePendingInvalidatedPaths();
	void RemoveEntry(const FSoftObjectPath& InAssetPath);
	void RemoveEntries(TFunctionRef<bool(const FSoftObjectPath&, const TSharedPtr<FAvaRundownManagedInstance>&)> InRemovePredicate, bool bInNotify);

	void OnPackageSaved(const FString& InPackageFileName, UPackage* InPackage, FObjectPostSaveContext InObjectSaveContext);
	void OnAvaSyncPackageModified(IAvaMediaSyncProvider* InAvaMediaSyncProvider, const FName& InPackageName);
	void OnAssetRemoved(const FAssetData& InAssetData);
	void OnPackageModified(const FName& InPackageName);
	void OnSettingChanged(UObject* , struct FPropertyChangedEvent&);

private:
	TMap<FSoftObjectPath, TSharedPtr<FAvaRundownManagedInstance>> Instances;
	TSet<FSoftObjectPath> PendingInvalidatedPaths;
	TArray<FSoftObjectPath> OrderQueue;
};