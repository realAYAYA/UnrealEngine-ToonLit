// Copyright Epic Games, Inc. All Rights Reserved.

/**
*
* A lightweight multi-threaded profiler with very low instrumentation overhead. Suitable for Test or even final Shipping builds
* Results are accumulated per-frame and emitted in CSV format
*/

#include "ProfilingDebugging/CsvProfiler.h"
#include "Async/TaskGraphInterfaces.h"
#include "CoreGlobals.h"
#include "HAL/RunnableThread.h"
#include "HAL/ThreadManager.h"
#include "HAL/ThreadHeartBeat.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/ScopeLock.h"
#include "Misc/CoreMisc.h"
#include "Containers/Map.h"
#include "Misc/CoreDelegates.h"
#include "Misc/App.h"
#include "HAL/Runnable.h"
#include "Misc/EngineVersion.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Stats/Stats.h"
#include "Stats/Stats2.h"
#include "HAL/LowLevelMemTracker.h"
#include "Misc/Compression.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Fork.h"
#include "Misc/Guid.h"
#include "Misc/WildcardString.h"
#include "Modules/ModuleManager.h"

#include "HAL/PlatformMisc.h"

#if CSV_PROFILER

#define CSV_PROFILER_INLINE FORCEINLINE

#ifndef CSV_PROFILER_SUPPORT_NAMED_EVENTS
// This doesn't actually enable named events. Use -csvNamedEvents or -csvNamedEventsTiming to enable for exclusive or normal timing stats respectively
#define CSV_PROFILER_SUPPORT_NAMED_EVENTS ENABLE_NAMED_EVENTS
#endif

#define REPAIR_MARKER_STACKS 1
#define CSV_THREAD_HIGH_PRI 0

// Global CSV category (no prefix)
FCsvCategory GGlobalCsvCategory(TEXT("GLOBAL"), true, true);

// Basic high level perf category
CSV_DEFINE_CATEGORY_MODULE(CORE_API, Basic, true);
CSV_DEFINE_CATEGORY_MODULE(CORE_API, Exclusive, true);
CSV_DEFINE_CATEGORY_MODULE(CORE_API, FileIO, true);

// Other categories
CSV_DEFINE_CATEGORY(CsvProfiler, true);

#if CSV_PROFILER_ALLOW_DEBUG_FEATURES
	CSV_DEFINE_CATEGORY(CsvTest, true);
	static bool GCsvTestingGT = false;
	static bool GCsvTestingRT = false;

	void CSVTest();
#endif

CSV_DEFINE_STAT_GLOBAL(FrameTime);

#define RECORD_TIMESTAMPS 1 

#define LIST_VALIDATION (DO_CHECK && 0)

DEFINE_LOG_CATEGORY_STATIC(LogCsvProfiler, Log, All);

const char * GDefaultWaitStatName = "EventWait";
const char * GIgnoreWaitStatName =  "[IGNORE]";

TAutoConsoleVariable<int32> CVarCsvBlockOnCaptureEnd(
	TEXT("csv.BlockOnCaptureEnd"), 
	1,
	TEXT("When 1, blocks the game thread until the CSV file has been written completely when the capture is ended.\r\n")
	TEXT("When 0, the game thread is not blocked whilst the file is written."),
	ECVF_Default
);

TAutoConsoleVariable<int32> CVarCsvContinuousWrites(
	TEXT("csv.ContinuousWrites"),
	1,
	TEXT("When 1, completed CSV rows are converted to CSV format strings and appended to the write buffer whilst the capture is in progress.\r\n")
	TEXT("When 0, CSV rows are accumulated in memory as binary data, and only converted to strings and flushed to disk at the end of the capture."),
	ECVF_Default
);

TAutoConsoleVariable<int32> CVarCsvForceExit(
	TEXT("csv.ForceExit"),
	0,
	TEXT("If 1, do a forced exit when if exitOnCompletion is enabled"),
	ECVF_Default
);

TAutoConsoleVariable<int32> CVarCsvTargetFrameRateOverride(
	TEXT("csv.TargetFrameRateOverride"),
	0,
	TEXT("If 0, Defaults to calculating the target frame rate using rhi.SyncInterval and Max refresh rate."),
	ECVF_Default
);

#if UE_BUILD_SHIPPING
TAutoConsoleVariable<int32> CVarCsvShippingContinuousWrites(
	TEXT("csv.Shipping.ContinuousWrites"),
	-1,
	TEXT("Only applies in shipping buids. If set, overrides csv.ContinousWrites."),
	ECVF_Default
);
#endif

TAutoConsoleVariable<int32> CVarCsvCompressionMode(
	TEXT("csv.CompressionMode"),
	-1,
	TEXT("Controls whether CSV files are compressed when written out.\r\n")
	TEXT(" -1 = (Default) Use compression if the code which started the capture opted for it.\r\n")
	TEXT("  0 = Force disable compression. All files will be written as uncompressed .csv files.\r\n")
	TEXT("  1 = Force enable compression. All files will be written as compressed .csv.gz files."),
	ECVF_Default
);

TAutoConsoleVariable<int32> CVarCsvStatCounts(
	TEXT("csv.statCounts"),
	0,
	TEXT("If 1, outputs count stats"),
	ECVF_Default
);

TAutoConsoleVariable<int32> CVarCsvWriteBufferSize(
	TEXT("csv.WriteBufferSize"),
	128 * 1024, // 128 KB
	TEXT("When non-zero, defines the size of the write buffer to use whilst writing the CSV file.\r\n")
	TEXT("A non-zero value is required for GZip compressed output."),
	ECVF_Default
);

TAutoConsoleVariable<int32> CVarCsvStreamFramesToBuffer(
	TEXT("csv.FramesToBuffer"),
	128,
	TEXT("Defines the minimum amount of frames to keep in memory before flushing them."),
	ECVF_Default
);

TAutoConsoleVariable<int32> CVarCsvPauseProcessingThread(
	TEXT("csv.PauseProcessingThread"),
	0,
	TEXT("Debug only - When 1, blocks the processing thread to simulate starvation"),
	ECVF_Default
);

TAutoConsoleVariable<int32> CVarMaxPerThreadStatDataSlackKB(
	TEXT("csv.MaxPerThreadStatDataSlackKB"),
	64,
	TEXT("Max amount of per thread slack data to allow during a capture.\r\n")
	TEXT("Higher values result in better performance due to fewer allocations but higher memory overhead"),
	ECVF_Default
);

static bool GCsvUseProcessingThread = true;
static int32 GCsvRepeatCount = 0;
static int32 GCsvRepeatFrameCount = 0;
static bool GCsvStatCounts = false;
static FString* GStartOnEvent = nullptr;
static FString* GStopOnEvent = nullptr;
static uint32 GCsvProcessingThreadId = 0;
static bool GGameThreadIsCsvProcessingThread = true;

static uint32 GCsvProfilerFrameNumber = 0;

static bool GCsvTrackWaitsOnAllThreads = false;
static bool GCsvTrackWaitsOnGameThread = true;
static bool GCsvTrackWaitsOnRenderThread = true;

static FAutoConsoleVariableRef CVarTrackWaitsAllThreads(TEXT("csv.trackWaitsAllThreads"), GCsvTrackWaitsOnAllThreads, TEXT("Determines whether to track waits on all threads. Note that this incurs a lot of overhead"), ECVF_Default);
static FAutoConsoleVariableRef CVarTrackWaitsGT(TEXT("csv.trackWaitsGT"), GCsvTrackWaitsOnGameThread, TEXT("Determines whether to track game thread waits. Note that this incurs overhead"), ECVF_Default);
static FAutoConsoleVariableRef CVarTrackWaitsRT(TEXT("csv.trackWaitsRT"), GCsvTrackWaitsOnRenderThread, TEXT("Determines whether to track render thread waits. Note that this incurs overhead"), ECVF_Default);

//
// Categories
//
static const uint32 CSV_MAX_CATEGORY_COUNT = 2048;
static bool GCsvCategoriesEnabled[CSV_MAX_CATEGORY_COUNT];

static bool GCsvProfilerIsCapturing = false;
static bool GCsvProfilerIsCapturingRT = false; // Renderthread version of the above

static bool GCsvProfilerIsWritingFile = false;
static FString GCsvFileName = FString();
static bool GCsvExitOnCompletion = false;

static thread_local bool GCsvThreadLocalWaitsEnabled = false;


// A unique ID for a CSV stat, either ansi or FName
union FCsvUniqueStatID
{
public:
	FCsvUniqueStatID(const FCsvUniqueStatID& Src)
	{
		Hash = Src.Hash;
	}
	FCsvUniqueStatID(uint64 InStatIDRaw, int32 InCategoryIndex, bool bInIsFName, bool bInIsCountStat = false)
	{
		check(InCategoryIndex < CSV_MAX_CATEGORY_COUNT);
		Fields.IsFName = bInIsFName ? 1 : 0;
		Fields.FNameOrIndex = InStatIDRaw;
		Fields.CategoryIndex = InCategoryIndex;
		Fields.IsCountStat = bInIsCountStat ? 1 : 0;
	}
	FCsvUniqueStatID(const FName& Name, int32 InCategoryIndex)
	{
		check(InCategoryIndex < CSV_MAX_CATEGORY_COUNT);
		Fields.FNameOrIndex = Name.ToUnstableInt();
		Fields.CategoryIndex = InCategoryIndex;
		Fields.IsFName = 1;
		Fields.IsCountStat = 0;
	}
	struct
	{
		uint64 IsFName : 1;
		uint64 IsCountStat : 1;
		uint64 CategoryIndex : 11;
		uint64 FNameOrIndex : 51;
	} Fields;
	uint64 Hash;
};


// Persistent custom stats
struct FCsvPersistentCustomStats
{
	void RecordStats()
	{
		FScopeLock Lock(&Cs);
		for (FCsvPersistentCustomStatBase* BaseStat : Stats)
		{
			switch(BaseStat->GetStatType())
			{
				case ECsvPersistentCustomStatType::Float:
				{
					RecordStat<float>(BaseStat);
					break;
				}
				case ECsvPersistentCustomStatType::Int:
				{
					RecordStat<int32>(BaseStat);
					break;
				}
			}
		}
	}


	template<class T>
	TCsvPersistentCustomStat<T>* GetOrCreatePersistentCustomStat(FName Name, int32 CategoryIndex, bool bResetEachFrame)
	{
		LLM_SCOPE(ELLMTag::CsvProfiler);
		FScopeLock Lock(&Cs);
		FCsvUniqueStatID Id(Name, CategoryIndex);
		FCsvPersistentCustomStatBase** FindStat = StatLookup.Find(Id.Hash);
		if (FindStat)
		{
			if (TCsvPersistentCustomStat<T>::GetClassStatType() == (*FindStat)->GetStatType())
			{
				return static_cast<TCsvPersistentCustomStat<T>*>(*FindStat);
			}
			UE_LOG(LogCsvProfiler, Fatal, TEXT("Error: Custom stat %s was already registered with a different type"), *Name.ToString());
		}
		// This will leak, and that's ok. These stats are intended to persist for the lifetime of the program
		TCsvPersistentCustomStat<T>* NewStat = new TCsvPersistentCustomStat<T>(Name, CategoryIndex, bResetEachFrame);
		StatLookup.Add(Id.Hash, NewStat);
		Stats.Add(NewStat);
		return NewStat;
	}

	template <class T>
	void RecordStat(FCsvPersistentCustomStatBase* BaseStat)
	{
		TCsvPersistentCustomStat<T>* Stat = static_cast<TCsvPersistentCustomStat<T>*>(BaseStat);
		FCsvProfiler::RecordCustomStat(Stat->Name, Stat->CategoryIndex, Stat->GetValue(), ECsvCustomStatOp::Set);
		if (Stat->bResetEachFrame)
		{
			Stat->Set(0);
		}
	}

	FCriticalSection Cs;
	TMap<uint64, FCsvPersistentCustomStatBase*> StatLookup;
	TArray<FCsvPersistentCustomStatBase*> Stats;
};
static FCsvPersistentCustomStats GCsvPersistentCustomStats;



#if CSV_PROFILER_SUPPORT_NAMED_EVENTS
bool GCsvProfilerNamedEventsExclusive = false;
bool GCsvProfilerNamedEventsTiming = false;

static FAutoConsoleVariableRef CVarNamedEventsExclusive(TEXT("csv.NamedEventsExclusive"), GCsvProfilerNamedEventsExclusive, TEXT("Determines whether to emit named events for exclusive stats"), ECVF_Default);
static FAutoConsoleVariableRef CVarNamedEventsTiming(TEXT("csv.NamedEventsTiming"), GCsvProfilerNamedEventsTiming, TEXT("Determines whether to emit named events for non-exclusive timing stats"), ECVF_Default);

void CsvBeginNamedEvent(FColor Color, const char* NamedEventName)
{
#if CPUPROFILERTRACE_ENABLED
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(CpuChannel))
	{
		FCpuProfilerTrace::OutputBeginDynamicEvent(NamedEventName, __FILE__, __LINE__);
	}
	else
#endif
	{
#if PLATFORM_IMPLEMENTS_BeginNamedEventStatic
		FPlatformMisc::BeginNamedEventStatic(Color, NamedEventName);
#else
		FPlatformMisc::BeginNamedEvent(Color, NamedEventName);
#endif
	}
}

void CsvBeginNamedEvent(FColor Color, const FName& StatName)
{
#if CPUPROFILERTRACE_ENABLED
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(CpuChannel))
	{
		FCpuProfilerTrace::OutputBeginDynamicEvent(StatName, __FILE__, __LINE__);
	}
	else
#endif
	{
#if PLATFORM_IMPLEMENTS_BeginNamedEventStatic
		FPlatformMisc::BeginNamedEventStatic(Color, *StatName.ToString());
#else
		FPlatformMisc::BeginNamedEvent(Color, *StatName.ToString());
#endif
	}
}

void CsvEndNamedEvent()
{
#if CPUPROFILERTRACE_ENABLED
	if (UE_TRACE_CHANNELEXPR_IS_ENABLED(CpuChannel))
	{
		FCpuProfilerTrace::OutputEndEvent();
	}
	else
#endif
	{
		FPlatformMisc::EndNamedEvent();
	}
}
#endif //CSV_PROFILER_SUPPORT_NAMED_EVENTS


static TMap<uint32, TArray<FString>>* GCsvFrameExecCmds = NULL;

bool IsContinuousWriteEnabled(bool bGameThread)
{
	int CVarValue = -1;
#if UE_BUILD_SHIPPING
	CVarValue = bGameThread ? CVarCsvShippingContinuousWrites.GetValueOnGameThread() : CVarCsvShippingContinuousWrites.GetValueOnAnyThread();
	if (CVarValue == -1)
#endif
	{
		CVarValue = bGameThread ? CVarCsvContinuousWrites.GetValueOnGameThread() : CVarCsvContinuousWrites.GetValueOnAnyThread();
	}
	return CVarValue > 0;
}

#if CSV_PROFILER_ALLOW_DEBUG_FEATURES
class FCsvABTest
{
public:
	FCsvABTest()
		: StatFrameOffset(0)
		, SwitchDuration(7)
		, bPrevCapturing(false)
		, bFastCVarSet(false)
	{}

	void AddCVarABData(const FString& CVarName, int32 Count)
	{
		Count = CVarValues.Num() - Count;
		IConsoleVariable* ConsoleVariable = IConsoleManager::Get().FindConsoleVariable(*CVarName);

		if (Count > 0 && ConsoleVariable != nullptr)
		{
			CVarABDataArray.Add({ CVarName, *CVarName, ConsoleVariable, ConsoleVariable->GetString(), Count, FLT_MAX });
		}
		else if (ConsoleVariable == nullptr)
		{
			UE_LOG(LogCsvProfiler, Log, TEXT("Skipping CVar %s - Not found"), *CVarName);
		}
		else if (Count == 0)
		{
			UE_LOG(LogCsvProfiler, Log, TEXT("Skipping CVar %s - No value specified"), *CVarName);
		}
	}

	void IterateABTestArguments(const FString& ABTestString)
	{
		int32 FindIndex;
		if (!ABTestString.FindChar(TEXT('='), FindIndex))
		{
			return;
		}

		int32 Count = CVarValues.Num();

		FString CVarName = ABTestString.Mid(0, FindIndex);
		FString ValueStr = ABTestString.Mid(FindIndex + 1);
		while (true)
		{
			int32 CommaIndex;
			bool bComma = ValueStr.FindChar(TEXT(','), CommaIndex);
			int32 SemiColonIndex;
			bool bSemiColon = ValueStr.FindChar(TEXT(';'), SemiColonIndex);

			if (bComma)
			{
				if (!bSemiColon || (bSemiColon && CommaIndex<SemiColonIndex))
				{
					FString Val = ValueStr.Mid(0, CommaIndex);
					CVarValues.Add(FCString::Atof(*Val));
					ValueStr.MidInline(CommaIndex + 1, MAX_int32, EAllowShrinking::No);
					continue;
				}
			}

			if (bSemiColon)
			{
				if (SemiColonIndex==0)
				{
					AddCVarABData(CVarName, Count);
					IterateABTestArguments(ValueStr.Mid(SemiColonIndex + 1));
					break;
				}
				else
				{
					FString Val = ValueStr.Mid(0, SemiColonIndex);
					CVarValues.Add(FCString::Atof(*Val));
					ValueStr.MidInline(SemiColonIndex, MAX_int32, EAllowShrinking::No);
					continue;
				}
			}

			CVarValues.Add(FCString::Atof(*ValueStr));
			AddCVarABData(CVarName, Count);
			break;
		}

	}

	void InitFromCommandline()
	{
		FString ABTestString;
		if (FParse::Value(FCommandLine::Get(), TEXT("csvABTest="), ABTestString, false))
		{
			IterateABTestArguments(ABTestString);

			if (CVarABDataArray.Num() > 0)
			{
				UE_LOG(LogCsvProfiler, Log, TEXT("Initialized CSV Profiler A/B test")); 
				
				int32 CVarValuesIndex = 0;
				for (int32 Index = 0; Index < CVarABDataArray.Num(); ++Index)
				{
					const FCVarABData& CVarABData = CVarABDataArray[Index];

					UE_LOG(LogCsvProfiler, Log, TEXT("  CVar %s [Original value: %s] AB Test with values:"), *CVarABData.CVarName, *CVarABData.OriginalValue);
					for (int32 i = 0; i < CVarABData.Count; ++i)
					{
						UE_LOG(LogCsvProfiler, Log, TEXT("    [%d] : %.2f"), i, CVarValues[CVarValuesIndex + i]);
					}

					CVarValuesIndex += CVarABData.Count;
				}

				FParse::Value(FCommandLine::Get(), TEXT("csvABTestStatFrameOffset="), StatFrameOffset);
				FParse::Value(FCommandLine::Get(), TEXT("csvABTestSwitchDuration="), SwitchDuration);
				bFastCVarSet = FParse::Param(FCommandLine::Get(), TEXT("csvABTestFastCVarSet"));
				UE_LOG(LogCsvProfiler, Log, TEXT("Stat Offset: %d frames"), StatFrameOffset);
				UE_LOG(LogCsvProfiler, Log, TEXT("Switch Duration : %d frames"), SwitchDuration);
				UE_LOG(LogCsvProfiler, Log, TEXT("Fast cvar set: %s"), bFastCVarSet ? TEXT("Enabled") : TEXT("Disabled"));

			}
			else
			{
				UE_LOG(LogCsvProfiler, Log, TEXT("CSV Profiler A/B has not initialized"));
			}
		}
	}

