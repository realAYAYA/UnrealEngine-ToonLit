// Copyright Epic Games, Inc. All Rights Reserved.

#include "RenderGraphEvent.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphPrivate.h"
#include "RenderGraphPass.h"
#include "RenderResource.h"

class FRDGTimingPool : public FRenderResource
{
public:
	FRenderQueryPoolRHIRef QueryPool;

	// Destructor
	virtual ~FRDGTimingPool() = default;

	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		check(IsInRenderingThread());
		bIsBudgetRecordingEnabled.SetAll(false);
		LastTimings.SetAll(uint64(0));
	}

	virtual void ReleaseRHI() override
	{
		check(IsInRenderingThread());

		if (QueryPool)
		{
			// Release all in-flight queries
			LandInFlightFrames(/* bWait = */ true);
			InFlightFrames.Reset();

			// Release the pool
			QueryPool.SafeRelease();
		}
	}

	/* Scopes of a timing budget. */
	struct FInFlightTimingScope
	{
		// Global index of the timing budget this scope belongs to.
		int32 BudgetId = -1;

		// Index of the query at the begining of the scope in FInFlightFrame::TimestampQueries
		int32 BeginQueryId = -1;

		// Index of the query at the ending of the scope in FInFlightFrame::TimestampQueries
		int32 EndQueryId = -1;

		bool IsValid() const
		{
			return BeginQueryId >= 0 && EndQueryId >= 0 && BudgetId >= 0;
		}
	};

	/* Full frame of timestamp queries in flight. */
	struct FInFlightFrame
	{
		static const int32 kTimingScopesPreallocation = 64;
		static const int32 kTimestampQueriesPreallocation = kTimingScopesPreallocation * 2;

		// Frame counter this frame belongs too, or -1 if this entry cane be reused for a new frame.
		uint64 FrameCounter;

		// Arrays of all timestamp queries issued in this frame.
		TArray<FRHIPooledRenderQuery> TimestampQueries;

		// Arrays of all scopes issued in this frame.
		TArray<FInFlightTimingScope> TimingScopes;

		// Indices of TimingScopes::FInFlightTimingScope that has been began but not ended of different pipes.
		DynamicRenderScaling::TMap<int32> LastTimingScopesGraphics;
		DynamicRenderScaling::TMap<int32> LastTimingScopesAsyncCompute;

		// Last timestamp queries issues for all pipes to  know whether an in-flight frame has landed.
		FRHIRenderQuery* LastQueryGraphics;
		FRHIRenderQuery* LastQueryAsyncCompute;

		// Fence for the RHI command to be completed before polling RHI queries.
		mutable FGraphEventRef RHIEndFence;

		FInFlightFrame()
		{
			 ResetValues();
		}

		// Begins a new timing scope for a specific budget on a pipeline.
		void BeginTimingScope(ERHIPipeline Pipeline, int32 BudgetId, int32 BeginQueryId)
		{
			check(Pipeline == ERHIPipeline::Graphics || Pipeline == ERHIPipeline::AsyncCompute);
			DynamicRenderScaling::TMap<int32>& LastTimingScopes = Pipeline == ERHIPipeline::Graphics ? LastTimingScopesGraphics : LastTimingScopesAsyncCompute;

			// Make sure there isn't an ongoing scope on going for that same budget.
			check(LastTimingScopes[BudgetId] == -1);

			if (TimingScopes.Num() == 0)
			{
				TimingScopes.Reserve(kTimingScopesPreallocation);
			}
			else if (TimingScopes.Num() == TimingScopes.Max())
			{
				TimingScopes.Reserve(TimingScopes.Max() * 2);
			}

			const int32 TimingScopeId = TimingScopes.Num();

			FInFlightTimingScope NewTimingScope;
			NewTimingScope.BudgetId = BudgetId;
			NewTimingScope.BeginQueryId = BeginQueryId;
			TimingScopes.Add(NewTimingScope);

			LastTimingScopes[BudgetId] = TimingScopeId;
		}

		// Ends a new timing scope for a specific budget on a pipeline.
		void EndTimingScope(ERHIPipeline Pipeline, int32 BudgetId, int32 EndQueryId)
		{
			check(Pipeline == ERHIPipeline::Graphics || Pipeline == ERHIPipeline::AsyncCompute);
			DynamicRenderScaling::TMap<int32>& LastTimingScopes = Pipeline == ERHIPipeline::Graphics ? LastTimingScopesGraphics : LastTimingScopesAsyncCompute;

			// Make sure there is an ongoing scope on going for that same budget.
			const int32 TimingScopeId = LastTimingScopes[BudgetId];
			check(TimingScopeId != -1);

			FInFlightTimingScope& NewTimingScope = TimingScopes[TimingScopeId];
			check(NewTimingScope.BudgetId == BudgetId);
			NewTimingScope.EndQueryId = EndQueryId;
			check(NewTimingScope.IsValid());

			LastTimingScopes[BudgetId] = -1;
		}

		// Whether this entry is currently in flight or can be reused as new in flight frame.
		bool IsInFlight() const
		{
			return FrameCounter != uint64(-1);
		}

		// Returns whether this frame is landed.
		bool IsLanded(bool bWait) const
		{
			check(IsInRenderingThread());
			check(IsInFlight());

			// If there is a RHI thread, make sure all the queries has been processed by RHI thread to avoid thread race
			// between RHIEndRenderQuery() and RHIGetRenderQueryResult()
			if (IsRunningRHIInSeparateThread())
			{
				if (RHIEndFence.GetReference() && !RHIEndFence->IsComplete())
				{
					if (bWait)
					{
						FRHICommandListExecutor::WaitOnRHIThreadFence(RHIEndFence);
					}
					else
					{
						// Not all RHIEndRenderQuery() has been processed by RHI thread, so return none of the queries have landed
						return false;
					}
				}
			}

			uint64 Timestamp = 0;
			bool bIsLanded = true;

			// verify last graphic and async compute queries are all landed.
			if (LastQueryGraphics)
			{
				bool bQueryIsLanded = RHIGetRenderQueryResult(LastQueryGraphics, /* out */ Timestamp, bWait);
				bIsLanded = bIsLanded && bQueryIsLanded;
			}
			if (LastQueryAsyncCompute)
			{
				bool bQueryIsLanded = RHIGetRenderQueryResult(LastQueryAsyncCompute, /* out */ Timestamp, bWait);
				bIsLanded = bIsLanded && bQueryIsLanded;
			}

			if (!bIsLanded)
			{
				return false;
			}

			// Verify all the queries are truly landed.
			for (int32 i = 0; i < TimestampQueries.Num(); i++)
			{
				bool bQueryIsLanded = RHIGetRenderQueryResult(TimestampQueries[i].GetQuery(), /* out */ Timestamp, bWait);
				bIsLanded = bIsLanded && bQueryIsLanded;
			}

			return bIsLanded;
		}

		// Aggregates all the timing from the different budget together.
		DynamicRenderScaling::TMap<uint64> AggregateLandedTimings(bool bWait) const
		{
			check(IsInRenderingThread());
			DynamicRenderScaling::TMap<uint64> Timings;
			Timings.SetAll(uint64(0));

			// Lands all tthe queries
			TArray<uint64> TimestampQueryResults;
			TimestampQueryResults.Reserve(TimestampQueries.Num());
			for (int32 i = 0; i < TimestampQueries.Num(); i++)
			{
				uint64 Timestamp = 0;
				FRHIRenderQuery* Query = TimestampQueries[i].GetQuery();
				bool bQueryIsLanded = RHIGetRenderQueryResult(TimestampQueries[i].GetQuery(), /* out */ Timestamp, bWait);
				check(bQueryIsLanded);
				TimestampQueryResults.Add(Timestamp);
			}

			for (const FInFlightTimingScope& Scope : TimingScopes)
			{
				Timings[Scope.BudgetId] += TimestampQueryResults[Scope.EndQueryId] - TimestampQueryResults[Scope.BeginQueryId];
			}
			return Timings;
		}

		void ResetValues()
		{
			FrameCounter = uint64(-1);
			TimestampQueries.Empty(TimestampQueries.Max());
			TimingScopes.Empty(TimingScopes.Max());
			LastTimingScopesGraphics.SetAll(-1);
			LastTimingScopesAsyncCompute.SetAll(-1);
			LastQueryGraphics = nullptr;
			LastQueryAsyncCompute = nullptr;
			RHIEndFence = FGraphEventRef();
		}
	};

	// Whether should record timing this frame.
	bool IsRecordingThisFrame() const
	{
		return CurrentFrameInFlightIndex >= 0;
	}

	// Whether should record timing this frame.
	bool IsRecordingThisFrame(const DynamicRenderScaling::FBudget& BudgetId) const
	{
		check(IsInRenderingThread());
		return IsRecordingThisFrame() && bIsBudgetRecordingEnabled[BudgetId];
	}

	void LandInFlightFrames(bool bWait)
	{
		check(!IsRecordingThisFrame());
		for (int32 i = 0; i < InFlightFrames.Num(); i++)
		{
			FInFlightFrame& InFlightFrame = InFlightFrames[i];

			if (!InFlightFrame.IsInFlight())
			{
				continue;
			}

			if (!InFlightFrame.IsLanded(bWait))
			{
				continue;
			}

			if (InFlightFrame.FrameCounter > LastTimingFrameCounter)
			{
				LastTimings = InFlightFrame.AggregateLandedTimings(bWait);
			}

			InFlightFrame.ResetValues();
		}

	}

	void BeginFrame(const DynamicRenderScaling::TMap<bool>& bInIsBudgetEnabled)
	{
		check(IsInRenderingThread());
		check(CurrentFrameInFlightIndex == -1);


		// Land frames
		{
			LandInFlightFrames(/* bWait = */ false);
		}

		bool bRecordThisFrame = false;
		for (TLinkedList<DynamicRenderScaling::FBudget*>::TIterator BudgetIt(DynamicRenderScaling::FBudget::GetGlobalList()); BudgetIt; BudgetIt.Next())
		{
			const DynamicRenderScaling::FBudget& Budget = **BudgetIt;
			bRecordThisFrame = bRecordThisFrame || bInIsBudgetEnabled[Budget];
		}

		// Allocate new inflight frame.
		if (bRecordThisFrame)
		{
			check(DynamicRenderScaling::IsSupported());

			if (!QueryPool.IsValid())
			{
				QueryPool = RHICreateRenderQueryPool(RQT_AbsoluteTime);
			}

			for (int32 i = 0; i < InFlightFrames.Num(); i++)
			{
				FInFlightFrame& InFlightFrame = InFlightFrames[i];
				if (!InFlightFrame.IsInFlight())
				{
					CurrentFrameInFlightIndex = i;
					break;
				}
			}

			// Allocate a new in-flight frame in the unlikely event.
			if (CurrentFrameInFlightIndex == -1)
			{
				CurrentFrameInFlightIndex = InFlightFrames.Num();
				InFlightFrames.Add(FInFlightFrame());

				// make sure there is no memory leak, we Really shouldn't have more than 10 frame in flight.
				ensure(InFlightFrames.Num() < 10);
			}

			InFlightFrames[CurrentFrameInFlightIndex].FrameCounter = GFrameCounterRenderThread;
			check(InFlightFrames[CurrentFrameInFlightIndex].IsInFlight());

			bIsBudgetRecordingEnabled = bInIsBudgetEnabled;
		}
	}

	void EndFrame()
	{
		check(IsInRenderingThread());

		if (IsRecordingThisFrame())
		{
			FInFlightFrame& InFlightFrame = InFlightFrames[CurrentFrameInFlightIndex];

			if (IsRunningRHIInSeparateThread())
			{
				FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
				InFlightFrame.RHIEndFence = RHICmdList.RHIThreadFence();
				RHICmdList.ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);
			}

			CurrentFrameInFlightIndex = -1;
			bIsBudgetRecordingEnabled.SetAll(false);
		}
	}

	const DynamicRenderScaling::TMap<uint64>& GetLatestTimings() const
	{
		check(IsInRenderingThread());
		return LastTimings;
	}

	FInFlightFrame* GetCurrentInFlightFrame()
	{
		check(IsRecordingThisFrame());
		check(InFlightFrames[CurrentFrameInFlightIndex].IsInFlight());
		return &InFlightFrames[CurrentFrameInFlightIndex];
	}

	void CreateTimestampQuery(ERHIPipeline Pipeline, FInFlightFrame* InFlightFrame, FRHIRenderQuery*& OutQuery, int32& OutQueryIndex)
	{
		if (InFlightFrame->TimestampQueries.Num() == 0)
		{
			InFlightFrame->TimestampQueries.Reserve(FInFlightFrame::kTimestampQueriesPreallocation);
		}
		else if (InFlightFrame->TimestampQueries.Num() == InFlightFrame->TimestampQueries.Max())
		{
			InFlightFrame->TimestampQueries.Reserve(InFlightFrame->TimestampQueries.Max() * 2);
		}

		OutQueryIndex = InFlightFrame->TimestampQueries.Num();
		InFlightFrame->TimestampQueries.Add(QueryPool->AllocateQuery());
		OutQuery = InFlightFrame->TimestampQueries.Last().GetQuery();

		if (Pipeline == ERHIPipeline::Graphics)
		{
			InFlightFrame->LastQueryGraphics = OutQuery;
		}
		else if (Pipeline == ERHIPipeline::AsyncCompute)
		{
			InFlightFrame->LastQueryAsyncCompute = OutQuery;
		}
		else
		{
			unimplemented();
		}
	}

	// List of frame queries in flight.
