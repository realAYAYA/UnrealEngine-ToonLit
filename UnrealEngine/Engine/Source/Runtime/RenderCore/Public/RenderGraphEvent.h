// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/StaticArray.h"
#include "Containers/UnrealString.h"
#include "DynamicRenderScaling.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "Misc/AssertionMacros.h"
#include "MultiGPU.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "ProfilingDebugging/CsvProfilerConfig.h"
#include "ProfilingDebugging/RealtimeGPUProfiler.h"
#include "RHI.h"
#include "RHIBreadcrumbs.h"
#include "RHICommandList.h"
#include "RenderGraphAllocator.h"
#include "RenderGraphDefinitions.h"
#include "RendererInterface.h"
#include "Stats/Stats2.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/NameTypes.h"

/** Macros for create render graph event names and scopes.
 *
 *  FRDGEventName Name = RDG_EVENT_NAME("MyPass %sx%s", ViewRect.Width(), ViewRect.Height());
 *
 *  RDG_EVENT_SCOPE(GraphBuilder, "MyProcessing %sx%s", ViewRect.Width(), ViewRect.Height());
 */
#if RDG_EVENTS == RDG_EVENTS_STRING_REF || RDG_EVENTS == RDG_EVENTS_STRING_COPY
	#define RDG_EVENT_NAME(Format, ...) FRDGEventName(TEXT(Format), ##__VA_ARGS__)
	#define RDG_EVENT_SCOPE(GraphBuilder, Format, ...) FRDGEventScopeGuard PREPROCESSOR_JOIN(__RDG_ScopeRef_,__LINE__) ((GraphBuilder), RDG_EVENT_NAME(Format, ##__VA_ARGS__))
	#define RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, Condition, Format, ...) FRDGEventScopeGuard PREPROCESSOR_JOIN(__RDG_ScopeRef_,__LINE__) ((GraphBuilder), RDG_EVENT_NAME(Format, ##__VA_ARGS__), Condition)
#elif RDG_EVENTS == RDG_EVENTS_NONE
	#define RDG_EVENT_NAME(Format, ...) FRDGEventName()
	#define RDG_EVENT_SCOPE(GraphBuilder, Format, ...)
	#define RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, Condition, Format, ...)
#else
	#error "RDG_EVENTS is not a valid value."
#endif

#if HAS_GPU_STATS
	#if STATS
		#define RDG_GPU_STAT_SCOPE(GraphBuilder, StatName) FRDGGPUStatScopeGuard PREPROCESSOR_JOIN(__RDG_GPUStatEvent_##StatName,__LINE__) ((GraphBuilder), CSV_STAT_FNAME(StatName), GET_STATID(Stat_GPU_##StatName).GetName(), nullptr, &DrawcallCountCategory_##StatName.Counters);
		#define RDG_GPU_STAT_SCOPE_VERBOSE(GraphBuilder, StatName, Description) FRDGGPUStatScopeGuard PREPROCESSOR_JOIN(__RDG_GPUStatEvent_##StatName,__LINE__) ((GraphBuilder), CSV_STAT_FNAME(StatName), GET_STATID(Stat_GPU_##StatName).GetName(), Description, &DrawcallCountCategory_##StatName.Counters);
	#else
		#define RDG_GPU_STAT_SCOPE(GraphBuilder, StatName) FRDGGPUStatScopeGuard PREPROCESSOR_JOIN(__RDG_GPUStatEvent_##StatName,__LINE__) ((GraphBuilder), CSV_STAT_FNAME(StatName), FName(), nullptr, &DrawcallCountCategory_##StatName.Counters);
		#define RDG_GPU_STAT_SCOPE_VERBOSE(GraphBuilder, StatName, Description) FRDGGPUStatScopeGuard PREPROCESSOR_JOIN(__RDG_GPUStatEvent_##StatName,__LINE__) ((GraphBuilder), CSV_STAT_FNAME(StatName), FName(), Description, &DrawcallCountCategory_##StatName.Counters);
	#endif
#else
	#define RDG_GPU_STAT_SCOPE(GraphBuilder, StatName)
	#define RDG_GPU_STAT_SCOPE_VERBOSE(GraphBuilder, StatName, Description)
#endif

