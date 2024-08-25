// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetRegistry/PackageReader.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "Cooker/CookProfiling.h"
#include "Cooker/MPCollector.h"
#include "CookOnTheSide/CookOnTheFlyServer.h" // ECookTickFlags
#include "DerivedDataRequestOwner.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/PlatformMath.h"
#include "HAL/Platform.h"
#include "Logging/TokenizedMessage.h"
#include "Misc/AssertionMacros.h"
#include "Serialization/PackageWriter.h"
#include "Templates/Function.h"
#include "UObject/CookEnums.h"
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
namespace UE::Cook { struct FInstigator; }

FCbWriter& operator<<(FCbWriter& Writer, const UE::Cook::FBeginCookConfigSettings& Value);
bool LoadFromCompactBinary(FCbFieldView Field, UE::Cook::FBeginCookConfigSettings& OutValue);
FCbWriter& operator<<(FCbWriter& Writer, const UE::Cook::FCookByTheBookOptions& Value);
bool LoadFromCompactBinary(FCbFieldView Field, UE::Cook::FCookByTheBookOptions& OutValue);
FCbWriter& operator<<(FCbWriter& Writer, const UE::Cook::FCookOnTheFlyOptions& Value);
bool LoadFromCompactBinary(FCbFieldView Field, UE::Cook::FCookOnTheFlyOptions& OutValue);
FCbWriter& operator<<(FCbWriter& Writer, const UE::Cook::FInitializeConfigSettings& Value);
bool LoadFromCompactBinary(FCbFieldView Field, UE::Cook::FInitializeConfigSettings& OutValue);

#define COOK_CHECKSLOW_PACKAGEDATA 0
#define DEBUG_COOKONTHEFLY 0

LLM_DECLARE_TAG(Cooker_CachedPlatformData);

inline constexpr uint32 ExpectedMaxNumPlatforms = 32;

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

	/** Return type for functions called reentrantly that can succeed,fail,or be incomplete */
	enum class EPollStatus : uint8
	{
		Success,
		Error,
		Incomplete,
	};

	/** The reasons that a FPackageData can change its state, used for diagnostics and some control flow. */
	enum class EStateChangeReason : uint8
	{
		Completed,
		DoneForNow,
		SaveError,
		RecreateObjectCache,
		CookerShutdown,
		ReassignAbortedPackages,
		Retraction,
		Discovered,
		Requested,
		RequestCluster,
		DirectorRequest,
		Loaded,
		Saved,
		CookSuppressed,
		GarbageCollected,
		GeneratorPreGarbageCollected,
		ForceRecook,
		UrgencyUpdated,
	};
	const TCHAR* LexToString(UE::Cook::EStateChangeReason Reason);

	enum class ESuppressCookReason : uint8
	{
		Invalid, // Used by containers for values not in container, not used in cases for passing between containers
		NotSuppressed,
		AlreadyCooked,
		NeverCook,
		DoesNotExistInWorkspaceDomain,
		ScriptPackage,
		NotInCurrentPlugin,
		Redirected,
		OrphanedGenerated,
		LoadError,
		ValidationError,
		SaveError,
		OnlyEditorOnly,
		CookCanceled,
		MultiprocessAssignmentError,
		RetractedByCookDirector,
		CookFilter,
	};
	const TCHAR* LexToString(UE::Cook::ESuppressCookReason Reason);

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

		float GetTickTimeSlice() const;
		double GetTickEndTimeSeconds() const;
		bool IsTickTimeUp() const;
		bool IsTickTimeUp(double CurrentTimeSeconds) const;
		double GetTickTimeRemain() const;
		double GetTickTimeTillNow() const;

		float GetActionTimeSlice() const;
		void SetActionTimeSlice(float InTimeSlice);
		void SetActionStartTime();
		void SetActionStartTime(double CurrentTimeSeconds);
		double GetActionEndTimeSeconds() const;
		bool IsActionTimeUp() const;
		bool IsActionTimeUp(double CurrentTimeSeconds) const;
		double GetActionTimeRemain() const;
		double GetActionTimeTillNow() const;

	public:
		const double TickStartTime;
		double ActionStartTime;
		const float TickTimeSlice;
		float ActionTimeSlice;
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
			ICookedPackageWriter* InPackageWriter, FStringView InWriterDebugName, FSavePackageSettings InSettings);
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
		FGenericPlatformMemoryStats::EMemoryPressureStatus MemoryTriggerGCAtPressureLevel;
		int32 MinFreeUObjectIndicesBeforeGC;
		int32 MaxNumPackagesBeforePartialGC;
		int32 SoftGCStartNumerator;
		int32 SoftGCDenominator;
		TArray<FString> ConfigSettingDenyList;
		TMap<FName, int32> MaxAsyncCacheForType; // max number of objects of a specific type which are allowed to async cache at once
		bool bUseSoftGC = false;

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
		bool bHybridIterativeAllowAllClasses = false;
		TArray<FName> NeverCookPackageList;
		TFastPointerMap<const ITargetPlatform*, TSet<FName>> PlatformSpecificNeverCookPackages;

		friend FCbWriter& ::operator<<(FCbWriter& Writer, const UE::Cook::FBeginCookConfigSettings& Value);
		friend bool ::LoadFromCompactBinary(FCbFieldView Field, UE::Cook::FBeginCookConfigSettings& Value);
	};

	/** Report whether commandline/config has disabled use of timeouts throughout the cooker, useful for debugging. */
	bool IsCookIgnoreTimeouts();
}