private:
	TArray<FInFlightFrame> InFlightFrames;

	DynamicRenderScaling::TMap<bool> bIsBudgetRecordingEnabled;
	DynamicRenderScaling::TMap<uint64> LastTimings;
	uint64 LastTimingFrameCounter = 0;

	// Current frame's in flight index.
	int32 CurrentFrameInFlightIndex = -1;
};

TGlobalResource<FRDGTimingPool> GRDGTimingPool;


FRDGTimingScopeOpArray::FRDGTimingScopeOpArray(ERHIPipeline Pipeline, const TRDGScopeOpArray<FRDGTimingScopeOp>& Ops)
{
	if (Ops.Num() == 0 || Pipeline != ERHIPipeline::Graphics)
	{
		return;
	}

	FRDGTimingPool::FInFlightFrame* InFlightFrame = GRDGTimingPool.GetCurrentInFlightFrame();

	// Issue a new timestamp query to share across the different FInFlightTimingScope
	int32 TimestampQueryIndex = -1;
	{
		GRDGTimingPool.CreateTimestampQuery(Pipeline, InFlightFrame, /* out */ TimestampQuery, /* out */ TimestampQueryIndex);
		check(TimestampQuery && TimestampQueryIndex >= 0);
	}

	for (int32 Index = 0; Index < Ops.Num(); ++Index)
	{
		FRDGTimingScopeOp Op = Ops[Index];
		check(Op.IsScope());

		int32 BudgetId = Op.Scope->GetBudgetId();

		if (Op.IsPush())
		{
			InFlightFrame->BeginTimingScope(Pipeline, BudgetId, TimestampQueryIndex);
		}
		else
		{
			InFlightFrame->EndTimingScope(Pipeline, BudgetId, TimestampQueryIndex);
		}
	}
}

