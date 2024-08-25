// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetRegistryImpl.h"
#include "UObject/Object.h"

#include "AssetRegistry.generated.h"

class FRWScopeLock;
namespace UE::AssetRegistry::Premade { struct FAsyncConsumer; }

/**
 * The AssetRegistry singleton gathers information about .uasset files in the background so things
 * like the content browser don't have to work with the filesystem
 */
PRAGMA_DISABLE_DEPRECATION_WARNINGS

UCLASS(transient)
class UAssetRegistryImpl : public UObject, public IAssetRegistry
{
	GENERATED_BODY()
public:
	UAssetRegistryImpl(const FObjectInitializer& ObjectInitializer);
	UAssetRegistryImpl(FVTableHelper& Helper);
	virtual ~UAssetRegistryImpl();
	virtual void FinishDestroy() override;

	/** Gets the asset registry singleton for asset registry module use */
	static UAssetRegistryImpl& Get();

	// IAssetRegistry implementation
	virtual bool HasAssets(const FName PackagePath, const bool bRecursive = false) const override;
	virtual bool GetAssetsByPackageName(FName PackageName, TArray<FAssetData>& OutAssetData, bool bIncludeOnlyOnDiskAssets = false, bool bSkipARFilteredAssets=true) const override;
	virtual bool GetAssetsByPath(FName PackagePath, TArray<FAssetData>& OutAssetData, bool bRecursive = false, bool bIncludeOnlyOnDiskAssets = false) const override;
	virtual bool GetAssetsByPaths(TArray<FName> PackagePath, TArray<FAssetData>& OutAssetData, bool bRecursive = false, bool bIncludeOnlyOnDiskAssets = false) const override;
	virtual bool GetAssetsByClass(FTopLevelAssetPath ClassPathName, TArray<FAssetData>& OutAssetData, bool bSearchSubClasses = false) const override;
	virtual bool GetAssetsByTags(const TArray<FName>& AssetTags, TArray<FAssetData>& OutAssetData) const override;
	virtual bool GetAssetsByTagValues(const TMultiMap<FName, FString>& AssetTagsAndValues, TArray<FAssetData>& OutAssetData) const override;
	virtual bool GetAssets(const FARFilter& Filter, TArray<FAssetData>& OutAssetData, bool bSkipARFilteredAssets = true) const override;
	virtual bool EnumerateAssets(const FARFilter& Filter, TFunctionRef<bool(const FAssetData&)> Callback, bool bSkipARFilteredAssets=true) const override;
	virtual bool EnumerateAssets(const FARCompiledFilter& Filter, TFunctionRef<bool(const FAssetData&)> Callback, bool bSkipARFilteredAssets = true) const override;
	UE_DEPRECATED(5.1, "Asset path FNames have been deprecated, use FSoftObjectPath instead.")
	virtual FAssetData GetAssetByObjectPath( const FName ObjectPath, bool bIncludeOnlyOnDiskAssets = false ) const override;
	virtual FAssetData GetAssetByObjectPath(const FSoftObjectPath& ObjectPath, bool bIncludeOnlyOnDiskAssets = false, bool bSkipARFilteredAssets = true) const override;
	virtual UE::AssetRegistry::EExists TryGetAssetByObjectPath(const FSoftObjectPath& ObjectPath, FAssetData& OutAssetData) const override;
	virtual UE::AssetRegistry::EExists TryGetAssetPackageData(const FName PackageName, FAssetPackageData& OutAssetPackageData) const override;
	virtual bool GetAllAssets(TArray<FAssetData>& OutAssetData, bool bIncludeOnlyOnDiskAssets = false) const override;
	virtual bool EnumerateAllAssets(TFunctionRef<bool(const FAssetData&)> Callback, bool bIncludeOnlyOnDiskAssets = false) const override;
	virtual void GetPackagesByName(FStringView PackageName, TArray<FName>& OutPackageNames) const override;
	virtual FName GetFirstPackageByName(FStringView PackageName) const override;
	virtual bool GetDependencies(const FAssetIdentifier& AssetIdentifier, TArray<FAssetIdentifier>& OutDependencies, UE::AssetRegistry::EDependencyCategory Category = UE::AssetRegistry::EDependencyCategory::All, const UE::AssetRegistry::FDependencyQuery& Flags = UE::AssetRegistry::FDependencyQuery()) const override;
	virtual bool GetDependencies(const FAssetIdentifier& AssetIdentifier, TArray<FAssetDependency>& OutDependencies, UE::AssetRegistry::EDependencyCategory Category = UE::AssetRegistry::EDependencyCategory::All, const UE::AssetRegistry::FDependencyQuery& Flags = UE::AssetRegistry::FDependencyQuery()) const override;
	virtual bool GetDependencies(FName PackageName, TArray<FName>& OutDependencies, UE::AssetRegistry::EDependencyCategory Category = UE::AssetRegistry::EDependencyCategory::Package, const UE::AssetRegistry::FDependencyQuery& Flags = UE::AssetRegistry::FDependencyQuery()) const override; //-V1101
	virtual bool GetReferencers(const FAssetIdentifier& AssetIdentifier, TArray<FAssetIdentifier>& OutReferencers, UE::AssetRegistry::EDependencyCategory Category = UE::AssetRegistry::EDependencyCategory::All, const UE::AssetRegistry::FDependencyQuery& Flags = UE::AssetRegistry::FDependencyQuery()) const override;
	virtual bool GetReferencers(const FAssetIdentifier& AssetIdentifier, TArray<FAssetDependency>& OutReferencers, UE::AssetRegistry::EDependencyCategory Category = UE::AssetRegistry::EDependencyCategory::All, const UE::AssetRegistry::FDependencyQuery& Flags = UE::AssetRegistry::FDependencyQuery()) const override;
	virtual bool GetReferencers(FName PackageName, TArray<FName>& OutReferencers, UE::AssetRegistry::EDependencyCategory Category = UE::AssetRegistry::EDependencyCategory::Package, const UE::AssetRegistry::FDependencyQuery& Flags = UE::AssetRegistry::FDependencyQuery()) const override; //-V1101
	virtual TOptional<FAssetPackageData> GetAssetPackageDataCopy(FName PackageName) const override;
	virtual void EnumerateAllPackages(TFunctionRef<void(FName PackageName, const FAssetPackageData& PackageData)> Callback) const override;
	virtual bool DoesPackageExistOnDisk(FName PackageName, FString* OutCorrectCasePackageName = nullptr, FString* OutExtension = nullptr) const override;
	virtual FSoftObjectPath GetRedirectedObjectPath(const FSoftObjectPath& ObjectPath) override;
	virtual bool GetAncestorClassNames(FTopLevelAssetPath ClassName, TArray<FTopLevelAssetPath>& OutAncestorClassNames) const override;
	virtual void GetDerivedClassNames(const TArray<FTopLevelAssetPath>& ClassNames, const TSet<FTopLevelAssetPath>& ExcludedClassNames, TSet<FTopLevelAssetPath>& OutDerivedClassNames) const override;	
	virtual void GetAllCachedPaths(TArray<FString>& OutPathList) const override;
	virtual void EnumerateAllCachedPaths(TFunctionRef<bool(FString)> Callback) const override;
	virtual void EnumerateAllCachedPaths(TFunctionRef<bool(FName)> Callback) const override;
	virtual void GetSubPaths(const FString& InBasePath, TArray<FString>& OutPathList, bool bInRecurse) const override;
	virtual void GetSubPaths(const FName& InBasePath, TArray<FName>& OutPathList, bool bInRecurse) const override;
	virtual void EnumerateSubPaths(const FString& InBasePath, TFunctionRef<bool(FString)> Callback, bool bInRecurse) const override;
	virtual void EnumerateSubPaths(const FName InBasePath, TFunctionRef<bool(FName)> Callback, bool bInRecurse) const override;
	virtual void RunAssetsThroughFilter (TArray<FAssetData>& AssetDataList, const FARFilter& Filter) const override;
	virtual void UseFilterToExcludeAssets(TArray<FAssetData>& AssetDataList, const FARFilter& Filter) const override;
	virtual void UseFilterToExcludeAssets(TArray<FAssetData>& AssetDataList, const FARCompiledFilter& CompiledFilter) const override;
	virtual bool IsAssetIncludedByFilter(const FAssetData& AssetData, const FARCompiledFilter& Filter) const override;
	virtual bool IsAssetExcludedByFilter(const FAssetData& AssetData, const FARCompiledFilter& Filter) const override;
	virtual void CompileFilter(const FARFilter& InFilter, FARCompiledFilter& OutCompiledFilter) const override;
	virtual void SetTemporaryCachingMode(bool bEnable) override;
	virtual void SetTemporaryCachingModeInvalidated() override;
	virtual bool GetTemporaryCachingMode() const override;
	virtual EAssetAvailability::Type GetAssetAvailability(const FAssetData& AssetData) const override;	
	virtual float GetAssetAvailabilityProgress(const FAssetData& AssetData, EAssetAvailabilityProgressReportingType::Type ReportType) const override;
	virtual bool GetAssetAvailabilityProgressTypeSupported(EAssetAvailabilityProgressReportingType::Type ReportType) const override;
	virtual void PrioritizeAssetInstall(const FAssetData& AssetData) const override;
	virtual bool HasVerseFiles(FName PackagePath, bool bRecursive = false) const override;
	virtual bool GetVerseFilesByPath(FName PackagePath, TArray<FName>& OutFilePaths, bool bRecursive = false) const override;
	virtual bool AddPath(const FString& PathToAdd) override;
	virtual bool RemovePath(const FString& PathToRemove) override;
	virtual bool PathExists(const FString& PathToTest) const override;
	virtual bool PathExists(const FName PathToTest) const override;
	virtual void SearchAllAssets(bool bSynchronousSearch) override;
	virtual bool IsSearchAllAssets() const override;
	virtual bool IsSearchAsync() const override;
	virtual void WaitForCompletion() override;
	virtual void WaitForPremadeAssetRegistry() override;
	virtual void ClearGathererCache() override;
	virtual void WaitForPackage(const FString& PackageName) override;
	virtual void ScanSynchronous(const TArray<FString>& InPaths, const TArray<FString>& InFilePaths, UE::AssetRegistry::EScanFlags InScanFlags = UE::AssetRegistry::EScanFlags::None) override;
	virtual void ScanPathsSynchronous(const TArray<FString>& InPaths, bool bForceRescan = false, bool bIgnoreDenyListScanFilters = false) override;
	virtual void ScanFilesSynchronous(const TArray<FString>& InFilePaths, bool bForceRescan = false) override;
	virtual void PrioritizeSearchPath(const FString& PathToPrioritize) override;
	virtual void ScanModifiedAssetFiles(const TArray<FString>& InFilePaths) override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void AppendState(const FAssetRegistryState& InState) override;
	virtual SIZE_T GetAllocatedSize(bool bLogDetailed = false) const override;
	virtual void LoadPackageRegistryData(FArchive& Ar, FLoadPackageRegistryData& InOutData) const override;
	virtual void LoadPackageRegistryData(const FString& PackageFilename, FLoadPackageRegistryData& InOutData) const override;
	virtual void InitializeTemporaryAssetRegistryState(FAssetRegistryState& OutState, const FAssetRegistrySerializationOptions& Options,
		bool bRefreshExisting = false, const TSet<FName>& RequiredPackages = TSet<FName>(),
		const TSet<FName>& RemovePackages = TSet<FName>()) const override;
#if ASSET_REGISTRY_STATE_DUMPING_ENABLED
	virtual void DumpState(const TArray<FString>& Arguments, TArray<FString>& OutPages, int32 LinesPerPage = 1) const override;
#endif

