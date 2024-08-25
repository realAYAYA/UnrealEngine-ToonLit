// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RenderingThread.h: Rendering thread definitions.
=============================================================================*/

#pragma once

#include "Async/TaskGraphInterfaces.h"
#include "Containers/Array.h"
#include "Containers/List.h"
#include "CoreGlobals.h"
#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "Delegates/Delegate.h"
#include "HAL/PlatformMemory.h"
#include "Misc/AssertionMacros.h"
#include "Misc/TVariant.h"
#include "MultiGPU.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "RHI.h"
#include "RHICommandList.h"
#include "Serialization/MemoryLayout.h"
#include "Stats/Stats.h"
#include "Stats/Stats2.h"
#include "Templates/Atomic.h"
#include "Templates/Function.h"
#include "Templates/UnrealTemplate.h"
#include "Templates/UnrealTypeTraits.h"
#include "Trace/Trace.h"
#include "Async/Mutex.h"
#include "Tasks/Pipe.h"
#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "RenderDeferredCleanup.h"
#endif

namespace UE { namespace Trace { class FChannel; } }

////////////////////////////////////
// Render thread API
////////////////////////////////////

/**
 * Whether the renderer is currently running in a separate thread.
 * If this is false, then all rendering commands will be executed immediately instead of being enqueued in the rendering command buffer.
 */
extern RENDERCORE_API bool GIsThreadedRendering;

/**
 * Whether the rendering thread should be created or not.
 * Currently set by command line parameter and by the ToggleRenderingThread console command.
 */
extern RENDERCORE_API bool GUseThreadedRendering;

extern RENDERCORE_API void SetRHIThreadEnabled(bool bEnableDedicatedThread, bool bEnableRHIOnTaskThreads);

#if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
	static FORCEINLINE void CheckNotBlockedOnRenderThread() {}
#else // #if (UE_BUILD_SHIPPING || UE_BUILD_TEST)
	/** Whether the main thread is currently blocked on the rendering thread, e.g. a call to FlushRenderingCommands. */
	extern RENDERCORE_API TAtomic<bool> GMainThreadBlockedOnRenderThread;

	/** Asserts if called from the main thread when the main thread is blocked on the rendering thread. */
	static FORCEINLINE void CheckNotBlockedOnRenderThread() { ensure(!GMainThreadBlockedOnRenderThread.Load(EMemoryOrder::Relaxed) || !IsInGameThread()); }
#endif // #if (UE_BUILD_SHIPPING || UE_BUILD_TEST)


/** Starts the rendering thread. */
extern RENDERCORE_API void StartRenderingThread();

/** Stops the rendering thread. */
extern RENDERCORE_API void StopRenderingThread();

/**
 * Checks if the rendering thread is healthy and running.
 * If it has crashed, UE_LOG is called with the exception information.
 */
extern RENDERCORE_API void CheckRenderingThreadHealth();

/** Checks if the rendering thread is healthy and running, without crashing */
extern RENDERCORE_API bool IsRenderingThreadHealthy();

/**
 * Advances stats for the rendering thread. Called from the game thread.
 */
extern RENDERCORE_API void AdvanceRenderingThreadStatsGT( bool bDiscardCallstack, int64 StatsFrame, int32 DisableChangeTagStartFrame );

/**
 * Waits for the rendering thread to finish executing all pending rendering commands.  Should only be used from the game thread.
 */
extern RENDERCORE_API void FlushRenderingCommands();

extern RENDERCORE_API void FlushPendingDeleteRHIResources_GameThread();
extern RENDERCORE_API void FlushPendingDeleteRHIResources_RenderThread();

extern RENDERCORE_API void TickRenderingTickables();

extern RENDERCORE_API void StartRenderCommandFenceBundler();
extern RENDERCORE_API void StopRenderCommandFenceBundler();

class FCoreRenderDelegates
{
public:
	DECLARE_MULTICAST_DELEGATE(FOnFlushRenderingCommandsStart);
	static RENDERCORE_API FOnFlushRenderingCommandsStart OnFlushRenderingCommandsStart;

	DECLARE_MULTICAST_DELEGATE(FOnFlushRenderingCommandsEnd);
	static RENDERCORE_API FOnFlushRenderingCommandsEnd OnFlushRenderingCommandsEnd;
};
////////////////////////////////////
// Render thread suspension
////////////////////////////////////