#if CSV_PROFILER
	#define RDG_CSV_STAT_EXCLUSIVE_SCOPE(GraphBuilder, StatName) FRDGScopedCsvStatExclusive RDGScopedCsvStatExclusive ## StatName (GraphBuilder, #StatName)
	#define RDG_CSV_STAT_EXCLUSIVE_SCOPE_CONDITIONAL(GraphBuilder, StatName, bCondition) FRDGScopedCsvStatExclusiveConditional RDGScopedCsvStatExclusiveConditional ## StatName (GraphBuilder, #StatName, bCondition)
#else
	#define RDG_CSV_STAT_EXCLUSIVE_SCOPE(GraphBuilder, StatName)
	#define RDG_CSV_STAT_EXCLUSIVE_SCOPE_CONDITIONAL(GraphBuilder, StatName, bCondition)
#endif

 /** Injects a scope onto both the RDG and RHI timeline. */
#define RDG_RHI_EVENT_SCOPE(GraphBuilder, Name) RDG_EVENT_SCOPE(GraphBuilder, #Name); SCOPED_DRAW_EVENT(GraphBuilder.RHICmdList, Name);
#define RDG_RHI_GPU_STAT_SCOPE(GraphBuilder, StatName) RDG_GPU_STAT_SCOPE(GraphBuilder, StatName); SCOPED_GPU_STAT(GraphBuilder.RHICmdList, StatName);

/** Returns whether the current frame is emitting render graph events. */
RENDERCORE_API bool GetEmitRDGEvents();

template <typename InScopeType>
class TRDGScopeOp
{
public:
	using ScopeType = InScopeType;

	TRDGScopeOp() = default;

	static TRDGScopeOp Push(const ScopeType* Scope)
	{
		TRDGScopeOp Op;
		Op.Scope = Scope;
		Op.Type = EType::Scope;
		Op.Op = EOp::Push;
		return Op;
	}

	static TRDGScopeOp Push(const TCHAR* Name)
	{
		TRDGScopeOp Op;
		Op.Name = Name;
		Op.Type = EType::Name;
		Op.Op = EOp::Push;
		return Op;
	}

	static TRDGScopeOp Pop(const ScopeType* Scope)
	{
		TRDGScopeOp Op;
		Op.Scope = Scope;
		Op.Type = EType::Scope;
		Op.Op = EOp::Pop;
		return Op;
	}

	static TRDGScopeOp Pop()
	{
		TRDGScopeOp Op;
		Op.Type = EType::Name;
		Op.Op = EOp::Pop;
		return Op;
	}

	inline bool IsPush() const { return Op == EOp::Push; }
	inline bool IsPop()  const { return Op == EOp::Pop;  }

	inline bool IsScope() const { return Type == EType::Scope; }
	inline bool IsName()  const { return Type == EType::Name;  }

	union
	{
		const ScopeType* Scope = nullptr;
		const TCHAR* Name;
	};

	enum class EType : uint8
	{
		Scope,
		Name

	} Type = EType::Scope;

	enum class EOp : uint8
	{
		Push,
		Pop

	} Op = EOp::Push;
};

template <typename ScopeOpType>
class TRDGScopeOpArray
{
public:
	TRDGScopeOpArray() = default;

	TRDGScopeOpArray(TArray<ScopeOpType, FRDGArrayAllocator>& InArray, int32 InOffset, int32 InCount)
		: Array(&InArray)
		, Offset(InOffset)
		, Count(InCount)
	{}

	inline int32 Num() const
	{
		return Count;
	}

	inline const ScopeOpType& operator[](int32 Index) const
	{
		check(Array);
		check(Index < Count);
		return (*Array)[Offset + Index];
	}

	inline ScopeOpType& operator[](int32 Index)
	{
		check(Array);
		check(Index < Count);
		return (*Array)[Offset + Index];
	}

private:
	TArray<ScopeOpType, FRDGArrayAllocator>* Array;
	int32 Offset = 0;
	int32 Count = 0;
};

template <typename ScopeOpType>
class TRDGScopeStackHelper
{
	static constexpr uint32 kScopeStackDepthMax = 8;
public:
	using ScopeType = typename ScopeOpType::ScopeType;

	TRDGScopeStackHelper()
		: ScopeStack(MakeUniformStaticArray<const ScopeType*, kScopeStackDepthMax>(nullptr))
	{}

	inline void ReserveOps(int32 OpCount)
	{
		Ops.Reserve(OpCount);
	}

	TRDGScopeOpArray<ScopeOpType> CompilePassPrologue(const ScopeType* ParentScope, const TCHAR* PassName);
	TRDGScopeOpArray<ScopeOpType> CompilePassEpilogue();
	TRDGScopeOpArray<ScopeOpType> EndCompile();

private:
	TStaticArray<const ScopeType*, kScopeStackDepthMax> ScopeStack;
	TArray<ScopeOpType, FRDGArrayAllocator> Ops;
	bool bNamePushed = false;
};