void FRDGTimingScopeOpArray::Execute(FRHIComputeCommandList& RHICmdList)
{
	ERHIPipeline Pipeline = RHICmdList.GetPipeline();
	if (Pipeline != ERHIPipeline::Graphics)
	{
		check(TimestampQuery == nullptr);
		return; // TODO: FRHIComputeCommandList::EndRenderQuery()
	}

	if (TimestampQuery != nullptr)
	{
		FRHICommandList& RHICmdListGraphics = static_cast<FRHICommandList&>(RHICmdList);
		RHICmdListGraphics.EndRenderQuery(TimestampQuery);
	}
}

FRDGTimingScopeOpArray FRDGTimingScopeStack::CompilePassPrologue(const FRDGPass* Pass)
{
	ERHIPipeline Pipeline = Pass->GetPipeline();
	TRDGScopeOpArray<FRDGTimingScopeOp> Ops = ScopeStack.CompilePassPrologue(Pass->GetGPUScopes().Timing);

	return FRDGTimingScopeOpArray(Pipeline, Ops);
}

namespace DynamicRenderScaling
{

FRDGScope::FRDGScope(FRDGBuilder& InGraphBuilder, const DynamicRenderScaling::FBudget& InBudget)
	: GraphBuilder(InGraphBuilder)
	, Budget(InBudget)
	, bIsEnabled(GRDGTimingPool.IsRecordingThisFrame(InBudget) && !GraphBuilder.GPUScopeStacks.IsTimingScopeAlreadyEnabled(InBudget))
{
	if (bIsEnabled)
	{
		GraphBuilder.GPUScopeStacks.BeginTimingScope(InBudget);
	}
}

FRDGScope::~FRDGScope()
{
	if (bIsEnabled)
	{
		GraphBuilder.GPUScopeStacks.EndTimingScope(Budget);
	}
}

bool IsSupported()
{
	return GRHISupportsGPUTimestampBubblesRemoval;
}

void BeginFrame(const DynamicRenderScaling::TMap<bool>& bIsBudgetEnabled)
{
	GRDGTimingPool.BeginFrame(bIsBudgetEnabled);
}

void EndFrame()
{
	GRDGTimingPool.EndFrame();
}

const TMap<uint64>& GetLastestTimings()
{
	return GRDGTimingPool.GetLatestTimings();
}

} // namespace DynamicRenderScaling


