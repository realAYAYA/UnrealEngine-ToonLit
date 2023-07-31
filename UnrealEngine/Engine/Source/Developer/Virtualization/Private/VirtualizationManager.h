// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Compression/CompressedBuffer.h"
#include "HAL/CriticalSection.h"
#include "Logging/LogMacros.h"
#include "Templates/UniquePtr.h"

#include "Virtualization/VirtualizationSystem.h"

class IConsoleObject;
class FOutputDevice;

/**
 * Configuring the backend hierarchy
 * 
 * The [Core.ContentVirtualization] section can contain a string 'BackendGraph' which will set with the name of  
 * the backend graph, if not set then the default 'ContentVirtualizationBackendGraph_None' will be used instead. 
 * This value can also be overridden from the command line by using 'BackendGraph=FooBar' where FooBar is the 
 * name of the graph.
 * 
 * The first entry in the graph to be parsed will be the 'Hierarchy' which describes which backends should be
 * mounted and in which order. For example 'Hierarchy=(Entry=Foo, Entry=Bar)' which should mount two backends
 * 'Foo' and 'Bar' in that order. 
 * 
 * Each referenced backend in the hierarchy will then require it's own entry in the graph where the key will be
 * it's name in the hierarchy and the value a string describing how to set it up. 
 * The value must contain 'Type=X' where X is the name used to find the correct IVirtualizationBackendFactory 
 * to create the backend with. 
 * Once the backend is created then reset of the string will be passed to it, so that additional customization
 * can be extracted. Depending on the backend implementation these values may or may not be required.
 * 
 * Example graph:
 * [ContentVirtualizationBackendGraph_Example] 
 * Hierarchy=(Entry=MemoryCache, Entry=NetworkShare)
 * MemoryCache=(Type=InMemory)
 * NetworkShare=(Type=FileSystem, Path="\\path\to\somewhere")
 * 
 * The graph is named 'ContentVirtualizationBackendGraph_Example'.
 * The hierarchy contains two entries 'InMemory' and 'NetworkShare' to be mounted in that order
 * MemoryCache creates a backend of type 'InMemory' and has no additional customization
 * NetworkShare creates a backend of type 'FileSystem' and provides an additional path, the filesystem backend would 
 * fatal error without this value.
 */

/**
 * Filtering
 * 
 * When pushing a payload it can be filtered based on the path of the package it belongs to. The filtering options 
 * are set up via the config files. 
 * Note that this only affects pushing a payload, if the filtering for a project is changed to exclude a package that
 * is already virtualized it will still be able to pull it's payloads as needed but will store them locally in the 
 * package the next time that it is saved.
 * @see ShouldVirtualizePackage or ShouldVirtualize for implementation details.
 * 
 * Basic Setup:
 * 
 * [Core.ContentVirtualization]
 * FilterMode=OptIn/OptOut					When 'OptIn' payloads will be virtualized by default, when 'OptOut' they will not be virtualized by default
 * FilterEngineContent=True/False			When true any payload from a package under Engine/Content/.. will be excluded from virtualization
 * FilterEnginePluginContent=True/False		When true any payload from a package under Engine/Plugins/../Content/.. will be excluded from virtualization
 * FilterMapContent=True/False				When true any payload stored in a .umap or _BuildData.uasset file will be excluded from virtualization
 * LazyInitConnections=True/False			When true the backends will not try to make their connections until first used.
 * 
 * PackagePath Setup:
 * 
 * In addition to the default filtering mode set above, payloads stored in packages can be filtered based on the
 * package path. This allows a package to be including in the virtualization process or excluded from it.
 * 
 * Note that these paths will be stored in the ini files under the Saved directory. To remove a path make sure to 
 * use the - syntax to remove the entry from the array, rather than removing the line itself. Otherwise it will
 * persist until the saved config file has been reset.
 *
 * [/Script/Virtualization.VirtualizationFilterSettings]
 * +ExcludePackagePaths="/MountPoint/PathToExclude/"				Excludes any package found under '/MountPoint/PathToExclude/' from the virtualization process
 * +ExcludePackagePaths="/MountPoint/PathTo/ThePackageToExclude"	Excludes the specific package '/MountPoint/PathTo/ThePackageToExclude' from the virtualization process
 * +IncludePackagePaths="/MountPoint/PathToInclude/"				Includes any package found under '/MountPoint/PathToInclude/' in the virtualization process
 * +IncludePackagePaths="/MountPoint/PathTo/ThePackageToInclude"	Includes the specific package '/MountPoint/PathTo/ThePackageToInclude' in the virtualization process
 */