/** A helper profiler class for tracking and evaluating hierarchical scopes in the context of render graph. */
template <typename ScopeOpType>
class TRDGScopeStack final
{
public:
	using ScopeType = typename ScopeOpType::ScopeType;

	TRDGScopeStack(FRDGAllocator& InAllocator)
		: Allocator(InAllocator)
	{}

	~TRDGScopeStack()
	{
		for (int32 Index = Scopes.Num() - 1; Index >= 0; --Index)
		{
			Scopes[Index]->~ScopeType();
		}
		Scopes.Empty();
	}

	template <typename... TScopeConstructArgs>
	inline void BeginScope(TScopeConstructArgs... ScopeConstructArgs)
	{
		auto Scope = Allocator.AllocNoDestruct<ScopeType>(CurrentScope, Forward<TScopeConstructArgs>(ScopeConstructArgs)...);
		Scopes.Add(Scope);
		CurrentScope = Scope;
	}

	inline void EndScope()
	{
		checkf(CurrentScope != nullptr, TEXT("Current scope is null."));
		CurrentScope = CurrentScope->ParentScope;
	}

	inline void ReserveOps(int32 NameCount = 0)
	{
		Helper.ReserveOps(Scopes.Num() * 2 + NameCount * 2);
	}

	inline TRDGScopeOpArray<ScopeOpType> CompilePassPrologue(const ScopeType* ParentScope, const TCHAR* Name = nullptr)
	{
		return Helper.CompilePassPrologue(ParentScope, Name);
	}

	inline TRDGScopeOpArray<ScopeOpType> CompilePassEpilogue()
	{
		return Helper.CompilePassEpilogue();
	}

	inline TRDGScopeOpArray<ScopeOpType> EndCompile()
	{
		checkf(CurrentScope == nullptr, TEXT("Render graph needs to have all scopes ended to execute."));
		return Helper.EndCompile();
	}

	inline const ScopeType* GetCurrentScope() const
	{
		return CurrentScope;
	}

private:
	FRDGAllocator& Allocator;

	/** The top of the scope stack during setup. */
	const ScopeType* CurrentScope = nullptr;

	/** Tracks scopes allocated through MemStack for destruction. */
	TArray<ScopeType*, FRDGArrayAllocator> Scopes;

	TRDGScopeStackHelper<ScopeOpType> Helper;
};

class FRDGBuilder;
class FRDGPass;

//////////////////////////////////////////////////////////////////////////
//
// GPU Timing
//
//////////////////////////////////////////////////////////////////////////

class FRDGTimingScope final
{
public:
	FRDGTimingScope(const FRDGTimingScope* InParentScope, const int32 InBudgetId)
		: ParentScope(InParentScope)
		, BudgetId(InBudgetId)
	{}

	const int32 GetBudgetId() const
	{
		return BudgetId;
	}

	const FRDGTimingScope* const ParentScope;
	const int32 BudgetId;
};

using FRDGTimingScopeOp = TRDGScopeOp<FRDGTimingScope>;

class RENDERCORE_API FRDGTimingScopeOpArray final
{
public:
	FRDGTimingScopeOpArray() = default;
	FRDGTimingScopeOpArray(ERHIPipeline Pipeline, const TRDGScopeOpArray<FRDGTimingScopeOp>& Ops);

	void Execute(FRHIComputeCommandList& RHICmdList);

private:
	FRHIRenderQuery* TimestampQuery = nullptr;
};

class RENDERCORE_API FRDGTimingScopeStack final
{
public:
	FRDGTimingScopeStack(FRDGAllocator& Allocator)
		: ScopeStack(Allocator)
	{}

	inline void BeginScope(const DynamicRenderScaling::FBudget& Budget)
	{
		int32 BudgetId = Budget.GetBudgetId();
		ScopeStack.BeginScope(BudgetId);
	}

	inline void EndScope()
	{
		ScopeStack.EndScope();
	}

	inline void ReserveOps()
	{
		ScopeStack.ReserveOps();
	}

	FRDGTimingScopeOpArray CompilePassPrologue(const FRDGPass* Pass);
	
	inline void EndExecute(FRHIComputeCommandList& RHICmdList)
	{
		return FRDGTimingScopeOpArray(RHICmdList.GetPipeline(), ScopeStack.EndCompile()).Execute(RHICmdList);
	}

