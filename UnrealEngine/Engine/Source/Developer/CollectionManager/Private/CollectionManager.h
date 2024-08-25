// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Misc/Guid.h"
#include "CollectionManagerTypes.h"
#include "ICollectionManager.h"
#include "Collection.h"

class ITextFilterExpressionContext;

namespace DirectoryWatcher { class FFileCache; }

/** Collection info for a given object - gives the collection name, as well as the reason this object is considered to be part of this collection */
struct FObjectCollectionInfo
{
	explicit FObjectCollectionInfo(const FCollectionNameType& InCollectionKey)
		: CollectionKey(InCollectionKey)
		, Reason(0)
	{
	}

	FObjectCollectionInfo(const FCollectionNameType& InCollectionKey, const ECollectionRecursionFlags::Flags InReason)
		: CollectionKey(InCollectionKey)
		, Reason(InReason)
	{
	}

	/** The key identifying the collection that contains this object */
	FCollectionNameType CollectionKey;
	/** The reason(s) why this collection contains this object - this can be tested against the recursion mode when getting the collections for an object */
	ECollectionRecursionFlags::Flags Reason;
};

typedef TMap<FCollectionNameType, TSharedRef<FCollection>> FAvailableCollectionsMap;
typedef TMap<FGuid, FCollectionNameType> FGuidToCollectionNamesMap;
typedef TMap<FSoftObjectPath, TArray<FObjectCollectionInfo>> FCollectionObjectsMap;
typedef TMap<FGuid, TArray<FGuid>> FCollectionHierarchyMap;
typedef TArray<FLinearColor> FCollectionColorArray;

/** Wraps up the lazy caching of the collection manager */
class FCollectionManagerCache
{
public:
	FCollectionManagerCache(FAvailableCollectionsMap& InAvailableCollections);

	/** Dirty the parts of the cache that need to change when a collection is added to our collection manager */
	void HandleCollectionAdded();
	
	/** Dirty the parts of the cache that need to change when a collection is removed from our collection manager */
	void HandleCollectionRemoved();

	/** Dirty the parts of the cache that need to change when a collection is modified */
	void HandleCollectionChanged();

	/** Access the CachedCollectionNamesFromGuids map, ensuring that it is up-to-date */
	const FGuidToCollectionNamesMap& GetCachedCollectionNamesFromGuids() const;

	/** Access the CachedObjects map, ensuring that it is up-to-date */
	const FCollectionObjectsMap& GetCachedObjects() const;

	/** Access the CachedHierarchy map, ensuring that it is up-to-date */
	const FCollectionHierarchyMap& GetCachedHierarchy() const;

	/** Access the CachedColors array, ensuring that it is up-to-date */
	const FCollectionColorArray& GetCachedColors() const;

	enum class ERecursiveWorkerFlowControl : uint8
	{
		Stop,
		Continue,
	};

	typedef TFunctionRef<ERecursiveWorkerFlowControl(const FCollectionNameType&, ECollectionRecursionFlags::Flag)> FRecursiveWorkerFunc;

	void RecursionHelper_DoWork(const FCollectionNameType& InCollectionKey, const ECollectionRecursionFlags::Flags InRecursionMode, FRecursiveWorkerFunc InWorkerFunc) const;
	ERecursiveWorkerFlowControl RecursionHelper_DoWorkOnParents(const FCollectionNameType& InCollectionKey, FRecursiveWorkerFunc InWorkerFunc) const;
	ERecursiveWorkerFlowControl RecursionHelper_DoWorkOnChildren(const FCollectionNameType& InCollectionKey, FRecursiveWorkerFunc InWorkerFunc) const;

private:
	/** Reference to the collections that are currently available in our owner collection manager */
	FAvailableCollectionsMap& AvailableCollections;

	/** A map of collection GUIDs to their associated collection names */
	mutable FGuidToCollectionNamesMap CachedCollectionNamesFromGuids_Internal;

	/** A map of object paths to their associated collection info - only objects that are in collections will appear in here */
	mutable FCollectionObjectsMap CachedObjects_Internal;

	/** A map of parent collection GUIDs to their child collection GUIDs - only collections that have children will appear in here */
	mutable FCollectionHierarchyMap CachedHierarchy_Internal;