RENDERCORE_API bool GetEmitRDGEvents()
{
#if RDG_EVENTS != RDG_EVENTS_NONE
	bool bRDGChannelEnabled = false;
#if RDG_ENABLE_TRACE
	bRDGChannelEnabled = UE_TRACE_CHANNELEXPR_IS_ENABLED(RDGChannel);
#endif // RDG_ENABLE_TRACE
	return GRDGEvents != 0 && (GRDGEmitDrawEvents_RenderThread != 0 || GRDGDebug != 0 || bRDGChannelEnabled != 0);
#else
	return false;
#endif
}

#if RDG_EVENTS == RDG_EVENTS_STRING_COPY

FRDGEventName::FRDGEventName(const TCHAR* InEventFormat, ...)
	: EventFormat(InEventFormat)
{
	check(InEventFormat);

	if (GRDGValidation != 0)
	{
		va_list VAList;
		va_start(VAList, InEventFormat);
		TCHAR TempStr[256];
		// Build the string in the temp buffer
		FCString::GetVarArgs(TempStr, UE_ARRAY_COUNT(TempStr), InEventFormat, VAList);
		va_end(VAList);

		FormattedEventName = TempStr;
	}
}

#endif

#if RDG_GPU_DEBUG_SCOPES

static void GetEventScopePathRecursive(const FRDGEventScope* Root, FString& String)
{
	if (Root->ParentScope)
	{
		GetEventScopePathRecursive(Root->ParentScope, String);
	}

	if (!String.IsEmpty())
	{
		String += TEXT(".");
	}

	String += Root->Name.GetTCHAR();
}

