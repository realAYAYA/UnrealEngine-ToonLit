// Copyright Epic Games, Inc. All Rights Reserved.

/**
*
* A lightweight multi-threaded CSV profiler which can be used for profiling in Test/Shipping builds
*/

#pragma once

#include "Async/Future.h"
#include "Async/TaskGraphFwd.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/Queue.h"
#include "Containers/SparseArray.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Delegates/Delegate.h"
#include "HAL/CriticalSection.h"
#include "HAL/PlatformCrt.h"
#include "HAL/PreprocessorHelpers.h"
#include "HAL/ThreadSafeCounter.h"
#include "Misc/EnumClassFlags.h"
#include "ProfilingDebugging/CsvProfilerConfig.h"
#include "ProfilingDebugging/CsvProfilerTrace.h"
#include "ProfilingDebugging/MiscTrace.h"
#include "Templates/IsArrayOrRefOfTypeByPredicate.h"
#include "Templates/IsValidVariadicFunctionArg.h"
#include "Templates/RefCounting.h"
#include "Templates/UniquePtr.h"
#include "Templates/UnrealTemplate.h"
#include "Traits/IsCharEncodingCompatibleWith.h"
#include "UObject/NameTypes.h"

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Async/TaskGraphInterfaces.h"
#endif


#include <atomic>

class FCsvProfiler;
class FEvent;
class FScopedCsvStat;
class FScopedCsvStatExclusive;
struct FCsvDeclaredStat;

#if CSV_PROFILER

// Helpers
#define CSV_CATEGORY_INDEX(CategoryName)						(_GCsvCategory_##CategoryName.Index)
#define CSV_CATEGORY_INDEX_GLOBAL								(0)
#define CSV_STAT_FNAME(StatName)								(_GCsvStat_##StatName.Name)

