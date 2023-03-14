// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "Cooker/CookProfiling.h"
#include "CookOnTheSide/CookOnTheFlyServer.h" // ECookTickFlags
#include "DerivedDataRequestOwner.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/Platform.h"
#include "Logging/TokenizedMessage.h"
#include "Misc/AssertionMacros.h"
#include "Serialization/PackageWriter.h"
#include "Templates/Function.h"
#include "UObject/NameTypes.h"
#include "UObject/SavePackage.h"

class FCbFieldView;
class FCbWriter;
class ITargetPlatform;
struct FWeakObjectPtr;
namespace UE::DerivedData { class FBuildDefinition; }
namespace UE::Cook { struct FBeginCookConfigSettings; }
namespace UE::Cook { struct FCookByTheBookOptions; }
namespace UE::Cook { struct FCookOnTheFlyOptions; }
namespace UE::Cook { struct FInitializeConfigSettings; }

FCbWriter& operator<<(FCbWriter& Writer, const UE::Cook::FBeginCookConfigSettings& Value);
bool LoadFromCompactBinary(FCbFieldView Field, UE::Cook::FBeginCookConfigSettings& Value);
FCbWriter& operator<<(FCbWriter& Writer, const UE::Cook::FCookByTheBookOptions& Value);
bool LoadFromCompactBinary(FCbFieldView Field, UE::Cook::FCookByTheBookOptions& Value);
FCbWriter& operator<<(FCbWriter& Writer, const UE::Cook::FCookOnTheFlyOptions& Value);
bool LoadFromCompactBinary(FCbFieldView Field, UE::Cook::FCookOnTheFlyOptions& Value);
FCbWriter& operator<<(FCbWriter& Writer, const UE::Cook::FInitializeConfigSettings& Value);
bool LoadFromCompactBinary(FCbFieldView Field, UE::Cook::FInitializeConfigSettings& Value);

#define COOK_CHECKSLOW_PACKAGEDATA 0
#define DEBUG_COOKONTHEFLY 0

/** A BaseKeyFuncs for Maps and Sets with a quicker hash function for pointers than TDefaultMapKeyFuncs */
template<typename KeyType>
struct TFastPointerSetKeyFuncs : public DefaultKeyFuncs<KeyType>
{
	using typename DefaultKeyFuncs<KeyType>::KeyInitType;
	static FORCEINLINE uint32 GetKeyHash(KeyInitType Key)
	{
#if PLATFORM_64BITS
		static_assert(sizeof(UPTRINT) == sizeof(uint64), "Expected pointer size to be 64 bits");
		// Ignoring the lower 4 bits since they are likely zero anyway.
		const uint64 ImportantBits = reinterpret_cast<uint64>(Key) >> 4;
		return GetTypeHash(ImportantBits);
#else
		static_assert(sizeof(UPTRINT) == sizeof(uint32), "Expected pointer size to be 32 bits");
		return static_cast<uint32>(reinterpret_cast<uint32>(Key));
#endif
	}
};

template<typename KeyType, typename ValueType, bool bInAllowDuplicateKeys>
struct TFastPointerMapKeyFuncs : public TDefaultMapKeyFuncs<KeyType, ValueType, bInAllowDuplicateKeys>
{
	using typename TDefaultMapKeyFuncs<KeyType, ValueType, bInAllowDuplicateKeys>::KeyInitType;
	static FORCEINLINE uint32 GetKeyHash(KeyInitType Key)
	{
		return TFastPointerSetKeyFuncs<KeyType>::GetKeyHash(Key);
	}
};

/** A TMap which uses TFastPointerMapKeyFuncs instead of TDefaultMapKeyFuncs */
template<typename KeyType, typename ValueType, typename SetAllocator = FDefaultSetAllocator>
class TFastPointerMap : public TMap<KeyType, ValueType, SetAllocator, TFastPointerMapKeyFuncs<KeyType, ValueType, false>>
{};

/** A TSet which uses TFastPointerSetKeyFuncs instead of DefaultKeyFuncs */
template<typename KeyType, typename SetAllocator = FDefaultSetAllocator>
class TFastPointerSet : public TSet<KeyType, TFastPointerSetKeyFuncs<KeyType>, SetAllocator>
{};

namespace UE::Cook
{
	struct FPackageData;
	struct FPlatformData;

	/** A function that is called when a requested package finishes cooking (when successful, failed, or skipped) */
	typedef TUniqueFunction<void(FPackageData*)> FCompletionCallback;

	class FPackageDataSet : public TFastPointerSet<FPackageData*>
	{
		using TFastPointerSet<FPackageData*>::TFastPointerSet;
	};

