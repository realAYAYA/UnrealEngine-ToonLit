// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenGLQuery.cpp: OpenGL query RHI implementation.
=============================================================================*/

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "OpenGLDrv.h"
#include "OpenGLDrvPrivate.h"
#include "RenderCore.h"

static int32 GOpenGLPollRenderQueryResult = 1;
static FAutoConsoleVariableRef CVarOpenGLPollRenderQueryResult(
	TEXT("r.OpenGL.PollRenderQueryResult"),
	GOpenGLPollRenderQueryResult,
	TEXT("Whether to poll render query for result until it's ready, otherwise do a blocking call to get result.")
	TEXT("0: Block, 1: Poll (default)"),
	ECVF_Default
	);

struct FQueryItem
{
	FRHIRenderQuery* Query;
	int32 BeginSequence;

	FQueryItem(FRHIRenderQuery* InQueryRHI)
		: Query(InQueryRHI)
	{
		FOpenGLRenderQuery* InQuery = FOpenGLDynamicRHI::ResourceCast(InQueryRHI);
		BeginSequence = InQuery->TotalBegins.GetValue();
	}
};

struct FGLQueryBatch
{
	TArray<FQueryItem> BatchContents;
	uint32 FrameNumberRenderThread;
	bool bHasFlushedSinceLastWait;

	FGLQueryBatch()
		: FrameNumberRenderThread(0)
		, bHasFlushedSinceLastWait(false)
	{

	}
};

struct FGLQueryBatcher
{
	FGLQueryBatch* NewBatch;
	TArray<FGLQueryBatch*> Batches;
	uint32 NextFrameNumberRenderThread;

	FGLQueryBatcher()
		: NewBatch(nullptr)
		, NextFrameNumberRenderThread(1)
	{
	}

	void Add(FRHIRenderQuery* Query)
	{
		if (NewBatch && NewBatch->FrameNumberRenderThread)
		{
			NewBatch->BatchContents.Add(FQueryItem(Query));
		}
	}

	void AddSingle(FRHIRenderQuery* Query)
	{
		FGLQueryBatch* SingleBatch = new FGLQueryBatch();
		SingleBatch->FrameNumberRenderThread = NextFrameNumberRenderThread;
		SingleBatch->BatchContents.Add(FQueryItem(Query));
		Batches.Add(SingleBatch);
	}

	void Waited()
	{
		for (int32 Index = 0; Index < Batches.Num(); Index++)
		{
			FGLQueryBatch* Batch = Batches[Index];
			Batch->bHasFlushedSinceLastWait = false;
		}
	}
	void Flush(FOpenGLDynamicRHI& RHI, FRHIRenderQuery* TargetQueryRHI)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FGLQueryBatcher_FlushScan);
		bool bFoundQuery = false;
		for (int32 Index = 0; Index < Batches.Num() && !bFoundQuery; Index++)
		{
			FGLQueryBatch* Batch = Batches[Index];
			if (Batch->bHasFlushedSinceLastWait)
			{
				break;
			}
			bool bAnyUnfinished = false;

			for (int32 IndexInner = 0; IndexInner < Batch->BatchContents.Num(); IndexInner++)
			{
				FQueryItem& Item = Batch->BatchContents[IndexInner];
				FRHIRenderQuery* QueryRHI = Item.Query;
				FOpenGLRenderQuery* Query = FOpenGLDynamicRHI::ResourceCast(QueryRHI);
				if (TargetQueryRHI == QueryRHI)
				{
					bFoundQuery = true;
				}

				if (Item.BeginSequence < Query->TotalBegins.GetValue())
				{
					// stale entry, was never checked, but was reused
					Batch->BatchContents.RemoveAtSwap(IndexInner--, 1, EAllowShrinking::No);
					continue;
				}
			
				RHI.GetRenderQueryResult_OnThisThread(Query, false);
				if (Query->TotalResults.GetValue() == Query->TotalBegins.GetValue())
				{
					Batch->BatchContents.RemoveAtSwap(IndexInner--, 1, EAllowShrinking::No);
				}
				else
				{
					bAnyUnfinished = true;
				}
			}
			if (!bAnyUnfinished || Batch->BatchContents.Num() == 0)
			{
				delete Batch;
				Batches.RemoveAt(Index--);
			}
			else
			{
				Batch->bHasFlushedSinceLastWait = true;
				break;
			}
		}
	}

	// this just tries to readback queries until it finds one that is not ready
	void SoftFlush(FOpenGLDynamicRHI& RHI, bool bResetHasFlushedSinceLastWait = false)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FGLQueryBatcher_SoftFlushScan);
		for (int32 Index = 0; Index < Batches.Num(); Index++)
		{
			FGLQueryBatch* Batch = Batches[Index];
			if (bResetHasFlushedSinceLastWait)
			{
				Batch->bHasFlushedSinceLastWait = false; // we will try a full scan if we get around to initviews
			}

			if (Batch->FrameNumberRenderThread == NextFrameNumberRenderThread)
			{
				// do not scan queries issued this frame, 
				// on some Android devices this causes stalls in the driver (eg. S7 Adreno with Android 7)
				break;
			}

			for (int32 IndexInner = 0; IndexInner < Batch->BatchContents.Num(); IndexInner++)
			{
				FQueryItem& Item = Batch->BatchContents[IndexInner];
				FRHIRenderQuery* QueryRHI = Item.Query;
				FOpenGLRenderQuery* Query = FOpenGLDynamicRHI::ResourceCast(QueryRHI);

				int32 Begins = Query->TotalBegins.GetValue();

				if (Item.BeginSequence < Query->TotalBegins.GetValue())
				{
					// stale entry, was never checked, but was reused
					Batch->BatchContents.RemoveAtSwap(IndexInner--, 1, EAllowShrinking::No);
					continue;
				}

				RHI.GetRenderQueryResult_OnThisThread(Query, false);
				if (Query->TotalResults.GetValue() == Query->TotalBegins.GetValue())
				{
					Batch->BatchContents.RemoveAtSwap(IndexInner--, 1, EAllowShrinking::No);
				}
			}
			if (Batch->BatchContents.Num() == 0)
			{
				delete Batch;
				Batches.RemoveAt(Index--);
			}
			else
			{
				break;
			}
		}
	}

	void PerFrameFlush()
	{
		NextFrameNumberRenderThread++;
		for (int32 Index = 0; Index < Batches.Num(); Index++)
		{
			FGLQueryBatch* Batch = Batches[Index];
			if (Batch->FrameNumberRenderThread <= NextFrameNumberRenderThread - 5)
			{
				delete Batch;
				Batches.RemoveAt(Index--);
			}
		}
	}

	void StartNewBatch(FOpenGLDynamicRHI& RHI)
	{
		check(!NewBatch);
		NewBatch = new FGLQueryBatch();
		NewBatch->FrameNumberRenderThread = NextFrameNumberRenderThread;
	}

	void EndBatch(FOpenGLDynamicRHI& RHI)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FGLQueryBatcher_EndBatch);
		SoftFlush(RHI, true);
		if (NewBatch)
		{
			Batches.Add(NewBatch);
			NewBatch = nullptr;
		}
	}

} GBatcher;