	void BeginFrameUpdate(int32 FrameNumber, bool bCapturing)
	{
		if (CVarABDataArray.Num() == 0)
		{
			return;
		}
		
		if (bCapturing)
		{
			int32 CVarValuesIndex = 0;
			for (int32 Index = 0; Index < CVarABDataArray.Num(); ++Index)
			{
				FCVarABData& CVarABData = CVarABDataArray[Index];

				int32 ValueIndex = (FrameNumber / SwitchDuration) % CVarABData.Count;
				int32 StatValueIndex = ((FrameNumber - StatFrameOffset) / SwitchDuration) % CVarABData.Count;
				
				ValueIndex += CVarValuesIndex;
				StatValueIndex += CVarValuesIndex;
				CVarValuesIndex += CVarABData.Count;

				{
					float Value = CVarValues[ValueIndex];
					if (Value != CVarABData.PreviousValue)
					{
						EConsoleVariableFlags CVarFlags = ECVF_SetByCode;
						if (bFastCVarSet)
						{
							CVarFlags = EConsoleVariableFlags(CVarFlags | ECVF_Set_NoSinkCall_Unsafe);
						}
						CVarABData.ConsoleVariable->Set(*FString::Printf(TEXT("%f"), Value), CVarFlags);

						CVarABData.PreviousValue = Value;
					}
				}

				FCsvProfiler::RecordCustomStat(CVarABData.CVarStatFName, CSV_CATEGORY_INDEX_GLOBAL, CVarValues[StatValueIndex], ECsvCustomStatOp::Set);
			}
		}
		else if (bPrevCapturing == true)
		{
			// Restore cvar to old value
			// TODO: Set Setby flag to the original value
			for (int32 Index = 0; Index < CVarABDataArray.Num(); ++Index)
			{
				CVarABDataArray[Index].ConsoleVariable->Set(*CVarABDataArray[Index].OriginalValue);

				UE_LOG(LogCsvProfiler, Log, TEXT("CSV Profiler A/B test - setting %s=%s"), *CVarABDataArray[Index].CVarName, *CVarABDataArray[Index].OriginalValue);
			}

		}
		bPrevCapturing = bCapturing;
	}

private:
	struct FCVarABData
	{
		FString CVarName;
		FName CVarStatFName;
		IConsoleVariable* ConsoleVariable;
		FString OriginalValue;
		int32 Count;
		float PreviousValue;
	};
	
	TArray<FCVarABData> CVarABDataArray;
	TArray<float> CVarValues;

	int32 StatFrameOffset;
	int32 SwitchDuration;
	bool bPrevCapturing;
	bool bFastCVarSet;
};
static FCsvABTest GCsvABTest;

#endif // CSV_PROFILER_ALLOW_DEBUG_FEATURES

class FCsvCategoryData
{
public:
	static FCsvCategoryData* Get()
	{
		if (!Instance)
		{
			Instance = new FCsvCategoryData;
			FMemory::Memzero(GCsvCategoriesEnabled, sizeof(GCsvCategoriesEnabled));
		}
		return Instance;
	}

	FString GetCategoryNameByIndex(int32 Index) const
	{
		FScopeLock Lock(&CS);
		return CategoryNames[Index];
	}

	int32 GetCategoryCount() const
	{
		return CategoryNames.Num();
	}

	int32 GetCategoryIndex(const FString& CategoryName) const
	{
		FScopeLock Lock(&CS);
		const int32* CategoryIndex = CategoryNameToIndex.Find(CategoryName.ToLower());
		if (CategoryIndex)
		{
			return *CategoryIndex;
		}
		return -1;
	}

	void UpdateCategoryFromConfig(int32 CategoryIndex)
	{
		for (FString const& EnabledCategory : CategoriesEnabledInConfig)
		{
			if (FWildcardString::IsMatch(*EnabledCategory, *CategoryNames[CategoryIndex]))
			{
				UE_LOG(LogCsvProfiler, Log, TEXT("Config enabled category %s"), *CategoryNames[CategoryIndex]);
				GCsvCategoriesEnabled[CategoryIndex] = true;
			}
		}

		for (FString const& DisabledCategory : CategoriesDisabledInConfig)
		{
			if (FWildcardString::IsMatch(*DisabledCategory, *CategoryNames[CategoryIndex]))
			{
				UE_LOG(LogCsvProfiler, Log, TEXT("Config disabled category %s"), *CategoryNames[CategoryIndex]);
				GCsvCategoriesEnabled[CategoryIndex] = false;
			}
		}
	}

	void UpdateCategoriesFromConfig()
	{
		TArray<FString> NewCategoriesDisabledInConfig;
		TArray<FString> NewCategoriesEnabledInConfig;

		GConfig->GetArray(TEXT("CsvProfiler"), TEXT("EnabledCategories"), NewCategoriesEnabledInConfig, GEngineIni);
		GConfig->GetArray(TEXT("CsvProfiler"), TEXT("DisabledCategories"), NewCategoriesDisabledInConfig, GEngineIni);

		// Check if the config changed. This prevents us from resetting categories unnecessarily at runtime (which could be confusing if debug category toggle commands are in play)
		if (NewCategoriesEnabledInConfig != CategoriesEnabledInConfig || NewCategoriesDisabledInConfig != CategoriesDisabledInConfig )
		{
			CategoriesEnabledInConfig = NewCategoriesEnabledInConfig;
			CategoriesDisabledInConfig = NewCategoriesDisabledInConfig;
			for (int i = 0; i < GetCategoryCount(); ++i)
			{
				UpdateCategoryFromConfig(i);
			}
		}
	}

	int32 RegisterCategory(const FString& CategoryName, bool bEnableByDefault, bool bIsGlobal)
	{
		int32 Index = -1;
		// During a hot-reload, we attempt to re-register categories and/or statics
		// that not have been loaded/init'd during a hot-reload, can result in crashing.
		// Thus when doing a reload, bail. 
		if (IsReloadActive())
		{
			return Index;
		}

		FScopeLock Lock(&CS);
		{
			Index = GetCategoryIndex(CategoryName);
			checkf(Index == -1, TEXT("CSV stat category already declared: %s. Note: Categories are not case sensitive"), *CategoryName);
			if (Index == -1)
			{
				if (bIsGlobal)
				{
					Index = 0;
				}
				else
				{
					Index = CategoryNames.Num();
					CategoryNames.AddDefaulted();
				}
				check(Index < CSV_MAX_CATEGORY_COUNT);
				if (Index < CSV_MAX_CATEGORY_COUNT)
				{
					GCsvCategoriesEnabled[Index] = bEnableByDefault;
					CategoryNames[Index] = CategoryName;
					CategoryNameToIndex.Add(CategoryName.ToLower(), Index);
					UpdateCategoryFromConfig(Index);
				}
				TRACE_CSV_PROFILER_REGISTER_CATEGORY(Index, *CategoryName);
			}
		}
		return Index;
	}


private:
	FCsvCategoryData()
	{
		// Category 0 is reserved for the global category
		CategoryNames.AddDefaulted(1);
	}

	mutable FCriticalSection CS;
	TMap<FString, int32> CategoryNameToIndex;
	TArray<FString> CategoryNames;
	TArray<FString> CategoriesDisabledInConfig;
	TArray<FString> CategoriesEnabledInConfig;

	static FCsvCategoryData* Instance;
};
FCsvCategoryData* FCsvCategoryData::Instance = nullptr;


int32 FCsvProfiler::GetCategoryIndex(const FString& CategoryName)
{
	return FCsvCategoryData::Get()->GetCategoryIndex(CategoryName);
}

int32 FCsvProfiler::RegisterCategory(const FString& CategoryName, bool bEnableByDefault, bool bIsGlobal)
{
	return FCsvCategoryData::Get()->RegisterCategory(CategoryName, bEnableByDefault, bIsGlobal);
}

void FCsvProfiler::GetFrameExecCommands(TArray<FString>& OutFrameCommands) const
{
	check(IsInGameThread());
	OutFrameCommands.Empty();
	if (GCsvProfilerIsCapturing && GCsvFrameExecCmds)
	{
		TArray<FString>* FrameCommands = GCsvFrameExecCmds->Find(CaptureFrameNumber);
		if (FrameCommands)
		{
			OutFrameCommands = *FrameCommands;
		}
	}
}



bool IsInCsvProcessingThread()
{
	uint32 ProcessingThreadId = GGameThreadIsCsvProcessingThread ? GGameThreadId : GCsvProcessingThreadId;
	return FPlatformTLS::GetCurrentThreadId() == ProcessingThreadId;
}

static void HandleCSVProfileCommand(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		return;
	}

	FString Param = Args[0];

	if (Param == TEXT("START"))
	{
		FCsvProfiler::Get()->BeginCapture(-1, FString(), GCsvFileName);
	}
	else if (Param == TEXT("STOP"))
	{
		FCsvProfiler::Get()->EndCapture();
	}
	else if (FParse::Value(*Param, TEXT("STARTFILE="), GCsvFileName))
	{
	}
	else if (Param == TEXT("EXITONCOMPLETION"))
	{
		GCsvExitOnCompletion = true;
	}
	else
	{
		int32 CaptureFrames = 0;
		if (FParse::Value(*Param, TEXT("FRAMES="), CaptureFrames))
		{
			FCsvProfiler::Get()->BeginCapture(CaptureFrames, FString(), GCsvFileName);
		}
		int32 RepeatCount = 0;
		if (FParse::Value(*Param, TEXT("REPEAT="), RepeatCount))
		{
			GCsvRepeatCount = RepeatCount;
		}
	}
}

static void CsvProfilerBeginFrame()
{
	FCsvProfiler::Get()->BeginFrame();
}

static void CsvProfilerEndFrame()
{
	FCsvProfiler::Get()->EndFrame();
}

static void CsvProfilerBeginFrameRT()
{
	FCsvProfiler::Get()->BeginFrameRT();
}

static void CsvProfilerEndFrameRT()
{
	FCsvProfiler::Get()->EndFrameRT();
}

static void CsvProfilerReadConfig()
{
	FCsvCategoryData::Get()->UpdateCategoriesFromConfig();
}


static FAutoConsoleCommand HandleCSVProfileCmd(
	TEXT("CsvProfile"),
	TEXT("Starts or stops Csv Profiles"),
	FConsoleCommandWithArgsDelegate::CreateStatic(&HandleCSVProfileCommand)
);

static void HandleCSVCategoryCommand(const TArray<FString>& Args, UWorld* World, FOutputDevice& OutputDevice)
{
	if ((Args.Num() >= 1) && (Args.Num() <= 2))
	{
		const FCsvProfiler* CsvProfiler = FCsvProfiler::Get();
		const FString& Category = Args[0];
		const int32 CategoryIndex = CsvProfiler->GetCategoryIndex(Category);
		if (CategoryIndex < 0)
		{
			OutputDevice.Logf(ELogVerbosity::Error, TEXT("CsvProfiler: category '%s' does not exist."), *Category);
			return;
		}

		bool bEnabled = true;
		bool bIsOperationValid = true;
		if (Args.Num() == 2)
		{
			const FString& Operation = Args[1];
			if (Operation.Compare(TEXT("disable"), ESearchCase::IgnoreCase) == 0)
			{
				bEnabled = false;
			}
			else if (Operation.Compare(TEXT("enable"), ESearchCase::IgnoreCase) != 0)
			{
				bIsOperationValid  = false;
			}
		}
		else
		{
			// Toggle by default
			bEnabled = !CsvProfiler->IsCategoryEnabled(CategoryIndex);
		}
		if (bIsOperationValid)
		{
			CsvProfiler->EnableCategoryByIndex(CategoryIndex, bEnabled);
			OutputDevice.Logf(ELogVerbosity::Log, TEXT("CsvProfiler: category '%s' is now %s."), *Category, bEnabled ? TEXT("enabled") : TEXT("disabled"));
			return;
		}
	}
	
	// We fall into here if there was a usage error
	OutputDevice.Logf(ELogVerbosity::Error, TEXT("CsvProfiler: Usage: csvcategory <category> [enable/disable] (toggles if second parameter is omitted)"));
}