	virtual const FAssetRegistryState* GetAssetRegistryState() const override;
	virtual TSet<FName> GetCachedEmptyPackagesCopy() const override;
	virtual const TSet<FName>& GetCachedEmptyPackages() const override;
	virtual bool ContainsTag(FName TagName) const override;
	virtual void InitializeSerializationOptions(FAssetRegistrySerializationOptions& Options, const FString& PlatformIniName = FString(), UE::AssetRegistry::ESerializationTarget Target = UE::AssetRegistry::ESerializationTarget::ForGame) const override;

	DECLARE_DERIVED_EVENT( UAssetRegistryImpl, IAssetRegistry::FFilesBlockedEvent, FFilesBlockedEvent);
	virtual FFilesBlockedEvent& OnFilesBlocked() override;

	DECLARE_DERIVED_EVENT( UAssetRegistryImpl, IAssetRegistry::FPathsEvent, FPathsEvent);
	virtual FPathsEvent& OnPathsAdded() override;
	virtual FPathsEvent& OnPathsRemoved() override;
	
	DECLARE_DERIVED_EVENT( UAssetRegistryImpl, IAssetRegistry::FPathAddedEvent, FPathAddedEvent);
	virtual FPathAddedEvent& OnPathAdded() override;

	DECLARE_DERIVED_EVENT( UAssetRegistryImpl, IAssetRegistry::FPathRemovedEvent, FPathRemovedEvent);
	virtual FPathRemovedEvent& OnPathRemoved() override;