void BeginFrame_QueryBatchCleanup()
{
	GBatcher.PerFrameFlush();
}

void BeginOcclusionQueryBatch(uint32 NumOcclusionQueries)
{
	if (IsRunningRHIInSeparateThread())
	{
		GBatcher.StartNewBatch(*GetDynamicRHI<FOpenGLDynamicRHI>());
	}
}

void EndOcclusionQueryBatch()
{
	if (IsRunningRHIInSeparateThread())
	{
		GBatcher.EndBatch(*GetDynamicRHI<FOpenGLDynamicRHI>());
	}
}

void OpenGL_PollAllFences();

void FOpenGLDynamicRHI::RHIPollOcclusionQueries()
{
	if (IsRunningRHIInSeparateThread())
	{
		GBatcher.SoftFlush(*GetDynamicRHI<FOpenGLDynamicRHI>());

		OpenGL_PollAllFences();
	}
}

FRenderQueryRHIRef FOpenGLDynamicRHI::RHICreateRenderQuery(ERenderQueryType QueryType)
{

	check(QueryType == RQT_Occlusion || QueryType == RQT_AbsoluteTime);

	if(QueryType == RQT_AbsoluteTime && FOpenGL::SupportsTimestampQueries() == false)
	{
		return NULL;
	}

	return new FOpenGLRenderQuery(QueryType);
}

void FOpenGLDynamicRHI::RHIBeginRenderQuery(FRHIRenderQuery* QueryRHI)
{
	VERIFY_GL_SCOPE();

	FOpenGLRenderQuery* Query = ResourceCast(QueryRHI);

	if (Query)
	{
		BeginRenderQuery_OnThisThread(Query);
		GBatcher.Add(QueryRHI);
	}
}

void FOpenGLDynamicRHI::RHIEndRenderQuery(FRHIRenderQuery* QueryRHI)
{
	VERIFY_GL_SCOPE();

	FOpenGLRenderQuery* Query = ResourceCast(QueryRHI);

	if (Query)
	{
		EndRenderQuery_OnThisThread(Query);
		if (Query->QueryType == RQT_AbsoluteTime)
		{
			GBatcher.AddSingle(Query);
		}
	}
}

void FOpenGLDynamicRHI::BeginRenderQuery_OnThisThread(FOpenGLRenderQuery* Query)
{
	VERIFY_GL_SCOPE();

	int32 NewVal = Query->TotalBegins.Increment();
	Query->TotalResults.Set(NewVal - 1);
	Query->Result = 0;
	Query->bResultWasSuccess = false;

	if (Query->QueryType == RQT_Occlusion)
	{
		check(PendingState.RunningOcclusionQuery == 0);

		if (!Query->bInvalidResource && !PlatformContextIsCurrent(Query->ResourceContext))
		{
			PlatformReleaseRenderQuery(Query->Resource, Query->ResourceContext);
			Query->bInvalidResource = true;
		}

		if (Query->bInvalidResource)
		{
			PlatformGetNewRenderQuery(&Query->Resource, &Query->ResourceContext);
			Query->bInvalidResource = false;
		}

		GLenum QueryType = FOpenGL::SupportsExactOcclusionQueries() ? UGL_SAMPLES_PASSED : UGL_ANY_SAMPLES_PASSED;
		FOpenGL::BeginQuery(QueryType, Query->Resource);
		PendingState.RunningOcclusionQuery = Query->Resource;
	}
	else
	{
		// not supported/needed for RQT_AbsoluteTime
		check(0);
	}
}