static FAutoConsoleCommandWithWorldArgsAndOutputDevice HandleCSVCategoryCmd(
	TEXT("CsvCategory"),
	TEXT("Changes whether a CSV category is included in captures."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(&HandleCSVCategoryCommand)
);

//-----------------------------------------------------------------------------
//	TSingleProducerSingleConsumerList : fast lock-free single producer/single 
//  consumer list implementation. 
//  Uses a linked list of blocks for allocations.
//-----------------------------------------------------------------------------
template <class T, int BlockSize>
class TSingleProducerSingleConsumerList
{
	// A block of BlockSize entries
	struct FBlock
	{
		FBlock() : Next(nullptr)
		{
		}
		T Entries[BlockSize];

#if LIST_VALIDATION
		int32 DebugIndices[BlockSize];
#endif
		FBlock* Next;
	};

public:
	TSingleProducerSingleConsumerList()
	{
		HeadBlock = nullptr;
		TailBlock = nullptr;
#if DO_GUARD_SLOW
		bElementReserved = false;
#endif
#if LIST_VALIDATION
		LastDebugIndex = -1;
#endif
		Counter = 0;
		ConsumerThreadReadIndex = 0;
		ConsumerThreadDeleteIndex = 0;
	}

	~TSingleProducerSingleConsumerList()
	{
		// Only safe to destruct when no other threads are using the list.

		// Delete all remaining blocks in the list
		while (HeadBlock)
		{
			FBlock* PrevBlock = HeadBlock;
			HeadBlock = HeadBlock->Next;
			delete PrevBlock;
		}

		HeadBlock = nullptr;
		TailBlock = nullptr;
	}

	// Reserve an element prior to writing it
	// Must be called from the Producer thread
	CSV_PROFILER_INLINE T* ReserveElement()
	{
#if DO_GUARD_SLOW
		checkSlow(!bElementReserved);
		bElementReserved = true;
#endif
		uint32 TailBlockSize = Counter % BlockSize;
		if (TailBlockSize == 0)
		{
			AddTailBlock();
		}
#if LIST_VALIDATION
		TailBlock->DebugIndices[Counter % BlockSize] = Counter;
#endif
		return &TailBlock->Entries[TailBlockSize];
	}

	// Commit an element after writing it
	// Must be called from the Producer thread after a call to ReserveElement
	CSV_PROFILER_INLINE void CommitElement()
	{
#if DO_GUARD_SLOW
		checkSlow(bElementReserved);
		bElementReserved = false;
#endif
		FPlatformMisc::MemoryBarrier();

		// Keep track of the count of all the elements we ever committed. This value is never reset, even on a PopAll
		Counter++;
	}

	// Called from the consumer thread
	bool HasNewData() const
	{
		volatile uint64 CurrentCounterValue = Counter;
		FPlatformMisc::MemoryBarrier();
		return CurrentCounterValue > ConsumerThreadReadIndex;
	}

	// Called from the consumer thread
	void PopAll(TArray<T>& ElementsOut, int64 MaxSlackMemBytes = -1 )
	{
		volatile uint64 CurrentCounterValue = Counter;
		FPlatformMisc::MemoryBarrier();

		uint32 MaxElementsToPop = uint32(CurrentCounterValue - ConsumerThreadReadIndex);

		// Shrink the output array if it has excessive slack (note: usually this will shrink to 0)
		int64 SlackMemBytes = ( (int64)ElementsOut.GetSlack() - (int64)MaxElementsToPop ) * (int64)sizeof(T);
		if ( MaxSlackMemBytes >= 0 && SlackMemBytes > MaxSlackMemBytes )
		{
			ElementsOut.Shrink();
		}

		// Presize the array capacity to avoid memory reallocation.
		ElementsOut.Reserve(ElementsOut.Num() + MaxElementsToPop);

		uint32 IndexInBlock = ConsumerThreadReadIndex % BlockSize;

		for (uint32 Index = 0; Index < MaxElementsToPop; ++Index)
		{
			// if this block is full and it's completed, delete it and move to the next block (update the head)
			if (ConsumerThreadReadIndex == (ConsumerThreadDeleteIndex + BlockSize))
			{
				// Both threads are done with the head block now, so we can safely delete it 
				// Note that the Producer thread only reads/writes to the HeadBlock pointer on startup, so it's safe to update it at this point
				// HeadBlock->Next is also safe to read, since the producer can't be writing to it if Counter has reached this block
				FBlock* PrevBlock = HeadBlock;
				HeadBlock = HeadBlock->Next;
				IndexInBlock = 0;
				delete PrevBlock;

				ConsumerThreadDeleteIndex = ConsumerThreadReadIndex;
			}
			check(HeadBlock != nullptr);
			check(IndexInBlock < BlockSize);

			T& Element = HeadBlock->Entries[IndexInBlock];

			// Move construct. Avoids mem allocations on FString members
			ElementsOut.Emplace(MoveTemp(Element));

#if LIST_VALIDATION
			int32 DebugIndex = HeadBlock->DebugIndices[IndexInBlock];
			ensure(DebugIndex == LastDebugIndex + 1);
			LastDebugIndex = DebugIndex;
#endif
			IndexInBlock++;
			ConsumerThreadReadIndex++;
		}
	}

	inline uint64 GetAllocatedSize() const
	{
		volatile uint64 CurrentCounterValue = Counter;
		FPlatformMisc::MemoryBarrier();

		// Use the delete index, so we count all blocks that haven't been deleted yet.
		uint64 NumElements = CurrentCounterValue - ConsumerThreadDeleteIndex;
		uint64 NumBlocks = FMath::DivideAndRoundUp(NumElements, (uint64)BlockSize);

		return NumBlocks * sizeof(FBlock);
	}

private:
	void AddTailBlock()
	{
		LLM_SCOPE(ELLMTag::CsvProfiler);
		FBlock* NewTail = new FBlock;
		if (TailBlock == nullptr)
		{
			// This must only happen on startup, otherwise it's not thread-safe
			checkSlow(Counter == 0);
			checkSlow(HeadBlock == nullptr);
			HeadBlock = NewTail;
		}
		else
		{
			TailBlock->Next = NewTail;
		}
		TailBlock = NewTail;
	}


	FBlock* HeadBlock;
	FBlock* TailBlock;

	volatile uint64 Counter;

	// Used from the consumer thread
	uint64 ConsumerThreadReadIndex;
	uint64 ConsumerThreadDeleteIndex;

#if DO_GUARD_SLOW
	bool bElementReserved;
#endif
#if LIST_VALIDATION
	int32 LastDebugIndex;
#endif

};

namespace ECsvTimeline
{
	enum Type
	{
		Gamethread,
		Renderthread,
		Count
	};
}

//-----------------------------------------------------------------------------
//	FFrameBoundaries : thread-safe class for managing thread boundary timestamps
//  These timestamps are written from the gamethread/renderthread, and consumed
//  by the CSVProfiling thread
//-----------------------------------------------------------------------------
class FFrameBoundaries
{
public:
	FFrameBoundaries() : CurrentReadFrameIndex(0)
	{}

	void Clear()
	{
		check(IsInCsvProcessingThread());
		Update();
		for (int i = 0; i < ECsvTimeline::Count; i++)
		{
			FrameBoundaryTimestamps[i].Empty();
		}
		CurrentReadFrameIndex = 0;
	}

	int32 GetFrameNumberForTimestamp(ECsvTimeline::Type Timeline, uint64 Timestamp) const
	{
		// If we have new frame data pending, grab it now
		if (FrameBoundaryTimestampsWriteBuffer[Timeline].HasNewData())
		{
			const_cast<FFrameBoundaries*>(this)->Update(Timeline);
		}

		const TArray<uint64>& ThreadTimestamps = FrameBoundaryTimestamps[Timeline];
		if (ThreadTimestamps.Num() == 0 || Timestamp < ThreadTimestamps[0])
		{
			// This timestamp is before the first frame, or there are no valid timestamps
			CurrentReadFrameIndex = 0;
			return -1;
		}

		if (CurrentReadFrameIndex >= ThreadTimestamps.Num())
		{
			CurrentReadFrameIndex = ThreadTimestamps.Num() - 1;
		}


		// Check if we need to rewind
		if (CurrentReadFrameIndex > 0 && ThreadTimestamps[CurrentReadFrameIndex - 1] > Timestamp)
		{
			// Binary search to < 4 and then resume linear searching
			int32 StartPos = 0;
			int32 EndPos = CurrentReadFrameIndex;
			while (true)
			{
				int32 Diff = (EndPos - StartPos);
				if (Diff <= 4)
				{
					CurrentReadFrameIndex = StartPos;
					break;
				}
				int32 MidPos = (EndPos + StartPos) / 2;
				if (ThreadTimestamps[MidPos] > Timestamp)
				{
					EndPos = MidPos;
				}
				else
				{
					StartPos = MidPos;
				}
			}
		}

		for (; CurrentReadFrameIndex < ThreadTimestamps.Num(); CurrentReadFrameIndex++)
		{
			if (Timestamp < ThreadTimestamps[CurrentReadFrameIndex])
			{
				// Might return -1 if this was before the first frame
				return CurrentReadFrameIndex - 1;
			}
		}
		return ThreadTimestamps.Num() - 1;
	}

	void AddBeginFrameTimestamp(ECsvTimeline::Type Timeline, const bool bDoThreadCheck = true)
	{
#if DO_CHECK
		if (bDoThreadCheck)
		{
			switch (Timeline)
			{
			case ECsvTimeline::Gamethread:
				check(IsInGameThread());
				break;
			case ECsvTimeline::Renderthread:
				check(IsInRenderingThread());
				break;
			}
		}
#endif
		uint64* Element = FrameBoundaryTimestampsWriteBuffer[Timeline].ReserveElement();
		*Element = FPlatformTime::Cycles64();
		FrameBoundaryTimestampsWriteBuffer[Timeline].CommitElement();
	}

private:
	void Update(ECsvTimeline::Type Timeline = ECsvTimeline::Count)
	{
		check(IsInCsvProcessingThread());
		if (Timeline == ECsvTimeline::Count)
		{
			for (int32 i = 0; i < int32(ECsvTimeline::Count); i++)
			{
				FrameBoundaryTimestampsWriteBuffer[i].PopAll(FrameBoundaryTimestamps[i]);
			}
		}
		else
		{
			FrameBoundaryTimestampsWriteBuffer[Timeline].PopAll(FrameBoundaryTimestamps[Timeline]);
		}
	}

	TSingleProducerSingleConsumerList<uint64, 16> FrameBoundaryTimestampsWriteBuffer[ECsvTimeline::Count];
	TArray<uint64> FrameBoundaryTimestamps[ECsvTimeline::Count];
	mutable int32 CurrentReadFrameIndex;
};
static FFrameBoundaries GFrameBoundaries;


static TMap<const ANSICHAR*, uint32> CharPtrToStringIndex;
static TMap<FString, uint32> UniqueNonFNameStatIDStrings;
static TArray<FString> UniqueNonFNameStatIDIndices;

struct FAnsiStringRegister
{
	static uint32 GetUniqueStringIndex(const ANSICHAR* AnsiStr)
	{
		uint32* IndexPtr = CharPtrToStringIndex.Find(AnsiStr);
		if (IndexPtr)
		{
			return *IndexPtr;
		}

		// If we haven't seen this pointer before, check the string register (this is slow!)
		FString Str = FString(StringCast<TCHAR>(AnsiStr).Get());
		uint32* Value = UniqueNonFNameStatIDStrings.Find(Str);
		if (Value)
		{
			// Cache in the index register
			CharPtrToStringIndex.Add(AnsiStr, *Value);
			return *Value;
		}
		// Otherwise, this string is totally new
		uint32 NewIndex = UniqueNonFNameStatIDIndices.Num();
		UniqueNonFNameStatIDStrings.Add(Str, NewIndex);
		UniqueNonFNameStatIDIndices.Add(Str);
		CharPtrToStringIndex.Add(AnsiStr, NewIndex);
		return NewIndex;
	}

	static FString GetString(uint32 Index)
	{
		return UniqueNonFNameStatIDIndices[Index];
	}
};


class FCsvStatRegister
{
	static const uint64 FNameOrIndexMask = 0x0007ffffffffffffull; // Lower 51 bits for fname or index

	struct FStatIDFlags
	{
		static const uint8 IsCountStat = 0x01;
	};

public:
	FCsvStatRegister()
	{
		Clear();
	}

	int32 GetUniqueIndex(uint64 InStatIDRaw, int32 InCategoryIndex, bool bInIsFName, bool bInIsCountStat)
	{
		check(IsInCsvProcessingThread());

		// Make a compound key
		FCsvUniqueStatID UniqueID(InStatIDRaw, InCategoryIndex, bInIsFName, bInIsCountStat);

		uint64 Hash = UniqueID.Hash;
		int32 *IndexPtr = StatIDToIndex.Find(Hash);
		if (IndexPtr)
		{
			return *IndexPtr;
		}
		else
		{
			int32 IndexOut = -1;
			FString NameStr;
			if (bInIsFName)
			{
				check((InStatIDRaw & FNameOrIndexMask) == InStatIDRaw);
				const FNameEntry* NameEntry = FName::GetEntry(FNameEntryId::FromUnstableInt(UniqueID.Fields.FNameOrIndex));
				NameStr = NameEntry->GetPlainNameString();
			}
			else
			{
				// With non-fname stats, the same string can appear with different pointers.
				// We need to look up the stat in the ansi stat register to see if it's actually unique
				uint32 AnsiNameIndex = FAnsiStringRegister::GetUniqueStringIndex((ANSICHAR*)InStatIDRaw);
				FCsvUniqueStatID AnsiUniqueID(UniqueID);
				AnsiUniqueID.Fields.FNameOrIndex = AnsiNameIndex;
				int32 *AnsiIndexPtr = AnsiStringStatIDToIndex.Find(AnsiUniqueID.Hash);
				if (AnsiIndexPtr)
				{
					// This isn't a new stat. Only the pointer is new, not the string itself
					IndexOut = *AnsiIndexPtr;
					// Update the master lookup table
					StatIDToIndex.Add(UniqueID.Hash, IndexOut);
					return IndexOut;
				}
				else
				{
					// Stat has never been seen before. Add it to the ansi map
					AnsiStringStatIDToIndex.Add(AnsiUniqueID.Hash, StatIndexCount);
				}
				NameStr = FAnsiStringRegister::GetString(AnsiNameIndex);
			}

			// Store the index in the master map
			IndexOut = StatIndexCount;
			StatIDToIndex.Add( UniqueID.Hash,  IndexOut);
			StatIndexCount++;

			// Store the name, category index and flags
			StatNames.Add(NameStr);
			StatCategoryIndices.Add(InCategoryIndex);

			uint8 Flags = 0;
			if (bInIsCountStat)
			{
				Flags |= FStatIDFlags::IsCountStat;
			}
			StatFlags.Add(Flags);

			return IndexOut;
		}
	}

	void Clear()
	{
		StatIndexCount = 0;
		StatIDToIndex.Reset();
		AnsiStringStatIDToIndex.Reset();
		StatNames.Empty();
		StatCategoryIndices.Empty();
		StatFlags.Empty();
	}

	const FString& GetStatName(int32 Index) const
	{
		return StatNames[Index];
	}
	int32 GetCategoryIndex(int32 Index) const
	{
		return StatCategoryIndices[Index];
	}

	bool IsCountStat(int32 Index) const
	{
		return !!(StatFlags[Index] & FStatIDFlags::IsCountStat);
	}

protected:
	TMap<uint64, int32> StatIDToIndex;
	TMap<uint64, int32> AnsiStringStatIDToIndex;
	uint32 StatIndexCount;
	TArray<FString> StatNames;
	TArray<int32> StatCategoryIndices;
	TArray<uint8> StatFlags;
};

//-----------------------------------------------------------------------------
//	FCsvTimingMarker : records timestamps. Uses StatName pointer as a unique ID
//-----------------------------------------------------------------------------
struct FCsvStatBase
{
	struct FFlags
	{
		static const uint8 StatIDIsFName = 0x01;
		static const uint8 TimestampBegin = 0x02;
		static const uint8 IsCustomStat = 0x04;
		static const uint8 IsInteger = 0x08;
		static const uint8 IsExclusiveTimestamp = 0x10;
		static const uint8 IsExclusiveInsertedMarker = 0x20;
	};

	CSV_PROFILER_INLINE void Init(uint64 InStatID, int32 InCategoryIndex, uint8 InFlags, uint64 InTimestamp)
	{
		Timestamp = InTimestamp;
		Flags = InFlags;
		RawStatID = InStatID;
		CategoryIndex = InCategoryIndex;
	}

	CSV_PROFILER_INLINE void Init(uint64 InStatID, int32 InCategoryIndex, uint8 InFlags, uint64 InTimestamp, uint8 InUserData)
	{
		Timestamp = InTimestamp;
		RawStatID = InStatID;
		CategoryIndex = InCategoryIndex;
		UserData = InUserData;
		Flags = InFlags;
	}

	CSV_PROFILER_INLINE uint32 GetUserData() const
	{
		return UserData;
	}

	CSV_PROFILER_INLINE uint64 GetTimestamp() const
	{
		return Timestamp;
	}

	CSV_PROFILER_INLINE bool IsCustomStat() const
	{
		return !!(Flags & FCsvStatBase::FFlags::IsCustomStat);
	}

	CSV_PROFILER_INLINE bool IsFNameStat() const
	{
		return !!(Flags & FCsvStatBase::FFlags::StatIDIsFName);
	}

	uint64 Timestamp;

	// Use with caution! In the case of non-fname stats, strings from different scopes may will have different RawStatIDs (in that case RawStatID is simply a char * cast to a uint64). 
	// Use GetSeriesStatID() (slower) to get a unique ID for a string where needed
	uint64 RawStatID;
	int32 CategoryIndex;

	uint8 UserData;
	uint8 Flags;
};

struct FCsvTimingMarker : public FCsvStatBase
{
	bool IsBeginMarker() const
	{
		return !!(Flags & FCsvStatBase::FFlags::TimestampBegin);
	}
	bool IsExclusiveMarker() const
	{
		return !!(Flags & FCsvStatBase::FFlags::IsExclusiveTimestamp);
	}
	bool IsExclusiveArtificialMarker() const
	{
		return !!(Flags & FCsvStatBase::FFlags::IsExclusiveInsertedMarker);
	}
};

struct FCsvCustomStat : public FCsvStatBase
{
	ECsvCustomStatOp GetCustomStatOp() const
	{
		return (ECsvCustomStatOp)GetUserData();
	}

	bool IsInteger() const
	{
		return !!(Flags & FCsvStatBase::FFlags::IsInteger);
	}

	double GetValueAsDouble() const
	{
		return IsInteger() ? double(Value.AsInt) : double(Value.AsFloat);
	}

	union FValue
	{
		float AsFloat;
		uint32 AsInt;
	} Value;
};

struct FCsvEvent
{
	inline uint64 GetAllocatedSize() const { return (uint64)EventText.GetAllocatedSize(); }

	FString EventText;
	uint64 Timestamp;
	uint32 CategoryIndex;
};


struct FCsvStatSeriesValue
{
	FCsvStatSeriesValue() { Value.AsInt = 0; }
	union
	{
		int32 AsInt;
		float AsFloat;
	} Value;
};


class FCsvWriterHelper
{
public:
	FCsvWriterHelper(const TSharedRef<FArchive>& InOutputFile, int32 InBufferSize, bool bInCompressOutput)
		: OutputFile(InOutputFile)
		, bIsLineStart(true)
		, BytesInBuffer(0)
	{
		if (InBufferSize > 0)
		{
			Buffer.SetNumUninitialized(InBufferSize);
			if (bInCompressOutput)
			{
				GZipBuffer.SetNumUninitialized(InBufferSize);
			}
		}
	}

	~FCsvWriterHelper()
	{
		Flush();
	}

	void WriteSemicolonSeparatedStringList(const TArray<FString>& Strings)
	{
		WriteEmptyString();

		for (int32 Index = 0; Index < Strings.Num(); ++Index)
		{
			FString SanitizedText = Strings[Index];

			// Remove semi-colons and commas from event strings so we can safely separate using them
			SanitizedText.ReplaceInline(TEXT(";"), TEXT("."));
			SanitizedText.ReplaceInline(TEXT(","), TEXT("."));

			if (Index > 0)
			{
				WriteChar(';');
			}

			WriteStringInternal(SanitizedText);
		}
	}

	void NewLine()
	{
		WriteChar('\n');
		bIsLineStart = true;
	}

	void WriteString(const FString& Str)
	{
		if (!bIsLineStart)
		{
			WriteChar(',');
		}
		bIsLineStart = false;
		WriteStringInternal(Str);
	}

	void WriteEmptyString()
	{
		if (!bIsLineStart)
		{
			WriteChar(',');
		}
		bIsLineStart = false;
	}

	void WriteValue(double Value)
	{
		if (!bIsLineStart)
		{
			WriteChar(',');
		}
		bIsLineStart = false;

		int32 StrLen;
		ANSICHAR StringBuffer[256];

		if (FMath::Frac((float)Value) == 0.0f)
		{
			StrLen = FCStringAnsi::Snprintf(StringBuffer, 256, "%d", int(Value));
		}
		else if (FMath::Abs(Value) < 0.1)
		{
			StrLen = FCStringAnsi::Snprintf(StringBuffer, 256, "%.6f", Value);
		}
		else
		{
			StrLen = FCStringAnsi::Snprintf(StringBuffer, 256, "%.4f", Value);
		}
		SerializeInternal((void*)StringBuffer, sizeof(ANSICHAR) * StrLen);
	}

	void WriteMetadataEntry(const FString& Key, const FString& Value)
	{
		WriteString(*FString::Printf(TEXT("[%s]"), *Key));
		WriteString(Value);
	}

private:
	void WriteStringInternal(const FString& Str)
	{
		auto AnsiStr = StringCast<ANSICHAR>(*Str);
		SerializeInternal((void*)AnsiStr.Get(), AnsiStr.Length());
	}

	void WriteChar(ANSICHAR Char)
	{
		SerializeInternal((void*)&Char, sizeof(ANSICHAR));
	}

	void SerializeInternal(void* Src, int32 NumBytes)
	{
		if (Buffer.Num() == 0)
		{
			OutputFile->Serialize(Src, NumBytes);
		}
		else
		{
			uint8* SrcPtr = (uint8*)Src;

			while (NumBytes)
			{
				int32 BytesToWrite = FMath::Min(Buffer.Num() - BytesInBuffer, NumBytes);
				if (BytesToWrite == 0)
				{
					Flush();
				}
				else
				{
					FMemory::Memcpy(&Buffer[BytesInBuffer], SrcPtr, BytesToWrite);
					BytesInBuffer += BytesToWrite;
					SrcPtr += BytesToWrite;
					NumBytes -= BytesToWrite;
				}
			}
		}
	}

	void Flush()
	{
		if (BytesInBuffer > 0)
		{
			if (GZipBuffer.Num() > 0)
			{
				// Compression is enabled.
				int32 CompressedSize;
				{
					while (true)
					{
						// Compress the data in Buffer into the GZipBuffer array
						CompressedSize = GZipBuffer.Num();
						if (FCompression::CompressMemory(
							NAME_Gzip,
							GZipBuffer.GetData(), CompressedSize,
							Buffer.GetData(), BytesInBuffer,
							ECompressionFlags::COMPRESS_BiasSpeed))
						{
							break;
						}

						// Compression failed.
						if (CompressedSize > GZipBuffer.Num())
						{
							// Failed because the buffer size was too small. Increase the buffer size.
							GZipBuffer.SetNumUninitialized(CompressedSize);
						}
						else
						{
							// Buffer was already large enough. Unknown error. Nothing we can do here but discard the data.
							UE_LOG(LogCsvProfiler, Error, TEXT("CSV data compression failed."));
							BytesInBuffer = 0;
							return;
						}
					}
				}

				{
					OutputFile->Serialize(GZipBuffer.GetData(), CompressedSize);
				}
			}
			else
			{
				// No compression. Write directly to the output file
				OutputFile->Serialize(Buffer.GetData(), BytesInBuffer);
			}

			BytesInBuffer = 0;
		}
	}

	TSharedRef<FArchive> OutputFile;
	bool bIsLineStart;

	int32 BytesInBuffer;
	TArray<uint8> Buffer;
	TArray<uint8> GZipBuffer;

public:
	inline uint64 GetAllocatedSize() const
	{
		return Buffer.GetAllocatedSize() + GZipBuffer.GetAllocatedSize();
	}
};

struct FCsvProcessedEvent
{
	inline uint64 GetAllocatedSize() const { return EventText.GetAllocatedSize(); }

	FString GetFullName() const
	{
		if (CategoryIndex == 0)
		{
			return EventText;
		}
		return FCsvCategoryData::Get()->GetCategoryNameByIndex(CategoryIndex) + TEXT("/") + EventText;
	}
	FString EventText;
	uint32 FrameNumber;
	uint32 CategoryIndex;
};

typedef int32 FCsvStatID;

struct FCsvStatSeries
{
	enum class EType : uint8
	{
		TimerData,
		CustomStatInt,
		CustomStatFloat
	};

	FCsvStatSeries(EType InSeriesType, const FCsvStatID& InStatID, FCsvStreamWriter* InWriter, FCsvStatRegister& StatRegister, const FString& ThreadName);
	void FlushIfDirty();

	void SetTimerValue(uint32 DataFrameNumber, uint64 ElapsedCycles)
	{
		check(SeriesType == EType::TimerData);
		ensure(CurrentWriteFrameNumber <= DataFrameNumber || CurrentWriteFrameNumber == -1);

		// If we're done with the previous frame, commit it
		if (CurrentWriteFrameNumber != DataFrameNumber)
		{
			if (CurrentWriteFrameNumber != -1)
			{
				FlushIfDirty();
			}
			CurrentWriteFrameNumber = DataFrameNumber;
			bDirty = true;
		}
		CurrentValue.AsTimerCycles += ElapsedCycles;
	}

	void SetCustomStatValue_Int(uint32 DataFrameNumber, ECsvCustomStatOp Op, int32 Value)
	{
		check(SeriesType == EType::CustomStatInt);
		ensure(CurrentWriteFrameNumber <= DataFrameNumber || CurrentWriteFrameNumber == -1);

		// Is this a new frame?
		if (CurrentWriteFrameNumber != DataFrameNumber)
		{
			// If we're done with the previous frame, commit it
			if (CurrentWriteFrameNumber != -1)
			{
				FlushIfDirty();
			}

			// The first op in a frame is always a set. Otherwise min/max don't work
			Op = ECsvCustomStatOp::Set;
			CurrentWriteFrameNumber = DataFrameNumber;
			bDirty = true;
		}

		switch (Op)
		{
		case ECsvCustomStatOp::Set:
			CurrentValue.AsIntValue = Value;
			break;

		case ECsvCustomStatOp::Min:
			CurrentValue.AsIntValue = FMath::Min(Value, CurrentValue.AsIntValue);
			break;

		case ECsvCustomStatOp::Max:
			CurrentValue.AsIntValue = FMath::Max(Value, CurrentValue.AsIntValue);
			break;
		case ECsvCustomStatOp::Accumulate:
			CurrentValue.AsIntValue += Value;
			break;
		}
	}

	void SetCustomStatValue_Float(uint32 DataFrameNumber, ECsvCustomStatOp Op, float Value)
	{
		check(SeriesType == EType::CustomStatFloat);
		ensure(CurrentWriteFrameNumber <= DataFrameNumber || CurrentWriteFrameNumber == -1);

		// Is this a new frame?
		if (CurrentWriteFrameNumber != DataFrameNumber)
		{
			// If we're done with the previous frame, commit it
			if (CurrentWriteFrameNumber != -1)
			{
				FlushIfDirty();
			}

			// The first op in a frame is always a set. Otherwise min/max don't work
			Op = ECsvCustomStatOp::Set;
			CurrentWriteFrameNumber = DataFrameNumber;
			bDirty = true;
		}

		switch (Op)
		{
		case ECsvCustomStatOp::Set:
			CurrentValue.AsFloatValue = Value;
			break;

		case ECsvCustomStatOp::Min:
			CurrentValue.AsFloatValue = FMath::Min(Value, CurrentValue.AsFloatValue);
			break;

		case ECsvCustomStatOp::Max:
			CurrentValue.AsFloatValue = FMath::Max(Value, CurrentValue.AsFloatValue);
			break;

		case ECsvCustomStatOp::Accumulate:
			CurrentValue.AsFloatValue += Value;
			break;
		}
	}

	bool IsCustomStat() const
	{
		return (SeriesType == EType::CustomStatFloat || SeriesType == EType::CustomStatInt);
	}

	inline uint64 GetAllocatedSize() const { return Name.GetAllocatedSize(); }

	FCsvStatID StatID;
	const EType SeriesType;
	FString Name;

	uint32 CurrentWriteFrameNumber;
	union
	{
		int32   AsIntValue;
		float   AsFloatValue;
		uint64  AsTimerCycles;
	} CurrentValue;

	FCsvStreamWriter* Writer;

	int32 ColumnIndex;

	bool bDirty;
};

struct FCsvProcessThreadDataStats
{
	FCsvProcessThreadDataStats()
		: TimestampCount(0)
		, CustomStatCount(0)
		, EventCount(0)
	{}

	uint32 TimestampCount;
	uint32 CustomStatCount;
	uint32 EventCount;
};

class FCsvStreamWriter
{
	struct FCsvRow
	{
		TArray<FCsvStatSeriesValue> Values;
		TArray<FCsvProcessedEvent> Events;

		inline uint64 GetAllocatedSize() const
		{
			uint64 Size = Values.GetAllocatedSize() + Events.GetAllocatedSize();
			for (const FCsvProcessedEvent& Event : Events)
			{
				Size += Event.GetAllocatedSize();
			}
			return Size;
		}
	};

	TMap<int64, FCsvRow> Rows;
	FCsvWriterHelper Stream;

	// There is no way to know what a frame is completed, to flush a CSV row to disk. Instead, we track the maximum
	// frame index we've seen from CSV data processing (WriteFrameIndex) and choose to flush all rows that have a
	// frame index less than (WriteFrameIndex - NumFramesToBuffer). NumFramesToBuffer should be large enough to avoid
	// flushing rows before all the timestamps for that frame have been processed, but small enough to avoid the
	// additional memory overhead of holding addition rows in memory unnecessarily.
	int64 NumFramesToBuffer;
	int64 WriteFrameIndex;
	int64 ReadFrameIndex;

	const bool bContinuousWrites;
	bool bFirstRow;

	TArray<FCsvStatSeries*> AllSeries;
	TArray<class FCsvProfilerThreadDataProcessor*> DataProcessors;

	uint32 RenderThreadId;
	uint32 RHIThreadId;

public:
	FCsvStreamWriter(const TSharedRef<FArchive>& InOutputFile, bool bInContinuousWrites, int32 InBufferSize, int64 InNumFramesToBuffer, bool bInCompressOutput, uint32 RenderThreadId, uint32 RHIThreadId);
	~FCsvStreamWriter();

	void AddSeries(FCsvStatSeries* Series);

	void PushValue(FCsvStatSeries* Series, int64 FrameNumber, const FCsvStatSeriesValue& Value);
	void PushEvent(const FCsvProcessedEvent& Event);

	void FinalizeNextRow();
	void Process(FCsvProcessThreadDataStats& OutStats);

	void Finalize(const TMap<FString, FString>& Metadata);

	inline uint64 GetAllocatedSize() const;
};

FCsvStatSeries::FCsvStatSeries(EType InSeriesType, const FCsvStatID& InStatID, FCsvStreamWriter* InWriter, FCsvStatRegister& StatRegister, const FString& ThreadName)
	: StatID(InStatID)
	, SeriesType(InSeriesType)
	, CurrentWriteFrameNumber(-1)
	, Writer(InWriter)
	, ColumnIndex(-1)
	, bDirty(false)
{
	CurrentValue.AsTimerCycles = 0;

	int32 StatCategoryIndex = StatRegister.GetCategoryIndex(StatID);

	Name = StatRegister.GetStatName(StatID);
	bool bIsCountStat = StatRegister.IsCountStat(StatID);

	if (!IsCustomStat() || bIsCountStat)
	{
		// Add a /<Threadname> prefix
		Name = ThreadName + TEXT("/") + Name;
	}

	if (StatCategoryIndex > 0)
	{
		// Categorized stats are prefixed with <CATEGORY>/
		Name = FCsvCategoryData::Get()->GetCategoryNameByIndex(StatCategoryIndex) + TEXT("/") + Name;
	}

	if (bIsCountStat)
	{
		// Add a counts prefix
		Name = TEXT("COUNTS/") + Name;
	}

	Writer->AddSeries(this);
}

void FCsvStatSeries::FlushIfDirty()
{
	if (bDirty)
	{
		FCsvStatSeriesValue Value;
		switch (SeriesType)
		{
		case EType::TimerData:
			Value.Value.AsFloat = (float)FPlatformTime::ToMilliseconds64(CurrentValue.AsTimerCycles);
			break;
		case EType::CustomStatInt:
			Value.Value.AsInt = CurrentValue.AsIntValue;
			break;
		case EType::CustomStatFloat:
			Value.Value.AsFloat = CurrentValue.AsFloatValue;
			break;
		}
		Writer->PushValue(this, CurrentWriteFrameNumber, Value);
		CurrentValue.AsTimerCycles = 0;
		bDirty = false;
	}
}

class FCsvProfilerThreadData
{
public:
	typedef TWeakPtr<FCsvProfilerThreadData, ESPMode::ThreadSafe> FWeakPtr;
	typedef TSharedPtr<FCsvProfilerThreadData, ESPMode::ThreadSafe> FSharedPtr;

private:
	static CSV_PROFILER_INLINE uint64 GetStatID(const char* StatName) { return uint64(StatName); }
	static CSV_PROFILER_INLINE uint64 GetStatID(const FName& StatId) { return StatId.ToUnstableInt(); }

	static FCriticalSection TlsCS;
	static TArray<FWeakPtr> TlsInstances;
	static uint32 TlsSlot;

public:
	static void InitTls()
	{
		if (!FPlatformTLS::IsValidTlsSlot(TlsSlot))
		{
			TlsSlot = FPlatformTLS::AllocTlsSlot();
			FPlatformMisc::MemoryBarrier();
		}
	}

	static FORCENOINLINE FCsvProfilerThreadData* CreateTLSData()
	{
		QUICK_SCOPE_CYCLE_COUNTER(CSVProfiler_ThreadData_CreateTLSData);
		FScopeLock Lock(&TlsCS);

		FSharedPtr ProfilerThreadPtr = MakeShareable(new FCsvProfilerThreadData());
		FPlatformTLS::SetTlsValue(TlsSlot, ProfilerThreadPtr.Get());

		// Keep a weak reference to this thread data in the global array.
		TlsInstances.Emplace(ProfilerThreadPtr);

		// Register the shared ptr in the thread's TLS auto-cleanup list.
		// When the thread exits, it will delete the shared ptr, releasing its reference.
		(new TTlsAutoCleanupValue<FSharedPtr>(ProfilerThreadPtr))->Register();

		return ProfilerThreadPtr.Get();
	}

	static CSV_PROFILER_INLINE FCsvProfilerThreadData& Get()
	{
		FCsvProfilerThreadData* ProfilerThread = (FCsvProfilerThreadData*)FPlatformTLS::GetTlsValue(TlsSlot);
		if (UNLIKELY(!ProfilerThread))
		{
			ProfilerThread = CreateTLSData();
		}
		return *ProfilerThread;
	}

	static inline void GetTlsInstances(TArray<FSharedPtr>& OutTlsInstances)
	{
		QUICK_SCOPE_CYCLE_COUNTER(CSVProfiler_ThreadData_GetTlsInstances);
		FScopeLock Lock(&TlsCS);
		OutTlsInstances.Empty(TlsInstances.Num());

		for (int32 Index = TlsInstances.Num() - 1; Index >= 0; --Index)
		{
			FSharedPtr SharedPtr = TlsInstances[Index].Pin();
			if (SharedPtr.IsValid())
			{
				// Thread is still alive.
				OutTlsInstances.Emplace(MoveTemp(SharedPtr));
			}
		}
	}

	FCsvProfilerThreadData()
		: ThreadId(FPlatformTLS::GetCurrentThreadId())
		, ThreadName(FThreadManager::GetThreadName(ThreadId))
		, DataProcessor(nullptr)
	{
	}

	~FCsvProfilerThreadData()
	{
		// Don't clean up TLS data once the app is exiting - containers may have already been destroyed
		if (!GIsRunning)
		{
			return;
		}

		// No thread data processors should have a reference to this TLS instance when we're being deleted.
		check(DataProcessor == nullptr);

		QUICK_SCOPE_CYCLE_COUNTER(CSVProfiler_ThreadData_Destructor);

		// Clean up dead entries in the thread data array.
		// This will remove both the current instance, and any others that have expired.
		FScopeLock Lock(&TlsCS);
		for (auto Iter = TlsInstances.CreateIterator(); Iter; ++Iter)
		{
			if (!Iter->IsValid())
			{
				Iter.RemoveCurrent();
			}
		}
	}

	void FlushResults(TArray<FCsvTimingMarker>& OutMarkers, TArray<FCsvCustomStat>& OutCustomStats, TArray<FCsvEvent>& OutEvents)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FCsvProfilerThreadData_FlushResults);
		
		check(IsInCsvProcessingThread());

		int64 MaxSlackMemBytes = (int64)CVarMaxPerThreadStatDataSlackKB.GetValueOnAnyThread() * 1024;

		TimingMarkers.PopAll(OutMarkers, MaxSlackMemBytes);
		CustomStats.PopAll(OutCustomStats, MaxSlackMemBytes);
		Events.PopAll(OutEvents, MaxSlackMemBytes);
	}

	CSV_PROFILER_INLINE void AddTimestampBegin(const char* StatName, int32 CategoryIndex)
	{
		uint64 Cycles = FPlatformTime::Cycles64();
		TRACE_CSV_PROFILER_BEGIN_STAT(StatName, CategoryIndex, Cycles);
		TimingMarkers.ReserveElement()->Init(GetStatID(StatName), CategoryIndex, FCsvStatBase::FFlags::TimestampBegin, Cycles);
		TimingMarkers.CommitElement();
	}

	CSV_PROFILER_INLINE void AddTimestampEnd(const char* StatName, int32 CategoryIndex)
	{
		uint64 Cycles = FPlatformTime::Cycles64();
		TRACE_CSV_PROFILER_END_STAT(StatName, CategoryIndex, Cycles);
		TimingMarkers.ReserveElement()->Init(GetStatID(StatName), CategoryIndex, 0, Cycles);
		TimingMarkers.CommitElement();
	}

	CSV_PROFILER_INLINE void AddTimestampExclusiveBegin(const char* StatName)
	{
		uint64 Cycles = FPlatformTime::Cycles64();
		TRACE_CSV_PROFILER_BEGIN_EXCLUSIVE_STAT(StatName, CSV_CATEGORY_INDEX(Exclusive), Cycles);
		TimingMarkers.ReserveElement()->Init(GetStatID(StatName), CSV_CATEGORY_INDEX(Exclusive), FCsvStatBase::FFlags::TimestampBegin | FCsvStatBase::FFlags::IsExclusiveTimestamp, Cycles);
		TimingMarkers.CommitElement();
	}

	CSV_PROFILER_INLINE void AddTimestampExclusiveEnd(const char* StatName)
	{
		uint64 Cycles = FPlatformTime::Cycles64();
		TRACE_CSV_PROFILER_END_EXCLUSIVE_STAT(StatName, CSV_CATEGORY_INDEX(Exclusive), Cycles);
		TimingMarkers.ReserveElement()->Init(GetStatID(StatName), CSV_CATEGORY_INDEX(Exclusive), FCsvStatBase::FFlags::IsExclusiveTimestamp, Cycles);
		TimingMarkers.CommitElement();
	}

	CSV_PROFILER_INLINE void AddTimestampBegin(const FName& StatName, int32 CategoryIndex)
	{
		uint64 Cycles = FPlatformTime::Cycles64();
		TRACE_CSV_PROFILER_BEGIN_STAT(StatName, CategoryIndex, Cycles);
		TimingMarkers.ReserveElement()->Init(GetStatID(StatName), CategoryIndex, FCsvStatBase::FFlags::StatIDIsFName | FCsvStatBase::FFlags::TimestampBegin, Cycles);
		TimingMarkers.CommitElement();
	}

	CSV_PROFILER_INLINE void AddTimestampEnd(const FName& StatName, int32 CategoryIndex)
	{
		uint64 Cycles = FPlatformTime::Cycles64();
		TRACE_CSV_PROFILER_END_STAT(StatName, CategoryIndex, Cycles);
		TimingMarkers.ReserveElement()->Init(GetStatID(StatName), CategoryIndex, FCsvStatBase::FFlags::StatIDIsFName, Cycles);
		TimingMarkers.CommitElement();
	}

	CSV_PROFILER_INLINE void AddCustomStat(const char* StatName, const int32 CategoryIndex, const float Value, const ECsvCustomStatOp CustomStatOp)
	{
		FCsvCustomStat* CustomStat = CustomStats.ReserveElement();
		uint64 Cycles = FPlatformTime::Cycles64();
		TRACE_CSV_PROFILER_CUSTOM_STAT(StatName, CategoryIndex, Value, uint8(CustomStatOp), Cycles);
		CustomStat->Init(GetStatID(StatName), CategoryIndex, FCsvStatBase::FFlags::IsCustomStat, Cycles, uint8(CustomStatOp));
		CustomStat->Value.AsFloat = Value;
		CustomStats.CommitElement();
	}

	CSV_PROFILER_INLINE void AddCustomStat(const FName& StatName, const int32 CategoryIndex, const float Value, const ECsvCustomStatOp CustomStatOp)
	{
		FCsvCustomStat* CustomStat = CustomStats.ReserveElement();
		uint64 Cycles = FPlatformTime::Cycles64();
		TRACE_CSV_PROFILER_CUSTOM_STAT(StatName, CategoryIndex, Value, uint8(CustomStatOp), Cycles);
		CustomStat->Init(GetStatID(StatName), CategoryIndex, FCsvStatBase::FFlags::IsCustomStat | FCsvStatBase::FFlags::StatIDIsFName, Cycles, uint8(CustomStatOp));
		CustomStat->Value.AsFloat = Value;
		CustomStats.CommitElement();
	}

	CSV_PROFILER_INLINE void AddCustomStat(const char* StatName, const int32 CategoryIndex, const int32 Value, const ECsvCustomStatOp CustomStatOp)
	{
		FCsvCustomStat* CustomStat = CustomStats.ReserveElement();
		uint64 Cycles = FPlatformTime::Cycles64();
		TRACE_CSV_PROFILER_CUSTOM_STAT(StatName, CategoryIndex, Value, uint8(CustomStatOp), Cycles);
		CustomStat->Init(GetStatID(StatName), CategoryIndex, FCsvStatBase::FFlags::IsCustomStat | FCsvStatBase::FFlags::IsInteger, Cycles, uint8(CustomStatOp));
		CustomStat->Value.AsInt = Value;
		CustomStats.CommitElement();
	}

	CSV_PROFILER_INLINE void AddCustomStat(const FName& StatName, const int32 CategoryIndex, const int32 Value, const ECsvCustomStatOp CustomStatOp)
	{
		FCsvCustomStat* CustomStat = CustomStats.ReserveElement();
		uint64 Cycles = FPlatformTime::Cycles64();
		TRACE_CSV_PROFILER_CUSTOM_STAT(StatName, CategoryIndex, Value, uint8(CustomStatOp), Cycles);
		CustomStat->Init(GetStatID(StatName), CategoryIndex, FCsvStatBase::FFlags::IsCustomStat | FCsvStatBase::FFlags::IsInteger | FCsvStatBase::FFlags::StatIDIsFName, Cycles, uint8(CustomStatOp));
		CustomStat->Value.AsInt = Value;
		CustomStats.CommitElement();
	}

	CSV_PROFILER_INLINE void AddEvent(const FString& EventText, const int32 CategoryIndex)
	{
		FCsvEvent* Event = Events.ReserveElement();
		uint64 Cycles = FPlatformTime::Cycles64();
		TRACE_CSV_PROFILER_EVENT(*EventText, CategoryIndex, Cycles);
		Event->EventText = EventText;
		Event->Timestamp = Cycles;
		Event->CategoryIndex = CategoryIndex;
		Events.CommitElement();
	}

	CSV_PROFILER_INLINE void AddEventWithTimestamp(const FString& EventText, const int32 CategoryIndex, const uint64 Timestamp)
	{
		TRACE_CSV_PROFILER_EVENT(*EventText, CategoryIndex, Timestamp);
		FCsvEvent* Event = Events.ReserveElement();
		Event->EventText = EventText;
		Event->Timestamp = Timestamp;
		Event->CategoryIndex = CategoryIndex;
		Events.CommitElement();
	}

	inline uint64 GetAllocatedSize() const
	{
		// Note, we're missing the csv event FString sizes.
		// There is no way to get the events from the list without popping them.
		return
			((uint64)TimingMarkers.GetAllocatedSize()) +
			((uint64)CustomStats.GetAllocatedSize()) +
			((uint64)Events.GetAllocatedSize());
	}

	CSV_PROFILER_INLINE const char * GetWaitStatName() const
	{
		return WaitStatNameStack.Num() == 0 ? GDefaultWaitStatName : WaitStatNameStack.Last();
	}

	CSV_PROFILER_INLINE void PushWaitStatName(const char * WaitStatName)
	{
		LLM_SCOPE(ELLMTag::CsvProfiler);
		WaitStatNameStack.Push(WaitStatName);
	}
	CSV_PROFILER_INLINE const char* PopWaitStatName()
	{
		check(WaitStatNameStack.Num() > 0);
		return WaitStatNameStack.Pop();
	}

	// Raw stat data (written from the thread)
	TSingleProducerSingleConsumerList<FCsvTimingMarker, 256> TimingMarkers;
	TSingleProducerSingleConsumerList<FCsvCustomStat, 256> CustomStats;
	TSingleProducerSingleConsumerList<FCsvEvent, 32> Events;

	const uint32 ThreadId;
	const FString ThreadName;

	class FCsvProfilerThreadDataProcessor* DataProcessor;
	TArray<const char*> WaitStatNameStack;
};