bool LexTryParseString(FPlatformMemoryStats::EMemoryPressureStatus& OutValue, FStringView Text);
FString LexToString(FPlatformMemoryStats::EMemoryPressureStatus Value);

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

/** Enum used by FDiscoveredPlatformSet to specify what source it will use for the set of platforms. */
enum class EDiscoveredPlatformSet
{
	EmbeddedList,
	EmbeddedBitField,
	CopyFromInstigator,
	Count,
};

/**
 * A provider of a set of platforms to mark reachable for a discovered package. It might be an embedded list or
 * it might hold instructions for where to get the platforms from other context data.
 */
struct FDiscoveredPlatformSet
{
	FDiscoveredPlatformSet(EDiscoveredPlatformSet InSource = EDiscoveredPlatformSet::EmbeddedList);
	explicit FDiscoveredPlatformSet(TConstArrayView<const ITargetPlatform*> InPlatforms);
	explicit FDiscoveredPlatformSet(const TBitArray<>& InOrderedPlatformBits);
	~FDiscoveredPlatformSet();
	FDiscoveredPlatformSet(const FDiscoveredPlatformSet& Other);
	FDiscoveredPlatformSet(FDiscoveredPlatformSet&& Other);
	FDiscoveredPlatformSet& operator=(const FDiscoveredPlatformSet& Other);
	FDiscoveredPlatformSet& operator=(FDiscoveredPlatformSet&& Other);

	EDiscoveredPlatformSet GetSource() const { return Source; }

	void RemapTargetPlatforms(const TMap<ITargetPlatform*, ITargetPlatform*>& Remap);
	void OnRemoveSessionPlatform(const ITargetPlatform* Platform, int32 RemovedIndex);
	void OnPlatformAddedToSession(const ITargetPlatform* Platform);
	TConstArrayView<const ITargetPlatform*> GetPlatforms(UCookOnTheFlyServer& COTFS,
		FInstigator* Instigator, TConstArrayView<const ITargetPlatform*> OrderedPlatforms,
		TArray<const ITargetPlatform*, TInlineAllocator<ExpectedMaxNumPlatforms>>* OutBuffer);
	/** If the current type is EmbeddedBitField, change it to EmbeddedList. */
	void ConvertFromBitfield(TConstArrayView<const ITargetPlatform*> OrderedPlatforms);
	/** If the current type is EmbeddedList, change it to EmbeddedBitfield. Asserts if the type is already EmbeddedBitfield. */
	void ConvertToBitfield(TConstArrayView<const ITargetPlatform*> OrderedPlatforms);

private:
	void DestructUnion();
	void ConstructUnion();
	friend void WriteToCompactBinary(FCbWriter& Writer, const FDiscoveredPlatformSet& Value,
		TConstArrayView<const ITargetPlatform*> OrderedSessionPlatforms);
	friend bool LoadFromCompactBinary(FCbFieldView Field, FDiscoveredPlatformSet& OutValue,
		TConstArrayView<const ITargetPlatform*> OrderedSessionPlatforms);

	union 
	{
		TArray<const ITargetPlatform*> Platforms;
		TBitArray<> OrderedPlatformBits;
	};
	EDiscoveredPlatformSet Source;
};

}


/** Helper struct for FBeginCookContext; holds the context data for each platform being cooked */
struct FBeginCookContextPlatform
{
	ITargetPlatform* TargetPlatform = nullptr;
	UE::Cook::FPlatformData* PlatformData = nullptr;
	TMap<FName, FString> CurrentCookSettings;

	/** If true, we are deleting all old results from the previous cook. If false, we are keeping the old results. */
	bool bFullBuild = false;
	/**
	 * If true, we will use results from the previous cook for the new cook, if present. If false, we will recook.
	 * -diffonly is the expected case where bFullBuild=false but bAllowIterativeResults=false.
	 */
	bool bAllowIterativeResults = true;
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

#define REMAPPED_PLUGINS TEXTVIEW("RemappedPlugins")
extern float GCookProgressWarnBusyTime;

inline constexpr float TickCookableObjectsFrameTime = .100f;

/**
 * Scoped struct to run a function when leaving the scope. The same purpose as ON_SCOPE_EXIT, 
 * but it can also be triggered early.
 */
struct FOnScopeExit
{
public:
	explicit FOnScopeExit(TUniqueFunction<void()>&& InExitFunction)
		:ExitFunction(MoveTemp(InExitFunction))
	{
	}
	~FOnScopeExit()
	{
		ExitEarly();
	}
	void ExitEarly()
	{
		if (ExitFunction)
		{
			ExitFunction();
			ExitFunction.Reset();
		}
	}
	void Abandon()
	{
		ExitFunction.Reset();
	}
private:
	TUniqueFunction<void()> ExitFunction;
};
/**
 * The linker results for a single realm of a package save
 * (e.g. the main package or the optional package that extends the main package for optionally packaged data)
 */
struct FPackageReaderResults
{
	TMap<FSoftObjectPath, FPackageReader::FObjectData> Exports;
	TMap<FSoftObjectPath, FPackageReader::FObjectData> Imports;
	TMap<FName, bool> SoftPackageReferences;
	bool bValid = false;
};

/** The linker results of saving a package. A SavePackage can have multiple outputs (for e.g. optional realm). */
struct FMultiPackageReaderResults
{
	FPackageReaderResults Realms[2];
	ESavePackageResult Result;
};

/** Save the package and read the LinkerTables of its saved data. */
FMultiPackageReaderResults GetSaveExportsAndImports(UPackage* Package, UObject* Asset, FSavePackageArgs SaveArgs);
