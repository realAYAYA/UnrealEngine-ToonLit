// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RenderingThread.h: Rendering thread definitions.
=============================================================================*/

#pragma once

#include "Async/TaskGraphInterfaces.h"
#include "Containers/Array.h"
#include "CoreGlobals.h"
#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "Delegates/Delegate.h"
#include "HAL/PlatformMemory.h"
#include "Misc/AssertionMacros.h"
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

class RENDERCORE_API FCoreRenderDelegates
{
public:
	DECLARE_MULTICAST_DELEGATE(FOnFlushRenderingCommandsStart);
	static FOnFlushRenderingCommandsStart OnFlushRenderingCommandsStart;

	DECLARE_MULTICAST_DELEGATE(FOnFlushRenderingCommandsEnd);
	static FOnFlushRenderingCommandsEnd OnFlushRenderingCommandsEnd;
};
////////////////////////////////////
// Render thread suspension
////////////////////////////////////

/**
 * Encapsulates stopping and starting the renderthread so that other threads can manipulate graphics resources.
 */
class RENDERCORE_API FSuspendRenderingThread
{
public:
	/**
	 *	Constructor that flushes and suspends the renderthread
	 *	@param bRecreateThread	- Whether the rendering thread should be completely destroyed and recreated, or just suspended.
	 */
	FSuspendRenderingThread( bool bRecreateThread );

	/** Destructor that starts the renderthread again */
	~FSuspendRenderingThread();

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

/** The parent class of commands stored in the rendering command queue. */
class RENDERCORE_API FRenderCommand
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

#define TASK_FUNCTION(Code) \
		void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent) \
		{ \
			FRHICommandListImmediate& RHICmdList = GetImmediateCommandList_ForRenderCommand(); \
			Code; \
		} 

#define TASKNAME_FUNCTION(TypeName) \
		FORCEINLINE TStatId GetStatId() const \
		{ \
			RETURN_QUICK_DECLARE_CYCLE_STAT(TypeName, STATGROUP_RenderThreadCommands); \
		}

template<typename TSTR, typename LAMBDA>
class TEnqueueUniqueRenderCommandType : public FRenderCommand
{
public:
	TEnqueueUniqueRenderCommandType(LAMBDA&& InLambda) : Lambda(Forward<LAMBDA>(InLambda)) {}

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE_ON_CHANNEL_STR(TSTR::TStr(), RenderCommandsChannel);
		FRHICommandListImmediate& RHICmdList = GetImmediateCommandList_ForRenderCommand();
		Lambda(RHICmdList);
	}

	FORCEINLINE_DEBUGGABLE TStatId GetStatId() const
	{
#if STATS
		static struct FThreadSafeStaticStat<FStat_EnqueueUniqueRenderCommandType> StatPtr_EnqueueUniqueRenderCommandType;
		return StatPtr_EnqueueUniqueRenderCommandType.GetStatId();
#else
		return TStatId();
#endif
	}

private:
#if STATS
	struct FStat_EnqueueUniqueRenderCommandType
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
#endif

private:
	LAMBDA Lambda;
};

template<typename TSTR, typename LAMBDA>
FORCEINLINE_DEBUGGABLE void EnqueueUniqueRenderCommand(LAMBDA&& Lambda)
{
	//QUICK_SCOPE_CYCLE_COUNTER(STAT_EnqueueUniqueRenderCommand);
	typedef TEnqueueUniqueRenderCommandType<TSTR, LAMBDA> EURCType;

#if 0 // UE_SERVER && UE_BUILD_DEBUG
	UE_LOG(LogRHI, Warning, TEXT("Render command '%s' is being executed on a dedicated server."), TSTR::TStr())
#endif
	if (IsInRenderingThread())
	{
		FRHICommandListImmediate& RHICmdList = GetImmediateCommandList_ForRenderCommand();
		Lambda(RHICmdList);
	}
	else
	{
		if (ShouldExecuteOnRenderThread())
		{
			CheckNotBlockedOnRenderThread();
			TGraphTask<EURCType>::CreateTask().ConstructAndDispatchWhenReady(Forward<LAMBDA>(Lambda));
		}
		else
		{
			EURCType TempCommand(Forward<LAMBDA>(Lambda));
			FScopeCycleCounter EURCMacro_Scope(TempCommand.GetStatId());
			TempCommand.DoTask(ENamedThreads::GameThread, FGraphEventRef());
		}
	}
}

#define ENQUEUE_RENDER_COMMAND(Type) \
	struct Type##Name \
	{  \
		static const char* CStr() { return #Type; } \
		static const TCHAR* TStr() { return TEXT(#Type); } \
	}; \
	EnqueueUniqueRenderCommand<Type##Name>

template<typename LAMBDA>
FORCEINLINE_DEBUGGABLE void EnqueueUniqueRenderCommand(LAMBDA& Lambda)
{
	static_assert(sizeof(LAMBDA) == 0, "EnqueueUniqueRenderCommand enforces use of rvalue and therefore move to avoid an extra copy of the Lambda");
}

////////////////////////////////////
// Deferred cleanup
////////////////////////////////////

/**
 * The base class of objects that need to defer deletion until the render command queue has been flushed.
 */
class RENDERCORE_API FDeferredCleanupInterface
{
public:
	virtual ~FDeferredCleanupInterface() {}

	UE_DEPRECATED(4.20, "FinishCleanup is deprecated. Use RAII in the destructor instead.")
	virtual void FinishCleanup() final {}
};

/**
 * A set of cleanup objects which are pending deletion.
 */
class FPendingCleanupObjects
{
	TArray<FDeferredCleanupInterface*> CleanupArray;
public:
	inline bool IsEmpty() const { return CleanupArray.IsEmpty(); }
	FPendingCleanupObjects();
	RENDERCORE_API ~FPendingCleanupObjects();
};

/**
 * Adds the specified deferred cleanup object to the current set of pending cleanup objects.
 */
extern RENDERCORE_API void BeginCleanup(FDeferredCleanupInterface* CleanupObject);

/**
 * Transfers ownership of the current set of pending cleanup objects to the caller.  A new set is created for subsequent BeginCleanup calls.
 * @return A pointer to the set of pending cleanup objects.  The called is responsible for deletion.
 */
extern RENDERCORE_API FPendingCleanupObjects* GetPendingCleanupObjects();

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

class RENDERCORE_API FRenderThreadScope
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