uint32 FCsvProfilerThreadData::TlsSlot = FPlatformTLS::InvalidTlsSlot;
FCriticalSection FCsvProfilerThreadData::TlsCS;
TArray<FCsvProfilerThreadData::FWeakPtr> FCsvProfilerThreadData::TlsInstances;

class FCsvProfilerThreadDataProcessor
{
	FCsvProfilerThreadData::FSharedPtr ThreadData;
	FCsvStreamWriter* Writer;

	TArray<FCsvTimingMarker> MarkerStack;
	TArray<FCsvTimingMarker> ExclusiveMarkerStack;

	TArray<FCsvStatSeries*> StatSeriesArray;
	FCsvStatRegister StatRegister;

	uint64 LastProcessedTimestamp;

	uint32 RenderThreadId;
	uint32 RHIThreadId;

public:
	FCsvProfilerThreadDataProcessor(FCsvProfilerThreadData::FSharedPtr InThreadData, FCsvStreamWriter* InWriter, uint32 InRenderThreadId, uint32 InRHIThreadId)
		: ThreadData(InThreadData)
		, Writer(InWriter)
		, LastProcessedTimestamp(0)
		, RenderThreadId(InRenderThreadId)
		, RHIThreadId(InRHIThreadId)
	{
		check(ThreadData->DataProcessor == nullptr);
		ThreadData->DataProcessor = this;
	}