	/** External Requests to the cooker can either by cook requests for a specific file, or arbitrary callbacks that need to execute within the Scheduler's lock. */
	enum class EExternalRequestType
	{
		None,
		Callback,
		Cook
	};

	/* The Result of a Cook */
	enum class ECookResult
	{
		Unseen,		/* The package has not finished cooking, or if it was previously cooked its result was removed due to e.g. modification of the package. */
		Succeeded,  /* The package was saved with success. */
		Failed,     /* The package was processed but failed to load or save. */
		Skipped     /* For reporting the ECookResults specific to a request: the package was skipped due to e.g. already being cooked or being in NeverCook packages. */
	};

	/** Return type for functions called reentrantly that can succeed,fail,or be incomplete */
	enum class EPollStatus : uint8
	{
		Success,
		Error,
		Incomplete,
	};

	/**
	 * The possible reasons that the save-state data on an FPackageData might be released.
	 * Different levels of teardown will happen based on the reason.
	 */
	enum class EReleaseSaveReason : uint8
	{
		Completed,
		DoneForNow,
		Demoted,
		AbortSave,
		RecreateObjectCache,
	};
	const TCHAR* LexToString(UE::Cook::EReleaseSaveReason Reason);

	enum class ESuppressCookReason : uint8
	{
		InvalidSuppressCookReason,
		AlreadyCooked,
		NeverCook,
		DoesNotExistInWorkspaceDomain,
		ScriptPackage,
		NotInCurrentPlugin,
		Redirected,
		OrphanedGenerated,
		LoadError,
		SaveError,
		OnlyEditorOnly,
		CookCanceled,
		MultiprocessAssignmentError,
	};

	/** The type of callback for External Requests that needs to be executed within the Scheduler's lock. */
	typedef TUniqueFunction<void()> FSchedulerCallback;

	/** Which phase of cooking a Package is in.  */
	enum class EPackageState
	{
		Idle = 0,	  /* The Package is not being operated on by the cooker, and is not in any queues.  This is the state both for packages that have never been requested and for packages that have finished cooking. */
		Request,	  /* The Package is in the RequestQueue; it is requested for cooking but has not had any operations performed on it. */
		AssignedToWorker, /* The Package is in the AssignedToWorkerSet; it has been sent a remote CookWorker for cooking and has not had any operations performed on it locally. */
		LoadPrepare,  /* The Package is in the LoadPrepareQueue. Preloading is in progress. */
		LoadReady,	  /* The package is in the LoadReadyQueue. Preloading is complete and it will be loaded when its turn comes up. */
		Save,		  /* The Package is in the SaveQueue; it has been fully loaded and some target data may have been calculated. */

		Min = Idle,
		Max = Save,
		Count = Max + 1, /* Number of values in this enum, not a valid value for any EPackageState variable. */
		BitCount = 3, /* Number of bits required to store a valid EPackageState */
	};

	enum class EPackageStateProperty // Bitfield
	{
		None		= 0,
		InProgress	= 0x1, /* The package is being worked on by the cooker. */
		Loading		= 0x2, /* The package is in one of the loading states and has preload data. */
		HasPackage	= 0x4, /* The package has progressed past the loading state, and the UPackage pointer is available on the FPackageData. */

		Min = InProgress,
		Max = HasPackage
	};
	ENUM_CLASS_FLAGS(EPackageStateProperty);

	/** Used as a helper to timeslice cooker functions. */
	struct FCookerTimer
	{
	public:
		enum EForever
		{
			Forever
		};
		enum ENoWait
		{
			NoWait
		};

		FCookerTimer(float InTimeSlice);
		FCookerTimer(EForever);
		FCookerTimer(ENoWait);

		double GetTimeTillNow() const;
		double GetEndTimeSeconds() const;
		bool IsTimeUp() const;
		bool IsTimeUp(double CurrentTimeSeconds) const;
		double GetTimeRemain() const;

	public:
		const double StartTime;
		const float TimeSlice;
	};

	/** Temporary-lifetime data about the current tick of the cooker. */
	struct FTickStackData
	{
		/** Time at which the current iteration of the DecideCookAction loop started. */
		double LoopStartTime = 0.;
		/** A bitmask of flags of type enum ECookOnTheSideResult that were set during the tick. */
		uint32 ResultFlags = 0;
		/** The CookerTimer for the current tick. Used by slow reentrant operations that need to check whether they have timed out. */
		FCookerTimer Timer;
		/** CookFlags describing details of the caller's desired behavior for the current tick. */
		ECookTickFlags TickFlags;