namespace UE::Virtualization
{
class IVirtualizationBackend;
class IVirtualizationBackendFactory;
class FPullRequestCollection;

/** The default mode of filtering to use with package paths that do not match entries in UVirtualizationFilterSettings */
enum class EPackageFilterMode : uint8
{
	/** Packages will be virtualized by default and must be opted out by the use of UVirtualizationFilterSettings::ExcludePackagePaths */
	OptOut = 0,
	/** Packages will not be virtualized by default and must be opted in by the user of UVirtualizationFilterSettings::IncludePackagePaths */
	OptIn
};

/** Attempt to convert a string buffer to EPackageFilterMode */
bool LexTryParseString(EPackageFilterMode& OutValue, FStringView Buffer);

/** This is used as a wrapper around the various potential back end implementations. 
	The calling code shouldn't need to care about which back ends are actually in use. */
class FVirtualizationManager final : public IVirtualizationSystem
{
public:
	using FRegistedFactories = TMap<FName, IVirtualizationBackendFactory*>;
	using FBackendArray = TArray<IVirtualizationBackend*>;

	FVirtualizationManager();
	virtual ~FVirtualizationManager();

private:
	/* IVirtualizationSystem implementation */

	virtual bool Initialize(const FInitParams& InitParams) override;

	virtual bool IsEnabled() const override;
	virtual bool IsPushingEnabled(EStorageType StorageType) const override;

	virtual EPayloadFilterReason FilterPayload(const UObject* Owner) const override;

	virtual bool AllowSubmitIfVirtualizationFailed() const override;
	
	virtual bool PushData(TArrayView<FPushRequest> Requests, EStorageType StorageType) override;
	virtual bool PullData(TArrayView<FPullRequest> Requests) override;

	virtual EQueryResult QueryPayloadStatuses(TArrayView<const FIoHash> Ids, EStorageType StorageType, TArray<EPayloadStatus>& OutStatuses) override;

	virtual EVirtualizationResult TryVirtualizePackages(TConstArrayView<FString> PackagePaths, TArray<FText>& OutDescriptionTags, TArray<FText>& OutErrors) override;

	virtual ERehydrationResult TryRehydratePackages(TConstArrayView<FString> PackagePaths, TArray<FText>& OutErrors) override;
	virtual ERehydrationResult TryRehydratePackages(TConstArrayView<FString> PackagePaths, uint64 PaddingAlignment, TArray<FText>& OutErrors, TArray<FSharedBuffer>& OutPackages, TArray<FRehydrationInfo>* OutInfo) override;

	virtual void DumpStats() const override;

	virtual FPayloadActivityInfo GetAccumualtedPayloadActivityInfo() const override;

	virtual void GetPayloadActivityInfo( GetPayloadActivityInfoFuncRef ) const override;

	virtual FOnNotification& GetNotificationEvent() override
	{
		return NotificationEvent;
	}
	
private:

	void ApplySettingsFromConfigFiles(const FConfigFile& ConfigFile);
	void ApplySettingsFromFromCmdline();
	void ApplySettingsFromCVar();

	void ApplyDebugSettingsFromFromCmdline();

	void RegisterConsoleCommands();

	void OnUpdateDebugMissBackendsFromConsole(const TArray<FString>& Args, FOutputDevice& OutputDevice);
	void OnUpdateDebugMissChanceFromConsole(const TArray<FString>& Args, FOutputDevice& OutputDevice);
	void OnUpdateDebugMissCountFromConsole(const TArray<FString>& Args, FOutputDevice& OutputDevice);

	void UpdateBackendDebugState();

	bool ShouldDebugDisablePulling(FStringView BackendConfigName) const;	
	bool ShouldDebugFailPulling();

	void MountBackends(const FConfigFile& ConfigFile);
	void ParseHierarchy(const FConfigFile& ConfigFile, const TCHAR* GraphName, const TCHAR* HierarchyKey, const TCHAR* LegacyHierarchyKey, const FRegistedFactories& FactoryLookupTable, FBackendArray& PushArray);
	bool CreateBackend(const FConfigFile& ConfigFile, const TCHAR* GraphName, const FString& ConfigEntryName, const FRegistedFactories& FactoryLookupTable, FBackendArray& PushArray);

	void AddBackend(TUniquePtr<IVirtualizationBackend> Backend, FBackendArray& PushArray);

	void EnsureBackendConnections();

	void CachePayloads(TArrayView<FPushRequest> Requests, const IVirtualizationBackend* BackendSource);

	bool TryCacheDataToBackend(IVirtualizationBackend& Backend, TArrayView<FPushRequest> Requests);
	bool TryPushDataToBackend(IVirtualizationBackend& Backend, TArrayView<FPushRequest> Requests);

	void PullDataFromAllBackends(TArrayView<FPullRequest> Requests);
	void PullDataFromBackend(IVirtualizationBackend& Backend, TArrayView<FPullRequest> Requests);

	bool ShouldVirtualizeAsset(const UObject* Owner) const;