	inline const FRDGTimingScope* GetCurrentScope() const
	{
		return ScopeStack.GetCurrentScope();
	}

	TRDGScopeStack<FRDGTimingScopeOp> ScopeStack;
};

namespace DynamicRenderScaling
{

class RENDERCORE_API FRDGScope final
{
public:
	FRDGScope(FRDGBuilder& InGraphBuilder, const FBudget& InBudget);
	FRDGScope(const FRDGScope&) = delete;
	~FRDGScope();

private:
	FRDGBuilder& GraphBuilder;
	const DynamicRenderScaling::FBudget& Budget;
	const bool bIsEnabled;
};

} // namespace DynamicRenderScaling


//////////////////////////////////////////////////////////////////////////
//
// GPU Events - Named hierarchical events emitted to external profiling tools.
//
//////////////////////////////////////////////////////////////////////////

/** Stores a GPU event name for the render graph. Draw events can be compiled out entirely from
 *  a release build for performance.
 */
class RENDERCORE_API FRDGEventName final
{
public:
	FRDGEventName() = default;

	explicit FRDGEventName(const TCHAR* EventFormat, ...);

	FRDGEventName(const FRDGEventName& Other);
	FRDGEventName(FRDGEventName&& Other);
	FRDGEventName& operator=(const FRDGEventName& Other);
	FRDGEventName& operator=(FRDGEventName&& Other);

	const TCHAR* GetTCHAR() const;

private:
#if RDG_EVENTS == RDG_EVENTS_STRING_REF || RDG_EVENTS == RDG_EVENTS_STRING_COPY
	// Event format kept around to still have a clue what error might be causing the problem in error messages.
	const TCHAR* EventFormat = TEXT("");

#if RDG_EVENTS == RDG_EVENTS_STRING_COPY
	FString FormattedEventName;
#endif
#endif
};

#if RDG_GPU_DEBUG_SCOPES

class FRDGEventScope final
{
public:
	FRDGEventScope(const FRDGEventScope* InParentScope, FRDGEventName&& InName, FRHIGPUMask InGPUMask)
		: ParentScope(InParentScope)
		, Name(Forward<FRDGEventName&&>(InName))
#if WITH_MGPU
		, GPUMask(InGPUMask)
#endif
	{}

	/** Returns a formatted path for debugging. */
	FString GetPath(const FRDGEventName& Event) const;

	const FRDGEventScope* const ParentScope;
	const FRDGEventName Name;
#if WITH_MGPU
	const FRHIGPUMask GPUMask;
#endif
};

using FRDGEventScopeOp = TRDGScopeOp<FRDGEventScope>;

#if RHI_WANT_BREADCRUMB_EVENTS
struct FRDGBreadcrumbState : TRHIBreadcrumbState<FRDGArrayAllocator>
{
	static FRDGBreadcrumbState* Create(FRDGAllocator& Allocator)
	{
		return Allocator.AllocNoDestruct<FRDGBreadcrumbState>();
	}

	FRDGBreadcrumbState* Copy(FRDGAllocator& Allocator) const
	{
		return Allocator.AllocNoDestruct<FRDGBreadcrumbState>(*this);
	}

	uint32 Version = 0;
};
#endif

class RENDERCORE_API FRDGEventScopeOpArray final
{
public:
	FRDGEventScopeOpArray(bool bInRDGEvents = true)
		: bRDGEvents(bInRDGEvents)
	{}

	FRDGEventScopeOpArray(TRDGScopeOpArray<FRDGEventScopeOp> InOps, bool bInRDGEvents = true)
		: Ops(InOps)
		, bRDGEvents(bInRDGEvents)
	{}

	void Execute(FRHIComputeCommandList& RHICmdList);

#if RHI_WANT_BREADCRUMB_EVENTS
	void Execute(FRDGBreadcrumbState& State);
#endif

	TRDGScopeOpArray<FRDGEventScopeOp> Ops;
	bool bRDGEvents;
};

/** Manages a stack of event scopes. Scopes are recorded ahead of time in a hierarchical fashion
 *  and later executed topologically during pass execution.
 */
class RENDERCORE_API FRDGEventScopeStack final
{
public:
	FRDGEventScopeStack(FRDGAllocator& Allocator)
		: ScopeStack(Allocator)
		, bRDGEvents(GetEmitRDGEvents())
	{}