/**
 * Encapsulates stopping and starting the renderthread so that other threads can manipulate graphics resources.
 */
class FSuspendRenderingThread
{
public:
	/**
	 *	Constructor that flushes and suspends the renderthread
	 *	@param bRecreateThread	- Whether the rendering thread should be completely destroyed and recreated, or just suspended.
	 */
	RENDERCORE_API FSuspendRenderingThread( bool bRecreateThread );

	/** Destructor that starts the renderthread again */
	RENDERCORE_API ~FSuspendRenderingThread();

private:
	/** Whether we should use a rendering thread or not */
	bool bUseRenderingThread;

	/** Whether the rendering thread was currently running or not */
	bool bWasRenderingThreadRunning;

	/** Whether the rendering thread should be completely destroyed and recreated, or just suspended */
	bool bRecreateThread;
};

/** Helper macro for safely flushing and suspending the rendering thread while manipulating graphics resources */
#define SCOPED_SUSPEND_RENDERING_THREAD(bRecreateThread)	FSuspendRenderingThread SuspendRenderingThread(bRecreateThread)

////////////////////////////////////
// Render commands
////////////////////////////////////

UE_TRACE_CHANNEL_EXTERN(RenderCommandsChannel, RENDERCORE_API);

//
// Macros for using render commands.
//
// ideally this would be inline, however that changes the module dependency situation
extern RENDERCORE_API class FRHICommandListImmediate& GetImmediateCommandList_ForRenderCommand();


DECLARE_STATS_GROUP(TEXT("Render Thread Commands"), STATGROUP_RenderThreadCommands, STATCAT_Advanced);

// Log render commands on server for debugging
#if 0 // UE_SERVER && UE_BUILD_DEBUG
	#define LogRenderCommand(TypeName)				UE_LOG(LogRHI, Warning, TEXT("Render command '%s' is being executed on a dedicated server."), TEXT(#TypeName))
#else
	#define LogRenderCommand(TypeName)
#endif 

// conditions when rendering commands are executed in the thread
#if UE_SERVER
	#define	ShouldExecuteOnRenderThread()			false
#else
	#define	ShouldExecuteOnRenderThread()			(LIKELY(GIsThreadedRendering || !IsInGameThread()))
#endif // UE_SERVER

/** Type that contains profiler data necessary to mark up render commands for various profilers. */
template <typename TSTR>
struct TRenderCommandTag
{
	static const TCHAR* GetName()
	{
		return TSTR::TStr();
	}

	static uint32& GetSpecId()
	{
		static uint32 SpecId;
		return SpecId;
	}

	static TStatId GetStatId()
	{
#if STATS
		struct FStatData
		{
			typedef FStatGroup_STATGROUP_RenderThreadCommands TGroup;
			static FORCEINLINE const char* GetStatName()
			{
				return TSTR::CStr();
			}
			static FORCEINLINE const TCHAR* GetDescription()
			{
				return TSTR::TStr();
			}
			static FORCEINLINE EStatDataType::Type GetStatType()
			{
				return EStatDataType::ST_int64;
			}
			static FORCEINLINE bool IsClearEveryFrame()
			{
				return true;
			}
			static FORCEINLINE bool IsCycleStat()
			{
				return true;
			}
			static FORCEINLINE FPlatformMemory::EMemoryCounterRegion GetMemoryRegion()
			{
				return FPlatformMemory::MCR_Invalid;
			}
		};
		static struct FThreadSafeStaticStat<FStatData> Stat;
		return Stat.GetStatId();
#else
		return TStatId();
#endif
	}
};