	virtual void AssetCreated(UObject* NewAsset) override;
	virtual void AssetDeleted(UObject* DeletedAsset) override;
	virtual void AssetRenamed(const UObject* RenamedAsset, const FString& OldObjectPath) override;
	UE_DEPRECATED(5.2, "Use the new AssetsSaved function that takes FAssetData.")
	virtual void AssetSaved(const UObject& SavedAsset) override;
	virtual void AssetsSaved(TArray<FAssetData>&& SavedAssets) override;
	virtual void AssetUpdateTags(UObject* Object, EAssetRegistryTagsCaller Caller) override;
	UE_DEPRECATED(5.4, "Call AssetUpdateTags with EAssetRegistryTagsCaller::Fast")
	virtual void AssetFullyUpdateTags(UObject* Object) override;
	virtual void AssetTagsFinalized(const UObject& FinalizedAsset) override;

	virtual bool VerseCreated(const FString& FilePath) override;
	virtual bool VerseDeleted(const FString& FilePath) override;

	virtual void PackageDeleted(UPackage* DeletedPackage) override;

	DECLARE_DERIVED_EVENT( UAssetRegistryImpl, IAssetRegistry::FAssetAddedEvent, FAssetAddedEvent);
	virtual FAssetAddedEvent& OnAssetAdded() override;