	/** An array of all unique colors currently used by collections */
	mutable FCollectionColorArray CachedColors_Internal;

	/** Flag to say whether the CachedCollectionNamesFromGuids map is dirty */
	mutable bool bIsCachedCollectionNamesFromGuidsDirty : 1;

	/** Flag to say whether the CachedObjects map is dirty */
	mutable bool bIsCachedObjectsDirty : 1;

	/** Flag to say whether the CachedHierarchy map is dirty */
	mutable bool bIsCachedHierarchyDirty : 1;

	/** Flag to say whether the CachedColors array is dirty */
	mutable bool bIsCachedColorsDirty : 1;
};

class FCollectionManager : public ICollectionManager
{
public:
	FCollectionManager();
	virtual ~FCollectionManager();

	// ICollectionManager implementation
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	virtual bool GetAssetsInCollection(FName CollectionName, ECollectionShareType::Type ShareType, TArray<FName>& AssetPaths, ECollectionRecursionFlags::Flags RecursionMode = ECollectionRecursionFlags::Self) const override
	{
		TArray<FSoftObjectPath> Temp;
		if (GetAssetsInCollection(CollectionName, ShareType, Temp, RecursionMode))
		{
			AssetPaths.Append(UE::SoftObjectPath::Private::ConvertSoftObjectPaths(Temp));
			return true;
		}
		return false;
	}
	virtual bool GetObjectsInCollection(FName CollectionName, ECollectionShareType::Type ShareType, TArray<FName>& ObjectPaths, ECollectionRecursionFlags::Flags RecursionMode = ECollectionRecursionFlags::Self) const override
	{
		TArray<FSoftObjectPath> Temp;
		if (GetObjectsInCollection(CollectionName, ShareType, Temp, RecursionMode))
		{
			ObjectPaths.Append(UE::SoftObjectPath::Private::ConvertSoftObjectPaths(Temp));
			return true;
		}
		return false;
	}
	virtual bool GetClassesInCollection(FName CollectionName, ECollectionShareType::Type ShareType, TArray<FName>& ClassPaths, ECollectionRecursionFlags::Flags RecursionMode = ECollectionRecursionFlags::Self) const override
	{
		TArray<FTopLevelAssetPath> Temp;
		if (GetClassesInCollection(CollectionName, ShareType, Temp, RecursionMode))
		{
			for (FTopLevelAssetPath Path : Temp)
			{
				ClassPaths.Add(Path.ToFName());
			}
		
			return true;
		}
		return false;
	}
	virtual void GetCollectionsContainingObject(FName ObjectPath, ECollectionShareType::Type ShareType, TArray<FName>& OutCollectionNames, ECollectionRecursionFlags::Flags RecursionMode = ECollectionRecursionFlags::Self) const override
	{
		GetCollectionsContainingObject(FSoftObjectPath(ObjectPath), ShareType, OutCollectionNames, RecursionMode);
	}
	virtual void GetCollectionsContainingObject(FName ObjectPath, TArray<FCollectionNameType>& OutCollections, ECollectionRecursionFlags::Flags RecursionMode = ECollectionRecursionFlags::Self) const override
	{
		GetCollectionsContainingObject(FSoftObjectPath(ObjectPath), OutCollections, RecursionMode);
	}
	virtual void GetCollectionsContainingObjects(const TArray<FName>& ObjectPathNames, TMap<FCollectionNameType, TArray<FName>>& OutCollectionsAndMatchedObjects, ECollectionRecursionFlags::Flags RecursionMode = ECollectionRecursionFlags::Self) const override
	{
		TArray<FSoftObjectPath> Paths = UE::SoftObjectPath::Private::ConvertObjectPathNames(ObjectPathNames);
		TMap<FCollectionNameType, TArray<FSoftObjectPath>> TmpMap;
		GetCollectionsContainingObjects(Paths, TmpMap, RecursionMode);
		for (const TPair<FCollectionNameType, TArray<FSoftObjectPath>>& Pair : TmpMap)
		{
			TArray<FName>& Names = OutCollectionsAndMatchedObjects.FindOrAdd(Pair.Key);
			Names.Append(UE::SoftObjectPath::Private::ConvertSoftObjectPaths(Pair.Value));
		}
	}
	virtual FString GetCollectionsStringForObject(FName ObjectPath, ECollectionShareType::Type ShareType, ECollectionRecursionFlags::Flags RecursionMode = ECollectionRecursionFlags::Self, bool bFullPaths = true) const override
	{
		return GetCollectionsStringForObject(FSoftObjectPath(ObjectPath), ShareType, RecursionMode, bFullPaths);
	}
	virtual bool AddToCollection(FName CollectionName, ECollectionShareType::Type ShareType, FName ObjectPath) override
	{
		return AddToCollection(CollectionName, ShareType, FSoftObjectPath(ObjectPath));
	}
	virtual bool AddToCollection(FName CollectionName, ECollectionShareType::Type ShareType, const TArray<FName>& ObjectPaths, int32* OutNumAdded = nullptr) override
	{
		return AddToCollection(CollectionName, ShareType, UE::SoftObjectPath::Private::ConvertObjectPathNames(ObjectPaths), OutNumAdded);
	}
	virtual bool RemoveFromCollection(FName CollectionName, ECollectionShareType::Type ShareType, FName ObjectPath) override
	{
		return RemoveFromCollection(CollectionName, ShareType, FSoftObjectPath(ObjectPath));
	}
	virtual bool RemoveFromCollection(FName CollectionName, ECollectionShareType::Type ShareType, const TArray<FName>& ObjectPaths, int32* OutNumRemoved = nullptr) override
	{
		return RemoveFromCollection(CollectionName, ShareType, UE::SoftObjectPath::Private::ConvertObjectPathNames(ObjectPaths), OutNumRemoved);
	}
	virtual bool IsObjectInCollection(FName ObjectPath, FName CollectionName, ECollectionShareType::Type ShareType, ECollectionRecursionFlags::Flags RecursionMode = ECollectionRecursionFlags::Self) const override
	{
		return IsObjectInCollection(FSoftObjectPath(ObjectPath), CollectionName, ShareType, RecursionMode);
	}
	virtual bool HandleRedirectorDeleted(const FName& ObjectPath) override
	{
		return HandleRedirectorDeleted(FSoftObjectPath(ObjectPath));
	}
	virtual void HandleObjectRenamed(const FName& OldObjectPath, const FName& NewObjectPath) override
	{
		return HandleObjectRenamed(FSoftObjectPath(OldObjectPath), FSoftObjectPath(NewObjectPath));
	}
	virtual void HandleObjectDeleted(const FName& ObjectPath) override
	{
		return HandleObjectDeleted(FSoftObjectPath(ObjectPath));
	}
	DECLARE_DERIVED_EVENT( FCollectionManager, ICollectionManager::FAssetsAddedEvent, FAssetsAddedEvent ); 
	virtual FAssetsAddedEvent& OnAssetsAdded() override { return AssetsAddedEvent; }