	~FCsvProfilerThreadDataProcessor()
	{
		check(ThreadData->DataProcessor == this);
		ThreadData->DataProcessor = nullptr;

		// Delete all the created stat series
		for (FCsvStatSeries* Series : StatSeriesArray)
		{
			delete Series;
		}
	}

	inline uint64 GetAllocatedSize() const
	{
		return
			((uint64)MarkerStack.GetAllocatedSize()) +
			((uint64)ExclusiveMarkerStack.GetAllocatedSize()) +
			((uint64)StatSeriesArray.GetAllocatedSize()) +
			((uint64)StatSeriesArray.Num() * sizeof(FCsvStatSeries)) +
			((uint64)ThreadData->GetAllocatedSize());
	}

	void Process(FCsvProcessThreadDataStats& OutStats, int32& OutMinFrameNumberProcessed);

private:
	/** Temporary storage of data collected with every Process() call. */
	TArray<FCsvTimingMarker> ThreadMarkers;
	TArray<FCsvCustomStat> CustomStats;
	TArray<FCsvEvent> Events;

	FCsvStatSeries* FindOrCreateStatSeries(const FCsvStatBase& Stat, FCsvStatSeries::EType SeriesType, bool bIsCountStat)
	{
		check(IsInCsvProcessingThread());
		const int32 StatIndex = StatRegister.GetUniqueIndex(Stat.RawStatID, Stat.CategoryIndex, Stat.IsFNameStat(), bIsCountStat);
		FCsvStatSeries* Series = nullptr;
		if (StatSeriesArray.Num() <= StatIndex)
		{
			int32 GrowBy = StatIndex + 1 - StatSeriesArray.Num();
			StatSeriesArray.AddZeroed(GrowBy);
		}
		if (StatSeriesArray[StatIndex] == nullptr)
		{
			Series = new FCsvStatSeries(SeriesType, StatIndex, Writer, StatRegister, ThreadData->ThreadName);
			StatSeriesArray[StatIndex] = Series;
		}
		else
		{
			Series = StatSeriesArray[StatIndex];
#if DO_CHECK
			checkf(SeriesType == Series->SeriesType, TEXT("Stat named %s was used in multiple stat types. Can't use same identifier for different stat types. Stat types are: Custom(Int), Custom(Float) and Timing"), *StatRegister.GetStatName(StatIndex));
#endif
		}
		return Series;
	}
};

FCsvStreamWriter::FCsvStreamWriter(const TSharedRef<FArchive>& InOutputFile, bool bInContinuousWrites, int32 InBufferSize, int64 InNumFramesToBuffer, bool bInCompressOutput, uint32 InRenderThreadId, uint32 InRHIThreadId)
	: Stream(InOutputFile, InBufferSize, bInCompressOutput)
	, NumFramesToBuffer(InNumFramesToBuffer)
	, WriteFrameIndex(-1)
	, ReadFrameIndex(-1)
	, bContinuousWrites(bInContinuousWrites)
	, bFirstRow(true)
	, RenderThreadId(InRenderThreadId)
	, RHIThreadId(InRHIThreadId)
{}

FCsvStreamWriter::~FCsvStreamWriter()
{
	// Delete all the thread data processors, freeing all memory associated with the CSV profile
	for (FCsvProfilerThreadDataProcessor* DataProcessor : DataProcessors)
	{
		delete DataProcessor;
	}
}

void FCsvStreamWriter::AddSeries(FCsvStatSeries* Series)
{
	check(Series->ColumnIndex == -1);
	Series->ColumnIndex = AllSeries.Num();
	AllSeries.Add(Series);
}

void FCsvStreamWriter::PushValue(FCsvStatSeries* Series, int64 FrameNumber, const FCsvStatSeriesValue& Value)
{
	check(Series->ColumnIndex != -1);

	WriteFrameIndex = FMath::Max(FrameNumber, WriteFrameIndex);

	FCsvRow& Row = Rows.FindOrAdd(FrameNumber);

	// Ensure the row is large enough to hold every series
	if (Row.Values.Num() < AllSeries.Num())
	{
		Row.Values.SetNumZeroed(AllSeries.Num(), EAllowShrinking::No);
	}

	Row.Values[Series->ColumnIndex] = Value;
}

void FCsvStreamWriter::PushEvent(const FCsvProcessedEvent& Event)
{
	Rows.FindOrAdd(Event.FrameNumber).Events.Add(Event);
}

void FCsvStreamWriter::FinalizeNextRow()
{
	ReadFrameIndex++;

	if (bFirstRow)
	{
		// Write the first header row
		Stream.WriteString("EVENTS");

		for (FCsvStatSeries* Series : AllSeries)
		{
			Stream.WriteString(Series->Name);
		}

		Stream.NewLine();
		bFirstRow = false;
	}

	// Don't remove yet. Flushing series may modify this row
	FCsvRow* Row = Rows.Find(ReadFrameIndex);
	if (Row)
	{
		if (Row->Events.Num() > 0)
		{
			// Write the events for this row
			TArray<FString> EventStrings;
			EventStrings.Reserve(Row->Events.Num());
			for (FCsvProcessedEvent& Event : Row->Events)
			{
				EventStrings.Add(Event.GetFullName());
			}

			Stream.WriteSemicolonSeparatedStringList(EventStrings);
		}
		else
		{
			// No events. Insert empty string at the start of the line
			Stream.WriteEmptyString();
		}

		for (FCsvStatSeries* Series : AllSeries)
		{
			// Stat values are held in the series until a new value arrives.
			// If we've caught up with the last value written to the series,
			// we need to flush to get the correct value for this frame.
			if (Series->CurrentWriteFrameNumber == ReadFrameIndex)
				Series->FlushIfDirty();

			if (Row->Values.IsValidIndex(Series->ColumnIndex))
			{
				const FCsvStatSeriesValue& Value = Row->Values[Series->ColumnIndex];
				if (Series->SeriesType == FCsvStatSeries::EType::CustomStatInt)
				{
					Stream.WriteValue(Value.Value.AsInt);
				}
				else
				{
					Stream.WriteValue(Value.Value.AsFloat);
				}
			}
			else
			{
				Stream.WriteValue(0);
			}
		}

		Stream.NewLine();

		// Finally remove the frame data
		Rows.FindAndRemoveChecked(ReadFrameIndex);
	}
}

void FCsvStreamWriter::Finalize(const TMap<FString, FString>& Metadata)
{
	// Flush all remaining data
	while (ReadFrameIndex < WriteFrameIndex)
	{
		FinalizeNextRow();
	}

	// Write a final summary header row
	Stream.WriteString("EVENTS");
	for (FCsvStatSeries* Series : AllSeries)
	{
		Stream.WriteString(Series->Name);
	}
	Stream.NewLine();

	// Insert some metadata to indicate the file has a summary header row
	Stream.WriteMetadataEntry((TEXT("HasHeaderRowAtEnd")), TEXT("1"));

	// Add metadata at the end of the file, making sure commandline is last (this is required for parsing)
	const TPair<FString, FString>* CommandlineEntry = NULL;
	for (const auto& Pair : Metadata)
	{
		if (Pair.Key == "Commandline")
		{
			CommandlineEntry = &Pair;
		}
		else
		{
			Stream.WriteMetadataEntry(Pair.Key, Pair.Value);
		}
	}
	if (CommandlineEntry)
	{
		Stream.WriteMetadataEntry(CommandlineEntry->Key, CommandlineEntry->Value);
	}
}

void FCsvStreamWriter::Process(FCsvProcessThreadDataStats& OutStats)
{
	TArray<FCsvProfilerThreadData::FSharedPtr> TlsData;
	FCsvProfilerThreadData::GetTlsInstances(TlsData);

	{
		QUICK_SCOPE_CYCLE_COUNTER(CSVProfiler_Writer_GetDataProcessors);
		for (FCsvProfilerThreadData::FSharedPtr Data : TlsData)
		{
			if (!Data->DataProcessor)
			{
				DataProcessors.Add(new FCsvProfilerThreadDataProcessor(Data, this, RenderThreadId, RHIThreadId));
			}
		}
	}
	

	int32 MinFrameNumberProcessed = MAX_int32;
	{
		QUICK_SCOPE_CYCLE_COUNTER(CSVProfiler_Writer_ProcessDataProcessors);
		for (FCsvProfilerThreadDataProcessor* DataProcessor : DataProcessors)
		{
			DataProcessor->Process(OutStats, MinFrameNumberProcessed);
		}
	}
	

	if (bContinuousWrites && MinFrameNumberProcessed < MAX_int32)
	{
		QUICK_SCOPE_CYCLE_COUNTER(CSVProfiler_Writer_FinalizeNextRow);
		int64 NewReadFrameIndex = MinFrameNumberProcessed - NumFramesToBuffer;
		while (ReadFrameIndex < NewReadFrameIndex)
		{
			FinalizeNextRow();
		}
	}
}

uint64 FCsvStreamWriter::GetAllocatedSize() const
{
	uint64 Size =
		((uint64)Rows.GetAllocatedSize()) +
		((uint64)AllSeries.GetAllocatedSize()) +
		((uint64)DataProcessors.GetAllocatedSize()) +
		((uint64)Stream.GetAllocatedSize());

	for (const auto& Pair          : Rows)           { Size += (uint64)Pair.Value.GetAllocatedSize();     }
	for (const auto& Series        : AllSeries)      { Size += (uint64)Series->GetAllocatedSize();        }
	for (const auto& DataProcessor : DataProcessors) { Size += (uint64)DataProcessor->GetAllocatedSize(); }

	return Size;
}

//-----------------------------------------------------------------------------
//	FCsvProfilerProcessingThread class : low priority thread to process 
//  profiling data
//-----------------------------------------------------------------------------
class FCsvProfilerProcessingThread : public FRunnable
{
	FThreadSafeCounter StopCounter;

public:
	FCsvProfilerProcessingThread(FCsvProfiler& InCsvProfiler)
		: CsvProfiler(InCsvProfiler)
	{
#if CSV_THREAD_HIGH_PRI
		Thread = FForkProcessHelper::CreateForkableThread(this, TEXT("CSVProfiler"), 0, TPri_Highest, FPlatformAffinity::GetTaskGraphThreadMask());
#else
		Thread = FForkProcessHelper::CreateForkableThread(this, TEXT("CSVProfiler"), 0, TPri_Lowest, FPlatformAffinity::GetTaskGraphBackgroundTaskMask());
#endif
	}

	virtual ~FCsvProfilerProcessingThread()
	{
		if (Thread)
		{
			Thread->Kill(true);
			delete Thread;
			Thread = nullptr;
		}
	}

	bool IsValid() const
	{
		return Thread != nullptr;
	}

	// FRunnable interface
	virtual bool Init() override
	{
		return true;
	}

	virtual uint32 Run() override
	{
		const float TimeBetweenUpdatesMS = 50.0f;
		GCsvProcessingThreadId = FPlatformTLS::GetCurrentThreadId();
		GGameThreadIsCsvProcessingThread = false;

		FMemory::SetupTLSCachesOnCurrentThread();

		LLM_SCOPE(ELLMTag::CsvProfiler);

		while (StopCounter.GetValue() == 0)
		{
			if (CVarCsvPauseProcessingThread.GetValueOnAnyThread())
			{
				FPlatformProcess::Sleep(TimeBetweenUpdatesMS / 1000.0f);
				continue;
			}
			float ElapsedMS = CsvProfiler.ProcessStatData();

			if (GCsvProfilerIsWritingFile)
			{
				CsvProfiler.FinalizeCsvFile();
				CsvProfiler.FileWriteBlockingEvent->Trigger();
			}

			float SleepTimeSeconds = FMath::Max(TimeBetweenUpdatesMS - ElapsedMS, 0.0f) / 1000.0f;
			FPlatformProcess::Sleep(SleepTimeSeconds);
		}

		FMemory::ClearAndDisableTLSCachesOnCurrentThread();

		return 0;
	}

	virtual void Stop() override
	{
		StopCounter.Increment();
	}

	virtual void Exit() override { }

private:
	FRunnableThread* Thread;
	FCsvProfiler& CsvProfiler;
};

void FCsvProfilerThreadDataProcessor::Process(FCsvProcessThreadDataStats& OutStats, int32& OutMinFrameNumberProcessed)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FCsvProfilerThreadData_ProcessThreadData);

	// We can call this from the game thread just before reading back the data, or from the CSV processing thread
	check(IsInCsvProcessingThread());

	// Read the raw CSV data
	ThreadMarkers.Reset(0);
	CustomStats.Reset(0);
	Events.Reset(0);
	ThreadData->FlushResults(ThreadMarkers, CustomStats, Events);

	OutStats.TimestampCount += ThreadMarkers.Num();
	OutStats.CustomStatCount += CustomStats.Num();
	OutStats.EventCount += Events.Num();

	// Flush the frame boundaries after the stat data. This way, we ensure the frame boundary data is up to date
	// (we do not want to encounter markers from a frame which hasn't been registered yet)
	FPlatformMisc::MemoryBarrier();
	ECsvTimeline::Type Timeline = (ThreadData->ThreadId == RenderThreadId || ThreadData->ThreadId == RHIThreadId) ? ECsvTimeline::Renderthread : ECsvTimeline::Gamethread;

	if (ThreadMarkers.Num() > 0)
	{
#if !UE_BUILD_SHIPPING
		ensure(ThreadMarkers[0].GetTimestamp() >= LastProcessedTimestamp);
#endif
		LastProcessedTimestamp = ThreadMarkers.Last().GetTimestamp();
	}

	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FCsvProfilerThreadData_TimingMarkers);

		// Process timing markers
		FCsvTimingMarker InsertedMarker;
		bool bAllowExclusiveMarkerInsertion = true;
		for (int i = 0; i < ThreadMarkers.Num(); i++)
		{
			FCsvTimingMarker* MarkerPtr = &ThreadMarkers[i];

			// Handle exclusive markers. This may insert an additional marker before this one
			bool bInsertExtraMarker = false;
			if (bAllowExclusiveMarkerInsertion && MarkerPtr->IsExclusiveMarker())
			{
				if (MarkerPtr->IsBeginMarker())
				{
					if (ExclusiveMarkerStack.Num() > 0)
					{
						// Insert an artificial end marker to end the previous marker on the stack at the same timestamp
						InsertedMarker = ExclusiveMarkerStack.Last();
						InsertedMarker.Flags &= (~FCsvStatBase::FFlags::TimestampBegin);
						InsertedMarker.Flags |= FCsvStatBase::FFlags::IsExclusiveInsertedMarker;
						InsertedMarker.Timestamp = MarkerPtr->Timestamp;

						bInsertExtraMarker = true;
					}
					ExclusiveMarkerStack.Add(*MarkerPtr);
				}
				else
				{
					if (ExclusiveMarkerStack.Num() > 0)
					{
						ExclusiveMarkerStack.Pop(EAllowShrinking::No);
						if (ExclusiveMarkerStack.Num() > 0)
						{
							// Insert an artificial begin marker to resume the marker on the stack at the same timestamp
							InsertedMarker = ExclusiveMarkerStack.Last();
							InsertedMarker.Flags |= FCsvStatBase::FFlags::TimestampBegin;
							InsertedMarker.Flags |= FCsvStatBase::FFlags::IsExclusiveInsertedMarker;
							InsertedMarker.Timestamp = MarkerPtr->Timestamp;

							bInsertExtraMarker = true;
						}
					}
				}
			}

			if (bInsertExtraMarker)
			{
				// Insert an extra exclusive marker this iteration and decrement the loop index.
				MarkerPtr = &InsertedMarker;
				i--;
			}
			// Prevent a marker being inserted on the next run if we just inserted one
			bAllowExclusiveMarkerInsertion = !bInsertExtraMarker;

			FCsvTimingMarker& Marker = *MarkerPtr;
			int32 FrameNumber = GFrameBoundaries.GetFrameNumberForTimestamp(Timeline, Marker.GetTimestamp());
			OutMinFrameNumberProcessed = FMath::Min(FrameNumber, OutMinFrameNumberProcessed);
			if (Marker.IsBeginMarker())
			{
				MarkerStack.Push(Marker);
			}
			else
			{
				// Markers might not match up if they were truncated mid-frame, so we need to be robust to that
				if (MarkerStack.Num() > 0)
				{
					// Find the start marker (might not actually be top of the stack, e.g if begin/end for two overlapping stats are independent)
					bool bFoundStart = false;
#if REPAIR_MARKER_STACKS
					FCsvTimingMarker StartMarker;
					// Prevent spurious MSVC warning about this being used uninitialized further down. Alternative is to implement a ctor, but that would add overhead
					StartMarker.Init(0, 0, 0, 0);

					for (int j = MarkerStack.Num() - 1; j >= 0; j--)
					{
						if (MarkerStack[j].RawStatID == Marker.RawStatID) // Note: only works with scopes!
						{
							StartMarker = MarkerStack[j];
							MarkerStack.RemoveAt(j, 1, EAllowShrinking::No);
							bFoundStart = true;
							break;
						}
					}
#else
					FCsvTimingMarker StartMarker = MarkerStack.Pop();
					bFoundStart = true;
#endif
					// TODO: if bFoundStart is false, this stat _never_ gets processed. Could we add it to a persistent list so it's considered next time?
					// Example where this could go wrong: staggered/overlapping exclusive stats ( e.g Abegin, Bbegin, AEnd, BEnd ), where processing ends after AEnd
					// AEnd would be missing 
					if (FrameNumber >= 0 && bFoundStart)
					{
#if !UE_BUILD_SHIPPING
						ensure(Marker.RawStatID == StartMarker.RawStatID);
						ensure(Marker.GetTimestamp() >= StartMarker.GetTimestamp());
#endif
						if (Marker.GetTimestamp() > StartMarker.GetTimestamp())
						{
							uint64 ElapsedCycles = Marker.GetTimestamp() - StartMarker.GetTimestamp();

							// Add the elapsed time to the table entry for this frame/stat
							FCsvStatSeries* Series = FindOrCreateStatSeries(Marker, FCsvStatSeries::EType::TimerData, false);
							Series->SetTimerValue(FrameNumber, ElapsedCycles);

							// Add the COUNT/ series if enabled. Ignore artificial markers (inserted above)
							if (GCsvStatCounts && !Marker.IsExclusiveArtificialMarker())
							{
								FCsvStatSeries* CountSeries = FindOrCreateStatSeries(Marker, FCsvStatSeries::EType::CustomStatInt, true);
								CountSeries->SetCustomStatValue_Int(FrameNumber, ECsvCustomStatOp::Accumulate, 1);
							}
						}
					}
				}
			}
		}
	}

	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FCsvProfilerThreadData_CustomStats);
		// Process the custom stats
		for (int i = 0; i < CustomStats.Num(); i++)
		{
			FCsvCustomStat& CustomStat = CustomStats[i];
			int32 FrameNumber = GFrameBoundaries.GetFrameNumberForTimestamp(Timeline, CustomStat.GetTimestamp());
			OutMinFrameNumberProcessed = FMath::Min(FrameNumber, OutMinFrameNumberProcessed);
			if (FrameNumber >= 0)
			{
				bool bIsInteger = CustomStat.IsInteger();
				FCsvStatSeries* Series = FindOrCreateStatSeries(CustomStat, bIsInteger ? FCsvStatSeries::EType::CustomStatInt : FCsvStatSeries::EType::CustomStatFloat, false);
				if (bIsInteger)
				{
					Series->SetCustomStatValue_Int(FrameNumber, CustomStat.GetCustomStatOp(), CustomStat.Value.AsInt);
				}
				else
				{
					Series->SetCustomStatValue_Float(FrameNumber, CustomStat.GetCustomStatOp(), CustomStat.Value.AsFloat);
				}

				// Add the COUNT/ series if enabled
				if (GCsvStatCounts)
				{
					FCsvStatSeries* CountSeries = FindOrCreateStatSeries(CustomStat, FCsvStatSeries::EType::CustomStatInt, true);
					CountSeries->SetCustomStatValue_Int(FrameNumber, ECsvCustomStatOp::Accumulate, 1);
				}
			}
		}
	}

	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FCsvProfilerThreadData_Events);

		// Process Events
		for (int i = 0; i < Events.Num(); i++)
		{
			FCsvEvent& Event = Events[i];
			int32 FrameNumber = GFrameBoundaries.GetFrameNumberForTimestamp(Timeline, Event.Timestamp);
			OutMinFrameNumberProcessed = FMath::Min(FrameNumber, OutMinFrameNumberProcessed);
			if (FrameNumber >= 0)
			{
				FCsvProcessedEvent ProcessedEvent;
				ProcessedEvent.EventText = Event.EventText;
				ProcessedEvent.FrameNumber = FrameNumber;
				ProcessedEvent.CategoryIndex = Event.CategoryIndex;
				Writer->PushEvent(ProcessedEvent);
			}
		}
	}
}