FString FRDGEventScope::GetPath(const FRDGEventName& Event) const
{
	FString Path;
	GetEventScopePathRecursive(this, Path);
	Path += TEXT(".");
	Path += Event.GetTCHAR();
	return MoveTemp(Path);
}

FRDGEventScopeGuard::FRDGEventScopeGuard(FRDGBuilder& InGraphBuilder, FRDGEventName&& ScopeName, bool InbCondition, ERDGEventScopeFlags InFlags)
	: GraphBuilder(InGraphBuilder)
	, bCondition(InbCondition && !GraphBuilder.GPUScopeStacks.bFinalEventScopeActive)
{
	if (bCondition)
	{
		if (GRDGEvents == 2)
		{
			EnumRemoveFlags(InFlags, ERDGEventScopeFlags::Final);
		}

		GraphBuilder.GPUScopeStacks.bFinalEventScopeActive = (EnumHasAnyFlags(InFlags, ERDGEventScopeFlags::Final));
		GraphBuilder.GPUScopeStacks.BeginEventScope(MoveTemp(ScopeName), GraphBuilder.RHICmdList.GetGPUMask(), InFlags);
	}
}

FRDGEventScopeGuard::~FRDGEventScopeGuard()
{
	if (bCondition)
	{
		GraphBuilder.GPUScopeStacks.EndEventScope();
		GraphBuilder.GPUScopeStacks.bFinalEventScopeActive = false;
	}
}