	/** 
	 * Determines if a package path should be virtualized or not based on any exclusion/inclusion patterns
	 * that might have been set in UVirtualizationFilterSettings.
	 * If the path does not match any pattern set in UVirtualizationFilterSettings then use the default 
	 * FilterMode to determine if the payload should be virtualized or not.
	 * 
	 * @param PackagePath	The path of the package to check. This can be empty which would indicate that
	 *						a payload is not owned by a specific package.
	 * @return				True if the package should be virtualized and false if the package path is 
	 *						excluded by the projects current filter set up.
	 */
	bool ShouldVirtualizePackage(const FPackagePath& PackagePath) const;

	/**
	 * Determines if a package should be virtualized or not based on the given content.
	 * If the context can be turned into a package path then ::ShouldVirtualizePackage 
	 * will be used instead.
	 * If the context is not a package path then we use the default FilterMode to determine
	 * if the payload should be virtualized or not.
	 * 
	 * @return True if the context should be virtualized and false if not.
	 */
	bool ShouldVirtualize(const FString& Context) const;

	/** Determines if the default filtering behavior is to virtualize a payload or not */
	bool ShouldVirtualizeAsDefault() const;

	void BroadcastEvent(TConstArrayView<FPullRequest> Ids, ENotification Event);
	
private:
	// The following members are set from the config file

	/** Are packages allowed to be virtualized when submitted to source control. Defaults to true. */
	bool bAllowPackageVirtualization;

	/** Should payloads be cached locally after being pulled from persistent storage? Defaults to true. */
	bool bEnableCacheAfterPull;

	/** The minimum length for a payload to be considered for virtualization. Defaults to 0 bytes. */
	int64 MinPayloadLength;

	/** The name of the backend graph to load from the config ini file that will describe the backend hierarchy */
	FString BackendGraphName;

	/** The tag that will be returned when the virtualization process has run, commonly used to post fix changelist descriptions */
	FString VirtualizationProcessTag;

	/** The default filtering mode to apply if a payload is not matched with an option in UVirtualizationFilterSettings */
	EPackageFilterMode FilteringMode;

	/** Should payloads in engine content packages be filtered out and never virtualized */
	bool bFilterEngineContent;
	
	/** Should payloads in engine plugin content packages be filtered out and never virtualized */
	bool bFilterEnginePluginContent;

	/** Should payloads in .umap files (or associated _BuildData files) be filtered out and never virtualized */
	bool bFilterMapContent;

	/** Should file submits be allowed to continue if a call to TryVirtualizePackages fails */
	bool bAllowSubmitIfVirtualizationFailed;
	
	/** Should backends defer connecting to their services until first use */
	bool bLazyInitConnections;

private:

	/** The name of the current project */
	FString ProjectName;

	/** The names of all asset types that should not virtualize. See @IsDisabledForObject */
	TSet<FName> DisabledAssetTypes;

	/** All of the backends that were mounted during graph creation */
	TArray<TUniquePtr<IVirtualizationBackend>> AllBackends;

	/** Backends used for caching operations (must support push operations). */
	FBackendArray CacheStorageBackends; 

	/** Backends used for persistent storage operations (must support push operations). */
	FBackendArray PersistentStorageBackends; 

	/** 
	 * The hierarchy of backends to pull from, this is assumed to be ordered from fastest to slowest
	 * and can contain a mixture of local cacheable and persistent backends 
	 */
	FBackendArray PullEnabledBackends;

	/** Do we have backends that have not yet tried connecting to their services */
	bool bPendingBackendConnections;

	/** Our notification Event */
	FOnNotification NotificationEvent;

	// Members after this point at used for debugging operations only!

	struct FDebugValues
	{
		/** All of the console commands/variables that we register, so they can be unregistered when the manager is destroyed */
		TArray<IConsoleObject*> ConsoleObjects;

		/** 
		 * Contains all of the delegate handles that we have bound to IConsoleVariable::OnChangedDelegate and need to be removed
		 * before it is safe to destroy the manager. Most likely due to having bound a lambda to the delegate that captured the
		 * FVirtualizationManager this pointer.
		 */
		TArray<TPair<IConsoleVariable*, FDelegateHandle>> ConsoleDelegateHandles;

		/** The critical section used to force single threaded access if bForceSingleThreaded is true */
		FCriticalSection ForceSingleThreadedCS;

		/** When enabled all public operations will be performed as single threaded */
		bool bSingleThreaded = false;

		/** 
		 * When enabled we will immediately 'pull' each payload after it has been pushed to either local or persistent
		 * storage and compare it to the original payload source to make sure that it was uploaded correctly 
		 */
		bool bValidateAfterPush = false;

		/** Array of backend names that should have their pull operation disabled */
		TArray<FString> MissBackends;

		/** The chance that a payload pull can just 'fail' to allow for testing */
		float MissChance;

		/** The number of upcoming payload pulls that should be failed */
		std::atomic<int32> MissCount = 0;
	} DebugValues;
};

} // namespace UE::Virtualization