void FOpenGLDynamicRHI::EndRenderQuery_OnThisThread(FOpenGLRenderQuery* Query)
{
	VERIFY_GL_SCOPE();

	if (Query)
	{
		if (Query->QueryType == RQT_Occlusion)
		{
			if (!Query->bInvalidResource && !PlatformContextIsCurrent(Query->ResourceContext))
			{
				PlatformReleaseRenderQuery(Query->Resource, Query->ResourceContext);
				Query->Resource = 0;
				Query->bInvalidResource = true;
			}

			if (!Query->bInvalidResource)
			{
				check(PendingState.RunningOcclusionQuery == Query->Resource);
				PendingState.RunningOcclusionQuery = 0;
				GLenum QueryType = FOpenGL::SupportsExactOcclusionQueries() ? UGL_SAMPLES_PASSED : UGL_ANY_SAMPLES_PASSED;
				FOpenGL::EndQuery(QueryType);
			}
		}
		else if (Query->QueryType == RQT_AbsoluteTime)
		{
			int32 NewVal = Query->TotalBegins.Increment();
			Query->TotalResults.Set(NewVal - 1);
			Query->Result = 0;
			Query->bResultWasSuccess = false;

			if (!Query->bInvalidResource && !PlatformContextIsCurrent(Query->ResourceContext))
			{
				PlatformReleaseRenderQuery(Query->Resource, Query->ResourceContext);
				Query->Resource = 0;
				Query->bInvalidResource = true;
			}

			// query can be silently invalidated in GetRenderQueryResult
			if (Query->bInvalidResource)
			{
				PlatformGetNewRenderQuery(&Query->Resource, &Query->ResourceContext);
				Query->bInvalidResource = false;
			}

			FOpenGL::QueryTimestampCounter(Query->Resource);
		}
	}
}

static void GetRenderQueryResult(FOpenGLRenderQuery* Query)
{
	VERIFY_GL_SCOPE();
	if (Query->QueryType == RQT_AbsoluteTime)
	{
		FOpenGL::GetQueryObject(Query->Resource, FOpenGL::QM_Result, &Query->Result);
	}
	else
	{
		GLuint Result32 = 0;
		FOpenGL::GetQueryObject(Query->Resource, FOpenGL::QM_Result, &Result32);
		Query->Result = Result32 * (FOpenGL::SupportsExactOcclusionQueries() ? 1 : 500000); // half a mega pixel display
	}
	Query->bResultWasSuccess = true;
	Query->TotalResults.Increment();
}

void FOpenGLDynamicRHI::GetRenderQueryResult_OnThisThread(FOpenGLRenderQuery* Query, bool bWait)
{
	if (Query->TotalResults.GetValue() == Query->TotalBegins.GetValue())
	{
		return;
	}
	check(Query->TotalResults.GetValue() + 1 == Query->TotalBegins.GetValue());

	VERIFY_GL_SCOPE();

	if (!Query->bInvalidResource && !PlatformContextIsCurrent(Query->ResourceContext))
	{
		PlatformReleaseRenderQuery(Query->Resource, Query->ResourceContext);
		Query->Resource = 0;
		Query->bInvalidResource = true;
	}

	// Check if the query is valid first
	if (Query->bInvalidResource)
	{
		Query->Result = 0;
		Query->TotalResults.Increment();
	}
	else
	{
		// Check if the query is finished
		GLuint Result = 0;
		FOpenGL::GetQueryObject(Query->Resource, FOpenGL::QM_ResultAvailable, &Result);
		if (Result == GL_TRUE)
		{
			GetRenderQueryResult(Query);
		}
		else if (bWait) // Isn't the query finished yet, and can we wait for it?
		{
			SCOPE_CYCLE_COUNTER(STAT_RenderQueryResultTime);

			FRenderThreadIdleScope IdleScope(ERenderThreadIdleTypes::WaitingForGPUQuery);
			GBatcher.Waited();
			
			if (GOpenGLPollRenderQueryResult == 0)
			{
				// block in the driver waiting for result
				GetRenderQueryResult(Query);
			}
			else
			{
				// poll result until it's ready
				double StartTime = FPlatformTime::Seconds();
				do
				{
					FPlatformProcess::Sleep(0);	// yield to other threads - some of them may be OpenGL driver's and we'd be starving them

					if (Query->bInvalidResource)
					{
						// Query got invalidated while we were sleeping.
						// Bail out, no sense to wait and generate OpenGL errors,
						// we're in a new OpenGL context that knows nothing about us.
						Query->Result = 1000;	// safe value
						Result = GL_FALSE;
						bWait = false;
						Query->bResultWasSuccess = true;
						break;
					}

					FOpenGL::GetQueryObject(Query->Resource, FOpenGL::QM_ResultAvailable, &Result);

					// timer queries are used for Benchmarks which can stall a bit more
					double TimeoutValue = (Query->QueryType == RQT_AbsoluteTime) ? 2.0 : 0.5;

					if ((FPlatformTime::Seconds() - StartTime) > TimeoutValue)
					{
						UE_LOG(LogRHI, Log, TEXT("Timed out while waiting for GPU to catch up. (%.1f s)"), TimeoutValue);
						break;
					}
				} while (Result == GL_FALSE);
				
				if (Result == GL_TRUE)
				{
					GetRenderQueryResult(Query);
				}
				else
				{
					Query->Result = 0;
					Query->TotalResults.Increment();
				}
			}
		}
	}
}