static void OnPushEvent(FRHIComputeCommandList& RHICmdList, const FRDGEventScope* Scope, bool bRDGEvents)
{
#if RHI_WANT_BREADCRUMB_EVENTS
	RHICmdList.PushBreadcrumb(Scope->Name.GetTCHAR());
#endif

	if (bRDGEvents)
	{
		SCOPED_GPU_MASK(RHICmdList, Scope->GPUMask);
		RHICmdList.PushEvent(Scope->Name.GetTCHAR(), FColor(0));
	}
}

static void OnPopEvent(FRHIComputeCommandList& RHICmdList, const FRDGEventScope* Scope, bool bRDGEvents)
{
	if (bRDGEvents)
	{
		SCOPED_GPU_MASK(RHICmdList, Scope->GPUMask);
		RHICmdList.PopEvent();
	}

#if RHI_WANT_BREADCRUMB_EVENTS
	RHICmdList.PopBreadcrumb();
#endif
}

void FRDGEventScopeOpArray::Execute(FRHIComputeCommandList& RHICmdList)
{
	for (int32 Index = 0; Index < Ops.Num(); ++Index)
	{
		FRDGEventScopeOp Op = Ops[Index];

		if (Op.IsScope())
		{
			if (Op.IsPush())
			{
				OnPushEvent(RHICmdList, Op.Scope, bRDGEvents);
			}
			else
			{
				OnPopEvent(RHICmdList, Op.Scope, bRDGEvents);
			}
		}
		else
		{
			if (Op.IsPush())
			{
				RHICmdList.PushEvent(Op.Name, FColor(255, 255, 255));
			}
			else
			{
				RHICmdList.PopEvent();
			}
		}
	}
}

#if RHI_WANT_BREADCRUMB_EVENTS

void FRDGEventScopeOpArray::Execute(FRDGBreadcrumbState& State)
{
	for (int32 Index = 0; Index < Ops.Num(); ++Index)
	{
		FRDGEventScopeOp Op = Ops[Index];

		if (Op.IsScope())
		{
			if (Op.IsPush())
			{
				State.PushBreadcrumb(Op.Scope->Name.GetTCHAR());
				State.Version++;
			}
			else
			{
				State.PopBreadcrumb();
				State.Version++;
			}
		}
	}
}

#endif