	DECLARE_DERIVED_EVENT( FCollectionManager, ICollectionManager::FAssetsRemovedEvent, FAssetsRemovedEvent );
	virtual FAssetsRemovedEvent& OnAssetsRemoved() override { return AssetsRemovedEvent; }
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	virtual bool HasCollections() const override;
	virtual void GetCollections(TArray<FCollectionNameType>& OutCollections) const override;
	virtual void GetCollections(FName CollectionName, TArray<FCollectionNameType>& OutCollections) const override;
	virtual void GetCollectionNames(ECollectionShareType::Type ShareType, TArray<FName>& CollectionNames) const override;
	virtual void GetRootCollections(TArray<FCollectionNameType>& OutCollections) const override;
	virtual void GetRootCollectionNames(ECollectionShareType::Type ShareType, TArray<FName>& CollectionNames) const override;
	virtual void GetChildCollections(FName CollectionName, ECollectionShareType::Type ShareType, TArray<FCollectionNameType>& OutCollections) const override;
	virtual void GetChildCollectionNames(FName CollectionName, ECollectionShareType::Type ShareType, ECollectionShareType::Type ChildShareType, TArray<FName>& CollectionNames) const override;
	virtual TOptional<FCollectionNameType> GetParentCollection(FName CollectionName, ECollectionShareType::Type ShareType) const override;
	virtual bool CollectionExists(FName CollectionName, ECollectionShareType::Type ShareType) const override;
	virtual bool GetAssetsInCollection(FName CollectionName, ECollectionShareType::Type ShareType, TArray<FSoftObjectPath>& AssetPaths, ECollectionRecursionFlags::Flags RecursionMode = ECollectionRecursionFlags::Self) const override;
	virtual bool GetObjectsInCollection(FName CollectionName, ECollectionShareType::Type ShareType, TArray<FSoftObjectPath>& ObjectPaths, ECollectionRecursionFlags::Flags RecursionMode = ECollectionRecursionFlags::Self) const override;
	virtual bool GetClassesInCollection(FName CollectionName, ECollectionShareType::Type ShareType, TArray<FTopLevelAssetPath>& ClassPaths, ECollectionRecursionFlags::Flags RecursionMode = ECollectionRecursionFlags::Self) const override;
	virtual void GetCollectionsContainingObject(const FSoftObjectPath& ObjectPath, ECollectionShareType::Type ShareType, TArray<FName>& OutCollectionNames, ECollectionRecursionFlags::Flags RecursionMode = ECollectionRecursionFlags::Self) const override;
	virtual void GetCollectionsContainingObject(const FSoftObjectPath& ObjectPath, TArray<FCollectionNameType>& OutCollections, ECollectionRecursionFlags::Flags RecursionMode = ECollectionRecursionFlags::Self) const override;
	virtual void GetCollectionsContainingObjects(const TArray<FSoftObjectPath>& ObjectPaths, TMap<FCollectionNameType, TArray<FSoftObjectPath>>& OutCollectionsAndMatchedObjects, ECollectionRecursionFlags::Flags RecursionMode = ECollectionRecursionFlags::Self) const override;
	virtual FString GetCollectionsStringForObject(const FSoftObjectPath& ObjectPath, ECollectionShareType::Type ShareType, ECollectionRecursionFlags::Flags RecursionMode = ECollectionRecursionFlags::Self, bool bFullPaths = true) const override;
	virtual void CreateUniqueCollectionName(const FName& BaseName, ECollectionShareType::Type ShareType, FName& OutCollectionName) const override;
	virtual bool IsValidCollectionName(const FString& CollectionName, ECollectionShareType::Type ShareType) const override;
	virtual bool CreateCollection(FName CollectionName, ECollectionShareType::Type ShareType, ECollectionStorageMode::Type StorageMode) override;
	virtual bool RenameCollection(FName CurrentCollectionName, ECollectionShareType::Type CurrentShareType, FName NewCollectionName, ECollectionShareType::Type NewShareType) override;
	virtual bool ReparentCollection(FName CollectionName, ECollectionShareType::Type ShareType, FName ParentCollectionName, ECollectionShareType::Type ParentShareType) override;
	virtual bool DestroyCollection(FName CollectionName, ECollectionShareType::Type ShareType) override;
	virtual bool AddToCollection(FName CollectionName, ECollectionShareType::Type ShareType, const FSoftObjectPath& ObjectPath) override;
	virtual bool AddToCollection(FName CollectionName, ECollectionShareType::Type ShareType, TConstArrayView<FSoftObjectPath> ObjectPaths, int32* OutNumAdded = nullptr) override;
	virtual bool RemoveFromCollection(FName CollectionName, ECollectionShareType::Type ShareType, const FSoftObjectPath& ObjectPath) override;
	virtual bool RemoveFromCollection(FName CollectionName, ECollectionShareType::Type ShareType, TConstArrayView<FSoftObjectPath> ObjectPaths, int32* OutNumRemoved = nullptr) override;
	virtual bool SetDynamicQueryText(FName CollectionName, ECollectionShareType::Type ShareType, const FString& InQueryText) override;
	virtual bool GetDynamicQueryText(FName CollectionName, ECollectionShareType::Type ShareType, FString& OutQueryText) const override;
	virtual bool TestDynamicQuery(FName CollectionName, ECollectionShareType::Type ShareType, const ITextFilterExpressionContext& InContext, bool& OutResult) const override;
	virtual bool EmptyCollection(FName CollectionName, ECollectionShareType::Type ShareType) override;
	virtual bool SaveCollection(FName CollectionName, ECollectionShareType::Type ShareType) override;
	virtual bool UpdateCollection(FName CollectionName, ECollectionShareType::Type ShareType) override;
	virtual bool GetCollectionStatusInfo(FName CollectionName, ECollectionShareType::Type ShareType, FCollectionStatusInfo& OutStatusInfo) const override;
	virtual bool HasCollectionColors(TArray<FLinearColor>* OutColors = nullptr) const override;
	virtual bool GetCollectionColor(FName CollectionName, ECollectionShareType::Type ShareType, TOptional<FLinearColor>& OutColor) const override;
	virtual bool SetCollectionColor(FName CollectionName, ECollectionShareType::Type ShareType, const TOptional<FLinearColor>& NewColor) override;
	virtual bool GetCollectionStorageMode(FName CollectionName, ECollectionShareType::Type ShareType, ECollectionStorageMode::Type& OutStorageMode) const override;
	virtual bool IsObjectInCollection(const FSoftObjectPath& ObjectPath, FName CollectionName, ECollectionShareType::Type ShareType, ECollectionRecursionFlags::Flags RecursionMode = ECollectionRecursionFlags::Self) const override;
	virtual bool IsValidParentCollection(FName CollectionName, ECollectionShareType::Type ShareType, FName ParentCollectionName, ECollectionShareType::Type ParentShareType) const override;
	virtual FText GetLastError() const override { return LastError; }
	virtual void HandleFixupRedirectors(ICollectionRedirectorFollower& InRedirectorFollower) override;
	virtual bool HandleRedirectorDeleted(const FSoftObjectPath& ObjectPath) override;
	virtual bool HandleRedirectorsDeleted(TConstArrayView<FSoftObjectPath> ObjectPaths) override;
	virtual void HandleObjectRenamed(const FSoftObjectPath& OldObjectPath, const FSoftObjectPath& NewObjectPath) override;
	virtual void HandleObjectDeleted(const FSoftObjectPath& ObjectPath) override;
	virtual void HandleObjectsDeleted(TConstArrayView<FSoftObjectPath> ObjectPaths) override;