	inline void BeginScope(FRDGEventName&& EventName, FRHIGPUMask GPUMask)
	{
		if (IsEnabled())
		{
			ScopeStack.BeginScope(Forward<FRDGEventName&&>(EventName), GPUMask);
		}
	}

	inline void EndScope()
	{
		if (IsEnabled())
		{
			ScopeStack.EndScope();
		}
	}

	inline void ReserveOps(int32 NameCount)
	{
		if (IsEnabled())
		{
			ScopeStack.ReserveOps(NameCount);
		}
	}

	FRDGEventScopeOpArray CompilePassPrologue(const FRDGPass* Pass);
	
	FRDGEventScopeOpArray CompilePassEpilogue();

	inline void EndExecute(FRHIComputeCommandList& RHICmdList, ERHIPipeline Pipeline)
	{
		if (IsEnabled())
		{
			FRDGEventScopeOpArray Array = ScopeStack.EndCompile();
			if (Array.Ops.Num())
			{
				FRHICommandListScopedPipeline Scope(RHICmdList, Pipeline);
				Array.Execute(RHICmdList);
			}
		}
	}

	inline const FRDGEventScope* GetCurrentScope() const
	{
		return ScopeStack.GetCurrentScope();
	}

private:
	inline bool IsEnabled()
	{
#if RHI_WANT_BREADCRUMB_EVENTS
		return true;
#elif RDG_EVENTS
		return bRDGEvents;
#else
		return false;
#endif
	}

	TRDGScopeStack<FRDGEventScopeOp> ScopeStack;
	/** Are RDG Events enabled for these scopes */
	bool bRDGEvents;
};

RENDERCORE_API FString GetRDGEventPath(const FRDGEventScope* Scope, const FRDGEventName& Event);

class RENDERCORE_API FRDGEventScopeGuard final
{
public:
	FRDGEventScopeGuard(FRDGBuilder& InGraphBuilder, FRDGEventName&& ScopeName, bool bCondition = true);
	FRDGEventScopeGuard(const FRDGEventScopeGuard&) = delete;
	~FRDGEventScopeGuard();

private:
	FRDGBuilder& GraphBuilder;
	bool bCondition = true;
};

#endif // RDG_GPU_DEBUG_SCOPES

//////////////////////////////////////////////////////////////////////////
//
// GPU Stats - Aggregated counters emitted to the runtime 'stat GPU' profiler.
//
//////////////////////////////////////////////////////////////////////////

#if RDG_GPU_DEBUG_SCOPES

class FRDGGPUStatScope final
{
public:
	FRDGGPUStatScope(const FRDGGPUStatScope* InParentScope, const FName& InName, const FName& InStatName, const TCHAR* InDescription, FRHIDrawCallsStatPtr InDrawCallCounter)
		: ParentScope(InParentScope)
		, Name(InName)
		, StatName(InStatName)
		, DrawCallCounter(InDrawCallCounter)
	{
		if (InDescription)
		{
			Description = InDescription;
		}
	}

	const FRDGGPUStatScope* const ParentScope;
	const FName Name;
	const FName StatName;
	FString Description;
	FRHIDrawCallsStatPtr DrawCallCounter;
};

class FRDGGPUStatScopeOp : public TRDGScopeOp<FRDGGPUStatScope>
{
	using Base = TRDGScopeOp<FRDGGPUStatScope>;
public:
	FRDGGPUStatScopeOp() = default;
	FRDGGPUStatScopeOp(const Base& InBase)
		: Base(InBase)
	{}

#if HAS_GPU_STATS
	FRealtimeGPUProfilerQuery Query;
#endif
};

class RENDERCORE_API FRDGGPUStatScopeOpArray final
{
public:
	static const int32 kInvalidEventIndex = -1;
	
	enum class EType
	{
		Prologue,
		Epilogue
	};

	FRDGGPUStatScopeOpArray() = default;
	FRDGGPUStatScopeOpArray(TRDGScopeOpArray<FRDGGPUStatScopeOp> InOps, FRHIGPUMask GPUMask);

	void Execute(FRHIComputeCommandList& RHICmdList);

	TRDGScopeOpArray<FRDGGPUStatScopeOp> Ops;
	int32 OverrideEventIndex = kInvalidEventIndex;
	EType Type = EType::Epilogue;
};

class RENDERCORE_API FRDGGPUStatScopeStack final
{
public:
	FRDGGPUStatScopeStack(FRDGAllocator& Allocator)
		: ScopeStack(Allocator)
#if HAS_GPU_STATS
		, bGPUStats(AreGPUStatsEnabled())
#endif
	{}