FRDGEventScopeOpArray FRDGEventScopeStack::CompilePassPrologue(const FRDGPass* Pass)
{
	FRDGEventScopeOpArray Ops(bRDGEvents);
	if (IsEnabled())
	{
		const FRDGEventScope* Scope = Pass->GetGPUScopes().Event;
		const bool bEmitPassName = GetEmitRDGEvents() && (!Scope || !EnumHasAnyFlags(Scope->Flags, ERDGEventScopeFlags::Final));

		Ops.Ops = ScopeStack.CompilePassPrologue(Scope, bEmitPassName ? Pass->GetEventName().GetTCHAR() : nullptr);
	}
	return MoveTemp(Ops);
}

FRDGEventScopeOpArray FRDGEventScopeStack::CompilePassEpilogue()
{
	FRDGEventScopeOpArray Ops(bRDGEvents);
	if (IsEnabled())
	{
		Ops.Ops = ScopeStack.CompilePassEpilogue();
	}
	return MoveTemp(Ops);
}

FRDGGPUStatScopeGuard::FRDGGPUStatScopeGuard(FRDGBuilder& InGraphBuilder, const FName& Name, const FName& StatName, const TCHAR* Description, FDrawCallCategoryName& Category)
	: GraphBuilder(InGraphBuilder)
{
	GraphBuilder.GPUScopeStacks.BeginStatScope(Name, StatName, Description, Category);
}

FRDGGPUStatScopeGuard::~FRDGGPUStatScopeGuard()
{
	GraphBuilder.GPUScopeStacks.EndStatScope();
}

FRDGGPUStatScopeOpArray::FRDGGPUStatScopeOpArray(TRDGScopeOpArray<FRDGGPUStatScopeOp> InOps, FRHIGPUMask GPUMask)
	: Ops(InOps)
	, Type(EType::Prologue)
{
#if HAS_GPU_STATS
	for (int32 Index = 0; Index < Ops.Num(); ++Index)
	{
		FRDGGPUStatScopeOp& Op = Ops[Index];

		if (Op.IsPush())
		{
			Op.Query = FRealtimeGPUProfiler::Get()->PushEvent(GPUMask, Op.Scope->Name, Op.Scope->StatName, *Op.Scope->Description);
		}
		else
		{
			Op.Query = FRealtimeGPUProfiler::Get()->PopEvent();
		}
	}
#endif
}

void FRDGGPUStatScopeOpArray::Execute(FRHIComputeCommandList& RHICmdListCompute)
{
#if HAS_GPU_STATS
	if (!RHICmdListCompute.IsGraphics())
	{
		return;
	}

	FRHICommandList& RHICmdList = static_cast<FRHICommandList&>(RHICmdListCompute);

	for (int32 Index = 0; Index < Ops.Num(); ++Index)
	{
		Ops[Index].Query.Submit(RHICmdList);
	}

	if (OverrideEventIndex != kInvalidEventIndex)
	{
		if (Type == EType::Prologue)
		{
			FRealtimeGPUProfiler::Get()->PushEventOverride(OverrideEventIndex);
		}
		else
		{
			FRealtimeGPUProfiler::Get()->PopEventOverride();
		}
	}

	for (int32 Index = Ops.Num() - 1; Index >= 0; --Index)
	{
		const FRDGGPUStatScopeOp Op = Ops[Index];
		const FRDGGPUStatScope* Scope = Op.Scope;

		if (Scope->Category.ShouldCountDraws())
		{
		    RHICmdList.SetStatsCategory(Op.IsPush()
			    ? &Scope->Category
			    : nullptr
		    );
		}
	}
#endif
}

FRDGGPUStatScopeOpArray FRDGGPUStatScopeStack::CompilePassPrologue(const FRDGPass* Pass, FRHIGPUMask GPUMask)
{
#if HAS_GPU_STATS
	if (IsEnabled() && Pass->GetPipeline() == ERHIPipeline::Graphics)
	{
		FRDGGPUStatScopeOpArray Ops(ScopeStack.CompilePassPrologue(Pass->GetGPUScopes().Stat), GPUMask);
		if (!Pass->IsParallelExecuteAllowed())
		{
			OverrideEventIndex = FRealtimeGPUProfiler::Get()->GetCurrentEventIndex();
			Ops.OverrideEventIndex = OverrideEventIndex;
		}
		return MoveTemp(Ops);
	}
#endif
	return {};
}