FCsvProfiler* FCsvProfiler::Get()
{
	static FCsvProfiler* InstancePtr;

	if (!InstancePtr)
	{
		// It's important that the initializer goes here to avoid the overhead of
		// "magic static" initialization on every call (mostly an issue with MSVC
		// because of their epoch-based initialization scheme which doesn't seem
		// to make any real sense on x86)

		static FCsvProfiler Instance;
		InstancePtr = &Instance;
	}

	return InstancePtr;
}

FCsvProfiler::FCsvProfiler()
	: NumFramesToCapture(-1)
	, CaptureFrameNumber(0)
	, CaptureFrameNumberRT(0)
	, CaptureOnEventFrameCount(-1)
	, bInsertEndFrameAtFrameStart(false)
	, bNamedEventsWasEnabled(false)
	, LastEndFrameTimestamp(0)
	, CaptureEndFrameCount(0)
	, ProcessingThread(nullptr)
	, FileWriteBlockingEvent(FPlatformProcess::GetSynchEventFromPool())
{
	check(IsInGameThread());

#if !CSV_PROFILER_USE_CUSTOM_FRAME_TIMINGS
	FCoreDelegates::OnBeginFrame.AddStatic(CsvProfilerBeginFrame);
	FCoreDelegates::OnEndFrame.AddStatic(CsvProfilerEndFrame);
	FCoreDelegates::OnBeginFrameRT.AddStatic(CsvProfilerBeginFrameRT);
	FCoreDelegates::OnEndFrameRT.AddStatic(CsvProfilerEndFrameRT);
#endif

	// add constant metadata
	FString PlatformStr = FString::Printf(TEXT("%s"), ANSI_TO_TCHAR(FPlatformProperties::IniPlatformName()));
	FString BuildConfigurationStr = LexToString(FApp::GetBuildConfiguration());
	FString CommandlineStr = FString("\"") + FCommandLine::Get() + FString("\"");
	// Strip newlines
	CommandlineStr.ReplaceInline(TEXT("\n"), TEXT(""));
	CommandlineStr.ReplaceInline(TEXT("\r"), TEXT(""));
	FString BuildVersionString = FApp::GetBuildVersion();
	FString EngineVersionString = FEngineVersion::Current().ToString();

	FString OSMajor, OSMinor;
	FPlatformMisc::GetOSVersions(OSMajor, OSMinor);
	OSMajor.TrimStartAndEndInline();
	OSMinor.TrimStartAndEndInline();
	FString OSString = FString::Printf(TEXT("%s %s"), *OSMajor, *OSMinor);

	SetMetadataInternal(TEXT("Platform"), *PlatformStr);
	SetMetadataInternal(TEXT("Config"), *BuildConfigurationStr);
	SetMetadataInternal(TEXT("BuildVersion"), *BuildVersionString);
	SetMetadataInternal(TEXT("EngineVersion"), *EngineVersionString);
	SetMetadataInternal(TEXT("OS"), *OSString);
	SetMetadataInternal(TEXT("CPU"), *FPlatformMisc::GetDeviceMakeAndModel());
	SetMetadataInternal(TEXT("PGOEnabled"), FPlatformMisc::IsPGOEnabled() ? TEXT("1") : TEXT("0"));
	SetMetadataInternal(TEXT("PGOProfilingEnabled"), PLATFORM_COMPILER_OPTIMIZATION_PG_PROFILING ? TEXT("1") : TEXT("0"));//True if Profile Guided Optimisation Instrumentation is enabled 
	SetMetadataInternal(TEXT("LTOEnabled"), PLATFORM_COMPILER_OPTIMIZATION_LTCG ? TEXT("1") : TEXT("0"));//True if Link Time Optimisation is enabled 
	SetMetadataInternal(TEXT("ASan"), USING_ADDRESS_SANITISER ? TEXT("1") : TEXT("0"));

#if !UE_BUILD_SHIPPING
	// for privacy, personal and free text fields are not allowed in shipping
	SetMetadataInternal(TEXT("Commandline"), *CommandlineStr, false);
	SetMetadataInternal(TEXT("LoginID"), *FPlatformMisc::GetLoginId());
	FString DeviceTag = FPlatformMisc::GetDeviceTag();
    if (!DeviceTag.IsEmpty())
    {
    	SetMetadataInternal(TEXT("DeviceTag"), *DeviceTag);
    }
	
	// Set the device ID if the platform supports it
	FString DeviceID = FPlatformMisc::GetDeviceId();
	if (!DeviceID.IsEmpty())
	{
		SetMetadataInternal(TEXT("DeviceID"), *DeviceID);
	}
#endif
}

FCsvProfiler::~FCsvProfiler()
{
	GCsvProfilerIsCapturing = false;
	IsShuttingDown.Increment();
	if (ProcessingThread)
	{
		delete ProcessingThread;
		ProcessingThread = nullptr;
	}

	if (FileWriteBlockingEvent)
	{
		FPlatformProcess::ReturnSynchEventToPool(FileWriteBlockingEvent);
		FileWriteBlockingEvent = nullptr;
	}

	if (GStartOnEvent)
	{
		delete GStartOnEvent;
		GStartOnEvent = nullptr;
	}

	if (GStopOnEvent)
	{
		delete GStopOnEvent;
		GStopOnEvent = nullptr;
	}
}

/** Per-frame update */
void FCsvProfiler::BeginFrame()
{
	LLM_SCOPE(ELLMTag::CsvProfiler);
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FCsvProfiler_BeginFrame);
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(CsvProfiler);

	check(IsInGameThread());

	// Set the thread-local waits enabled flag
	GCsvThreadLocalWaitsEnabled = GCsvTrackWaitsOnGameThread;

	if (bInsertEndFrameAtFrameStart)
	{
		bInsertEndFrameAtFrameStart = false;
		EndFrame();
	}

	if (!GCsvProfilerIsWritingFile)
	{
		// Process the command queue for start commands
		FCsvCaptureCommand CurrentCommand;
		if (CommandQueue.Peek(CurrentCommand) && CurrentCommand.CommandType == ECsvCommandType::Start)
		{
			CommandQueue.Dequeue(CurrentCommand);
			if (GCsvProfilerIsCapturing)
			{
				UE_LOG(LogCsvProfiler, Warning, TEXT("Capture start requested, but a capture was already running"));
			}
			else
			{
				UE_LOG(LogCsvProfiler, Display, TEXT("Capture Starting"));

				if (GConfig)
				{
					// Update categories from the config. The config may have changed if there were hotfixes
					FCsvCategoryData::Get()->UpdateCategoriesFromConfig();
				}

				// signal external profiler that we are capturing
				OnCSVProfileStartDelegate.Broadcast();

				// Latch the cvars when we start a capture
				int32 BufferSize = FMath::Max(CVarCsvWriteBufferSize.GetValueOnAnyThread(), 0);
				bool bContinuousWrites = IsContinuousWriteEnabled(true);

				// Allow overriding of compression based on the "csv.CompressionMode" CVar
				bool bCompressOutput;
				switch (CVarCsvCompressionMode.GetValueOnGameThread())
				{
				case 0:
					bCompressOutput = false;
					break;

				case 1:
					bCompressOutput = (BufferSize > 0); 
					break;

				default:
					bCompressOutput = EnumHasAnyFlags(CurrentCommand.Flags, ECsvProfilerFlags::CompressOutput) && (BufferSize > 0);
					break;
				}

				const TCHAR* CsvExtension = bCompressOutput ? TEXT(".csv.gz") : TEXT(".csv");

				// Determine the output path and filename based on override params
				FString DestinationFolder = CurrentCommand.DestinationFolder.IsEmpty() ? FPaths::ProfilingDir() + TEXT("CSV/") : CurrentCommand.DestinationFolder + TEXT("/");
				FString Filename = CurrentCommand.Filename.IsEmpty() ? FString::Printf(TEXT("Profile(%s)%s"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")), CsvExtension) : CurrentCommand.Filename;
				OutputFilename = DestinationFolder + Filename;

				TSharedPtr<FArchive> OutputFile = MakeShareable(IFileManager::Get().CreateFileWriter(*OutputFilename));
				if (!OutputFile)
				{
					UE_LOG(LogCsvProfiler, Error, TEXT("Failed to create CSV file \"%s\". Capture will not start."), *OutputFilename);
				}
				else
				{
					int64 NumFramesToBuffer = CVarCsvStreamFramesToBuffer.GetValueOnAnyThread();
					CsvWriter = new FCsvStreamWriter(OutputFile.ToSharedRef(), bContinuousWrites, BufferSize, NumFramesToBuffer, bCompressOutput, RenderThreadId, RHIThreadId);

					NumFramesToCapture = CurrentCommand.Value;
					GCsvRepeatFrameCount = NumFramesToCapture;
					CaptureFrameNumber = 0;
					CaptureFrameNumberRT = 0;
					LastEndFrameTimestamp = FPlatformTime::Cycles64();
					CurrentFlags = CurrentCommand.Flags;

					if (GCsvUseProcessingThread && ProcessingThread == nullptr)
					{
						// Lazily create the CSV processing thread
						ProcessingThread = new FCsvProfilerProcessingThread(*this);
						if (ProcessingThread->IsValid() == false)
						{
							UE_LOG(LogCsvProfiler, Error, TEXT("CSV Processing Thread could not be created due to being in a single-thread environment "));
							delete ProcessingThread;
							ProcessingThread = nullptr;
							GCsvUseProcessingThread = false;
						}
					}
					 
					// Set the CSV ID and mirror it to the log
					FString CsvId = FGuid::NewGuid().ToString();
					SetMetadataInternal(TEXT("CsvID"), *CsvId);
					UE_LOG(LogCsvProfiler, Display, TEXT("Capture started. CSV ID: %s"), *CsvId);

					int32 TargetFPS = FPlatformMisc::GetMaxRefreshRate();
					static IConsoleVariable* CsvTargetFrameRateCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("csv.TargetFrameRateOverride"));
					static IConsoleVariable* MaxFPSCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"));
					static IConsoleVariable* SyncIntervalCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("rhi.SyncInterval"));
					int32 CmdLineTargetFPS = TargetFPS;
					if (CsvTargetFrameRateCVar && CsvTargetFrameRateCVar->GetInt() > 0)
					{
						TargetFPS = CsvTargetFrameRateCVar->GetInt();
					}
					else if (FParse::Value(FCommandLine::Get(), TEXT("csv.TargetFrameRateOverride"), CmdLineTargetFPS)) // Too early to set CsvTargetFrameRateCVar with execcmds
					{
						TargetFPS = CmdLineTargetFPS;
					}
					else
					{
						// Figure out the target framerate
						if (MaxFPSCVar && MaxFPSCVar->GetInt() > 0)
						{
							TargetFPS = MaxFPSCVar->GetInt();
						}
						if (SyncIntervalCVar && SyncIntervalCVar->GetInt() > 0)
						{
							TargetFPS = FMath::Min(TargetFPS, FPlatformMisc::GetMaxRefreshRate() / SyncIntervalCVar->GetInt());
						}
					}

					SetMetadataInternal(TEXT("TargetFramerate"), *FString::FromInt(TargetFPS));
					SetMetadataInternal(TEXT("StartTimestamp"), *FString::Printf(TEXT("%lld"), FDateTime::UtcNow().ToUnixTimestamp()));
					SetMetadataInternal(TEXT("NamedEvents"), (GCycleStatsShouldEmitNamedEvents > 0) ? TEXT("1") : TEXT("0"));
					bNamedEventsWasEnabled = (GCycleStatsShouldEmitNamedEvents > 0);

					GCsvStatCounts = !!CVarCsvStatCounts.GetValueOnGameThread();

					// Initialize tls before setting the capturing flag to true.
					FCsvProfilerThreadData::InitTls();
					TRACE_CSV_PROFILER_BEGIN_CAPTURE(*Filename, RenderThreadId, RHIThreadId, GDefaultWaitStatName, GCsvStatCounts);
					GCsvProfilerIsCapturing = true;
				}
			}
		}

		if (GCsvProfilerIsCapturing)
		{
			GFrameBoundaries.AddBeginFrameTimestamp(ECsvTimeline::Gamethread);

			if (CaptureFrameNumber == 0)
			{
				OnCSVProfileFirstFrameDelegate.Broadcast();
			}

			if (!bNamedEventsWasEnabled && (GCycleStatsShouldEmitNamedEvents > 0))
			{
				bNamedEventsWasEnabled = true;
#if !UE_SERVER
				// Servers with -EnableMetrics run perf collection a few times per match which includes
				// insights/framepro captures and namedevents on.
				// We don't want these server csvs to be filtered out by PRS, so
				// excluding this logic from server builds for now.
				SetMetadataInternal(TEXT("NamedEvents"), TEXT("1"));
#endif
			}
		}
	}

#if CSV_PROFILER_ALLOW_DEBUG_FEATURES
	if (GCsvTestingGT)
	{
		CSVTest();
	}

	GCsvABTest.BeginFrameUpdate(CaptureFrameNumber, GCsvProfilerIsCapturing);
#endif // CSV_PROFILER_ALLOW_DEBUG_FEATURES
}