		bool bCookComplete = false;
		bool bCookCancelled = false;

		explicit FTickStackData(float TimeSlice, ECookTickFlags InTickFlags)
			:Timer(TimeSlice), TickFlags(InTickFlags)
		{
		}
	};

	/** 
	* Context data passed into SavePackage for a given TargetPlatform. Constant across packages, and internal
	* to the cooker.
	*/
	struct FCookSavePackageContext
	{
		FCookSavePackageContext(const ITargetPlatform* InTargetPlatform,
			ICookedPackageWriter* InPackageWriter, FStringView InWriterDebugName);
		~FCookSavePackageContext();

		FSavePackageContext SaveContext;
		FString WriterDebugName;
		ICookedPackageWriter* PackageWriter;
		ICookedPackageWriter::FCookCapabilities PackageWriterCapabilities;
	};

	/* Thread Local Storage access to identify which thread is the SchedulerThread for cooking. */
	void InitializeTls();
	bool IsSchedulerThread();
	void SetIsSchedulerThread(bool bValue);


	/** Placeholder to handle executing BuildDefintions for requested but not-yet-loaded packages. */
	class FBuildDefinitions
	{
	public:
		FBuildDefinitions();
		~FBuildDefinitions();

		void AddBuildDefinitionList(FName PackageName, const ITargetPlatform* TargetPlatform,
			TConstArrayView<UE::DerivedData::FBuildDefinition> BuildDefinitionList);
		bool TryRemovePendingBuilds(FName PackageName);
		void Wait();
		void Cancel();

	private:
		bool bTestPendingBuilds = false;
		struct FPendingBuildData
		{
			bool bTryRemoved = false;
		};
		TMap<FName, FPendingBuildData> PendingBuilds;
	};

	struct FInitializeConfigSettings
	{
	public:
		void LoadLocal(const FString& InOutputDirectoryOverride);
		void CopyFromLocal(const UCookOnTheFlyServer& COTFS);
		void MoveToLocal(UCookOnTheFlyServer& COTFS);
	private:
		template <typename SourceType, typename TargetType>
		void MoveOrCopy(SourceType&& Source, TargetType&& Target);

	public:
		FString OutputDirectoryOverride;
		int32 MaxPrecacheShaderJobs = 0;
		int32 MaxConcurrentShaderJobs = 0;
		uint32 PackagesPerGC = 0;
		float MemoryExpectedFreedToSpreadRatio = 0.f;
		double IdleTimeToGC = 0.;
		uint64 MemoryMaxUsedVirtual;
		uint64 MemoryMaxUsedPhysical;
		uint64 MemoryMinFreeVirtual;
		uint64 MemoryMinFreePhysical;
		int32 MinFreeUObjectIndicesBeforeGC;
		int32 MaxNumPackagesBeforePartialGC;
		TArray<FString> ConfigSettingDenyList;
		TMap<FName, int32> MaxAsyncCacheForType; // max number of objects of a specific type which are allowed to async cache at once
		bool bHybridIterativeDebug = false;

		friend FCbWriter& ::operator<<(FCbWriter& Writer, const UE::Cook::FInitializeConfigSettings& Value);
		friend bool ::LoadFromCompactBinary(FCbFieldView Field, UE::Cook::FInitializeConfigSettings& Value);
	};

	struct FBeginCookConfigSettings
	{
		/** Initialize NeverCookPackageList from packaging settings and platform-specific sources. */
		void LoadLocal(FBeginCookContext& BeginContext);
		void LoadNeverCookLocal(FBeginCookContext& BeginContext);
		void CopyFromLocal(const UCookOnTheFlyServer& COTFS);

		FString CookShowInstigator;
		bool bHybridIterativeEnabled = true;
		TArray<FName> NeverCookPackageList;
		TFastPointerMap<const ITargetPlatform*, TSet<FName>> PlatformSpecificNeverCookPackages;

		friend FCbWriter& ::operator<<(FCbWriter& Writer, const UE::Cook::FBeginCookConfigSettings& Value);
		friend bool ::LoadFromCompactBinary(FCbFieldView Field, UE::Cook::FBeginCookConfigSettings& Value);
	};

	/**
	 *  Identifier for a CookWorker process launched from a Director process, or for the local process.
	 *  A director can have multiple CookWorkers.
	 */
	struct FWorkerId
	{
	public:
		FWorkerId() { Id = InvalidId; }
		constexpr static FWorkerId Invalid() { return FWorkerId(InvalidId); }
		constexpr static FWorkerId Local() { return FWorkerId(LocalId); }
		static FWorkerId FromRemoteIndex(uint8 Index) { check(Index < InvalidId-1U);  return FWorkerId(Index + 1U); }
		static FWorkerId FromLocalOrRemoteIndex(uint8 Index) { check(Index < InvalidId);  return FWorkerId(Index); }