	/** Event for when collections are created */
	DECLARE_DERIVED_EVENT( FCollectionManager, ICollectionManager::FCollectionCreatedEvent, FCollectionCreatedEvent );
	virtual FCollectionCreatedEvent& OnCollectionCreated() override { return CollectionCreatedEvent; }

	/** Event for when collections are destroyed */
	DECLARE_DERIVED_EVENT( FCollectionManager, ICollectionManager::FCollectionDestroyedEvent, FCollectionDestroyedEvent );
	virtual FCollectionDestroyedEvent& OnCollectionDestroyed() override { return CollectionDestroyedEvent; }

	/** Event for when assets are added to a collection */
	virtual FOnAssetsAddedToCollection& OnAssetsAddedToCollection() override { return AssetsAddedToCollectionDelegate; }

	/** Event for when assets are removed from a collection */
	virtual FOnAssetsRemovedFromCollection& OnAssetsRemovedFromCollection() override { return AssetsRemovedFromCollectionDelegate; }

	/** Event for when collections are renamed */
	DECLARE_DERIVED_EVENT( FCollectionManager, ICollectionManager::FCollectionRenamedEvent, FCollectionRenamedEvent );
	virtual FCollectionRenamedEvent& OnCollectionRenamed() override { return CollectionRenamedEvent; }