	inline void BeginScope(const FName& Name, const FName& StatName, const TCHAR* Description, FRHIDrawCallsStatPtr DrawCallCounter)
	{
		if (IsEnabled())
		{
			check(DrawCallCounter != nullptr);
			ScopeStack.BeginScope(Name, StatName, Description, DrawCallCounter);
		}
	}

	inline void EndScope()
	{
		if (IsEnabled())
		{
			ScopeStack.EndScope();
		}
	}

	inline void ReserveOps()
	{
		if (IsEnabled())
		{
			ScopeStack.ReserveOps();
		}
	}

	FRDGGPUStatScopeOpArray CompilePassPrologue(const FRDGPass* Pass, FRHIGPUMask GPUMask);

	FRDGGPUStatScopeOpArray CompilePassEpilogue();

	inline void EndExecute(FRHIComputeCommandList& RHICmdList, ERHIPipeline Pipeline)
	{
		// These ops are only relevant to the graphics pipe
		if (IsEnabled() && Pipeline == ERHIPipeline::Graphics)
		{
			FRHICommandListScopedPipeline Scope(RHICmdList, Pipeline);
			FRDGGPUStatScopeOpArray(ScopeStack.EndCompile(), RHICmdList.GetGPUMask()).Execute(RHICmdList);
		}
	}

	inline const FRDGGPUStatScope* GetCurrentScope() const
	{
		return ScopeStack.GetCurrentScope();
	}

private:
	inline bool IsEnabled()
	{
#if HAS_GPU_STATS
		return bGPUStats;
#else
		return false;
#endif
	}
	TRDGScopeStack<FRDGGPUStatScopeOp> ScopeStack;
	int32 OverrideEventIndex = FRDGGPUStatScopeOpArray::kInvalidEventIndex;
	/** Are GPU Stats enabled for these scopes */
	bool bGPUStats = false;
};

class RENDERCORE_API FRDGGPUStatScopeGuard final
{
public:
	FRDGGPUStatScopeGuard(FRDGBuilder& InGraphBuilder, const FName& Name, const FName& StatName, const TCHAR* Description, FRHIDrawCallsStatPtr DrawCallCounter);
	FRDGGPUStatScopeGuard(const FRDGGPUStatScopeGuard&) = delete;
	~FRDGGPUStatScopeGuard();

private:
	FRDGBuilder& GraphBuilder;
};

#endif // RDG_GPU_DEBUG_SCOPES

//////////////////////////////////////////////////////////////////////////
//
// General GPU scopes
//
//////////////////////////////////////////////////////////////////////////

struct FRDGGPUScopes
{
	const FRDGTimingScope* Timing = nullptr;
	IF_RDG_GPU_DEBUG_SCOPES(const FRDGEventScope* Event = nullptr);
	IF_RDG_GPU_DEBUG_SCOPES(const FRDGGPUStatScope* Stat = nullptr);
};

struct FRDGGPUScopeOpArrays
{
	inline void Execute(FRHIComputeCommandList& RHICmdList)
	{
		Timing.Execute(RHICmdList);
		IF_RDG_GPU_DEBUG_SCOPES(Event.Execute(RHICmdList));
		IF_RDG_GPU_DEBUG_SCOPES(Stat.Execute(RHICmdList));
	}

	FRDGTimingScopeOpArray Timing;
	IF_RDG_GPU_DEBUG_SCOPES(FRDGEventScopeOpArray Event);
	IF_RDG_GPU_DEBUG_SCOPES(FRDGGPUStatScopeOpArray Stat);
};

/** The complete set of scope stack implementations. */
struct FRDGGPUScopeStacks
{
	FRDGGPUScopeStacks(FRDGAllocator& Allocator)
		: Timing(Allocator)
#if RDG_GPU_DEBUG_SCOPES
		, Event(Allocator)
		, Stat(Allocator)
#endif
	{}

	inline void ReserveOps(int32 PassCount)
	{
		Timing.ReserveOps();
		IF_RDG_GPU_DEBUG_SCOPES(Event.ReserveOps(PassCount));
		IF_RDG_GPU_DEBUG_SCOPES(Stat.ReserveOps());
	}

	inline FRDGGPUScopeOpArrays CompilePassPrologue(const FRDGPass* Pass, FRHIGPUMask GPUMask)
	{
		FRDGGPUScopeOpArrays Result;
		Result.Timing = Timing.CompilePassPrologue(Pass);
		IF_RDG_GPU_DEBUG_SCOPES(Result.Event = Event.CompilePassPrologue(Pass));
		IF_RDG_GPU_DEBUG_SCOPES(Result.Stat = Stat.CompilePassPrologue(Pass, GPUMask));
		return MoveTemp(Result);
	}