		bool IsValid() const { return Id != InvalidId; }
		bool IsInvalid() const { return Id == InvalidId; }
		bool IsLocal() const { return Id == LocalId; }
		bool IsRemote() const { return Id != InvalidId && Id != LocalId; }
		uint8 GetRemoteIndex() const { check(IsRemote()); return Id - 1U; }
		uint8 GetLocalOrRemoteIndex() const { check(IsValid()); return Id; }
		bool operator<(const FWorkerId& Other) const { return Id < Other.Id; }

	private:
		constexpr explicit FWorkerId(uint8 InId) : Id(InId) {}

	private:
		uint8 Id;

		constexpr static uint8 InvalidId = 255;
		constexpr static uint8 LocalId = 0;
	};

	/** Report whether commandline/config has disabled use of timeouts throughout the cooker, useful for debugging. */
	bool IsCookIgnoreTimeouts();
}

inline void RouteBeginCacheForCookedPlatformData(UObject* Obj, const ITargetPlatform* TargetPlatform)
{
	UE_SCOPED_TEXT_COOKTIMER(*WriteToString<128>(GetClassTraceScope(Obj), TEXT("_BeginCacheForCookedPlatformData")));
	UE_SCOPED_COOK_STAT(Obj->GetPackage()->GetFName(), EPackageEventStatType::BeginCacheForCookedPlatformData);
	Obj->BeginCacheForCookedPlatformData(TargetPlatform);
}

inline bool RouteIsCachedCookedPlatformDataLoaded(UObject* Obj, const ITargetPlatform* TargetPlatform)
{
	UE_SCOPED_TEXT_COOKTIMER(*WriteToString<128>(GetClassTraceScope(Obj), TEXT("_IsCachedCookedPlatformDataLoaded")));
	UE_SCOPED_COOK_STAT(Obj->GetPackage()->GetFName(), EPackageEventStatType::IsCachedCookedPlatformDataLoaded);
	return Obj->IsCachedCookedPlatformDataLoaded(TargetPlatform);
}

//////////////////////////////////////////////////////////////////////////
// Cook by the book options

namespace UE::Cook
{

struct FCookByTheBookOptions
{
public:
	// Process-lifetime variables
	TArray<FName>					StartupPackages;

	// Session-lifetime variables
	/**
	 * The list of UObjects that existed at the start of the cook. This is used to tell which UObjects
	 * were created during the cook.
	 */
	TArray<FWeakObjectPtr>			SessionStartupObjects;

	/** DlcName setup if we are cooking dlc will be used as the directory to save cooked files to */
	FString							DlcName;

	/** Create a release from this manifest and store it in the releases directory for this cgame */
	FString							CreateReleaseVersion;

	/** If we are based on a release version of the game this is the set of packages which were cooked in that release. Map from platform name to list of uncooked package filenames */
	TMap<FName, TArray<FName>>		BasedOnReleaseCookedPackages;

	/** Mapping from source packages to their localized variants (based on the culture list in FCookByTheBookStartupOptions) */
	TMap<FName, TArray<FName>>		SourceToLocalizedPackageVariants;
	/** List of all the cultures (e.g. "en") that need to be cooked */
	TArray<FString>					AllCulturesToCook;

	/** Timing information about cook by the book */
	double							CookTime = 0.0;
	double							CookStartTime = 0.0;

	ECookByTheBookOptions			StartupOptions = ECookByTheBookOptions::None;

	/** Should we generate streaming install manifests (only valid option in cook by the book) */
	bool							bGenerateStreamingInstallManifests = false;

	/** Should we generate a seperate manifest for map dependencies */
	bool							bGenerateDependenciesForMaps = false;

	/** error when detecting engine content being used in this cook */
	bool							bErrorOnEngineContentUse = false;
	bool							bAllowUncookedAssetReferences = false; // this is a flag for dlc, will allow DLC to be cook when the fixed base might be missing references.
	bool							bSkipHardReferences = false;
	bool							bSkipSoftReferences = false;
	bool							bFullLoadAndSave = false;
	bool							bCookAgainstFixedBase = false;
	bool							bDlcLoadMainAssetRegistry = false;

	void ClearSessionData()
	{
		FCookByTheBookOptions EmptyOptions;
		// Preserve Process-lifetime variables
		EmptyOptions.StartupPackages = MoveTemp(StartupPackages);
		*this = MoveTemp(EmptyOptions);
	}

