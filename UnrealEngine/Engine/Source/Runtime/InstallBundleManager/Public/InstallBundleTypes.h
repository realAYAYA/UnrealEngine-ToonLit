// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "HAL/Platform.h"
#include "Internationalization/Text.h"
#include "Misc/DateTime.h"
#include "Misc/EnumClassFlags.h"
#include "Misc/EnumRange.h"
#include "UObject/NameTypes.h"

#if !defined(WITH_PLATFORM_INSTALL_BUNDLE_SOURCE)
	#define WITH_PLATFORM_INSTALL_BUNDLE_SOURCE 0
#endif

enum class EInstallBundleSourceType : int
{
	Bulk,
	Launcher,
	BuildPatchServices,
#if WITH_PLATFORM_INSTALL_BUNDLE_SOURCE
	Platform,
#endif // WITH_PLATFORM_INSTALL_BUNDLE_SOURCE
	GameCustom,
	Streaming,
	Count,
};
ENUM_RANGE_BY_COUNT(EInstallBundleSourceType, EInstallBundleSourceType::Count);
INSTALLBUNDLEMANAGER_API const TCHAR* LexToString(EInstallBundleSourceType Type);
INSTALLBUNDLEMANAGER_API void LexFromString(EInstallBundleSourceType& OutType, const TCHAR* String);

enum class EInstallBundleManagerInitState : int
{
	NotInitialized,
	Failed,
	Succeeded
};

enum class EInstallBundleManagerInitResult : int
{
	OK,
	BuildMetaDataNotFound,
	RemoteBuildMetaDataNotFound,
	BuildMetaDataDownloadError,
	BuildMetaDataParsingError,
	DistributionRootParseError,
	DistributionRootDownloadError,
	ManifestArchiveError,
	ManifestCreationError,
	ManifestDownloadError,
	BackgroundDownloadsIniDownloadError,
	NoInternetConnectionError,
	ConfigurationError,
	ClientPatchRequiredError,
	Count
};
INSTALLBUNDLEMANAGER_API const TCHAR* LexToString(EInstallBundleManagerInitResult Result);

enum class EInstallBundleInstallState : int
{
	NotInstalled,
	NeedsUpdate,
	UpToDate,
	Count,
};
INSTALLBUNDLEMANAGER_API const TCHAR* LexToString(EInstallBundleInstallState State);

struct FInstallBundleCombinedInstallState
{
	TMap<FName, EInstallBundleInstallState> IndividualBundleStates;

	INSTALLBUNDLEMANAGER_API bool GetAllBundlesHaveState(EInstallBundleInstallState State, TArrayView<const FName> ExcludedBundles = TArrayView<const FName>()) const;
	INSTALLBUNDLEMANAGER_API bool GetAnyBundleHasState(EInstallBundleInstallState State, TArrayView<const FName> ExcludedBundles = TArrayView<const FName>()) const;
};

struct FInstallBundleContentState
{
	EInstallBundleInstallState State = EInstallBundleInstallState::NotInstalled;
	float Weight = 0.0f;
	TMap<EInstallBundleSourceType, FString> Version;
};

struct FInstallBundleCombinedContentState
{
	TMap<FName, FInstallBundleContentState> IndividualBundleStates;
	TMap<EInstallBundleSourceType, FString> CurrentVersion;
	uint64 DownloadSize = 0;
	uint64 InstallSize = 0;
	uint64 InstallOverheadSize = 0;
	uint64 FreeSpace = 0;

	INSTALLBUNDLEMANAGER_API bool GetAllBundlesHaveState(EInstallBundleInstallState State, TArrayView<const FName> ExcludedBundles = TArrayView<const FName>()) const;	
	INSTALLBUNDLEMANAGER_API bool GetAnyBundleHasState(EInstallBundleInstallState State, TArrayView<const FName> ExcludedBundles = TArrayView<const FName>()) const;
};

enum class EInstallBundleGetContentStateFlags : uint32
{
	None = 0,
	ForceNoPatching = (1 << 0),
};
ENUM_CLASS_FLAGS(EInstallBundleGetContentStateFlags);

DECLARE_DELEGATE_OneParam(FInstallBundleGetContentStateDelegate, FInstallBundleCombinedContentState);