class FPollQueriesRHIThreadTask
{
	FOpenGLRenderQuery* Query;
	FOpenGLDynamicRHI* RHI;
	bool bWait;

public:

	FPollQueriesRHIThreadTask(FOpenGLRenderQuery* InQuery, FOpenGLDynamicRHI* InRHI, bool bInWait)
		: Query(InQuery)
		, RHI(InRHI)
		, bWait(bInWait)
	{
	}

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FPollQueriesRHIThreadTask, STATGROUP_TaskGraphTasks);
	}

	ENamedThreads::Type GetDesiredThread()
	{
		return ENamedThreads::SetTaskPriority(ENamedThreads::RHIThread, ENamedThreads::HighTaskPriority);
	}

	static ESubsequentsMode::Type GetSubsequentsMode() { return ESubsequentsMode::TrackSubsequents; }

	void DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		check(IsInRHIThread());
		check(IsRunningRHIInDedicatedThread() && IsInRHIThread()); // this should never be used on a platform that doesn't support the RHI thread, and it can't quite work when running the RHI stuff on task threads
		if (bWait)
		{
			RHI->GetRenderQueryResult_OnThisThread(Query, true); // we must get this one if bWait is true;
			RHI->RHIPollOcclusionQueries(); // finish any other ones, but don't wait
		}
		else
		{
			RHI->GetRenderQueryResult_OnThisThread(Query, false);
			if (Query->TotalResults.GetValue() == Query->TotalBegins.GetValue())
			{
				RHI->RHIPollOcclusionQueries(); // If the target query was ready, then go ahead and scan to see what else is ready.
			}
		}
	}
};


bool FOpenGLDynamicRHI::RHIGetRenderQueryResult(FRHIRenderQuery* QueryRHI, uint64& OutResult, bool bWait, uint32 GPUIndex)
{
	check(IsInRenderingThread() || IsInRHIThread());

	FOpenGLRenderQuery* Query = ResourceCast(QueryRHI);

	if (!Query)
	{
		// If timer queries are unsupported, just make sure that OutResult does not contain any random values.
		OutResult = 0;
		return false;
	}

	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

	const bool bCanRunOnThisThread = RHICmdList.Bypass() || (!IsRunningRHIInSeparateThread() && IsInRenderingThread()) || IsInRHIThread();

	if (Query->TotalResults.GetValue() != Query->TotalBegins.GetValue())
	{
		if (bCanRunOnThisThread)
		{
			GetRenderQueryResult_OnThisThread(Query, bWait);
		}
		else
		{
			if (bWait)
			{
				QUICK_SCOPE_CYCLE_COUNTER(STAT_WaitForRHIThreadOcclusionReadback);
				if (IsRunningRHIInDedicatedThread())
				{
					// send a command that will wait, so if the RHIT runs out of work, it just blocks and waits for the GPU
					ALLOC_COMMAND_CL(RHICmdList, FRHICommandGLCommand)([this, QueryRHI]() {GetRenderQueryResult_OnThisThread(ResourceCast(QueryRHI), true); });
					FGraphEventRef Done = RHICmdList.RHIThreadFence(false);
					ALLOC_COMMAND_CL(RHICmdList, FRHICommandGLCommand)([this, QueryRHI]() {GBatcher.Flush(*this, QueryRHI); });
					RHICmdList.ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);
					while (!Done->IsComplete())
					{
						FGraphEventRef RHITask = TGraphTask<FPollQueriesRHIThreadTask>::CreateTask().ConstructAndDispatchWhenReady(ResourceCast(QueryRHI), this, false);
						FTaskGraphInterface::Get().WaitUntilTaskCompletes(RHITask);

						if (Query->TotalResults.GetValue() == Query->TotalBegins.GetValue())
						{
							break;
						}
						// We want to keep the RHIT working, but we want keep checking between command lists so that we can get the results as soon as the GPU has them

						// this isn't really a spin, the ping-pong between threads will not consume CPU (usually a bad thing, not here).
					}
				}
				else
				{
					ALLOC_COMMAND_CL(RHICmdList, FRHICommandGLCommand)([this, QueryRHI]() {GetRenderQueryResult_OnThisThread(ResourceCast(QueryRHI), true); });
					FGraphEventRef Done = RHICmdList.RHIThreadFence(false);
					ALLOC_COMMAND_CL(RHICmdList, FRHICommandGLCommand)([this, QueryRHI]() {GBatcher.Flush(*this, QueryRHI); });
					RHICmdList.ImmediateFlush(EImmediateFlushType::DispatchToRHIThread);
					FRHICommandListExecutor::WaitOnRHIThreadFence(Done);
				}
				check(Query->TotalResults.GetValue() == Query->TotalBegins.GetValue());
			}
			else
			{
				ALLOC_COMMAND_CL(RHICmdList, FRHICommandGLCommand)([this, QueryRHI]() {GetRenderQueryResult_OnThisThread(ResourceCast(QueryRHI), false); GBatcher.Flush(*this, QueryRHI);  });
			}
		}	
	}
	if (Query->TotalResults.GetValue() == Query->TotalBegins.GetValue() && Query->bResultWasSuccess)
	{
		if (Query->QueryType == RQT_AbsoluteTime)
		{
			// GetTimingFrequency is the number of ticks per second
			uint64 Div = FMath::Max(1llu, FOpenGLBufferedGPUTiming::GetTimingFrequency() / (1000 * 1000));

			// convert from GPU specific timestamp to micro sec (1 / 1 000 000 s) which seems a reasonable resolution
			OutResult = Query->Result / Div;
		}
		else
		{
			OutResult = Query->Result;
		}
		return true;
	}
	OutResult = 0;
	return false;
}