	friend FCbWriter& ::operator<<(FCbWriter& Writer, const UE::Cook::FCookByTheBookOptions& Value);
	friend bool ::LoadFromCompactBinary(FCbFieldView Field, UE::Cook::FCookByTheBookOptions& Value);
};

//////////////////////////////////////////////////////////////////////////
// Cook on the fly startup options
struct FCookOnTheFlyOptions
{
	/** Wether the network file server or the I/O store connection server should bind to any port */
	bool bBindAnyPort = false;
	/** Whether the network file server should use a platform-specific communication protocol instead of TCP (used when bZenStore == false) */
	bool bPlatformProtocol = false;

	friend FCbWriter& ::operator<<(FCbWriter& Writer, const UE::Cook::FCookOnTheFlyOptions& Value);
	friend bool ::LoadFromCompactBinary(FCbFieldView Field, UE::Cook::FCookOnTheFlyOptions& Value);
};

}


/** Helper struct for FBeginCookContext; holds the context data for each platform being cooked */
struct FBeginCookContextPlatform
{
	ITargetPlatform* TargetPlatform = nullptr;
	UE::Cook::FPlatformData* PlatformData = nullptr;
	TMap<FName, FString> CurrentCookSettings;

	/** If true, we are deleting all old results from disk and rebuilding every package. If false, we are building iteratively. */
	bool bFullBuild = false;
	/** If true, a cook has already been run in the current process and we still have results from it. */
	bool bHasMemoryResults = false;
	/** If true, we should delete the in-memory results from an earlier cook in the same process, if we have any. */
	bool bClearMemoryResults = false;
	/** If true, we should load results that previous cooks left on disk into the current cook's results; this is one way to cook iteratively. */
	bool bPopulateMemoryResultsFromDiskResults = false;
	/** If true we are cooking iteratively, from results in a shared build (e.g. from buildfarm) rather than from our previous cook. */
	bool bIterateSharedBuild = false;
	/** If true we are a CookWorker, and we are working on a Sandbox directory that has already been populated by a remote Director process. */
	bool bWorkerOnSharedSandbox = false;
};
FCbWriter& operator<<(FCbWriter& Writer, const FBeginCookContextPlatform& Value);
bool LoadFromCompactBinary(FCbFieldView Field, FBeginCookContextPlatform& Value);

/** Data held on the stack and shared with multiple subfunctions when running StartCookByTheBook or StartCookOnTheFly */
struct FBeginCookContext
{
	FBeginCookContext(UCookOnTheFlyServer& InCOTFS)
		: COTFS(InCOTFS)
	{
	}

	const UCookOnTheFlyServer::FCookByTheBookStartupOptions* StartupOptions = nullptr;
	/** List of the platforms we are building, with startup context data about each one */
	TArray<FBeginCookContextPlatform> PlatformContexts;
	/** The list of platforms by themselves, for passing to functions that need just a list of platforms */
	TArray<ITargetPlatform*> TargetPlatforms;
	const UCookOnTheFlyServer& COTFS;
};

/** Helper struct for FBeginCookContextForWorker; holds the context data for each platform being cooked */
struct FBeginCookContextForWorkerPlatform
{
	void Set(const FBeginCookContextPlatform& InContext);

	const ITargetPlatform* TargetPlatform = nullptr;
	/** If true, we are deleting all old results from disk and rebuilding every package. If false, we are building iteratively. */
	bool bFullBuild = false;
};
FCbWriter& operator<<(FCbWriter& Writer, const FBeginCookContextForWorkerPlatform& Value);
bool LoadFromCompactBinary(FCbFieldView Field, FBeginCookContextForWorkerPlatform& Value);

/** Data from the director's FBeginCookContext that needs to be copied to workers. */
struct FBeginCookContextForWorker
{
	void Set(const FBeginCookContext& InContext);

	/** List of the platforms we are building, with startup context data about each one */
	TArray<FBeginCookContextForWorkerPlatform> PlatformContexts;
};
FCbWriter& operator<<(FCbWriter& Writer, const FBeginCookContextForWorker& Value);
bool LoadFromCompactBinary(FCbFieldView Field, FBeginCookContextForWorker& Value);

void LogCookerMessage(const FString& MessageText, EMessageSeverity::Type Severity);
LLM_DECLARE_TAG(Cooker);

constexpr uint32 ExpectedMaxNumPlatforms = 32;
#define REMAPPED_PLUGINS TEXT("RemappedPlugins")
extern float GCookProgressWarnBusyTime;

constexpr float TickCookableObjectsFrameTime = .100f;