	/** Event for when collections are re-parented */
	DECLARE_DERIVED_EVENT( FCollectionManager, ICollectionManager::FCollectionReparentedEvent, FCollectionReparentedEvent );
	virtual FCollectionReparentedEvent& OnCollectionReparented() override { return CollectionReparentedEvent; }

	/** Event for when collections is updated, or otherwise changed and we can't tell exactly how (eg, after updating from source control and merging) */
	DECLARE_DERIVED_EVENT( FCollectionManager, ICollectionManager::FCollectionUpdatedEvent, FCollectionUpdatedEvent );
	virtual FCollectionUpdatedEvent& OnCollectionUpdated() override { return CollectionUpdatedEvent; }

	/** Event for when collections is updated, or otherwise changed and we can't tell exactly how (eg, after updating from source control and merging) */
	DECLARE_DERIVED_EVENT( FCollectionManager, ICollectionManager::FAddToCollectionCheckinDescriptionEvent, FAddToCollectionCheckinDescriptionEvent);
	virtual FAddToCollectionCheckinDescriptionEvent& OnAddToCollectionCheckinDescriptionEvent() override { return AddToCollectionCheckinDescriptionEvent; }

private:
	/** Tick this collection manager so it can process any file cache events */
	bool TickFileCache(float InDeltaTime);