	DECLARE_DERIVED_EVENT( UAssetRegistryImpl, IAssetRegistry::FAssetRemovedEvent, FAssetRemovedEvent);
	virtual FAssetRemovedEvent& OnAssetRemoved() override;
	
	DECLARE_DERIVED_EVENT( UAssetRegistryImpl, IAssetRegistry::FAssetRenamedEvent, FAssetRenamedEvent);
	virtual FAssetRenamedEvent& OnAssetRenamed() override;

	DECLARE_DERIVED_EVENT( UAssetRegistryImpl, IAssetRegistry::FAssetUpdatedEvent, FAssetUpdatedEvent );
	virtual FAssetUpdatedEvent& OnAssetUpdated() override;
	virtual FAssetUpdatedEvent& OnAssetUpdatedOnDisk() override;

	// Batch events
	DECLARE_DERIVED_EVENT( UAssetRegistryImpl, IAssetRegistry::FAssetsEvent, FAssetsEvent);
	virtual FAssetsEvent& OnAssetsAdded() override;
	virtual FAssetsEvent& OnAssetsRemoved() override;
	virtual FAssetsEvent& OnAssetsUpdated() override;
	virtual FAssetsEvent& OnAssetsUpdatedOnDisk() override;
	
	DECLARE_DERIVED_EVENT( UAssetRegistryImpl, IAssetRegistry::FInMemoryAssetCreatedEvent, FInMemoryAssetCreatedEvent );
	virtual FInMemoryAssetCreatedEvent& OnInMemoryAssetCreated() override;

	DECLARE_DERIVED_EVENT( UAssetRegistryImpl, IAssetRegistry::FInMemoryAssetDeletedEvent, FInMemoryAssetDeletedEvent );
	virtual FInMemoryAssetDeletedEvent& OnInMemoryAssetDeleted() override;

	DECLARE_DERIVED_EVENT( UAssetRegistryImpl, IAssetRegistry::FVerseAddedEvent, FVerseAddedEvent);
	virtual FVerseAddedEvent& OnVerseAdded() override;

	DECLARE_DERIVED_EVENT( UAssetRegistryImpl, IAssetRegistry::FVerseRemovedEvent, FVerseRemovedEvent);
	virtual FVerseRemovedEvent& OnVerseRemoved() override;

	DECLARE_DERIVED_EVENT( UAssetRegistryImpl, IAssetRegistry::FFilesLoadedEvent, FFilesLoadedEvent );
	virtual FFilesLoadedEvent& OnFilesLoaded() override;

	DECLARE_DERIVED_EVENT( UAssetRegistryImpl, IAssetRegistry::FFileLoadProgressUpdatedEvent, FFileLoadProgressUpdatedEvent );
	virtual FFileLoadProgressUpdatedEvent& OnFileLoadProgressUpdated() override;

	virtual bool IsLoadingAssets() const override;
	virtual bool ShouldUpdateDiskCacheAfterLoad() const override
	{
#if WITH_EDITORONLY_DATA
		return bUpdateDiskCacheAfterLoad;
#else
		return false;
#endif
	}