enum class EInstallBundleRequestInfoFlags : int32
{
	None							= 0,
	EnqueuedBundles					= (1 << 0),
	SkippedAlreadyMountedBundles	= (1 << 1),
	SkippedAlreadyUpdatedBundles	= (1 << 2), // Only possible with EInstallBundleRequestFlags::SkipMount
	SkippedAlreadyReleasedBundles	= (1 << 3),
	SkippedAlreadyRemovedBundles	= (1 << 4), // Only possible with EInstallBundleReleaseRequestFlags::RemoveFilesIfPossible
	SkippedUnknownBundles			= (1 << 5),
	SkippedInvalidBundles			= (1 << 6), // Bundle can't be used with this build
	SkippedUnusableLanguageBundles	= (1 << 7), // Can't enqueue language bundles because of current system settings
	SkippedBundlesDueToBundleSource	= (1 << 8), // A bundle source rejected a bundle for some reason
};
ENUM_CLASS_FLAGS(EInstallBundleRequestInfoFlags);

enum class EInstallBundleResult : uint32
{
	OK,
	FailedPrereqRequiresLatestClient,
	FailedPrereqRequiresLatestContent,
	FailedCacheReserve,
	InstallError,
	InstallerOutOfDiskSpaceError,
	ManifestArchiveError,
	ConnectivityError,
	UserCancelledError,
	InitializationError,
	InitializationPending,
	Count,
};
INSTALLBUNDLEMANAGER_API const TCHAR* LexToString(EInstallBundleResult Result);

// TODO: Should probably be renamed to EInstallBundleRequestUpdateFlags 
enum class EInstallBundleRequestFlags : uint32
{
	None = 0,
	CheckForCellularDataUsage = (1 << 0),
	UseBackgroundDownloads = (1 << 1),
	SendNotificationIfDownloadCompletesInBackground = (1 << 2),
	ForceNoPatching = (1 << 3),
	TrackPersistentBundleStats = (1 << 4),
	SkipMount = (1 << 5),
	AsyncMount = (1 << 6),
	Defaults = UseBackgroundDownloads,
};
ENUM_CLASS_FLAGS(EInstallBundleRequestFlags)

enum class EInstallBundleReleaseResult : uint32
{
	OK,
	ManifestArchiveError,
	UserCancelledError,
	Count,
};
INSTALLBUNDLEMANAGER_API const TCHAR* LexToString(EInstallBundleReleaseResult Result);

enum class EInstallBundleReleaseRequestFlags : uint32
{
	None = 0,
	RemoveFilesIfPossible = (1 << 0),  // Bundle sources must support removal, and bundle must not be part of the source's cache
	ExplicitRemoveList = (1 << 1),	   // Only attempt to remove explicitly supplied bundles instead of automatically removing dependencies 
};
ENUM_CLASS_FLAGS(EInstallBundleReleaseRequestFlags)

enum class EInstallBundlePauseFlags : uint32
{
	None = 0,
	OnCellularNetwork = (1 << 0),
	NoInternetConnection = (1 << 1),
	UserPaused = (1 << 2)
};
ENUM_CLASS_FLAGS(EInstallBundlePauseFlags);

enum class EInstallBundleStatus : int
{
	Requested,
	Updating,
	Finishing,
	Ready,
	Count,
};
INSTALLBUNDLEMANAGER_API const TCHAR* LexToString(EInstallBundleStatus Status);

enum class EInstallBundleManagerPatchCheckResult : uint32
{
	/** No patch required */
	NoPatchRequired,
	/** Client Patch required to continue */
	ClientPatchRequired,
	/** Content Patch required to continue */
	ContentPatchRequired,
	/** Logged in user required for a patch check */
	NoLoggedInUser,
	/** Patch check failed */
	PatchCheckFailure,
	Count,
};
INSTALLBUNDLEMANAGER_API const TCHAR* LexToString(EInstallBundleManagerPatchCheckResult EnumVal);

/**
 * Enum used to describe download priority. Higher priorities will be downloaded first.
 * Note: Should always be kept in High -> Low priority order if adding more Priorities!
 */
enum class EInstallBundlePriority : uint8
{
	High,
	Normal,
	Low,
	Count
};
INSTALLBUNDLEMANAGER_API const TCHAR* LexToString(EInstallBundlePriority Priority);
INSTALLBUNDLEMANAGER_API bool LexTryParseString(EInstallBundlePriority& OutMode, const TCHAR* InBuffer);

struct FInstallBundleSourceInitInfo
{
	EInstallBundleManagerInitResult Result = EInstallBundleManagerInitResult::OK;
	bool bShouldUseFallbackSource = false;
};