	inline FRDGGPUScopeOpArrays CompilePassEpilogue()
	{
		FRDGGPUScopeOpArrays Result;
		IF_RDG_GPU_DEBUG_SCOPES(Result.Event = Event.CompilePassEpilogue());
		IF_RDG_GPU_DEBUG_SCOPES(Result.Stat = Stat.CompilePassEpilogue());
		return MoveTemp(Result);
	}

	inline void EndExecute(FRHIComputeCommandList& RHICmdList, ERHIPipeline Pipeline)
	{
		Timing.EndExecute(RHICmdList);
		IF_RDG_GPU_DEBUG_SCOPES(Event.EndExecute(RHICmdList, Pipeline));
		IF_RDG_GPU_DEBUG_SCOPES(Stat.EndExecute(RHICmdList, Pipeline));
	}

	inline FRDGGPUScopes GetCurrentScopes() const
	{
		FRDGGPUScopes Scopes;
		Scopes.Timing = Timing.GetCurrentScope();
		IF_RDG_GPU_DEBUG_SCOPES(Scopes.Event = Event.GetCurrentScope());
		IF_RDG_GPU_DEBUG_SCOPES(Scopes.Stat = Stat.GetCurrentScope());
		return Scopes;
	}

	FRDGTimingScopeStack Timing;
	IF_RDG_GPU_DEBUG_SCOPES(FRDGEventScopeStack Event);
	IF_RDG_GPU_DEBUG_SCOPES(FRDGGPUStatScopeStack Stat);
};

struct RENDERCORE_API FRDGGPUScopeStacksByPipeline
{
	FRDGGPUScopeStacksByPipeline(FRDGAllocator& Allocator)
		: Graphics(Allocator)
		, AsyncCompute(Allocator)
	{
		IsTimingIsEnabled.SetAll(false);
	}

	inline bool IsTimingScopeAlreadyEnabled(const DynamicRenderScaling::FBudget& Budget) const
	{
		return IsTimingIsEnabled[Budget];
	}

	inline void BeginTimingScope(const DynamicRenderScaling::FBudget& Budget)
	{
		check(!IsTimingIsEnabled[Budget]);
		IsTimingIsEnabled[Budget] = true;
		Graphics.Timing.BeginScope(Budget);
		AsyncCompute.Timing.BeginScope(Budget);
	}

	inline void EndTimingScope(const DynamicRenderScaling::FBudget& Budget)
	{
		check(IsTimingIsEnabled[Budget]);
		IsTimingIsEnabled[Budget] = false;
		Graphics.Timing.EndScope();
		AsyncCompute.Timing.EndScope();
	}

#if RDG_GPU_DEBUG_SCOPES
	inline void BeginEventScope(FRDGEventName&& ScopeName, FRHIGPUMask GPUMask)
	{
		FRDGEventName ScopeNameCopy = ScopeName;
		Graphics.Event.BeginScope(MoveTemp(ScopeNameCopy), GPUMask);
		AsyncCompute.Event.BeginScope(MoveTemp(ScopeName), GPUMask);
	}

	inline void EndEventScope()
	{
		Graphics.Event.EndScope();
		AsyncCompute.Event.EndScope();
	}

	inline void BeginStatScope(const FName& Name, const FName& StatName, const TCHAR* Description, FRHIDrawCallsStatPtr DrawCallCounter)
	{
		Graphics.Stat.BeginScope(Name, StatName, Description, DrawCallCounter);
	}

	inline void EndStatScope()
	{
		Graphics.Stat.EndScope();
	}
#endif

	inline void ReserveOps(int32 PassCount)
	{
		Graphics.ReserveOps(PassCount);
		AsyncCompute.ReserveOps(PassCount);
	}

	FRDGGPUScopeOpArrays CompilePassPrologue(const FRDGPass* Pass, FRHIGPUMask GPUMask);

	FRDGGPUScopeOpArrays CompilePassEpilogue(const FRDGPass* Pass);

	const FRDGGPUScopeStacks& GetScopeStacks(ERHIPipeline Pipeline) const;

	FRDGGPUScopeStacks& GetScopeStacks(ERHIPipeline Pipeline);