FRenderQueryPoolRHIRef FOpenGLDynamicRHI::RHICreateRenderQueryPool(ERenderQueryType QueryType, uint32 /*NumQueries*/)
{
	// Workaround to fix UE-77873: pass <QueryType> to primary implementation and ignore <NumQueries> for pre-allocated queries
	return FDynamicRHI::RHICreateRenderQueryPool(QueryType);
}

extern void OnQueryCreation( FOpenGLRenderQuery* Query );
extern void OnQueryDeletion( FOpenGLRenderQuery* Query );

FOpenGLRenderQuery::FOpenGLRenderQuery(ERenderQueryType InQueryType)
	: Result(0)
	, bInvalidResource(true)
	, QueryType(InQueryType)
{
	check(IsInRenderingThread());
	FRHICommandListExecutor::GetImmediateCommandList().EnqueueLambda(
		[this](FRHICommandListImmediate&) { AcquireResource(); }
	);
}


FOpenGLRenderQuery::~FOpenGLRenderQuery()
{
	VERIFY_GL_SCOPE();
	OnQueryDeletion( this );

	if (Resource && !bInvalidResource)
	{
		bInvalidResource = true;
		ReleaseResource(Resource, ResourceContext);
	}
}

void FOpenGLRenderQuery::AcquireResource()
{
	VERIFY_GL_SCOPE();
	bInvalidResource = false;
	PlatformGetNewRenderQuery(&Resource, &ResourceContext);
	OnQueryCreation(this);
}
void FOpenGLRenderQuery::ReleaseResource(GLuint Resource, uint64 ResourceContext)
{
	VERIFY_GL_SCOPE();
	check(Resource);
	PlatformReleaseRenderQuery(Resource, ResourceContext);
}



void FOpenGLEventQuery::IssueEvent()
{
	VERIFY_GL_SCOPE();
	if(Sync)
	{
		FOpenGL::DeleteSync(Sync);
		Sync = UGLsync();
	}
	Sync = FOpenGL::FenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	FOpenGL::Flush();

	checkSlow(FOpenGL::IsSync(Sync));
}

void FOpenGLEventQuery::WaitForCompletion()
{
	VERIFY_GL_SCOPE();

	QUICK_SCOPE_CYCLE_COUNTER(STAT_FOpenGLEventQuery_WaitForCompletion);

	checkSlow(FOpenGL::IsSync(Sync));

	// Wait up to 1/2 second for sync execution
	FOpenGL::EFenceResult Status = FOpenGL::ClientWaitSync( Sync, 0, 500*1000*1000);

	switch (Status)
	{
	case FOpenGL::FR_AlreadySignaled:
	case FOpenGL::FR_ConditionSatisfied:
		break;

	case FOpenGL::FR_TimeoutExpired:
		UE_LOG(LogRHI, Log, TEXT("Timed out while waiting for GPU to catch up. (500 ms)"));
		break;

	case FOpenGL::FR_WaitFailed:
		UE_LOG(LogRHI, Log, TEXT("Wait on GPU failed in driver"));
		break;

	default:
	    UE_LOG(LogRHI, Log, TEXT("Unknown error while waiting on GPU"));
	    check(0);
		break;
	}	
}

FOpenGLEventQuery::FOpenGLEventQuery()
{
	VERIFY_GL_SCOPE();

	// Initialize the query by issuing an initial event.
	IssueEvent();

	check(FOpenGL::IsSync(Sync));
}

FOpenGLEventQuery::~FOpenGLEventQuery()
{
	VERIFY_GL_SCOPE();
	FOpenGL::DeleteSync(Sync);
}

/*=============================================================================
 * class FOpenGLBufferedGPUTiming
 *=============================================================================*/

/**
 * Constructor.
 *
 * @param InOpenGLRHI			RHI interface
 * @param InBufferSize		Number of buffered measurements
 */
FOpenGLBufferedGPUTiming::FOpenGLBufferedGPUTiming( FOpenGLDynamicRHI* InOpenGLRHI, int32 InBufferSize )
:	OpenGLRHI( InOpenGLRHI )
,	BufferSize( InBufferSize )
,	CurrentTimestamp( -1 )
,	NumIssuedTimestamps( 0 )
,	bIsTiming( false )
{
}

/**
 * Initializes the static variables, if necessary.
 */
void FOpenGLBufferedGPUTiming::PlatformStaticInitialize(void* UserData)
{
	// Are the static variables initialized?
	if ( !GAreGlobalsInitialized )
	{
		GIsSupported = FOpenGL::SupportsTimestampQueries();
		SetTimingFrequency(1000 * 1000 * 1000);
		GAreGlobalsInitialized = true;
	}
}

/**
 * Initializes all OpenGL resources and if necessary, the static variables.
 */

static TArray<FOpenGLRenderQuery*> TimerQueryPool;

static FOpenGLRenderQuery* GetTimeQuery()
{
	if (TimerQueryPool.Num())
	{
		return TimerQueryPool.Pop();
	}
	return new FOpenGLRenderQuery(RQT_AbsoluteTime);
}