void FCsvProfiler::EndFrame()
{
	LLM_SCOPE(ELLMTag::CsvProfiler);

	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(CsvProfiler);

	check(IsInGameThread());
	if (GCsvProfilerIsCapturing)
	{
		OnCSVProfileEndFrameDelegate.Broadcast();

		GCsvPersistentCustomStats.RecordStats();

		QUICK_SCOPE_CYCLE_COUNTER(STAT_FCsvProfiler_EndFrame_Capturing);
		if (NumFramesToCapture >= 0)
		{
			NumFramesToCapture--;
			if (NumFramesToCapture == 0)
			{
				EndCapture();
			}
		}

		// Record the frametime (measured since the last EndFrame)
		uint64 CurrentTimeStamp = FPlatformTime::Cycles64();
		uint64 ElapsedCycles = CurrentTimeStamp - LastEndFrameTimestamp;
		float ElapsedMs = (float)FPlatformTime::ToMilliseconds64(ElapsedCycles);
		CSV_CUSTOM_STAT_DEFINED(FrameTime, ElapsedMs, ECsvCustomStatOp::Set);

		FPlatformMemoryStats MemoryStats = FPlatformMemory::GetStats();

		float PhysicalMBFree = float(MemoryStats.AvailablePhysical) / (1024.0f * 1024.0f);
		float UsedExtendedMB = 0;
		float PhysicalMBUsed = float(MemoryStats.UsedPhysical) / (1024.0f * 1024.0f);
		float VirtualMBUsed = float(MemoryStats.UsedVirtual) / (1024.0f * 1024.0f);

		// infer the max we can allocate
		float TotalSystemMB = PhysicalMBFree + PhysicalMBUsed;
#if !UE_BUILD_SHIPPING
		// Subtract any extra development memory from physical free. This can result in negative values in cases where we would have crashed OOM
		PhysicalMBFree -= float(FPlatformMemory::GetExtraDevelopmentMemorySize() / 1024ull / 1024ull);
		UsedExtendedMB = PhysicalMBFree < 0.0f ? -PhysicalMBFree : 0;

		TotalSystemMB -= float(FPlatformMemory::GetExtraDevelopmentMemorySize() / 1024ull / 1024ull);
#endif
		
		CSV_CUSTOM_STAT_GLOBAL(MemoryFreeMB, PhysicalMBFree, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_GLOBAL(PhysicalUsedMB, PhysicalMBUsed, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_GLOBAL(VirtualUsedMB, VirtualMBUsed, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_GLOBAL(ExtendedUsedMB, UsedExtendedMB, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT_GLOBAL(SystemMaxMB, TotalSystemMB, ECsvCustomStatOp::Set);

		MemoryStats.SetEndFrameCsvStats();

		// If we're single-threaded, process the stat data here
		if (ProcessingThread == nullptr)
		{
			ProcessStatData();
		}

		LastEndFrameTimestamp = CurrentTimeStamp;
		CaptureFrameNumber++;
	}

	// Process the command queue for stop commands
	FCsvCaptureCommand CurrentCommand;
	if (CommandQueue.Peek(CurrentCommand) && CurrentCommand.CommandType == ECsvCommandType::Stop)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FCsvProfiler_EndFrame_Stop);
		bool bCaptureComplete = false;

		if (!GCsvProfilerIsCapturing && !GCsvProfilerIsWritingFile)
		{
			bCaptureComplete = true;
		}
		else
		{
			// Delay end capture by a frame to allow RT stats to catch up
			if (CurrentCommand.FrameRequested == GCsvProfilerFrameNumber)
			{
				CaptureEndFrameCount = CaptureFrameNumber;
			}
			else
			{
				UE_LOG(LogCsvProfiler, Display, TEXT("Capture Stop requested"));

				// signal external profiler that we are done
				OnCSVProfileEndDelegate.Broadcast();

				// Signal to the processing thread to write the file out (if we have one).
				GCsvProfilerIsWritingFile = true;
				GCsvProfilerIsCapturing = false;

				TRACE_CSV_PROFILER_END_CAPTURE();

				if (!ProcessingThread)
				{
					// Suspend the hang and hitch heartbeats, as this is a long running task.
					FSlowHeartBeatScope SuspendHeartBeat;
					FDisableHitchDetectorScope SuspendGameThreadHitch;

					// No processing thread, block and write the file out on the game thread.
					FinalizeCsvFile();
					bCaptureComplete = true;
				}
				else if (CVarCsvBlockOnCaptureEnd.GetValueOnGameThread() == 1)
				{
					// Suspend the hang and hitch heartbeats, as this is a long running task.
					FSlowHeartBeatScope SuspendHeartBeat;
					FDisableHitchDetectorScope SuspendGameThreadHitch;

					// Block the game thread here whilst the result file is written out.
					FileWriteBlockingEvent->Wait();
				}
			}
		}

		if (bCaptureComplete)
		{
			check(!GCsvProfilerIsCapturing && !GCsvProfilerIsWritingFile);

			// Pop the 'stop' command now that the capture has ended (or we weren't capturing anyway).
			CommandQueue.Dequeue(CurrentCommand);

			// Signal the async completion callback, if one was provided when the capture was stopped.
			if (CurrentCommand.Completion)
			{
				CurrentCommand.Completion->SetValue(OutputFilename);
				delete CurrentCommand.Completion;
			}

			FileWriteBlockingEvent->Reset();

			// No output filename means we weren't running a capture.
			bool bCaptureEnded = true;
			if (OutputFilename.IsEmpty())
			{
				UE_LOG(LogCsvProfiler, Warning, TEXT("Capture Stop requested, but no capture was running!"));
			}
			else
			{
				OutputFilename.Reset();

				// Handle repeats
				if (GCsvRepeatCount != 0 && GCsvRepeatFrameCount > 0)
				{
					if (GCsvRepeatCount > 0)
					{
						GCsvRepeatCount--;
					}
					if (GCsvRepeatCount != 0)
					{
						bCaptureEnded = false;

						// TODO: support directories
						BeginCapture(GCsvRepeatFrameCount);
					}
				}
			}

			if (bCaptureEnded && (GCsvExitOnCompletion || FParse::Param(FCommandLine::Get(), TEXT("ExitAfterCsvProfiling"))))
			{
				bool bForceExit = !!CVarCsvForceExit.GetValueOnGameThread();
				FPlatformMisc::RequestExit(bForceExit, TEXT("CsvProfiler.ExitAfterCsvProfiling"));
			}
		}
	}

	GCsvProfilerFrameNumber++;
}

void FCsvProfiler::OnEndFramePostFork()
{
	// Reinitialize commandline-based configuration
	GCsvUseProcessingThread = FForkProcessHelper::IsForkedMultithreadInstance() && !FParse::Param(FCommandLine::Get(), TEXT("csvNoProcessingThread"));
	GGameThreadIsCsvProcessingThread = !GCsvUseProcessingThread;
	// Make sure no one called BeginCapture() before forking, as the runnable doesn't fully support the transition
	checkf(ProcessingThread == nullptr, TEXT("CSV profiling should not be started pre-fork"));
}

/** Per-frame update */
void FCsvProfiler::BeginFrameRT()
{
	LLM_SCOPE(ELLMTag::CsvProfiler);
	RenderThreadId = FPlatformTLS::GetCurrentThreadId();

	check(IsInRenderingThread());
	if (GCsvProfilerIsCapturing)
	{
		// Mark where the renderthread frames begin
		GFrameBoundaries.AddBeginFrameTimestamp(ECsvTimeline::Renderthread);
	}
	GCsvProfilerIsCapturingRT = GCsvProfilerIsCapturing;

#if CSV_PROFILER_ALLOW_DEBUG_FEATURES
	if (GCsvTestingRT)
	{
		CSVTest();
	}
#endif // CSV_PROFILER_ALLOW_DEBUG_FEATURES

	// Set the thread-local waits enabled flag
	GCsvThreadLocalWaitsEnabled = GCsvTrackWaitsOnRenderThread;
}

void FCsvProfiler::EndFrameRT()
{
	LLM_SCOPE(ELLMTag::CsvProfiler);
	check(IsInRenderingThread());
	if (GCsvProfilerIsCapturing)
	{
		CaptureFrameNumberRT++;
	}
}

void FCsvProfiler::BeginCapture(int InNumFramesToCapture, 
	const FString& InDestinationFolder, 
	const FString& InFilename,
	ECsvProfilerFlags InFlags)
{
	LLM_SCOPE(ELLMTag::CsvProfiler);
	check(IsInGameThread());

	// If there's already a start command in flight for this capture, warn and continue
	FCsvCaptureCommand CurrentCommand;
	if (CommandQueue.Peek(CurrentCommand) && CurrentCommand.CommandType == ECsvCommandType::Start)
	{
		UE_LOG(LogCsvProfiler, Warning, TEXT("BeginCapture() called, but there is already a pending start command. Ignoring!"));
		return;
	}


	CommandQueue.Enqueue(FCsvCaptureCommand(ECsvCommandType::Start, GCsvProfilerFrameNumber, InNumFramesToCapture, InDestinationFolder, InFilename, InFlags));
}

TSharedFuture<FString> FCsvProfiler::EndCapture(FGraphEventRef EventToSignal)
{
	LLM_SCOPE(ELLMTag::CsvProfiler);
	check(IsInGameThread());

	// If there's already a stop command in flight for this capture, warn and continue
	FCsvCaptureCommand CurrentCommand;
	if (CommandQueue.Peek(CurrentCommand) && CurrentCommand.CommandType == ECsvCommandType::Stop)
	{
		UE_LOG(LogCsvProfiler, Warning, TEXT("EndCapture() called, but there is already a pending stop command. Ignoring!"));
		return CurrentCommand.Future;
	}

	if (CommandQueue.Peek(CurrentCommand) && CurrentCommand.CommandType == ECsvCommandType::Start)
	{
		UE_LOG(LogCsvProfiler, Warning, TEXT("EndCapture() called, but there is already a pending start command!"));
	}

	// Fire before we copy the metadata so it gives other systems a chance to write any final information.
	OnCSVProfileEndRequestedDelegate.Broadcast();

	SetNonPersistentMetadata(TEXT("EndTimestamp"), *FString::Printf(TEXT("%lld"), FDateTime::UtcNow().ToUnixTimestamp()));

	TPromise<FString>* Completion = new TPromise<FString>([EventToSignal]()
	{
		if (EventToSignal)
		{
			EventToSignal->DispatchSubsequents();
		}
	});

	// Copy the metadata array for the next FinalizeCsvFile
	TMap<FString, FString> CopyMetadataMap;
	{
		FScopeLock Lock(&MetadataCS);
		CopyMetadataMap = MetadataMap;
		// Merge the metadata
		CopyMetadataMap.Append(MoveTemp(NonPersistentMetadataMap));
		// Clear now that the capture is finished.
		NonPersistentMetadataMap = TMap<FString, FString>();
	}
	MetadataQueue.Enqueue(MoveTemp(CopyMetadataMap));

	TSharedFuture<FString> Future = Completion->GetFuture().Share();
	CommandQueue.Enqueue(FCsvCaptureCommand(ECsvCommandType::Stop, GCsvProfilerFrameNumber, Completion, Future));

	return Future;
}

void FCsvProfiler::FinalizeCsvFile()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FCsvProfiler_FinalizeCsvFile);
	check(IsInCsvProcessingThread());

	UE_LOG(LogCsvProfiler, Display, TEXT("Capture Ending"));

	double FinalizeStartTime = FPlatformTime::Seconds();

	// Do a final process of the stat data
	ProcessStatData();

	uint64 MemoryBytesAtEndOfCapture = CsvWriter->GetAllocatedSize();
	
	// Get the queued metadata for the next csv finalize
	TMap<FString, FString> CurrentMetadata;
	MetadataQueue.Dequeue(CurrentMetadata);

	CsvWriter->Finalize(CurrentMetadata);

	delete CsvWriter;
	CsvWriter = nullptr;

	// TODO - Probably need to clear the frame boundaries after each completed CSV row
	GFrameBoundaries.Clear();

	UE_LOG(LogCsvProfiler, Display, TEXT("Capture Ended. Writing CSV to file : %s"), *OutputFilename);
	UE_LOG(LogCsvProfiler, Display, TEXT("  Frames : %d"), CaptureEndFrameCount);
	UE_LOG(LogCsvProfiler, Display, TEXT("  Peak memory usage  : %.2fMB"), float(MemoryBytesAtEndOfCapture) / (1024.0f * 1024.0f));

	OnCSVProfileFinished().Broadcast(OutputFilename);

	float FinalizeDuration = float(FPlatformTime::Seconds() - FinalizeStartTime);
	UE_LOG(LogCsvProfiler, Display, TEXT("  CSV finalize time : %.3f seconds"), FinalizeDuration);

	GCsvProfilerIsWritingFile = false;
}

void FCsvProfiler::SetDeviceProfileName(FString InDeviceProfileName)
{
	CSV_METADATA(TEXT("DeviceProfile"), *InDeviceProfileName);
}

/** Push/pop events */
void FCsvProfiler::BeginStat(const char * StatName, uint32 CategoryIndex, const char * NamedEventName)
{
#if RECORD_TIMESTAMPS
	if (GCsvProfilerIsCapturing && GCsvCategoriesEnabled[CategoryIndex])
	{
#if CSV_PROFILER_SUPPORT_NAMED_EVENTS
		if (UNLIKELY(GCsvProfilerNamedEventsTiming))
		{
			CsvBeginNamedEvent(FColor(255, 128, 255), NamedEventName ? NamedEventName : StatName);
		}
#endif
		FCsvProfilerThreadData::Get().AddTimestampBegin(StatName, CategoryIndex);
	}
#endif
}

void FCsvProfiler::BeginStat(const FName& StatName, uint32 CategoryIndex)
{
#if RECORD_TIMESTAMPS
	if (GCsvProfilerIsCapturing && GCsvCategoriesEnabled[CategoryIndex])
	{
#if CSV_PROFILER_SUPPORT_NAMED_EVENTS
		if (UNLIKELY(GCsvProfilerNamedEventsTiming))
		{
			CsvBeginNamedEvent(FColor(255, 128, 255), StatName);
		}
#endif
		FCsvProfilerThreadData::Get().AddTimestampBegin(StatName, CategoryIndex);
	}
#endif
}

void FCsvProfiler::EndStat(const char * StatName, uint32 CategoryIndex)
{
#if RECORD_TIMESTAMPS
	if (GCsvProfilerIsCapturing && GCsvCategoriesEnabled[CategoryIndex])
	{
		FCsvProfilerThreadData::Get().AddTimestampEnd(StatName, CategoryIndex);
#if CSV_PROFILER_SUPPORT_NAMED_EVENTS
		if (UNLIKELY(GCsvProfilerNamedEventsTiming))
		{
			CsvEndNamedEvent();
		}
#endif
	}
#endif
}

void FCsvProfiler::EndStat(const FName& StatName, uint32 CategoryIndex)
{
#if RECORD_TIMESTAMPS
	if (GCsvProfilerIsCapturing && GCsvCategoriesEnabled[CategoryIndex])
	{
		FCsvProfilerThreadData::Get().AddTimestampEnd(StatName, CategoryIndex);
#if CSV_PROFILER_SUPPORT_NAMED_EVENTS
		if (UNLIKELY(GCsvProfilerNamedEventsTiming))
		{
			CsvEndNamedEvent();
		}
#endif
	}
#endif
}

void FCsvProfiler::BeginExclusiveStat(const char * StatName, const char * NamedEventName)
{
#if RECORD_TIMESTAMPS
	if (GCsvProfilerIsCapturing && GCsvCategoriesEnabled[CSV_CATEGORY_INDEX(Exclusive)])
	{
#if CSV_PROFILER_SUPPORT_NAMED_EVENTS
		if (UNLIKELY(GCsvProfilerNamedEventsExclusive))
		{
			CsvBeginNamedEvent(FColor(255, 128, 128), NamedEventName ? NamedEventName : StatName);
		}
#endif
		FCsvProfilerThreadData::Get().AddTimestampExclusiveBegin(StatName);
	}
#endif
}

void FCsvProfiler::EndExclusiveStat(const char * StatName)
{
#if RECORD_TIMESTAMPS
	if (GCsvProfilerIsCapturing && GCsvCategoriesEnabled[CSV_CATEGORY_INDEX(Exclusive)])
	{
		FCsvProfilerThreadData::Get().AddTimestampExclusiveEnd(StatName);
#if CSV_PROFILER_SUPPORT_NAMED_EVENTS
		if (UNLIKELY(GCsvProfilerNamedEventsExclusive))
		{
			CsvEndNamedEvent();
		}
#endif
	}
#endif
}


void FCsvProfiler::BeginSetWaitStat(const char * StatName)
{
#if RECORD_TIMESTAMPS
	if (GCsvProfilerIsCapturing && GCsvCategoriesEnabled[CSV_CATEGORY_INDEX(Exclusive)])
	{
#if CSV_PROFILER_SUPPORT_NAMED_EVENTS
		if (UNLIKELY(GCsvProfilerNamedEventsExclusive))
		{
			FPlatformMisc::BeginNamedEvent(FColor::Yellow, *FString::Printf(TEXT("CsvWaitStat_%s"), StringCast<TCHAR>(StatName).Get()));
		}
#endif
		FCsvProfilerThreadData::Get().PushWaitStatName(StatName == nullptr ? GIgnoreWaitStatName : StatName);
	}
#endif
}

void FCsvProfiler::EndSetWaitStat()
{
#if RECORD_TIMESTAMPS
	if (GCsvProfilerIsCapturing && GCsvCategoriesEnabled[CSV_CATEGORY_INDEX(Exclusive)])
	{
#if CSV_PROFILER_SUPPORT_NAMED_EVENTS
		if (UNLIKELY(GCsvProfilerNamedEventsExclusive))
		{
			FPlatformMisc::EndNamedEvent();
		}
#endif
		FCsvProfilerThreadData::Get().PopWaitStatName();
	}
#endif
}

void FCsvProfiler::BeginWait()
{
#if RECORD_TIMESTAMPS
	if (GCsvProfilerIsCapturing && GCsvCategoriesEnabled[CSV_CATEGORY_INDEX(Exclusive)])
	{
		const char* WaitStatName = FCsvProfilerThreadData::Get().GetWaitStatName();
		if (WaitStatName != GIgnoreWaitStatName)
		{
#if CSV_PROFILER_SUPPORT_NAMED_EVENTS
			if (UNLIKELY(GCsvProfilerNamedEventsExclusive))
			{
				if ( FThreadIdleStats::Get().IsCriticalPath() )
				{
					CsvBeginNamedEvent(FColor(192, 96, 96), "CsvEventWait");
				}
				else
				{
					CsvBeginNamedEvent(FColor(255, 128, 128), "CsvEventWait (Non-CP)");
				}
			}
#endif
			FCsvProfilerThreadData::Get().AddTimestampExclusiveBegin(WaitStatName);
		}
	}
#endif
}

void FCsvProfiler::EndWait()
{
#if RECORD_TIMESTAMPS
	if (GCsvProfilerIsCapturing && GCsvCategoriesEnabled[CSV_CATEGORY_INDEX(Exclusive)])
	{
		const char* WaitStatName = FCsvProfilerThreadData::Get().GetWaitStatName();
		if (WaitStatName != GIgnoreWaitStatName)
		{
			FCsvProfilerThreadData::Get().AddTimestampExclusiveEnd(FCsvProfilerThreadData::Get().GetWaitStatName());
#if CSV_PROFILER_SUPPORT_NAMED_EVENTS
			if (UNLIKELY(GCsvProfilerNamedEventsExclusive))
			{
				CsvEndNamedEvent();
			}
#endif
		}
	}
#endif
}


void FCsvProfiler::RecordEventfInternal(int32 CategoryIndex, const TCHAR* Fmt, ...)
{
	bool bIsCsvRecording = GCsvProfilerIsCapturing && GCsvCategoriesEnabled[CategoryIndex];
	if (bIsCsvRecording || GStartOnEvent)
	{
		LLM_SCOPE(ELLMTag::CsvProfiler);
		TCHAR Buffer[256];
		GET_TYPED_VARARGS(TCHAR, Buffer, UE_ARRAY_COUNT(Buffer), UE_ARRAY_COUNT(Buffer) - 1, Fmt, Fmt);
		Buffer[255] = '\0';
		FString Str = Buffer;

		if (bIsCsvRecording)
		{
			RecordEvent(CategoryIndex, Str);

			if (GStopOnEvent && GStopOnEvent->Equals(Str, ESearchCase::IgnoreCase))
			{
				FCsvProfiler::Get()->EndCapture();
			}
		}
		else
		{
			if (GStartOnEvent && GStartOnEvent->Equals(Str, ESearchCase::IgnoreCase))
			{
				FCsvProfiler::Get()->BeginCapture(FCsvProfiler::Get()->GetNumFrameToCaptureOnEvent());
			}
		}
	}
}

void FCsvProfiler::RecordEvent(int32 CategoryIndex, const FString& EventText)
{
	if (GCsvProfilerIsCapturing && GCsvCategoriesEnabled[CategoryIndex])
	{
		LLM_SCOPE(ELLMTag::CsvProfiler);
		UE_LOG(LogCsvProfiler, Display, TEXT("CSVEvent \"%s\" [Frame %d]"), *EventText, FCsvProfiler::Get()->GetCaptureFrameNumber());
		FCsvProfilerThreadData::Get().AddEvent(EventText, CategoryIndex);
	}
}

void FCsvProfiler::RecordEventAtFrameStart(int32 CategoryIndex, const FString& EventText)
{
	RecordEventAtTimestamp(CategoryIndex, EventText, FCsvProfiler::Get()->LastEndFrameTimestamp);
}

void FCsvProfiler::SetMetadata(const TCHAR* Key, const TCHAR* Value)
{
	FCsvProfiler::Get()->SetMetadataInternal(Key, Value, true, EMetadataPersistenceType::Persistent);
}

void FCsvProfiler::SetNonPersistentMetadata(const TCHAR* Key, const TCHAR* Value)
{
	FCsvProfiler::Get()->SetMetadataInternal(Key, Value, true, EMetadataPersistenceType::NonPersistent);
}

TMap<FString, FString> FCsvProfiler::GetMetadataMapCopy()
{
	LLM_SCOPE(ELLMTag::CsvProfiler);
	FScopeLock Lock(&MetadataCS);
	TMap<FString, FString> MetadataMapCopy = MetadataMap;
	MetadataMapCopy.Append(NonPersistentMetadataMap);
	return MetadataMapCopy;
}

void FCsvProfiler::SetMetadataInternal(const TCHAR* Key, const TCHAR* Value, bool bSanitize, EMetadataPersistenceType PersistenceType)
{
	// Always gather CSV metadata, even if we're not currently capturing.
	// Metadata is applied to the next CSV profile, when the file is written.
	LLM_SCOPE(ELLMTag::CsvProfiler);
	FString KeyLower = FString(Key).ToLower();

	TMap<FString, FString>& CurrentMetadataMap = PersistenceType == EMetadataPersistenceType::Persistent ? MetadataMap : NonPersistentMetadataMap;
	if (Value == nullptr)
	{
		FScopeLock Lock(&MetadataCS);
		if (CurrentMetadataMap.Contains(KeyLower))
		{
			UE_LOG(LogCsvProfiler, Display, TEXT("Metadata unset : %s"), *KeyLower);
			CurrentMetadataMap.Remove(KeyLower);
		}
	}
	else
	{
		TRACE_CSV_PROFILER_METADATA(Key, Value);
		FString ValueStr = Value;
		if (bSanitize)
		{
			check(!KeyLower.Contains(TEXT(",")));
			if (ValueStr.ReplaceInline(TEXT(","), TEXT("&#44;")) > 0)
			{
				UE_LOG(LogCsvProfiler, Warning, TEXT("Metadata value sanitized due to invalid characters: %s=\"%s\""), *KeyLower, Value);
			}
		}
		// Only log if the metadata changed, to prevent logspam 
		FScopeLock Lock(&MetadataCS);
		if (!CurrentMetadataMap.Contains(KeyLower) || CurrentMetadataMap[KeyLower] != ValueStr)
		{
			UE_LOG(LogCsvProfiler, Display, TEXT("Metadata set : %s=\"%s\""), *KeyLower, *ValueStr);
		}
		CurrentMetadataMap.FindOrAdd(KeyLower) = ValueStr;
	}
}