// Inline stats (no up front definition)
#define CSV_SCOPED_TIMING_STAT(Category,StatName) \
	TRACE_CSV_PROFILER_INLINE_STAT(#StatName, CSV_CATEGORY_INDEX(Category)); \
	FScopedCsvStat _ScopedCsvStat_ ## StatName (#StatName, CSV_CATEGORY_INDEX(Category), "CSV_"#StatName);
#define CSV_SCOPED_TIMING_STAT_GLOBAL(StatName) \
	TRACE_CSV_PROFILER_INLINE_STAT(#StatName, CSV_CATEGORY_INDEX_GLOBAL); \
	FScopedCsvStat _ScopedCsvStat_ ## StatName (#StatName, CSV_CATEGORY_INDEX_GLOBAL, "CSV_"#StatName);
#define CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StatName) \
	TRACE_CSV_PROFILER_INLINE_STAT_EXCLUSIVE(#StatName); \
	FScopedCsvStatExclusive _ScopedCsvStatExclusive_ ## StatName (#StatName, "CSV_"#StatName);
#define CSV_SCOPED_TIMING_STAT_EXCLUSIVE_CONDITIONAL(StatName,Condition) \
	TRACE_CSV_PROFILER_INLINE_STAT_EXCLUSIVE(#StatName); \
	FScopedCsvStatExclusiveConditional _ScopedCsvStatExclusive_ ## StatName (#StatName,Condition, "CSV_"#StatName);
#define CSV_SCOPED_TIMING_STAT_RECURSIVE(Category,StatName) \
	TRACE_CSV_PROFILER_INLINE_STAT(#StatName, CSV_CATEGORY_INDEX(Category)); \
	static thread_local int32 _ScopedCsvStatRecursive_EntryCount_ ## StatName = 0; \
	FScopedCsvStatRecursive _ScopedCsvStatRecursive_ ## StatName (_ScopedCsvStatRecursive_EntryCount_ ## StatName, #StatName, CSV_CATEGORY_INDEX(Category), "CSV_"#StatName);
#define CSV_SCOPED_TIMING_STAT_RECURSIVE_CONDITIONAL(Category,StatName,Condition) \
	TRACE_CSV_PROFILER_INLINE_STAT(#StatName, CSV_CATEGORY_INDEX(Category)); \
	static thread_local int32 _ScopedCsvStatRecursive_EntryCount_ ## StatName = 0; \
	FScopedCsvStatRecursiveConditional _ScopedCsvStatRecursive_ ## StatName (_ScopedCsvStatRecursive_EntryCount_ ## StatName, #StatName, CSV_CATEGORY_INDEX(Category), Condition, "CSV_"#StatName);

#define CSV_SCOPED_WAIT(WaitTime)							FScopedCsvWaitConditional _ScopedCsvWait(WaitTime>0 && FCsvProfiler::IsWaitTrackingEnabledOnCurrentThread());
#define CSV_SCOPED_WAIT_CONDITIONAL(Condition)				FScopedCsvWaitConditional _ScopedCsvWait(Condition);

#define CSV_SCOPED_SET_WAIT_STAT(StatName) \
	TRACE_CSV_PROFILER_INLINE_STAT_EXCLUSIVE("EventWait/"#StatName); \
	FScopedCsvSetWaitStat _ScopedCsvSetWaitStat ## StatName("EventWait/"#StatName);

#define CSV_SCOPED_SET_WAIT_STAT_IGNORE()						FScopedCsvSetWaitStat _ScopedCsvSetWaitStat ## StatName();

#define CSV_CUSTOM_STAT(Category,StatName,Value,Op) \
	TRACE_CSV_PROFILER_INLINE_STAT(#StatName, CSV_CATEGORY_INDEX(Category)); \
	FCsvProfiler::RecordCustomStat(#StatName, CSV_CATEGORY_INDEX(Category), Value, Op);
#define CSV_CUSTOM_STAT_GLOBAL(StatName,Value,Op) \
	TRACE_CSV_PROFILER_INLINE_STAT(#StatName, CSV_CATEGORY_INDEX_GLOBAL); \
	FCsvProfiler::RecordCustomStat(#StatName, CSV_CATEGORY_INDEX_GLOBAL, Value, Op); 

// Stats declared up front
#define CSV_DEFINE_STAT(Category,StatName)						FCsvDeclaredStat _GCsvStat_##StatName((TCHAR*)TEXT(#StatName), CSV_CATEGORY_INDEX(Category));
#define CSV_DEFINE_STAT_GLOBAL(StatName)						FCsvDeclaredStat _GCsvStat_##StatName((TCHAR*)TEXT(#StatName), CSV_CATEGORY_INDEX_GLOBAL);
#define CSV_DECLARE_STAT_EXTERN(Category,StatName)				extern FCsvDeclaredStat _GCsvStat_##StatName
#define CSV_CUSTOM_STAT_DEFINED(StatName,Value,Op)				FCsvProfiler::RecordCustomStat(_GCsvStat_##StatName.Name, _GCsvStat_##StatName.CategoryIndex, Value, Op);

// Categories
#define CSV_DEFINE_CATEGORY(CategoryName,bDefaultValue)			FCsvCategory _GCsvCategory_##CategoryName(TEXT(#CategoryName),bDefaultValue)
#define CSV_DECLARE_CATEGORY_EXTERN(CategoryName)				extern FCsvCategory _GCsvCategory_##CategoryName

#define CSV_DEFINE_CATEGORY_MODULE(Module_API,CategoryName,bDefaultValue)	FCsvCategory Module_API _GCsvCategory_##CategoryName(TEXT(#CategoryName),bDefaultValue)
#define CSV_DECLARE_CATEGORY_MODULE_EXTERN(Module_API,CategoryName)			extern Module_API FCsvCategory _GCsvCategory_##CategoryName

// Events
#define CSV_EVENT(Category, Format, ...) \
	FCsvProfiler::RecordEventf( CSV_CATEGORY_INDEX(Category), Format, ##__VA_ARGS__ ); \
	TRACE_BOOKMARK(TEXT(PREPROCESSOR_TO_STRING(Category)) TEXT("/") Format, ##__VA_ARGS__)

#define CSV_EVENT_GLOBAL(Format, ...) \
	FCsvProfiler::RecordEventf( CSV_CATEGORY_INDEX_GLOBAL, Format, ##__VA_ARGS__ ); \
	TRACE_BOOKMARK(Format, ##__VA_ARGS__)

// Metadata
#define CSV_METADATA(Key,Value)									FCsvProfiler::SetMetadata( Key, Value )
#define CSV_NON_PERSISTENT_METADATA(Key,Value)					FCsvProfiler::SetNonPersistentMetadata( Key, Value )

#else
  #define CSV_CATEGORY_INDEX(CategoryName)						
  #define CSV_CATEGORY_INDEX_GLOBAL								
  #define CSV_STAT_FNAME(StatName)								
  #define CSV_SCOPED_TIMING_STAT(Category,StatName)				
  #define CSV_SCOPED_TIMING_STAT_GLOBAL(StatName)					
  #define CSV_SCOPED_TIMING_STAT_EXCLUSIVE(StatName)
  #define CSV_SCOPED_TIMING_STAT_EXCLUSIVE_CONDITIONAL(StatName,Condition)
  #define CSV_SCOPED_TIMING_STAT_RECURSIVE(Category,StatName)
  #define CSV_SCOPED_TIMING_STAT_RECURSIVE_CONDITIONAL(Category,StatName,Condition)
  #define CSV_SCOPED_WAIT(WaitTime)
  #define CSV_SCOPED_WAIT_CONDITIONAL(Condition)
  #define CSV_SCOPED_SET_WAIT_STAT(StatName)
  #define CSV_SCOPED_SET_WAIT_STAT_IGNORE()
  #define CSV_CUSTOM_STAT(Category,StatName,Value,Op)				
  #define CSV_CUSTOM_STAT_GLOBAL(StatName,Value,Op) 				
  #define CSV_DEFINE_STAT(Category,StatName)						
  #define CSV_DEFINE_STAT_GLOBAL(StatName)						
  #define CSV_DECLARE_STAT_EXTERN(Category,StatName)				
  #define CSV_CUSTOM_STAT_DEFINED(StatName,Value,Op)				
  #define CSV_DEFINE_CATEGORY(CategoryName,bDefaultValue)			
  #define CSV_DECLARE_CATEGORY_EXTERN(CategoryName)				
  #define CSV_DEFINE_CATEGORY_MODULE(Module_API,CategoryName,bDefaultValue)	
  #define CSV_DECLARE_CATEGORY_MODULE_EXTERN(Module_API,CategoryName)			
  #define CSV_EVENT(Category, Format, ...) 						
  #define CSV_EVENT_GLOBAL(Format, ...)
  #define CSV_METADATA(Key,Value)
  #define CSV_NON_PERSISTENT_METADATA(Key,Value)
#endif


#if CSV_PROFILER

class FCsvProfilerFrame;
class FCsvProfilerProcessingThread;
class FCsvProfilerThreadData;
class FName;
struct FCsvPersistentCustomStats;

enum class ECsvCustomStatOp : uint8
{
	Set,
	Min,
	Max,
	Accumulate,
};

enum class ECsvCommandType : uint8
{
	Start,
	Stop,
	Count
};

struct FCsvCategory;

struct FCsvDeclaredStat
{
	FCsvDeclaredStat(const TCHAR* InNameString, uint32 InCategoryIndex) 
		: Name(InNameString)
		, CategoryIndex(InCategoryIndex) 
	{
		TRACE_CSV_PROFILER_DECLARED_STAT(Name, InCategoryIndex);
	}

	FName Name;
	uint32 CategoryIndex;
};

enum class ECsvProfilerFlags
{
	None = 0,
	WriteCompletionFile = 1,
	CompressOutput = 2
};
ENUM_CLASS_FLAGS(ECsvProfilerFlags);

struct FCsvCaptureCommand
{
	FCsvCaptureCommand()
		: CommandType(ECsvCommandType::Count)
		, FrameRequested(-1)
		, Value(-1)
	{}

	FCsvCaptureCommand(ECsvCommandType InCommandType, uint32 InFrameRequested, uint32 InValue, const FString& InDestinationFolder, const FString& InFilename, ECsvProfilerFlags InFlags)
		: CommandType(InCommandType)
		, FrameRequested(InFrameRequested)
		, Value(InValue)
		, DestinationFolder(InDestinationFolder)
		, Filename(InFilename)
		, Flags(InFlags)
	{}

	FCsvCaptureCommand(ECsvCommandType InCommandType, uint32 InFrameRequested, TPromise<FString>* InCompletion, TSharedFuture<FString> InFuture)
		: CommandType(InCommandType)
		, FrameRequested(InFrameRequested)
		, Completion(MoveTemp(InCompletion))
		, Future(InFuture)
	{}

	ECsvCommandType CommandType;
	uint32 FrameRequested;
	uint32 Value;
	FString DestinationFolder;
	FString Filename;
	ECsvProfilerFlags Flags;
	TPromise<FString>* Completion;
	TSharedFuture<FString> Future;
};

//
// Persistent custom stats
// 
enum class ECsvPersistentCustomStatType : uint8
{
	Float,
	Int,
	Count
};

class FCsvPersistentCustomStatBase
{
public:
	FCsvPersistentCustomStatBase(FName InName, uint32 InCategoryIndex, bool bInResetEachFrame, ECsvPersistentCustomStatType InStatType)
		: Name(InName), CategoryIndex(InCategoryIndex), bResetEachFrame(bInResetEachFrame), StatType(InStatType)
	{
	}

	ECsvPersistentCustomStatType GetStatType() const
	{
		return StatType;
	}
protected:
	FName Name;
	uint32 CategoryIndex;
	bool bResetEachFrame;
	ECsvPersistentCustomStatType StatType;
};


template <class T> 
class TCsvPersistentCustomStat : public FCsvPersistentCustomStatBase
{
	friend struct FCsvPersistentCustomStats;
private:
	std::atomic<T> Value;
};


template <>
class TCsvPersistentCustomStat<float> : public FCsvPersistentCustomStatBase
{
	friend struct FCsvPersistentCustomStats;
public:
	TCsvPersistentCustomStat<float>(FName InName, uint32 InCategoryIndex, bool bInResetEachFrame)
		: FCsvPersistentCustomStatBase(InName, InCategoryIndex, bInResetEachFrame, ECsvPersistentCustomStatType::Float)
		, Value(0.0f)
	{}

	float Add(float Rhs)
	{
		float Previous = Value.load(std::memory_order_consume);
		float Desired = Previous + Rhs;
		while (!Value.compare_exchange_weak(Previous, Desired, std::memory_order_release, std::memory_order_consume))
		{
			Desired = Previous + Rhs;
		}
		return Desired;
	}
	float Sub(float Rhs)
	{
		return Add(-Rhs);
	}
	void Set(float NewVal)
	{
		return Value.store(NewVal, std::memory_order_relaxed);
	}
	float GetValue()
	{
		return Value.load();
	}
	static ECsvPersistentCustomStatType GetClassStatType() { return ECsvPersistentCustomStatType::Float; }
protected:
	std::atomic<float> Value;
};


template <>
class TCsvPersistentCustomStat<int32> : public FCsvPersistentCustomStatBase
{
	friend struct FCsvPersistentCustomStats;
public:
	TCsvPersistentCustomStat<int32>(FName InName, uint32 InCategoryIndex, bool bInResetEachFrame)
		: FCsvPersistentCustomStatBase(InName, InCategoryIndex, bInResetEachFrame, ECsvPersistentCustomStatType::Int)
		, Value(0)
	{}

	int32 Add(int32 Rhs)
	{
		return Value.fetch_add(Rhs, std::memory_order_relaxed);
	}
	int32 Sub(int32 Rhs)
	{
		return Value.fetch_sub(Rhs, std::memory_order_relaxed);
	}
	void Set(int32 NewVal)
	{
		Value.store(NewVal, std::memory_order_relaxed);
	}
	int32 GetValue()
	{
		return Value.load();
	}
	static ECsvPersistentCustomStatType GetClassStatType() { return ECsvPersistentCustomStatType::Int; }
protected:
	std::atomic<int> Value;
};


/**
* FCsvProfiler class. This manages recording and reporting all for CSV stats
*/
class FCsvProfiler
{
	friend class FCsvProfilerProcessingThread;
	friend class FCsvProfilerThreadData;
	friend struct FCsvCategory;
public:
	FCsvProfiler();
	~FCsvProfiler();
	static CORE_API FCsvProfiler* Get();

	CORE_API void Init();

	/** Begin static interface (used by macros)*/
	/** Push/pop events */
	CORE_API static void BeginStat(const char* StatName, uint32 CategoryIndex, const char* NamedEventName = nullptr);
	CORE_API static void BeginStat(const FName& StatName, uint32 CategoryIndex);
	CORE_API static void EndStat(const char* StatName, uint32 CategoryIndex);
	CORE_API static void EndStat(const FName& StatName, uint32 CategoryIndex);

	CORE_API static void BeginExclusiveStat(const char * StatName, const char* NamedEventName = nullptr);
	CORE_API static void EndExclusiveStat(const char * StatName);

	CORE_API static void RecordCustomStat(const char * StatName, uint32 CategoryIndex, float Value, const ECsvCustomStatOp CustomStatOp);
	CORE_API static void RecordCustomStat(const FName& StatName, uint32 CategoryIndex, float Value, const ECsvCustomStatOp CustomStatOp);
	CORE_API static void RecordCustomStat(const char* StatName, uint32 CategoryIndex, double Value, const ECsvCustomStatOp CustomStatOp);
	CORE_API static void RecordCustomStat(const FName& StatName, uint32 CategoryIndex, double Value, const ECsvCustomStatOp CustomStatOp);
	CORE_API static void RecordCustomStat(const char * StatName, uint32 CategoryIndex, int32 Value, const ECsvCustomStatOp CustomStatOp);
	CORE_API static void RecordCustomStat(const FName& StatName, uint32 CategoryIndex, int32 Value, const ECsvCustomStatOp CustomStatOp);

	CORE_API static void RecordEvent(int32 CategoryIndex, const FString& EventText);
	CORE_API static void RecordEventAtFrameStart(int32 CategoryIndex, const FString& EventText);
	CORE_API static void RecordEventAtTimestamp(int32 CategoryIndex, const FString& EventText, uint64 Cycles64);

	/** Metadata values set with this function will persist between captures. */
	CORE_API static void SetMetadata(const TCHAR* Key, const TCHAR* Value);
	/** Metadata values set with this function will be cleared after the capture ends. */
	CORE_API static void SetNonPersistentMetadata(const TCHAR* Key, const TCHAR* Value);
	
	static CORE_API int32 RegisterCategory(const FString& Name, bool bEnableByDefault, bool bIsGlobal);
	static CORE_API int32 GetCategoryIndex(const FString& Name);

	template <typename FmtType, typename... Types>
	FORCEINLINE static void RecordEventf(int32 CategoryIndex, const FmtType& Fmt, Types... Args)
	{
		static_assert(TIsArrayOrRefOfTypeByPredicate<FmtType, TIsCharEncodingCompatibleWithTCHAR>::Value, "Formatting string must be a TCHAR array.");
		static_assert((TIsValidVariadicFunctionArg<Types>::Value && ...), "Invalid argument(s) passed to FCsvProfiler::RecordEventf");
		RecordEventfInternal(CategoryIndex, (const TCHAR*)Fmt, Args...);
	}

	CORE_API static void BeginSetWaitStat(const char * StatName);
	CORE_API static void EndSetWaitStat();

	CORE_API static void BeginWait();
	CORE_API static void EndWait();

	/** Singleton interface */
	CORE_API bool IsCapturing() const;
	CORE_API bool IsCapturing_Renderthread() const;
	CORE_API bool IsWritingFile() const;
	CORE_API bool IsEndCapturePending() const;

	CORE_API int32 GetCaptureFrameNumber() const;
	CORE_API int32 GetCaptureFrameNumberRT() const;
	CORE_API int32 GetNumFrameToCaptureOnEvent() const;

	CORE_API bool EnableCategoryByString(const FString& CategoryName) const;
	CORE_API void EnableCategoryByIndex(uint32 CategoryIndex, bool bEnable) const;
	CORE_API bool IsCategoryEnabled(uint32 CategoryIndex) const;

	/** Per-frame update */
	CORE_API void BeginFrame();
	CORE_API void EndFrame();

	/** Begin Capture */
	CORE_API void BeginCapture(int InNumFramesToCapture = -1,
		const FString& InDestinationFolder = FString(),
		const FString& InFilename = FString(),
		ECsvProfilerFlags InFlags = ECsvProfilerFlags::None);

	/**
	 * End Capture
	 * EventToSignal is optional. If provided, the CSV profiler will signal the event when the async file write is complete.
	 * The returned TFuture can be waited on to retrieve the filename of the csv file that was written to disk.
	 */
	CORE_API TSharedFuture<FString> EndCapture(FGraphEventRef EventToSignal = nullptr);

	/** Called at the end of the first frame after forking */
	CORE_API void OnEndFramePostFork();

	/** Renderthread begin/end frame */
	CORE_API void BeginFrameRT();
	CORE_API void EndFrameRT();

	CORE_API void SetDeviceProfileName(FString InDeviceProfileName);

	FString GetOutputFilename() const { return OutputFilename; }
	CORE_API TMap<FString, FString> GetMetadataMapCopy();

	CORE_API static bool IsWaitTrackingEnabledOnCurrentThread();

	CORE_API void GetFrameExecCommands(TArray<FString>& OutFrameCommands) const;

	/** Called right before we start capturing. */
	DECLARE_MULTICAST_DELEGATE(FOnCSVProfileStart);
	FOnCSVProfileStart& OnCSVProfileStart() { return OnCSVProfileStartDelegate; }

	/** Called when csv frame 0 begins its capture. This is when CsvEvents and CustomStats will start being collected. */
	DECLARE_MULTICAST_DELEGATE(FOnCSVProfileFirstFrame);
	FOnCSVProfileFirstFrame& OnCSVProfileFirstFrame() { return OnCSVProfileFirstFrameDelegate; }

	/** Called when the capture is requested to end, allowing any final information to be written (eg. metadata). */
	DECLARE_MULTICAST_DELEGATE(FOnCSVProfileEndRequested);
	FOnCSVProfileEndRequested& OnCSVProfileEndRequested() { return OnCSVProfileEndRequestedDelegate; }

	DECLARE_MULTICAST_DELEGATE(FOnCSVProfileEnd);
	FOnCSVProfileEnd& OnCSVProfileEnd() { return OnCSVProfileEndDelegate; }

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnCSVProfileFinished, const FString& /*Filename */);
	FOnCSVProfileFinished& OnCSVProfileFinished() { return OnCSVProfileFinishedDelegate; }

	/** Called at the end of the frame when capturing, before end frame processing completes */
	DECLARE_MULTICAST_DELEGATE(FOnCSVProfileEndFrame);
	FOnCSVProfileEndFrame& OnCSVProfileEndFrame() { return OnCSVProfileEndFrameDelegate; }

	void SetRenderThreadId(uint32 InRenderThreadId)
	{
		RenderThreadId = InRenderThreadId;
	}

	void SetRHIThreadId(uint32 InRHIThreadId)
	{
		RHIThreadId = InRHIThreadId;
	}

	// Persistent custom stat methods. These are pre-registered custom stats whose value can persist over multiple frames
	// They are lower overhead than normal custom stats because accumulate ops don't require memory allocation
	CORE_API TCsvPersistentCustomStat<int32>* GetOrCreatePersistentCustomStatInt(FName Name, int32 CategoryIndex = CSV_CATEGORY_INDEX_GLOBAL, bool bResetEachFrame = false);
	CORE_API TCsvPersistentCustomStat<float>* GetOrCreatePersistentCustomStatFloat(FName Name, int32 CategoryIndex = CSV_CATEGORY_INDEX_GLOBAL, bool bResetEachFrame = false);

private:
	CORE_API static void VARARGS RecordEventfInternal(int32 CategoryIndex, const TCHAR* Fmt, ...);

	enum class EMetadataPersistenceType : int8
	{
		Persistent,
		NonPersistent
	};

	void SetMetadataInternal(const TCHAR* Key, const TCHAR* Value, bool bSanitize=true, EMetadataPersistenceType PersistenceType = EMetadataPersistenceType::Persistent);

	void FinalizeCsvFile();

	float ProcessStatData();

	int32 NumFramesToCapture;
	int32 CaptureFrameNumber;
	int32 CaptureFrameNumberRT;
	int32 CaptureOnEventFrameCount;

	bool bInsertEndFrameAtFrameStart;
	bool bNamedEventsWasEnabled;

	uint64 LastEndFrameTimestamp;
	uint32 CaptureEndFrameCount;

	FString OutputFilename;
	TQueue<FCsvCaptureCommand> CommandQueue;
	FCsvProfilerProcessingThread* ProcessingThread;

	FEvent* FileWriteBlockingEvent;
	FThreadSafeCounter IsShuttingDown;

	TMap<FString, FString> MetadataMap;
	TMap<FString, FString> NonPersistentMetadataMap;
	TQueue<TMap<FString, FString>> MetadataQueue;
	FCriticalSection MetadataCS;

	class FCsvStreamWriter* CsvWriter;

	ECsvProfilerFlags CurrentFlags;

	FOnCSVProfileStart OnCSVProfileStartDelegate;
	FOnCSVProfileFirstFrame OnCSVProfileFirstFrameDelegate;
	FOnCSVProfileEndRequested OnCSVProfileEndRequestedDelegate;
	FOnCSVProfileEnd OnCSVProfileEndDelegate;
	FOnCSVProfileEndFrame OnCSVProfileEndFrameDelegate;
	FOnCSVProfileFinished OnCSVProfileFinishedDelegate;

	std::atomic<uint32> RenderThreadId{ 0 };
	std::atomic<uint32> RHIThreadId{ 0 };
};

class FScopedCsvStat
{
public:
	FScopedCsvStat(const char * InStatName, uint32 InCategoryIndex, const char * InNamedEventName = nullptr)
		: StatName(InStatName)
		, CategoryIndex(InCategoryIndex)
	{
		FCsvProfiler::BeginStat(StatName, CategoryIndex, InNamedEventName);
	}

	~FScopedCsvStat()
	{
		FCsvProfiler::EndStat(StatName, CategoryIndex);
	}
	const char * StatName;
	uint32 CategoryIndex;
};

class FScopedCsvStatExclusive 
{
public:
	FScopedCsvStatExclusive(const char * InStatName, const char* InNamedEventName = nullptr)
		: StatName(InStatName)
	{
		FCsvProfiler::BeginExclusiveStat(StatName, InNamedEventName);
	}

	~FScopedCsvStatExclusive()
	{
		FCsvProfiler::EndExclusiveStat(StatName);
	}
	const char * StatName;
};

class FScopedCsvStatExclusiveConditional
{
public:
	FScopedCsvStatExclusiveConditional(const char * InStatName, bool bInCondition, const char* InNamedEventName = nullptr)
		: StatName(InStatName)
		, bCondition(bInCondition)
	{
		if (bCondition)
		{
			FCsvProfiler::BeginExclusiveStat(StatName, InNamedEventName);
		}
	}

	~FScopedCsvStatExclusiveConditional()
	{
		if (bCondition)
		{
			FCsvProfiler::EndExclusiveStat(StatName);
		}
	}
	const char * StatName;
	bool bCondition;
};

class FScopedCsvStatRecursive
{
	const char* StatName;
	uint32 CategoryIndex;
	int32& EntryCounter;
public:
	FScopedCsvStatRecursive(int32& InEntryCounter, const char* InStatName, uint32 InCategoryIndex, const char* InNamedEventName = nullptr)
		: StatName(InStatName)
		, CategoryIndex(InCategoryIndex)
		, EntryCounter(InEntryCounter)
	{
		++EntryCounter; // this needs to happen before BeginStat in case BeginStat causes reentry
		if (EntryCounter == 1)
		{
			FCsvProfiler::BeginStat(StatName, CategoryIndex, InNamedEventName);
		}
	}
	~FScopedCsvStatRecursive()
	{
		if (EntryCounter == 1)
		{
			FCsvProfiler::EndStat(StatName, CategoryIndex);
		}
		--EntryCounter;
	}
};

class FScopedCsvStatRecursiveConditional
{
	const char* StatName;
	uint32 CategoryIndex;
	int32& EntryCounter;
	bool bCondition;
public:
	FScopedCsvStatRecursiveConditional(int32& InEntryCounter, const char* InStatName, uint32 InCategoryIndex, bool bInCondition, const char* InNamedEventName = nullptr)
		: StatName(InStatName)
		, CategoryIndex(InCategoryIndex)
		, EntryCounter(InEntryCounter)
		, bCondition(bInCondition)
	{
		if (bCondition)
		{
			++EntryCounter; // this needs to happen before BeginStat in case BeginStat causes reentry
			if (EntryCounter == 1)
			{
				FCsvProfiler::BeginStat(StatName, CategoryIndex, InNamedEventName);
			}
		}
	}
	~FScopedCsvStatRecursiveConditional()
	{
		if (bCondition)
		{
			if (EntryCounter == 1)
			{
				FCsvProfiler::EndStat(StatName, CategoryIndex);
			}
			--EntryCounter;
		}
	}
};

class FScopedCsvWaitConditional
{
public:
	FScopedCsvWaitConditional(bool bInCondition)
		: bCondition(bInCondition)
	{
		if (bCondition)
		{
			FCsvProfiler::BeginWait();
		}
	}

	~FScopedCsvWaitConditional()
	{
		if (bCondition)
		{
			FCsvProfiler::EndWait();
		}
	}
	bool bCondition;
};


class FScopedCsvSetWaitStat
{
public:
	FScopedCsvSetWaitStat(const char * InStatName = nullptr)
		: StatName(InStatName)
	{
		FCsvProfiler::BeginSetWaitStat(StatName);
	}

	~FScopedCsvSetWaitStat()
	{
		FCsvProfiler::EndSetWaitStat();
	}
	const char * StatName;
};

struct FCsvCategory
{
	FCsvCategory() : Index(-1) {}
	FCsvCategory(const TCHAR* CategoryString, bool bDefaultValue, bool bIsGlobal = false)
	{
		Name = CategoryString;
		Index = FCsvProfiler::RegisterCategory(Name, bDefaultValue, bIsGlobal);
	}

	uint32 Index;
	FString Name;
};


CSV_DECLARE_CATEGORY_MODULE_EXTERN(CORE_API, Exclusive);


#endif //CSV_PROFILER