void FOpenGLBufferedGPUTiming::InitResources()
{
	StaticInitialize(OpenGLRHI, PlatformStaticInitialize);

	CurrentTimestamp = 0;
	NumIssuedTimestamps = 0;
	bIsTiming = false;
	GIsSupported = FOpenGL::SupportsTimestampQueries();

	if ( GIsSupported )
	{
		StartTimestamps.Reserve(BufferSize);
		EndTimestamps.Reserve(BufferSize);

		for(int32 BufferIndex = 0; BufferIndex < BufferSize; ++BufferIndex)
		{
			StartTimestamps.Add(GetTimeQuery());
			EndTimestamps.Add(GetTimeQuery());
		}
	}
}

/**
 * Releases all OpenGL resources.
 */
void FOpenGLBufferedGPUTiming::ReleaseResources()
{
	VERIFY_GL_SCOPE();

	for(FOpenGLRenderQuery* Query : StartTimestamps)
	{
		TimerQueryPool.Add(Query);
	}

	for(FOpenGLRenderQuery* Query : EndTimestamps)
	{
		TimerQueryPool.Add(Query);
	}

	StartTimestamps.Reset();
	EndTimestamps.Reset();

}

/**
 * Start a GPU timing measurement.
 */
void FOpenGLBufferedGPUTiming::StartTiming()
{
	VERIFY_GL_SCOPE();
	// Issue a timestamp query for the 'start' time.
	if ( GIsSupported && !bIsTiming )
	{
		int32 NewTimestampIndex = (CurrentTimestamp + 1) % BufferSize;
		FOpenGLRenderQuery* TimerQuery = StartTimestamps[NewTimestampIndex];
		{
			if (!TimerQuery->bInvalidResource && !PlatformContextIsCurrent(TimerQuery->ResourceContext))
			{
				PlatformReleaseRenderQuery(TimerQuery->Resource, TimerQuery->ResourceContext);
				TimerQuery->bInvalidResource = true;
			}

			if (TimerQuery->bInvalidResource)
			{
				PlatformGetNewRenderQuery(&TimerQuery->Resource, &TimerQuery->ResourceContext);
				TimerQuery->bInvalidResource = false;
			}
		}

		FOpenGL::QueryTimestampCounter(StartTimestamps[NewTimestampIndex]->Resource);
		CurrentTimestamp = NewTimestampIndex;
		bIsTiming = true;
	}
}

/**
 * End a GPU timing measurement.
 * The timing for this particular measurement will be resolved at a later time by the GPU.
 */
void FOpenGLBufferedGPUTiming::EndTiming()
{
	VERIFY_GL_SCOPE();
	// Issue a timestamp query for the 'end' time.
	if ( GIsSupported && bIsTiming )
	{
		checkSlow( CurrentTimestamp >= 0 && CurrentTimestamp < BufferSize );

		FOpenGLRenderQuery* TimerQuery = EndTimestamps[CurrentTimestamp];
		{
			if (!TimerQuery->bInvalidResource && !PlatformContextIsCurrent(TimerQuery->ResourceContext))
			{
				PlatformReleaseRenderQuery(TimerQuery->Resource, TimerQuery->ResourceContext);
				TimerQuery->bInvalidResource = true;
			}

			if (TimerQuery->bInvalidResource && PlatformOpenGLContextValid())
			{
				PlatformGetNewRenderQuery(&TimerQuery->Resource, &TimerQuery->ResourceContext);
				TimerQuery->bInvalidResource = false;
			}
		}

		FOpenGL::QueryTimestampCounter(EndTimestamps[CurrentTimestamp]->Resource);
		NumIssuedTimestamps = FMath::Min<int32>(NumIssuedTimestamps + 1, BufferSize);
		bIsTiming = false;
	}
}

/**
 * Retrieves the most recently resolved timing measurement.
 * The unit is the same as for FPlatformTime::Cycles(). Returns 0 if there are no resolved measurements.
 *
 * @return	Value of the most recently resolved timing, or 0 if no measurements have been resolved by the GPU yet.
 */