	virtual void Tick (float DeltaTime) override;

	virtual void ReadLockEnumerateTagToAssetDatas(TFunctionRef<void(FName TagName, const TArray<const FAssetData*>& Assets)> Callback) const override;

	virtual bool IsPathBeautificationNeeded(const FString& InAssetPath) const override;

protected:
	virtual void SetManageReferences(const TMultiMap<FAssetIdentifier, FAssetIdentifier>& ManagerMap, bool bClearExisting, UE::AssetRegistry::EDependencyCategory RecurseType, TSet<FDependsNode*>& ExistingManagedNodes, ShouldSetManagerPredicate ShouldSetManager = nullptr) override;
	virtual bool SetPrimaryAssetIdForObjectPath(const FSoftObjectPath& ObjectPath, FPrimaryAssetId PrimaryAssetId) override;

private:
	void OnEnginePreExit();
#if WITH_EDITOR
	void OnFEngineLoopInitCompleteSearchAllAssets();
	/** Called when new gatherer is registered. Requires subsequent call to RebuildAssetDependencyGathererMapIfNeeded */
	void OnAssetDependencyGathererRegistered();
#endif
	void InitializeEvents(UE::AssetRegistry::Impl::FInitializeContext& Context);
	void Broadcast(UE::AssetRegistry::Impl::FEventContext& EventContext);

	bool OnResolveRedirect(const FString& InPackageName, FString& OutPackageName);

#if WITH_EDITOR
	/** Called when a file in a content directory changes on disk */
	void OnDirectoryChanged(const TArray<struct FFileChangeData>& Files);

	/** Called when an asset is loaded, it will possibly update the cache */
	void OnAssetLoaded(UObject* AssetLoaded);
#endif

	/**
	 * Called by the engine core when a new content path is added dynamically at runtime.  This is wired to
	 * FPackageName's static delegate.
	 *
	 * @param	AssetPath		The new content root asset path that was added (e.g. "/MyPlugin/")
	 * @param	FileSystemPath	The filesystem path that the AssetPath is mapped to
	 */
	void OnContentPathMounted(const FString& AssetPath, const FString& FileSystemPath);

	/**
	 * Called by the engine core when a content path is removed dynamically at runtime.  This is wired to
	 * FPackageName's static delegate.
	 *
	 * @param	AssetPath		The new content root asset path that was added (e.g. "/MyPlugin/")
	 * @param	FileSystemPath	The filesystem path that the AssetPath is mapped to
	 */
	void OnContentPathDismounted(const FString& AssetPath, const FString& FileSystemPath);

	/** Called to refresh the native classes list, called at end of engine initialization. */
	void OnRefreshNativeClasses();

	/** Called from the PluginManager's loading phase, used to scan classes that were loaded by plugins. */
	void OnPluginLoadingPhaseComplete(ELoadingPhase::Type LoadingPhase, bool bPhaseSuccessful);

	/** Shared helper for Scan*Synchronous function */
	void ScanPathsSynchronousInternal(const TArray<FString>& InDirs, const TArray<FString>& InFiles,
		UE::AssetRegistry::EScanFlags InScanFlags);

#if WITH_EDITOR
	/** Create FAssetData from any loaded UObject assets and store the updated AssetData in the state */
	void ProcessLoadedAssetsToUpdateCache(UE::AssetRegistry::Impl::FEventContext& EventContext,
		const double TickStartTime, UE::AssetRegistry::Impl::EGatherStatus Status);
#endif
	/**
	 * Remain under the given lock and return an InheritanceContext based on the appropriate choice of the persistent
	 * caching buffer or the function-scope-only passed in StackBuffer. Mark whether the buffer needs to be updated
	 * before being used. If the buffer needs to be updated and its the persistent buffer (which is protected data),
	 * convert the given lock to a write lock if not one already.
	 */
	void GetInheritanceContextWithRequiredLock(FRWScopeLock& InOutScopeLock,
		UE::AssetRegistry::Impl::FClassInheritanceContext& InheritanceContext,
		UE::AssetRegistry::Impl::FClassInheritanceBuffer& StackBuffer);
	void GetInheritanceContextWithRequiredLock(FWriteScopeLock& InOutScopeLock,
		UE::AssetRegistry::Impl::FClassInheritanceContext& InheritanceContext,
		UE::AssetRegistry::Impl::FClassInheritanceBuffer& StackBuffer);
	void GetInheritanceContextAfterVerifyingLock(uint64 CurrentGeneratorClassesVersionNumber,
		uint64 CurrentAllClassesVersionNumber,
		UE::AssetRegistry::Impl::FClassInheritanceContext& InheritanceContext,
		UE::AssetRegistry::Impl::FClassInheritanceBuffer& StackBuffer);


#if WITH_EDITOR
	/**
	 * Callback for FObject::FAssetRegistryTag::OnGetExtraObjectTags
	 * If bAddMetaDataTagsToOnGetExtraObjectTags is true, this function will add missing UMetaData tags to cooked assets
	 */
	void OnGetExtraObjectTags(FAssetRegistryTagsContext Context);