struct FInstallBundleSourceAsyncInitInfo : public FInstallBundleSourceInitInfo
{
	// Reserved for future use
};

// Bundle Info communicated from bundle source to bundle manager at any time
struct FInstallBundleSourceUpdateBundleInfo
{
	FName BundleName;
	FString BundleNameString;
	EInstallBundlePriority Priority = EInstallBundlePriority::Low;
	uint64 FullInstallSize = 0; // Total disk footprint when this bundle is fully installed
	uint64 InstallOverheadSize = 0; // Any additional space required to complete installation
	FDateTime LastAccessTime = FDateTime::MinValue(); // If cached, used to decide eviction order
	EInstallBundleInstallState BundleContentState = EInstallBundleInstallState::NotInstalled; // Whether this bundle is up to date
	bool bIsCached = false; // Whether this bundle should be cached if this source has a bundle cache
	bool bUseChunkDBs = false; // Whether this bundle should attempt to use ChunkDBs at all
};

struct FInstallBundleSourceUpdateBundleInfoResult
{
	TMap<FName, FInstallBundleSourceUpdateBundleInfo> SourceBundleInfoMap;
};

// Persisted Bundle Info communicated from bundle source to bundle manager on startup
struct FInstallBundleSourcePersistentBundleInfo : FInstallBundleSourceUpdateBundleInfo
{
	uint64 CurrentInstallSize = 0; // Disk footprint of the bundle in it's current state
	bool bIsStartup = false; // Only one startup bundle allowed.  All sources must agree on this.
	bool bDoPatchCheck = false; // This bundle should do a patch check and fail if it doesn't pas	
};

struct FInstallBundleSourceBundleInfoQueryResult
{
	TMap<FName, FInstallBundleSourcePersistentBundleInfo> SourceBundleInfoMap;
};

enum class EInstallBundleSourceUpdateBundleInfoResult : uint8
{
	OK,
	NotInitailized,
	AlreadyMounted,
	AlreadyRequested,
	IllegalCacheStatus,
	Count,
};
INSTALLBUNDLEMANAGER_API const TCHAR* LexToString(EInstallBundleSourceUpdateBundleInfoResult Result);

struct FInstallBundleSourceUpdateContentResultInfo
{
	FName BundleName;
	EInstallBundleResult Result = EInstallBundleResult::OK;

	// Forward any errors from the underlying implementation for a specific source
	// Currently, these just forward BPT Error info
	FText OptionalErrorText;
	FString OptionalErrorCode;

	TArray<FString> ContentPaths;
	TArray<FString> AdditionalRootDirs;
	// Support platforms that need shaderlibs in the physical FS
	TSet<FString> NonUFSShaderLibPaths;

	uint64 CurrentInstallSize = 0;
	FDateTime LastAccessTime = FDateTime::MinValue(); // If cached, used to decide eviction order

	bool bContentWasInstalled = false; // If true, the source did work to update the content
	
	bool DidBundleSourceDoWork() const { return (ContentPaths.Num() != 0);} 
};

struct FInstallBundleSourceReleaseContentResultInfo
{
	FName BundleName;
	EInstallBundleReleaseResult Result = EInstallBundleReleaseResult::OK;

	FDateTime LastAccessTime = FDateTime::MinValue(); // If cached, used to decide eviction order

	// Indicates content was actually removed and bundle manager should consider
	// this bundle as no longer installed.
	bool bContentWasRemoved = false;
};

struct FInstallBundleSourceProgress
{
	FName BundleName;

	float Install_Percent = 0;
};

struct FInstallBundleSourcePauseInfo
{
	FName BundleName;
	EInstallBundlePauseFlags PauseFlags = EInstallBundlePauseFlags::None;
	// True if the bundle actually transitioned to/from paused,
	// which is different than the flags changing
	bool bDidPauseChange = false;
};

enum class EInstallBundleSourceBundleSkipReason : uint32
{
	None = 0,
	LanguageNotCurrent = (1 << 0), // The platform language must be changed to make it valid to request this bundle
	NotValid = (1 << 1), // Bundle can't be used with this build
};
ENUM_CLASS_FLAGS(EInstallBundleSourceBundleSkipReason);

struct FInstallBundleCacheStats
{
	FName CacheName;
	uint64 MaxSize = 0;
	uint64 UsedSize = 0;
	uint64 ReservedSize = 0;
	uint64 FreeSize = 0;
};

enum class EInstallBundleCacheDumpToLog : int8
{
	None = 0,
	Default,
	CSV
};