	/** Loads all collection files from disk */
	void LoadCollections();

	/** Returns true if the specified share type requires source control */
	bool ShouldUseSCC(ECollectionShareType::Type ShareType) const;

	/** Given a collection name and share type, work out the full filename for the collection to use on disk */
	FString GetCollectionFilename(const FName& InCollectionName, const ECollectionShareType::Type InCollectionShareType) const;

	/** Adds a collection to the lookup maps */
	bool AddCollection(const TSharedRef<FCollection>& CollectionRef, ECollectionShareType::Type ShareType);

	/** Removes a collection from the lookup maps */
	bool RemoveCollection(const TSharedRef<FCollection>& CollectionRef, ECollectionShareType::Type ShareType);

	/** Removes an object from any collections that contain it */
	void RemoveObjectFromCollections(const FSoftObjectPath& ObjectPath, TArray<FCollectionNameType>& OutUpdatedCollections);

	/** Replaces an object with another in any collections that contain it */
	void ReplaceObjectInCollections(const FSoftObjectPath& OldObjectPath, const FSoftObjectPath& NewObjectPath, TArray<FCollectionNameType>& OutUpdatedCollections);

	/** Internal common functionality for saving a collection
	 * bForceCommitToRevisionControl - If the collection's storage mode will save it to source control, then bForceCommitToRevisionControl will ensure that it is committed
	 * after save.  If this is false, then the collection will be left as a modified file which can be advantageous for slow source control servers.
	 */
	bool InternalSaveCollection(const TSharedRef<FCollection>& CollectionRef, FText& OutError, bool bForceCommitToRevisionControl);

private:
	/** The folders that contain collections */
	FString CollectionFolders[ECollectionShareType::CST_All];

	/** The extension used for collection files */
	FString CollectionExtension;

	/** Array of file cache instances that are watching for the collection files changing on disk */
	TSharedPtr<DirectoryWatcher::FFileCache> CollectionFileCaches[ECollectionShareType::CST_All];

	/** Delegate handle for the TickFileCache function */
	FTSTicker::FDelegateHandle TickFileCacheDelegateHandle;

	/** A map of collection names to FCollection objects */
	TMap<FCollectionNameType, TSharedRef<FCollection>> AvailableCollections;

	/** The lazily updated cache for this collection manager */
	FCollectionManagerCache CollectionCache;

	/** The most recent error that occurred */
	mutable FText LastError;

	/** Event for when assets are added to a collection */
	FOnAssetsAddedToCollection AssetsAddedToCollectionDelegate;

	UE_DEPRECATED(5.1, "This event has been replaced by AssetsAddedToCollectionEvent")
	FAssetsAddedEvent AssetsAddedEvent;

	/** Event for when assets are removed from a collection */
	FOnAssetsRemovedFromCollection AssetsRemovedFromCollectionDelegate;

	UE_DEPRECATED(5.1, "This event has been replaced by AssetsRemovedFromCollectionEvent")
	FAssetsRemovedEvent AssetsRemovedEvent;

	/** Event for when collections are renamed */
	FCollectionRenamedEvent CollectionRenamedEvent;

	/** Event for when collections are re-parented */
	FCollectionReparentedEvent CollectionReparentedEvent;

	/** Event for when collections are updated, or otherwise changed and we can't tell exactly how (eg, after updating from source control and merging) */
	FCollectionUpdatedEvent CollectionUpdatedEvent;

	/** Event for when collections are created */
	FCollectionCreatedEvent CollectionCreatedEvent;

	/** Event for when collections are destroyed */
	FCollectionDestroyedEvent CollectionDestroyedEvent;

	/** When a collection checkin happens, use this event to add additional text to the changelist description */
	FAddToCollectionCheckinDescriptionEvent AddToCollectionCheckinDescriptionEvent;

	/** When true, redirectors will not be automatically followed in collections during startup */
	bool bNoFixupRedirectors;
};