void FCsvProfiler::RecordEventAtTimestamp(int32 CategoryIndex, const FString& EventText, uint64 Cycles64)
{
	if (GCsvProfilerIsCapturing && GCsvCategoriesEnabled[CategoryIndex])
	{
		LLM_SCOPE(ELLMTag::CsvProfiler);
		UE_LOG(LogCsvProfiler, Display, TEXT("CSVEvent [Frame %d] : \"%s\""), FCsvProfiler::Get()->GetCaptureFrameNumber(), *EventText);
		FCsvProfilerThreadData::Get().AddEventWithTimestamp(EventText, CategoryIndex,Cycles64);

		if (IsContinuousWriteEnabled(false))
		{
			UE_LOG(LogCsvProfiler, Warning, 
				TEXT("RecordEventAtTimestamp is not compatible with continuous CSV writing. ")
				TEXT("Some events may be missing in the output file. Set 'csv.ContinuousWrites' ")
				TEXT("to 0 to ensure events recorded with specific timestamps are captured correctly."));
		}
	}
}

void FCsvProfiler::RecordCustomStat(const char * StatName, uint32 CategoryIndex, float Value, const ECsvCustomStatOp CustomStatOp)
{
	if (GCsvProfilerIsCapturing && GCsvCategoriesEnabled[CategoryIndex])
	{
		FCsvProfilerThreadData::Get().AddCustomStat(StatName, CategoryIndex, Value, CustomStatOp);
	}
}

void FCsvProfiler::RecordCustomStat(const char* StatName, uint32 CategoryIndex, double Value, const ECsvCustomStatOp CustomStatOp)
{
	// LWC_TODO: Double support for FCsvProfiler::RecordCustomStat
	RecordCustomStat(StatName, CategoryIndex, (float)Value, CustomStatOp);
}

void FCsvProfiler::RecordCustomStat(const FName& StatName, uint32 CategoryIndex, float Value, const ECsvCustomStatOp CustomStatOp)
{
	if (GCsvProfilerIsCapturing && GCsvCategoriesEnabled[CategoryIndex])
	{
		FCsvProfilerThreadData::Get().AddCustomStat(StatName, CategoryIndex, Value, CustomStatOp);
	}
}

void FCsvProfiler::RecordCustomStat(const FName& StatName, uint32 CategoryIndex, double Value, const ECsvCustomStatOp CustomStatOp)
{
	// LWC_TODO: Double support for FCsvProfiler::RecordCustomStat
	RecordCustomStat(StatName, CategoryIndex, (float)Value, CustomStatOp);
}


void FCsvProfiler::RecordCustomStat(const char * StatName, uint32 CategoryIndex, int32 Value, const ECsvCustomStatOp CustomStatOp)
{
	if (GCsvProfilerIsCapturing && GCsvCategoriesEnabled[CategoryIndex])
	{
		FCsvProfilerThreadData::Get().AddCustomStat(StatName, CategoryIndex, Value, CustomStatOp);
	}
}

void FCsvProfiler::RecordCustomStat(const FName& StatName, uint32 CategoryIndex, int32 Value, const ECsvCustomStatOp CustomStatOp)
{
	if (GCsvProfilerIsCapturing && GCsvCategoriesEnabled[CategoryIndex])
	{
		FCsvProfilerThreadData::Get().AddCustomStat(StatName, CategoryIndex, Value, CustomStatOp);
	}
}

void FCsvProfiler::Init()
{
#if CSV_PROFILER_ALLOW_DEBUG_FEATURES
	FParse::Value(FCommandLine::Get(), TEXT("csvCaptureOnEventFrameCount="), CaptureOnEventFrameCount);

	GStartOnEvent = new FString();
	FParse::Value(FCommandLine::Get(), TEXT("csvStartOnEvent="), *GStartOnEvent);

	if (GStartOnEvent->IsEmpty())
	{
		delete GStartOnEvent;
		GStartOnEvent = nullptr;
	}

	GStopOnEvent = new FString();
	FParse::Value(FCommandLine::Get(), TEXT("csvStopOnEvent="), *GStopOnEvent);

	if (GStopOnEvent->IsEmpty())
	{
		delete GStopOnEvent;
		GStopOnEvent = nullptr;
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("csvGpuStats")))
	{
		IConsoleVariable* CVarGPUCsvStatsEnabled = IConsoleManager::Get().FindConsoleVariable(TEXT("r.GPUCsvStatsEnabled"));
		if (CVarGPUCsvStatsEnabled)
		{
			CVarGPUCsvStatsEnabled->Set(1);
		}
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("csvTest")))
	{
		GCsvTestingGT = true;
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("csvTestMT")))
	{
		GCsvTestingGT = true;
		GCsvTestingRT = true;
	}

	FString CsvCategoriesStr;
	if (FParse::Value(FCommandLine::Get(), TEXT("csvCategories="), CsvCategoriesStr, /*bShouldStopOnSeparator*/false))
	{
		TArray<FString> CsvCategories;
		CsvCategoriesStr.ParseIntoArray(CsvCategories, TEXT(","), true);
		for (int i = 0; i < CsvCategories.Num(); i++)
		{
			EnableCategoryByString(CsvCategories[i]);
		}
	}

	FString CsvMetadataStr;
	if (FParse::Value(FCommandLine::Get(), TEXT("csvMetadata="), CsvMetadataStr, false))
	{ 
		TArray<FString> CsvMetadataList;
		CsvMetadataStr.ParseIntoArray(CsvMetadataList, TEXT(","), true);
		for (int i = 0; i < CsvMetadataList.Num(); i++)
		{
			const FString& Metadata = CsvMetadataList[i];
			FString Key;
			FString Value;
			if (Metadata.Split(TEXT("="), &Key, &Value))
			{
				SetMetadata(*Key, *Value);
			}
		}
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("csvNoProcessingThread")))
	{
		GCsvUseProcessingThread = false;
	}
#if CSV_PROFILER_SUPPORT_NAMED_EVENTS
	if (FParse::Param(FCommandLine::Get(), TEXT("csvNamedEvents")))
	{
		GCsvProfilerNamedEventsExclusive = true;
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("csvNamedEventsTiming")))
	{
		GCsvProfilerNamedEventsTiming = true;
	}
#endif
	if (FParse::Param(FCommandLine::Get(), TEXT("csvStatCounts")))
	{
		CVarCsvStatCounts.AsVariable()->Set(1);
	}
	int32 NumCsvFrames = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("csvCaptureFrames="), NumCsvFrames))
	{
		check(IsInGameThread());
		BeginCapture(NumCsvFrames);

		// Call BeginFrame() to start capturing a dummy first "frame"
		// signal bInsertEndFrameAtFrameStart to insert an EndFrame() at the start of the first _real_ frame
		// We also add a FrameBeginTimestampsRT timestamp here, to create a dummy renderthread frame, to ensure the rows match up in the CSV
		BeginFrame();
		GFrameBoundaries.AddBeginFrameTimestamp(ECsvTimeline::Renderthread, false);
		bInsertEndFrameAtFrameStart = true;
	}
	FParse::Value(FCommandLine::Get(), TEXT("csvRepeat="), GCsvRepeatCount);

	int32 CompressionMode;
	if (FParse::Value(FCommandLine::Get(), TEXT("csvCompression="), CompressionMode))
	{
		switch (CompressionMode)
		{
		case 0: CVarCsvCompressionMode->Set(0); break;
		case 1: CVarCsvCompressionMode->Set(1); break;
		default:
			UE_LOG(LogCsvProfiler, Warning, TEXT("Invalid command line compression mode \"%d\"."), CompressionMode);
			break;
		}
	}
	GCsvABTest.InitFromCommandline();

	// Handle -csvExeccmds
	FString CsvExecCommandsStr;
	if (FParse::Value(FCommandLine::Get(), TEXT("-csvExecCmds="), CsvExecCommandsStr, false))
	{
		GCsvFrameExecCmds = new TMap<uint32, TArray<FString>>();

		TArray<FString> CsvExecCommandsList;
		if (CsvExecCommandsStr.ParseIntoArray(CsvExecCommandsList, TEXT(","), true) > 0)
		{
			for (FString FrameAndCommand : CsvExecCommandsList)
			{
				int32 ColonIndex = -1;
				if (FrameAndCommand.FindChar(TEXT(':'), ColonIndex))
				{
					FString FrameStr = FrameAndCommand.Mid(0,ColonIndex);
					FString CommandStr = FrameAndCommand.Mid(ColonIndex+1);
					uint32 Frame = FCString::Atoi(*FrameStr);
					if (!GCsvFrameExecCmds->Find(Frame))
					{
						GCsvFrameExecCmds->Add(Frame, TArray<FString>());
					}
					(*GCsvFrameExecCmds)[Frame].Add(CommandStr);
					UE_LOG(LogCsvProfiler, Display, TEXT("Added CsvExecCommand - frame %d : %s"), Frame, *CommandStr);
				}
			}
		}
	}

#endif // CSV_PROFILER_ALLOW_DEBUG_FEATURES

	// Always disable the CSV profiling thread if the platform does not support threading.
	if (!FPlatformProcess::SupportsMultithreading())
	{
		GCsvUseProcessingThread = false;
	}

	if (GConfig != nullptr && GConfig->IsReadyForUse())
	{
		CsvProfilerReadConfig();
	}
	else
	{
		FCoreDelegates::OnInit.AddStatic(CsvProfilerReadConfig);
	}
}

bool FCsvProfiler::IsCapturing() const
{
	check(IsInGameThread());
	return GCsvProfilerIsCapturing;
}

bool FCsvProfiler::IsWritingFile() const
{
	check(IsInGameThread());
	return GCsvProfilerIsWritingFile;
}

bool FCsvProfiler::IsEndCapturePending() const
{
	check(IsInGameThread());
	// Return true if the next command is Stop. If the next command is Start then we ignore, since any further Stop command corresponds to a different capture
	FCsvCaptureCommand CurrentCommand;
	return CommandQueue.Peek(CurrentCommand) && CurrentCommand.CommandType == ECsvCommandType::Stop;
}


bool FCsvProfiler::IsWaitTrackingEnabledOnCurrentThread()
{
	return GCsvTrackWaitsOnAllThreads || GCsvThreadLocalWaitsEnabled;
}

/*Get the current frame capture count*/
int32 FCsvProfiler::GetCaptureFrameNumber() const
{
	return CaptureFrameNumber;
}

int32 FCsvProfiler::GetCaptureFrameNumberRT() const
{
	return CaptureFrameNumberRT;
}


//Get the total frame to capture when we are capturing on event. 
//Example:  -csvStartOnEvent="My Event"
//			-csvCaptureOnEventFrameCount=2500
int32 FCsvProfiler::GetNumFrameToCaptureOnEvent() const
{
	return CaptureOnEventFrameCount;
}

bool FCsvProfiler::EnableCategoryByString(const FString& CategoryName) const
{
	int32 Category = FCsvCategoryData::Get()->GetCategoryIndex(CategoryName);
	if (Category >= 0)
	{
		UE_LOG(LogCsvProfiler, Log, TEXT("Enabled category %s"), *CategoryName);
		GCsvCategoriesEnabled[Category] = true;
		return true;
	}
	UE_LOG(LogCsvProfiler, Warning, TEXT("Error: Can't find category %s"), *CategoryName);
	return false;
}

void FCsvProfiler::EnableCategoryByIndex(uint32 CategoryIndex, bool bEnable) const
{
	check(CategoryIndex < CSV_MAX_CATEGORY_COUNT);
	GCsvCategoriesEnabled[CategoryIndex] = bEnable;
}

bool FCsvProfiler::IsCategoryEnabled(uint32 CategoryIndex) const
{
	check(CategoryIndex < CSV_MAX_CATEGORY_COUNT);
	return GCsvCategoriesEnabled[CategoryIndex];
}

bool FCsvProfiler::IsCapturing_Renderthread() const
{
	check(IsInParallelRenderingThread());
	return GCsvProfilerIsCapturingRT;
}

float FCsvProfiler::ProcessStatData()
{
	check(IsInCsvProcessingThread());

	QUICK_SCOPE_CYCLE_COUNTER(STAT_FCsvProfiler_ProcessStatData);

	float ElapsedMS = 0.0f;
	if (!IsShuttingDown.GetValue())
	{
		double StartTime = FPlatformTime::Seconds();

		FCsvProcessThreadDataStats Stats;
		if (CsvWriter)
		{
			CsvWriter->Process(Stats);
		}
		ElapsedMS = float(FPlatformTime::Seconds() - StartTime) * 1000.0f;
		CSV_CUSTOM_STAT(CsvProfiler, NumTimestampsProcessed, (int32)Stats.TimestampCount, ECsvCustomStatOp::Accumulate);
		CSV_CUSTOM_STAT(CsvProfiler, NumCustomStatsProcessed, (int32)Stats.CustomStatCount, ECsvCustomStatOp::Accumulate);
		CSV_CUSTOM_STAT(CsvProfiler, NumEventsProcessed, (int32)Stats.EventCount, ECsvCustomStatOp::Accumulate);
		CSV_CUSTOM_STAT(CsvProfiler, ProcessCSVStats, ElapsedMS, ECsvCustomStatOp::Accumulate);
	}
	return ElapsedMS;
}

TCsvPersistentCustomStat<int32>* FCsvProfiler::GetOrCreatePersistentCustomStatInt(FName Name, int32 CategoryIndex, bool bResetEachFrame)
{
	return GCsvPersistentCustomStats.GetOrCreatePersistentCustomStat<int32>(Name, CategoryIndex, bResetEachFrame);
}

TCsvPersistentCustomStat<float>* FCsvProfiler::GetOrCreatePersistentCustomStatFloat(FName Name, int32 CategoryIndex, bool bResetEachFrame)
{
	return GCsvPersistentCustomStats.GetOrCreatePersistentCustomStat<float>(Name, CategoryIndex, bResetEachFrame);
}

#if CSV_PROFILER_ALLOW_DEBUG_FEATURES

// Simple benchmarking and debugging tests for the csv profiler. Enable with -csvtest, e.g -csvtest -csvcaptureframes=400
void CSVTest()
{
	TCsvPersistentCustomStat<float>* PersistentStatFloat = FCsvProfiler::Get()->GetOrCreatePersistentCustomStatFloat(TEXT("PersistentStatFloat"));
	PersistentStatFloat->Add(0.15f);
	PersistentStatFloat->Sub(0.1f);

	TCsvPersistentCustomStat<int>* PersistentStatInt = FCsvProfiler::Get()->GetOrCreatePersistentCustomStatInt(TEXT("PersistentStatInt"), CSV_CATEGORY_INDEX(CsvTest));
	PersistentStatInt->Add(15);
	PersistentStatInt->Sub(1);

	uint32 FrameNumber = FCsvProfiler::Get()->GetCaptureFrameNumber();
	CSV_SCOPED_TIMING_STAT(CsvTest, CsvTestStat);
	CSV_CUSTOM_STAT(CsvTest, CaptureFrameNumber, int32(FrameNumber), ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(CsvTest, SameCustomStat, 1, ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(CsvTest, SameCustomStat, 1, ECsvCustomStatOp::Accumulate);
	for (int i = 0; i < 3; i++)
	{
		CSV_SCOPED_TIMING_STAT(CsvTest, RepeatStat1MS);
		FPlatformProcess::Sleep(0.001f);
	}

	{
		// This stat measures the overhead of submitting 10k timing stat scopes in a frame. 
		// Multiply the ms result by 100 to get the per-scope cost in ns
		// (currently ~150ns/scope on last-gen consoles if CSVPROFILERTRACE_ENABLED is 0)
		// Note that each scope emits two timestamps, so the per-timestamp cost is half this
		CSV_SCOPED_TIMING_STAT(CsvTest, TimerStatTimer);
		for (int i = 0; i < 2500; i++)
		{
			CSV_SCOPED_TIMING_STAT(CsvTest, BeginEndbenchmarkInner0);
			CSV_SCOPED_TIMING_STAT(CsvTest, BeginEndbenchmarkInner1);
			CSV_SCOPED_TIMING_STAT(CsvTest, BeginEndbenchmarkInner2);
			CSV_SCOPED_TIMING_STAT(CsvTest, BeginEndbenchmarkInner3);
		}
	}

	{
		CSV_SCOPED_TIMING_STAT(CsvTest, CustomStatTimer);
		for (int i = 0; i < 100; i++)
		{
			CSV_CUSTOM_STAT(CsvTest, SetStat_99, i, ECsvCustomStatOp::Set); // Should be 99
			CSV_CUSTOM_STAT(CsvTest, MaxStat_99, 99 - i, ECsvCustomStatOp::Max); // Should be 99
			CSV_CUSTOM_STAT(CsvTest, MinStat_0, i, ECsvCustomStatOp::Min); // Should be 0
			CSV_CUSTOM_STAT(CsvTest, AccStat_4950, i, ECsvCustomStatOp::Accumulate); // Should be 4950
		}
		if (FrameNumber > 100)
		{
			CSV_SCOPED_TIMING_STAT(CsvTest, TimerOver100);
			CSV_CUSTOM_STAT(CsvTest, CustomStatOver100, int32(FrameNumber - 100), ECsvCustomStatOp::Set);
		}
	}
	{
		CSV_SCOPED_TIMING_STAT(CsvTest, EventTimer);
		if (FrameNumber % 20 < 2)
		{
			CSV_EVENT(CsvTest, TEXT("This is frame %d"), GFrameNumber);
		}
		if (FrameNumber % 50 == 0)
		{
			for (int i = 0; i < 5; i++)
			{
				CSV_EVENT(CsvTest, TEXT("Multiple Event %d"), i);
			}
		}
	}
	//for (int i = 0; i < 2048; i++)
	//{
	//	GCsvCategoriesEnabled[i] = false;
	//} 
	//GCsvCategoriesEnabled[CSV_CATEGORY_INDEX(Exclusive)] = true;
	//GCsvCategoriesEnabled[CSV_CATEGORY_INDEX(CsvTest)] = true;

	{
		CSV_SCOPED_TIMING_STAT_EXCLUSIVE(ExclusiveLevel0);
		{
			CSV_SCOPED_TIMING_STAT_EXCLUSIVE(ExclusiveLevel1);
			CSV_SCOPED_TIMING_STAT(CsvTest, NonExclusiveTestLevel1);
			FPlatformProcess::Sleep(0.002f);
			{
				CSV_SCOPED_TIMING_STAT_EXCLUSIVE(ExclusiveLevel2);
				CSV_SCOPED_TIMING_STAT(CsvTest, NonExclusiveTestLevel2);
				FPlatformProcess::Sleep(0.003f);
			}
		}
		FPlatformProcess::Sleep(0.001f);
	}
	{
		CSV_SCOPED_TIMING_STAT(CsvTest, ExclusiveTimerStatTimer);
		for (int i = 0; i < 100; i++)
		{
			CSV_SCOPED_TIMING_STAT_EXCLUSIVE(ExclusiveBeginEndbenchmarkInner0);
			CSV_SCOPED_TIMING_STAT_EXCLUSIVE(ExclusiveBeginEndbenchmarkInner1);
			CSV_SCOPED_TIMING_STAT_EXCLUSIVE(ExclusiveBeginEndbenchmarkInner2);
			CSV_SCOPED_TIMING_STAT_EXCLUSIVE(ExclusiveBeginEndbenchmarkInner3);
		}
	}

}

#endif // CSV_PROFILER_ALLOW_DEBUG_FEATURES

#endif // CSV_PROFILER