uint64 FOpenGLBufferedGPUTiming::GetTiming(bool bGetCurrentResultsAndBlock)
{

	VERIFY_GL_SCOPE();

	if ( GIsSupported )
	{
		checkSlow( CurrentTimestamp >= 0 && CurrentTimestamp < BufferSize );
		GLuint64 StartTime, EndTime;

		int32 TimestampIndex = CurrentTimestamp;

		{
			FOpenGLRenderQuery* EndStamp = EndTimestamps[TimestampIndex];
			if (!EndStamp->bInvalidResource && !PlatformContextIsCurrent(EndStamp->ResourceContext))
			{
				PlatformReleaseRenderQuery(EndStamp->Resource, EndStamp->ResourceContext);
				EndStamp->bInvalidResource = true;
			}

			FOpenGLRenderQuery* StartStamp = StartTimestamps[TimestampIndex];
			if (!StartStamp->bInvalidResource && !PlatformContextIsCurrent(StartStamp->ResourceContext))
			{
				PlatformReleaseRenderQuery(StartStamp->Resource, StartStamp->ResourceContext);
				StartStamp->bInvalidResource = true;

			}

			if(StartStamp->bInvalidResource || EndStamp->bInvalidResource)
			{
				UE_LOG(LogRHI, Log, TEXT("timing invalid, since the stamp queries have invalid resources"));
				return 0.0f;
			}
		}

		if (!bGetCurrentResultsAndBlock)
		{
			// Quickly check the most recent measurements to see if any of them has been resolved.  Do not flush these queries.
			for ( int32 IssueIndex = 1; IssueIndex < NumIssuedTimestamps; ++IssueIndex )
			{
				GLuint EndAvailable = GL_FALSE;
				FOpenGL::GetQueryObject(EndTimestamps[TimestampIndex]->Resource, FOpenGL::QM_ResultAvailable, &EndAvailable);

				if ( EndAvailable == GL_TRUE )
				{
					GLuint StartAvailable = GL_FALSE;
					FOpenGL::GetQueryObject(StartTimestamps[TimestampIndex]->Resource, FOpenGL::QM_ResultAvailable, &StartAvailable);

					if(StartAvailable == GL_TRUE)
					{
						FOpenGL::GetQueryObject(EndTimestamps[TimestampIndex]->Resource, FOpenGL::QM_Result, &EndTime);
						FOpenGL::GetQueryObject(StartTimestamps[TimestampIndex]->Resource, FOpenGL::QM_Result, &StartTime);
						if (EndTime > StartTime)
						{
							return EndTime - StartTime;
						}
					}
				}

				TimestampIndex = (TimestampIndex + BufferSize - 1) % BufferSize;
			}
		}

		if ( NumIssuedTimestamps > 0 || bGetCurrentResultsAndBlock )
		{
			// None of the (NumIssuedTimestamps - 1) measurements were ready yet,
			// so check the oldest measurement more thoroughly.
			// This really only happens if occlusion and frame sync event queries are disabled, otherwise those will block until the GPU catches up to 1 frame behind
			const bool bBlocking = ( NumIssuedTimestamps == BufferSize ) || bGetCurrentResultsAndBlock;

			GLuint EndAvailable = GL_FALSE;
			{
				FRenderThreadIdleScope IdleScope(ERenderThreadIdleTypes::WaitingForGPUQuery);
				double StartTimeoutTime = FPlatformTime::Seconds();

				SCOPE_CYCLE_COUNTER( STAT_RenderQueryResultTime );
				// If we are blocking, retry until the GPU processes the time stamp command
				do
				{
					FOpenGL::GetQueryObject(EndTimestamps[TimestampIndex]->Resource, FOpenGL::QM_ResultAvailable, &EndAvailable);

					if ((FPlatformTime::Seconds() - StartTimeoutTime) > 0.5)
					{
						UE_LOG(LogRHI, Log, TEXT("Timed out while waiting for GPU to catch up. (500 ms) EndTimeStamp"));
						return 0;
					}
				} while ( EndAvailable == GL_FALSE && bBlocking );
			}

			if ( EndAvailable == GL_TRUE )
			{
				GLuint StartAvailable = GL_FALSE;
				{
					FRenderThreadIdleScope IdleScope(ERenderThreadIdleTypes::WaitingForGPUQuery);
					double StartTimeoutTime = FPlatformTime::Seconds();

					do
					{
						FOpenGL::GetQueryObject(StartTimestamps[TimestampIndex]->Resource, FOpenGL::QM_ResultAvailable, &StartAvailable);

						if ((FPlatformTime::Seconds() - StartTimeoutTime) > 0.5)
						{
							UE_LOG(LogRHI, Log, TEXT("Timed out while waiting for GPU to catch up. (500 ms) StartTimeStamp"));
							return 0;
						}
					} while ( StartAvailable == GL_FALSE && bBlocking );
				}

				if(StartAvailable == GL_TRUE)
				{
					FOpenGL::GetQueryObject(EndTimestamps[TimestampIndex]->Resource, FOpenGL::QM_Result, &EndTime);
					FOpenGL::GetQueryObject(StartTimestamps[TimestampIndex]->Resource, FOpenGL::QM_Result, &StartTime);
					if (EndTime > StartTime)
					{
						return EndTime - StartTime;
					}
				}
			}
		}
	}
	return 0;
}

FOpenGLDisjointTimeStampQuery::FOpenGLDisjointTimeStampQuery(class FOpenGLDynamicRHI* InOpenGLRHI)
:	bIsResultValid(false)
,	DisjointQuery(0)
,	Context(0)
,	OpenGLRHI(InOpenGLRHI)
{
}

void FOpenGLDisjointTimeStampQuery::StartTracking()
{
	VERIFY_GL_SCOPE();
	if (IsSupported())
	{

		if (!PlatformContextIsCurrent(Context))
		{
			PlatformReleaseRenderQuery(DisjointQuery, Context);
			PlatformGetNewRenderQuery(&DisjointQuery, &Context);
		}
		// Dummy query to reset the driver's internal disjoint status
		FOpenGL::TimerQueryDisjoint();
		FOpenGL::BeginQuery(UGL_TIME_ELAPSED, DisjointQuery);
	}
}

void FOpenGLDisjointTimeStampQuery::EndTracking()
{
	VERIFY_GL_SCOPE();

	if(IsSupported())
	{
		FOpenGL::EndQuery( UGL_TIME_ELAPSED );

		// Check if the GPU changed clock frequency since the last time GL_GPU_DISJOINT_EXT was checked.
		// If so, any timer query will be undefined.
		bIsResultValid = !FOpenGL::TimerQueryDisjoint();
	}

}

bool FOpenGLDisjointTimeStampQuery::IsResultValid()
{
	checkSlow(IsSupported());
	return bIsResultValid;
}