	FRDGGPUScopes GetCurrentScopes(ERHIPipeline Pipeline) const
	{
		return GetScopeStacks(Pipeline).GetCurrentScopes();
	}

	FRDGGPUScopeStacks Graphics;
	FRDGGPUScopeStacks AsyncCompute;

private:
	DynamicRenderScaling::TMap<bool> IsTimingIsEnabled;
};

//////////////////////////////////////////////////////////////////////////
//
// CPU CSV Stats
//
//////////////////////////////////////////////////////////////////////////

#if RDG_CPU_SCOPES

class FRDGCSVStatScope final
{
public:
	FRDGCSVStatScope(const FRDGCSVStatScope* InParentScope, const char* InStatName)
		: ParentScope(InParentScope)
		, StatName(InStatName)
	{}

	const FRDGCSVStatScope* const ParentScope;
	const char* StatName;
};

using FRDGCSVStatScopeOp = TRDGScopeOp<FRDGCSVStatScope>;

class RENDERCORE_API FRDGCSVStatScopeOpArray final
{
public:
	FRDGCSVStatScopeOpArray() = default;
	FRDGCSVStatScopeOpArray(TRDGScopeOpArray<FRDGCSVStatScopeOp> InOps)
		: Ops(InOps)
	{}

	void Execute();

	TRDGScopeOpArray<FRDGCSVStatScopeOp> Ops;
};

class RENDERCORE_API FRDGCSVStatScopeStack final
{
public:
	FRDGCSVStatScopeStack(FRDGAllocator& Allocator)
		: ScopeStack(Allocator)
	{}

	void BeginScope(const char* StatName)
	{
		if (IsEnabled())
		{
			ScopeStack.BeginScope(StatName);
		}
	}

	void EndScope()
	{
		if (IsEnabled())
		{
			ScopeStack.EndScope();
		}
	}

	inline void ReserveOps()
	{
		if (IsEnabled())
		{
			ScopeStack.ReserveOps();
		}
	}

	FRDGCSVStatScopeOpArray CompilePassPrologue(const FRDGPass* Pass);

	void EndExecute()
	{
		if (IsEnabled())
		{
			FRDGCSVStatScopeOpArray(ScopeStack.EndCompile()).Execute();
		}
	}

	const FRDGCSVStatScope* GetCurrentScope() const
	{
		return ScopeStack.GetCurrentScope();
	}

private:
	static bool IsEnabled()
	{
#if CSV_PROFILER
		return true;
#else
		return false;
#endif
	}
	TRDGScopeStack<FRDGCSVStatScopeOp> ScopeStack;
};

#if CSV_PROFILER

class RENDERCORE_API FRDGScopedCsvStatExclusive : public FScopedCsvStatExclusive
{
public:
	FRDGScopedCsvStatExclusive(FRDGBuilder& InGraphBuilder, const char* InStatName);
	~FRDGScopedCsvStatExclusive();

private:
	FRDGBuilder& GraphBuilder;
};

class RENDERCORE_API FRDGScopedCsvStatExclusiveConditional : public FScopedCsvStatExclusiveConditional
{
public:
	FRDGScopedCsvStatExclusiveConditional(FRDGBuilder& InGraphBuilder, const char* InStatName, bool bInCondition);
	~FRDGScopedCsvStatExclusiveConditional();

private:
	FRDGBuilder& GraphBuilder;
};

#endif

struct FRDGCPUScopes
{
	const FRDGCSVStatScope* CSV = nullptr;
};

struct FRDGCPUScopeOpArrays
{
	void Execute()
	{
		CSV.Execute();
	}

	FRDGCSVStatScopeOpArray CSV;
};

struct FRDGCPUScopeStacks
{
	FRDGCPUScopeStacks(FRDGAllocator& Allocator)
		: CSV(Allocator)
	{}

	inline void ReserveOps()
	{
		CSV.ReserveOps();
	}

	FRDGCPUScopeOpArrays CompilePassPrologue(const FRDGPass* Pass)
	{
		FRDGCPUScopeOpArrays Result;
		Result.CSV = CSV.CompilePassPrologue(Pass);
		return MoveTemp(Result);
	}

	FRDGCPUScopes GetCurrentScopes() const
	{
		FRDGCPUScopes Scopes;
		Scopes.CSV = CSV.GetCurrentScope();
		return Scopes;
	}

	void EndExecute()
	{
		CSV.EndExecute();
	}

	FRDGCSVStatScopeStack CSV;
};

#endif

#include "RenderGraphEvent.inl" // IWYU pragma: export