/** Declares a new render command tag type from a name. */
#define DECLARE_RENDER_COMMAND_TAG(Type, Name) \
	struct PREPROCESSOR_JOIN(TSTR_, PREPROCESSOR_JOIN(Name, __LINE__)) \
	{  \
		static const char* CStr() { return #Name; } \
		static const TCHAR* TStr() { return TEXT(#Name); } \
	}; \
	using Type = TRenderCommandTag<PREPROCESSOR_JOIN(TSTR_, PREPROCESSOR_JOIN(Name, __LINE__))>;

/** The parent class of commands stored in the rendering command queue. */
class FRenderCommand
{
public:
	// All render commands run on the render thread
	static ENamedThreads::Type GetDesiredThread()
	{
		check(!GIsThreadedRendering || ENamedThreads::GetRenderThread() != ENamedThreads::GameThread);
		return ENamedThreads::GetRenderThread();
	}

	static ESubsequentsMode::Type GetSubsequentsMode()
	{
		// Don't support tasks having dependencies on us, reduces task graph overhead tracking and dealing with subsequents
		return ESubsequentsMode::FireAndForget;
	}
};

template <typename TagType, typename LambdaType>
class TEnqueueUniqueRenderCommandType : public FRenderCommand
{
public:
	TEnqueueUniqueRenderCommandType(LambdaType&& InLambda) : Lambda(Forward<LambdaType>(InLambda)) {}

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE_ON_CHANNEL_STR(TagType::GetName(), RenderCommandsChannel);
		FRHICommandListImmediate& RHICmdList = GetImmediateCommandList_ForRenderCommand();
		Lambda(RHICmdList);
	}

	FORCEINLINE_DEBUGGABLE TStatId GetStatId() const
	{
		return TagType::GetStatId();
	}

private:
	LambdaType Lambda;
};

/** Describes which pipes are configured to use the render command pipe system. */
enum class ERenderCommandPipeMode
{
	/** Bypasses the render command pipe system altogether. Render commands are issued using tasks. */
	None,

	/** The render command pipe on the render thread pipe is active, and all other pipes forward to the render thread pipe. */
	RenderThread,

	/** All render command pipes are active. */
	All
};

enum class ERenderCommandPipeFlags : uint8
{
	None = 0,

	/** Initializes the render command pipe in a disabled state. */
	Disabled = 1 << 0
};

ENUM_CLASS_FLAGS(ERenderCommandPipeFlags);

class FRenderCommandPipe;
using FRenderCommandPipeBitArrayAllocator = TInlineAllocator<1, FConcurrentLinearBitArrayAllocator>;
using FRenderCommandPipeBitArray = TBitArray<FRenderCommandPipeBitArrayAllocator>;
using FRenderCommandPipeSetBitIterator = TConstSetBitIterator<FRenderCommandPipeBitArrayAllocator>;

namespace UE::RenderCommandPipe
{
	// [Game Thread] Initializes all statically initialized render command pipes.
	extern RENDERCORE_API void Initialize();

	// [Game Thread (Parallel)] Returns whether any render command pipes are currently recording on the game thread timeline.
	extern RENDERCORE_API bool IsRecording();

	// [Render Thread (Parallel)] Returns whether any render command pipes are currently replaying commands on the render thread timeline.
	extern RENDERCORE_API bool IsReplaying();

	// [Render Thread (Parallel)] Returns whether the specific render command pipe is replaying.
	extern RENDERCORE_API bool IsReplaying(const FRenderCommandPipe& Pipe);

	// [Game Thread] Starts recording render commands into pipes. Returns whether the operation succeeded.
	extern RENDERCORE_API void StartRecording();
	extern RENDERCORE_API void StartRecording(const FRenderCommandPipeBitArray& PipeBits);

	// [Game Thread] Stops recording commands into pipes and syncs all remaining pipe work to the render thread. Returns whether the operation succeeded.
	extern RENDERCORE_API FRenderCommandPipeBitArray StopRecording();
	extern RENDERCORE_API FRenderCommandPipeBitArray StopRecording(TConstArrayView<FRenderCommandPipe*> Pipes);

	// Returns the list of all registered pipes.
	extern TConstArrayView<FRenderCommandPipe*> GetPipes();

	// [Game Thread] Stops render command pipe recording during the duration of the scope and restarts recording once the scope is complete.
	class RENDERCORE_API FSyncScope
	{
	public:
		FSyncScope();
		FSyncScope(TConstArrayView<FRenderCommandPipe*> Pipes);
		~FSyncScope();

	private:
		FRenderCommandPipeBitArray PipeBits;
	};
}

extern RENDERCORE_API ERenderCommandPipeMode GRenderCommandPipeMode;

class FRenderThreadCommandPipe
{
public:
	template <typename RenderCommandTag, typename LambdaType>
	FORCEINLINE_DEBUGGABLE static void Enqueue(LambdaType&& Lambda)
	{
		if (GRenderCommandPipeMode != ERenderCommandPipeMode::None)
		{
			Instance.EnqueueAndLaunch(RenderCommandTag::GetName(), RenderCommandTag::GetSpecId(), RenderCommandTag::GetStatId(), MoveTemp(Lambda));
		}
		else
		{
			TGraphTask<TEnqueueUniqueRenderCommandType<RenderCommandTag, LambdaType>>::CreateTask().ConstructAndDispatchWhenReady(MoveTemp(Lambda));
		}
	}

private:
	static RENDERCORE_API FRenderThreadCommandPipe Instance;

	RENDERCORE_API void EnqueueAndLaunch(const TCHAR* Name, uint32& SpecId, TStatId StatId, TUniqueFunction<void(FRHICommandListImmediate&)>&& Function);

	struct FCommand
	{
		FCommand(const TCHAR* InName, uint32& OutSpecId, TStatId InStatId, TUniqueFunction<void(FRHICommandListImmediate&)>&& InFunction)
			: Name(InName)
			, SpecId(&OutSpecId)
			, StatId(InStatId)
			, Function(MoveTemp(InFunction))
		{}

		const TCHAR* Name;
		uint32* SpecId;
		TStatId StatId;
		TUniqueFunction<void(FRHICommandListImmediate&)> Function;
	};

	int32 ProduceIndex = 0;
	TStaticArray<TArray<FCommand>, 2> Queues;
	UE::FMutex Mutex;
};

template<typename RenderCommandTag, typename LambdaType>
FORCEINLINE_DEBUGGABLE void EnqueueUniqueRenderCommand(LambdaType&& Lambda)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_USE_ON_CHANNEL(RenderCommandTag::GetSpecId(), RenderCommandTag::GetName(), EventScope, RenderCommandsChannel, true); 

	if (IsInRenderingThread())
	{
		Lambda(GetImmediateCommandList_ForRenderCommand());
	}
	else if (ShouldExecuteOnRenderThread())
	{
		CheckNotBlockedOnRenderThread();
		FRenderThreadCommandPipe::Enqueue<RenderCommandTag, LambdaType>(MoveTemp(Lambda));
	}
	else
	{
		FScopeCycleCounter CycleScope(RenderCommandTag::GetStatId());
		Lambda(GetImmediateCommandList_ForRenderCommand());
	}
}

template<typename RenderCommandTag, typename LambdaType>
FORCEINLINE_DEBUGGABLE void EnqueueUniqueRenderCommand(LambdaType& Lambda)
{
	static_assert(sizeof(LambdaType) == 0, "EnqueueUniqueRenderCommand enforces use of rvalue and therefore move to avoid an extra copy of the Lambda");
}

class FRenderCommandPipe
{
public:
	using FCommandListFunction = TUniqueFunction<void(FRHICommandList&)>;
	using FEmptyFunction = TUniqueFunction<void()>;

	RENDERCORE_API FRenderCommandPipe(const TCHAR* Name, ERenderCommandPipeFlags Flags, const TCHAR* CVarName, const TCHAR* CVarDescription);
	RENDERCORE_API ~FRenderCommandPipe();

	FORCEINLINE const TCHAR* GetName() const
	{
		return Name;
	}

	FORCEINLINE bool IsReplaying() const
	{
		ensure(IsInParallelRenderingThread());
		return Frame_RenderThread != nullptr;
	}

	FORCEINLINE bool IsRecording() const
	{
		return bRecording;
	}

	FORCEINLINE bool IsEmpty() const
	{
		return NumInFlightCommands.load(std::memory_order_relaxed) == 0;
	}

	void SetEnabled(bool bInIsEnabled)
	{
		check(IsInGameThread());
		bEnabled = bInIsEnabled;
	}

	template <typename RenderCommandTag>
	static void Enqueue(FRenderCommandPipe* Pipe, FCommandListFunction&& Function)
	{
		if (GRenderCommandPipeMode == ERenderCommandPipeMode::All && Pipe)
		{
			UE::TScopeLock Lock(Pipe->Mutex);

			// Execute the function directly if this is being called recursively from within another pipe command.
			if (UE::RenderCommandPipe::IsReplaying(*Pipe))
			{
				Pipe->ExecuteCommand(MoveTemp(Function), RenderCommandTag::GetName(), RenderCommandTag::GetSpecId());
				return;
			}

			if (Pipe->Frame_GameThread)
			{
				Pipe->EnqueueAndLaunch(MoveTemp(Function), RenderCommandTag::GetName(), RenderCommandTag::GetSpecId());
				return;
			}
		}

		EnqueueUniqueRenderCommand<RenderCommandTag>([Function = MoveTemp(Function)](FRHICommandListImmediate& RHICmdList) { Function(RHICmdList); });
	}

	template <typename RenderCommandTag>
	static void Enqueue(FRenderCommandPipe& Pipe, FCommandListFunction&& Function)
	{
		Enqueue<RenderCommandTag>(&Pipe, MoveTemp(Function));
	}

	template <typename RenderCommandTag>
	static void Enqueue(FRenderCommandPipe* Pipe, FEmptyFunction&& Function)
	{
		if (GRenderCommandPipeMode == ERenderCommandPipeMode::All && Pipe)
		{
			UE::TScopeLock Lock(Pipe->Mutex);

			// Execute the function directly if this is being called recursively from within another pipe command.
			if (UE::RenderCommandPipe::IsReplaying(*Pipe))
			{
				Pipe->ExecuteCommand(MoveTemp(Function), RenderCommandTag::GetName(), RenderCommandTag::GetSpecId());
				return;
			}

			if (Pipe->Frame_GameThread)
			{
				Pipe->EnqueueAndLaunch(MoveTemp(Function), RenderCommandTag::GetName(), RenderCommandTag::GetSpecId());
				return;
			}
		}

		EnqueueUniqueRenderCommand<RenderCommandTag>([Function = MoveTemp(Function)](FRHICommandListImmediate&) { Function(); });
	}

	template <typename RenderCommandTag>
	static void Enqueue(FRenderCommandPipe& Pipe, FEmptyFunction&& Function)
	{
		Enqueue<RenderCommandTag>(&Pipe, MoveTemp(Function));
	}

	template <typename RenderCommandTag, typename LambdaType>
	FORCEINLINE static void Enqueue(LambdaType&& Lambda)
	{
		EnqueueUniqueRenderCommand<RenderCommandTag>(MoveTemp(Lambda));
	}

private:
	friend class FRenderCommandPipeRegistry;

	using FFunctionVariant = TVariant<FEmptyFunction, FCommandListFunction>;

	RENDERCORE_API void EnqueueAndLaunch(FFunctionVariant&& FunctionVariant, const TCHAR* Name, uint32& SpecId);

	void EnqueueAndLaunch(FCommandListFunction&& Function, const TCHAR* CommandName, uint32& CommandSpecId)
	{
		EnqueueAndLaunch(FFunctionVariant(TInPlaceType<FCommandListFunction>(), MoveTemp(Function)), CommandName, CommandSpecId);
	}

	void EnqueueAndLaunch(FEmptyFunction&& Function, const TCHAR* CommandName, uint32& CommandSpecId)
	{
		EnqueueAndLaunch(FFunctionVariant(TInPlaceType<FEmptyFunction>(), MoveTemp(Function)), CommandName, CommandSpecId);
	}

	RENDERCORE_API void ExecuteCommand(FFunctionVariant&& FunctionVariant, const TCHAR* CommandName, uint32& CommandSpecId);

	void ExecuteCommand(FCommandListFunction&& Function, const TCHAR* CommandName, uint32& CommandSpecId)
	{
		ExecuteCommand(FFunctionVariant(TInPlaceType<FCommandListFunction>(), MoveTemp(Function)), CommandName, CommandSpecId);
	}

	void ExecuteCommand(FEmptyFunction&& Function, const TCHAR* CommandName, uint32& CommandSpecId)
	{
		ExecuteCommand(FFunctionVariant(TInPlaceType<FEmptyFunction>(), MoveTemp(Function)), CommandName, CommandSpecId);
	}

	struct FCommand
	{
		FCommand(FFunctionVariant&& InFunction, const TCHAR* InName, uint32& InOutSpecId)
			: Function(MoveTemp(InFunction))
			, Name(InName)
			, SpecId(&InOutSpecId)
		{}

		FFunctionVariant Function;
		const TCHAR* Name;
		uint32* SpecId;
	};

	struct FFrame : public TConcurrentLinearObject<FFrame>
	{
		FFrame(const TCHAR* Name, const UE::Tasks::FTaskEvent& InTaskEvent)
			: Pipe(Name)
			, TaskEvent(InTaskEvent)
		{}

		UE::Tasks::FPipe Pipe;
		UE::Tasks::FTaskEvent TaskEvent;
		TArray<FCommand> Queue;
		FRHICommandList* RHICmdList = nullptr;
	};

	const TCHAR* Name;
	UE::FMutex Mutex;
	FFrame* Frame_GameThread = nullptr;
	FFrame* Frame_RenderThread = nullptr;
	TLinkedList<FRenderCommandPipe*> GlobalListLink;
	FAutoConsoleVariable ConsoleVariable;
	std::atomic_int32_t NumInFlightCommands{ 0 };
	uint16 Index = uint16(-1);
	bool bRecording = false;
	bool bEnabled = true;
};

/** Declares an extern reference to a render command pipe. */
#define DECLARE_RENDER_COMMAND_PIPE(Name, PrefixKeywords) \
	namespace UE::RenderCommandPipe { extern PrefixKeywords FRenderCommandPipe Name; }

/** Defines a render command pipe. */
#define DEFINE_RENDER_COMMAND_PIPE(Name, Flags) \
	namespace UE::RenderCommandPipe \
	{ \
		FRenderCommandPipe Name( \
			TEXT(#Name), \
			Flags, \
			TEXT("r.RenderCommandPipe." #Name), \
			TEXT("Whether to enable the " #Name " Render Command Pipe") \
			TEXT(" 0: off;") \
			TEXT(" 1: on (default)") \
		); \
	}

/** Enqueues a render command to a render pipe. The default implementation takes a lambda and schedules on the render thread.
 *  Alternative implementations accept either a reference or pointer to an FRenderCommandPipe instance to schedule on an async
 *  pipe, if enabled.
 */
#define ENQUEUE_RENDER_COMMAND(Type) \
	DECLARE_RENDER_COMMAND_TAG(PREPROCESSOR_JOIN(FRenderCommandTag_, PREPROCESSOR_JOIN(Type, __LINE__)), Type) \
	FRenderCommandPipe::Enqueue<PREPROCESSOR_JOIN(FRenderCommandTag_, PREPROCESSOR_JOIN(Type, __LINE__))>

////////////////////////////////////
// RenderThread scoped work
////////////////////////////////////

/** A utility to record RHI commands asynchronously and then enqueue the resulting commands to the render thread. */
class FRHIAsyncCommandList
{
public:
	FRHIAsyncCommandList(FRHIGPUMask InGPUMask = FRHIGPUMask::All())
		: RHICmdListStack(InGPUMask, FRHICommandList::ERecordingThread::Any)
	{}

	FRHICommandList& GetCommandList()
	{
		return RHICmdListStack;
	}

	FRHICommandList& operator*()
	{
		return RHICmdListStack;
	}

	FRHICommandList* operator->()
	{
		return &RHICmdListStack;
	}

	~FRHIAsyncCommandList()
	{
		RHICmdListStack.FinishRecording();
		if (RHICmdListStack.HasCommands())
		{
			ENQUEUE_RENDER_COMMAND(AsyncCommandListScope)(
				[RHICmdList = new FRHICommandList(MoveTemp(RHICmdListStack))](FRHICommandListImmediate& RHICmdListImmediate)
			{
				RHICmdListImmediate.QueueAsyncCommandListSubmit(RHICmdList);
			});
		}
	}

private:
	FRHICommandList RHICmdListStack;
};

class FRenderThreadScope
{
	typedef TFunction<void(FRHICommandListImmediate&)> RenderCommandFunction;
	typedef TArray<RenderCommandFunction> RenderCommandFunctionArray;
public:
	FRenderThreadScope()
	{
		RenderCommands = new RenderCommandFunctionArray;
	}

	~FRenderThreadScope()
	{
		RenderCommandFunctionArray* RenderCommandArray = RenderCommands;

		ENQUEUE_RENDER_COMMAND(DispatchScopeCommands)(
			[RenderCommandArray](FRHICommandListImmediate& RHICmdList)
		{
			for(int32 Index = 0; Index < RenderCommandArray->Num(); Index++)
			{
				(*RenderCommandArray)[Index](RHICmdList);
			}

			delete RenderCommandArray;
		});
	}

	void EnqueueRenderCommand(RenderCommandFunction&& Lambda)
	{
		RenderCommands->Add(MoveTemp(Lambda));
	}

private:
	RenderCommandFunctionArray* RenderCommands;
};

struct FRenderThreadStructBase
{
	FRenderThreadStructBase() = default;

	// Copy construction is not allowed. Used to avoid accidental copying in the command lambda.
	FRenderThreadStructBase(const FRenderThreadStructBase&) = delete;

	void InitRHI(FRHICommandListImmediate&) {}
	void ReleaseRHI(FRHICommandListImmediate&) {}
};

/**  Represents a struct with a lifetime that spans multiple render commands with scoped initialization
 *   and release on the render thread.
 * 
 *   Example:
 * 
 *   struct FMyStruct : public FRenderThreadStructBase
 *   {
 *       FInitializer { int32 Foo; int32 Bar; };
 * 
 *       FMyStruct(const FInitializer& InInitializer)
 *            : Initializer(InInitializer)
 *       {
 *            // Called immediately by TRenderThreadStruct when created.
 *       }
 * 
 *       ~FMyStruct()
 *       {
 *           // Called on the render thread when TRenderThreadStruct goes out of scope.
 *       }
 * 
 *       void InitRHI(FRHICommandListImmediate& RHICmdList)
 *       {
 *           // Called on the render thread by TRenderThreadStruct when created.
 *       }
 * 
 *       void ReleaseRHI(FRHICommandListImmediate& RHICmdList)
 *       {
 *           // Called on the render thread when TRenderThreadStruct goes out of scope.
 *       }
 * 
 *       FInitializer Initializer;
 *   };
 *
 *   // On Main Thread
 * 
 *   {
 *       TRenderThreadStruct<FMyStruct> MyStruct(FMyStruct::FInitializer{1, 2});
 * 
 *       ENQUEUE_RENDER_COMMAND(CommandA)[MyStruct = MyStruct.Get()](FRHICommandListImmediate& RHICmdList)
 *       {
 *           // Do something with MyStruct.
 *       };
 * 
 *       ENQUEUE_RENDER_COMMAND(CommandB)[MyStruct = MyStrucft.Get()](FRHICommandListImmediate& RHICmdList)
 *       {
 *           // Do something else with MyStruct.
 *       };
 * 
 *       // MyStruct instance is automatically released and deleted on the render thread.
 *   }
 */
template <typename StructType>
class TRenderThreadStruct
{
public:
	static_assert(TIsDerivedFrom<StructType, FRenderThreadStructBase>::IsDerived, "StructType must be derived from FRenderThreadStructBase.");

	template <typename... TArgs>
	TRenderThreadStruct(TArgs&&... Args)
		: Struct(new StructType(Forward<TArgs&&>(Args)...))
	{
		ENQUEUE_RENDER_COMMAND(InitStruct)([Struct = Struct](FRHICommandListImmediate& RHICmdList)
		{
			Struct->InitRHI(RHICmdList);
		});
	}

	~TRenderThreadStruct()
	{
		ENQUEUE_RENDER_COMMAND(DeleteStruct)([Struct = Struct](FRHICommandListImmediate& RHICmdList)
		{
			Struct->ReleaseRHI(RHICmdList);
			delete Struct;
		});
		Struct = nullptr;
	}

	TRenderThreadStruct(const TRenderThreadStruct&) = delete;

	const StructType* operator->() const
	{
		return Struct;
	}

	StructType* operator->()
	{
		return Struct;
	}

	const StructType& operator*() const
	{
		return *Struct;
	}

	StructType& operator*()
	{
		return *Struct;
	}

	const StructType* Get() const
	{
		return Struct;
	}

	StructType* Get()
	{
		return Struct;
	}

private:
	StructType* Struct;
};

DECLARE_MULTICAST_DELEGATE(FStopRenderingThread);
using FStopRenderingThreadDelegate = FStopRenderingThread::FDelegate;

extern RENDERCORE_API FDelegateHandle RegisterStopRenderingThreadDelegate(const FStopRenderingThreadDelegate& InDelegate);

extern RENDERCORE_API void UnregisterStopRenderingThreadDelegate(FDelegateHandle InDelegateHandle);