bool FOpenGLDisjointTimeStampQuery::GetResult( uint64* OutResult/*=NULL*/ )
{
	VERIFY_GL_SCOPE();

	if (IsSupported())
	{
		GLuint Result = 0;
		FOpenGL::GetQueryObject(DisjointQuery, FOpenGL::QM_ResultAvailable, &Result);
		const double StartTime = FPlatformTime::Seconds();

		while (Result == GL_FALSE && (FPlatformTime::Seconds() - StartTime) < 0.5)
		{
			FPlatformProcess::Sleep(0.005f);
			FOpenGL::GetQueryObject(DisjointQuery, FOpenGL::QM_ResultAvailable, &Result);
		}

		// Presently just discarding the result, because timing is handled by timestamps inside
		if (Result != GL_FALSE)
		{
			GLuint64 ElapsedTime = 0;
			FOpenGL::GetQueryObject(DisjointQuery, FOpenGL::QM_Result, &ElapsedTime);
			if (OutResult)
			{
				*OutResult = ElapsedTime;
			}
		}
		bIsResultValid = Result != GL_FALSE;
	}
	return bIsResultValid;
}

void FOpenGLDisjointTimeStampQuery::InitResources()
{
	if (IsSupported())
	{
		FRHICommandListExecutor::GetImmediateCommandList().EnqueueLambda([this](FRHICommandListImmediate&) { 
				PlatformGetNewRenderQuery(&DisjointQuery, &Context); 
		});
	}
}

void FOpenGLDisjointTimeStampQuery::ReleaseResources()
{
	VERIFY_GL_SCOPE();
	if ( IsSupported() )
	{
		PlatformReleaseRenderQuery(DisjointQuery, Context);
	}
}

// Fence implementation
struct FOpenGLGPUFenceProxy
{
	FOpenGLGPUFenceProxy()
		: Fence(0)
		, bIsSignaled(false)
	{
	}

	~FOpenGLGPUFenceProxy()
	{
		AllOpenGLGPUFences.RemoveSwap(this);

		if (Fence != 0)
		{
			FOpenGL::DeleteSync(Fence);
		}
	}

	void Write()
	{
		checkf(Fence == 0, TEXT("Fence must be cleared before re-using it."))
		
		if (Fence != 0)
		{
			FOpenGL::DeleteSync(Fence);
		}
		else
		{
			AllOpenGLGPUFences.Add(this);
		}

		Fence = FOpenGL::FenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		bIsSignaled = false;
	}
	
	void Poll()
	{
		if (Fence != 0 && !bIsSignaled)
		{
			FOpenGLBase::EFenceResult Result = (FOpenGL::ClientWaitSync(Fence, 0, 0));
			bIsSignaled = (Result == FOpenGLBase::FR_AlreadySignaled || Result == FOpenGLBase::FR_ConditionSatisfied);
		}
	}

	UGLsync Fence;
	bool bIsSignaled;

	//
	static TArray<FOpenGLGPUFenceProxy*> AllOpenGLGPUFences;
	
	static void PollAllFences()
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FOpenGLGPUFence_PollAllFences);

		for (int32 FenceIdx = 0; FenceIdx < AllOpenGLGPUFences.Num(); ++FenceIdx)
		{
			AllOpenGLGPUFences[FenceIdx]->Poll();
		}
	}
};

TArray<FOpenGLGPUFenceProxy*> FOpenGLGPUFenceProxy::AllOpenGLGPUFences;

void OpenGL_PollAllFences()
{
	if (IsRunningRHIInSeparateThread())
	{
		RunOnGLRenderContextThread([]()
		{
			VERIFY_GL_SCOPE();
			FOpenGLGPUFenceProxy::PollAllFences();
		});
	}
}

FGPUFenceRHIRef FOpenGLDynamicRHI::RHICreateGPUFence(const FName &Name)
{
	return new FOpenGLGPUFence(Name);
}

FOpenGLGPUFence::FOpenGLGPUFence(FName InName)
	: FRHIGPUFence(InName)
{
	Proxy = new FOpenGLGPUFenceProxy();
}

FOpenGLGPUFence::~FOpenGLGPUFence()
{
	RunOnGLRenderContextThread([Proxy = Proxy]()
	{
		VERIFY_GL_SCOPE();
		delete Proxy;
	});
}

void FOpenGLGPUFence::Clear()
{
	RunOnGLRenderContextThread([Proxy = Proxy]()
	{
		VERIFY_GL_SCOPE();
		delete Proxy;
	});

	Proxy = new FOpenGLGPUFenceProxy();
}

bool FOpenGLGPUFence::Poll() const
{
	if (Proxy->bIsSignaled)
	{
		return true;
	}

	if (!(IsInRenderingThread() || IsInRHIThread()))
	{
		ENQUEUE_RENDER_COMMAND(FOpenGLGPUFence_Poll)(
		[this](FRHICommandListImmediate& RHICmdList)
		{
			Poll();
		});
	}
	else
	{
		RunOnGLRenderContextThread([Proxy = Proxy]()
		{
			VERIFY_GL_SCOPE();
			check(Proxy != nullptr);
			Proxy->Poll();
		});
	}

	return Proxy->bIsSignaled;
}

void FOpenGLGPUFence::WriteInternal()
{
	VERIFY_GL_SCOPE();
	Proxy->Write();
}