FRDGGPUStatScopeOpArray FRDGGPUStatScopeStack::CompilePassEpilogue()
{
#if HAS_GPU_STATS
	if (OverrideEventIndex != FRDGGPUStatScopeOpArray::kInvalidEventIndex)
	{
		FRDGGPUStatScopeOpArray Ops;
		Ops.OverrideEventIndex = OverrideEventIndex;
		OverrideEventIndex = FRDGGPUStatScopeOpArray::kInvalidEventIndex;
		return MoveTemp(Ops);
	}
#endif
	return {};
}

#endif // RDG_GPU_DEBUG_SCOPES

FRDGGPUScopeOpArrays FRDGGPUScopeStacksByPipeline::CompilePassPrologue(const FRDGPass* Pass, FRHIGPUMask GPUMask)
{
	return GetScopeStacks(Pass->GetPipeline()).CompilePassPrologue(Pass, GPUMask);
}

FRDGGPUScopeOpArrays FRDGGPUScopeStacksByPipeline::CompilePassEpilogue(const FRDGPass* Pass)
{
	return GetScopeStacks(Pass->GetPipeline()).CompilePassEpilogue();
}

//////////////////////////////////////////////////////////////////////////
// CPU Scopes
//////////////////////////////////////////////////////////////////////////

#if RDG_CPU_SCOPES

#if CSV_PROFILER

FRDGScopedCsvStatExclusive::FRDGScopedCsvStatExclusive(FRDGBuilder& InGraphBuilder, const char* InStatName)
	: FScopedCsvStatExclusive(InStatName)
	, GraphBuilder(InGraphBuilder)
{
	GraphBuilder.CPUScopeStacks.CSV.BeginScope(InStatName);
}

FRDGScopedCsvStatExclusive::~FRDGScopedCsvStatExclusive()
{
	GraphBuilder.CPUScopeStacks.CSV.EndScope();
}

FRDGScopedCsvStatExclusiveConditional::FRDGScopedCsvStatExclusiveConditional(FRDGBuilder& InGraphBuilder, const char* InStatName, bool bInCondition)
	: FScopedCsvStatExclusiveConditional(InStatName, bInCondition)
	, GraphBuilder(InGraphBuilder)
{
	if (bCondition)
	{
		GraphBuilder.CPUScopeStacks.CSV.BeginScope(InStatName);
	}
}

FRDGScopedCsvStatExclusiveConditional::~FRDGScopedCsvStatExclusiveConditional()
{
	if (bCondition)
	{
		GraphBuilder.CPUScopeStacks.CSV.EndScope();
	}
}

#endif

inline void OnPushCSVStat(const FRDGCSVStatScope* Scope)
{
#if CSV_PROFILER
	FCsvProfiler::BeginExclusiveStat(Scope->StatName);
#endif
}

inline void OnPopCSVStat(const FRDGCSVStatScope* Scope)
{
#if CSV_PROFILER
	FCsvProfiler::EndExclusiveStat(Scope->StatName);
#endif
}

void FRDGCSVStatScopeOpArray::Execute()
{
	for (int32 Index = 0; Index < Ops.Num(); ++Index)
	{
		FRDGCSVStatScopeOp Op = Ops[Index];

		if (Op.IsPush())
		{
			OnPushCSVStat(Op.Scope);
		}
		else
		{
			OnPopCSVStat(Op.Scope);
		}
	}
}

FRDGCSVStatScopeOpArray FRDGCSVStatScopeStack::CompilePassPrologue(const FRDGPass* Pass)
{
	if (IsEnabled())
	{
		return ScopeStack.CompilePassPrologue(Pass->GetCPUScopes().CSV);
	}
	return {};
}

#endif