	/**
	 * Checks whether the given path is already covered by the general directory watches, or whether we need to setup a
	 * new directory watcher. The caller must ensure that the Directory parameter is in FPaths::CreateStandardFilename format.
	 */
	bool IsDirAlreadyWatchedByRootWatchers(const FString& Directory) const;
#endif

private:

	UE::AssetRegistry::FAssetRegistryImpl GuardedData;

	/** Lock guarding the GuardedData */
	mutable FRWLock InterfaceLock;

#if WITH_EDITOR
	/** Handles to all registered OnDirectoryChanged delegates */
	TMap<FString, FDelegateHandle> OnDirectoryChangedDelegateHandles;
	TArray<FString> DirectoryWatchRoots;
#endif

#if WITH_EDITORONLY_DATA
	/** If true, the asset registry will inject missing tags from UMetaData for cooked assets only in GetAssetRegistryTags */
	bool bAddMetaDataTagsToOnGetExtraObjectTags = true;

	/** If true, the AssetRegistry updates its on-disk information for an Asset whenever that Asset loads. */
	bool bUpdateDiskCacheAfterLoad = true;
#endif

	/** The delegate to execute when one or more files have been blocked from the registry */
	FFilesBlockedEvent FilesBlockedEvent;

	/** The delegate to execute when a batch of paths are added to the registry */
	FPathsEvent PathsAddedEvent;

	/** The delegate to execute when a batch of paths are removed from the registry */
	FPathsEvent PathsRemovedEvent;
	
	/** The delegate to execute when an asset path is added to the registry */
	FPathAddedEvent PathAddedEvent;

	/** The delegate to execute when an asset path is removed from the registry */
	FPathRemovedEvent PathRemovedEvent;

	/** The delegate to execute when an asset is added to the registry */
	FAssetAddedEvent AssetAddedEvent;
	
	/** The delegate to execute when an asset is removed from the registry */
	FAssetRemovedEvent AssetRemovedEvent;

	/** The delegate to execute when an asset is renamed in the registry */
	FAssetRenamedEvent AssetRenamedEvent;

	/** The delegate to execute when an asset is updated in the registry */
	FAssetUpdatedEvent AssetUpdatedEvent;

	/** The delegate to execute when an asset is updated on disk and has been reloaded in assetregistry */
	FAssetUpdatedEvent AssetUpdatedOnDiskEvent;

	/** The delegates to execute when assets are added/removed/updated in the registry, indexed by FEventContext::EEvent or returned with public accessors */
	static constexpr SIZE_T NumBatchedEvents = static_cast<SIZE_T>(UE::AssetRegistry::Impl::FEventContext::EEvent::MAX);
	FAssetsEvent BatchedAssetEvents[NumBatchedEvents];

	/** The delegate to execute when an in-memory asset was just created */
	FInMemoryAssetCreatedEvent InMemoryAssetCreatedEvent;

	/** The delegate to execute when an in-memory asset was just deleted */
	FInMemoryAssetDeletedEvent InMemoryAssetDeletedEvent;

	/** The delegate to execute when a Verse file is added to the registry */
	FVerseAddedEvent VerseAddedEvent;

	/** The delegate to execute when a Verse file is removed from the registry */
	FVerseRemovedEvent VerseRemovedEvent;

	/** The delegate to execute when finished loading files */
	FFilesLoadedEvent FileLoadedEvent;

	/** The delegate to execute while loading files to update progress */
	FFileLoadProgressUpdatedEvent FileLoadProgressUpdatedEvent;

	UE::AssetRegistry::Impl::FEventContext DeferredEvents;

	friend class UE::AssetRegistry::FAssetRegistryImpl;
	friend struct UE::AssetRegistry::Premade::FAsyncConsumer;
};
PRAGMA_ENABLE_DEPRECATION_WARNINGS
