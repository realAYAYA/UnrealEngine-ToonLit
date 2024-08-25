// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D11Query.cpp: D3D query RHI implementation.
=============================================================================*/

#include "D3D11RHIPrivate.h"
#include "RenderCore.h"

float GD3D11AbsoluteTimeQueryTimeoutValue = 30.0f;
static FAutoConsoleVariableRef CVarD3D11AbsoluteTimeQueryTimeoutValue(
	TEXT("r.D3D11.AbsoluteTimeQueryTimeoutValue"),
	GD3D11AbsoluteTimeQueryTimeoutValue,
	TEXT("Set the timeout value, in seconds, to wait for a D3D11 absolute time query."),
	ECVF_Default
);

float GD3D11QueryTimeoutValue = 5.0f;
static FAutoConsoleVariableRef CVarD3D11QueryTimeoutValue(
	TEXT("r.D3D11.QueryTimeoutValue"),
	GD3D11QueryTimeoutValue,
	TEXT("Set the timeout value, in seconds, to wait for a D3D11 query. This value does not apply to absolute time queries (which are controlled by r.D3D11.AbsoluteTimeQueryTimeoutValue)."),
	ECVF_Default
);

class FD3D11RenderQueryBatcher final
{
public:
	static FD3D11RenderQueryBatcher& Get()
	{
		static FD3D11RenderQueryBatcher Singleton;
		return Singleton;
	}

	inline void StartNewBatch()
	{
		FRenderQueryBatch& NewBatch = ActiveBatches[ActiveBatches.AddDefaulted()];
		NewBatch.Id = CurrentId;
	}

	void PerFrameFlush()
	{
		for (int32 BatchIdx = 0; BatchIdx < ActiveBatches.Num(); ++BatchIdx)
		{
			FRenderQueryBatch& Batch = ActiveBatches[BatchIdx];

			// This is correct even if CurrentId overflowed
			if (CurrentId - Batch.Id >= NumBufferedFrames)
			{
				TArray<FRenderQueryRHIRef>& Queries = ActiveBatches[BatchIdx].Queries;
				for (int32 QueryIdx = 0; QueryIdx < Queries.Num(); ++QueryIdx)
				{
					FD3D11RenderQuery* Query = FD3D11DynamicRHI::ResourceCast(Queries[QueryIdx].GetReference());

					if (Query->bResultIsCached || Query->GetRefCount() == 1)
					{
						continue;
					}

					if (Query->QueryType == ERenderQueryType::RQT_AbsoluteTime)
					{
						check(GD3D11RHI->GetQueryData(Query->Resource, &Query->Result, sizeof(Query->Result), Query->QueryType, true, false));
						Query->bResultIsCached = true;
					}
				}
				ActiveBatches.RemoveAtSwap(BatchIdx--);
			}
		}

		++CurrentId;
	}

	inline void Release()
	{
		ActiveBatches.Empty();
	}

	inline void Add(FRenderQueryRHIRef NewQuery)
	{
		const uint32 Id = CurrentId;
		FRenderQueryBatch* CurrentBatch = ActiveBatches.FindByPredicate([Id](const FRenderQueryBatch& Batch){ return Batch.Id == Id; });

		if (CurrentBatch == nullptr)
		{
			StartNewBatch();
			CurrentBatch = &ActiveBatches.Last();
		}

		CurrentBatch->Queries.Add(NewQuery);
	}

	void PollQueryResults()
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_PollQueryResults);
		for (int32 BatchIdx = 0; BatchIdx < ActiveBatches.Num(); ++BatchIdx)
		{
			TArray<FRenderQueryRHIRef>& Queries = ActiveBatches[BatchIdx].Queries;

			for (int32 QueryIdx = 0; QueryIdx < Queries.Num(); ++QueryIdx)
			{
				FD3D11RenderQuery* Query = FD3D11DynamicRHI::ResourceCast(Queries[QueryIdx].GetReference());

				if (Query->bResultIsCached || Query->GetRefCount() == 1)
				{
					Queries.RemoveAtSwap(QueryIdx--);
				}
				else if (GD3D11RHI->GetQueryData(Query->Resource, &Query->Result, sizeof(Query->Result), Query->QueryType, false, false))
				{
					Query->bResultIsCached = true;
					Queries.RemoveAtSwap(QueryIdx--);
				}
			}

			if (!Queries.Num())
			{
				ActiveBatches.RemoveAtSwap(BatchIdx--);
			}
		}
	}

	void PollQueryResults_RenderThread()
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_PollQueryResults_RenderThread);
		FScopedRHIThreadStaller RHITStaller(FRHICommandListExecutor::GetImmediateCommandList());
		PollQueryResults();
	}

	inline uint32 GetCurrentId() const
	{
		return CurrentId;
	}

	FD3D11RenderQueryBatcher(const FD3D11RenderQueryBatcher&) = delete;
	FD3D11RenderQueryBatcher(FD3D11RenderQueryBatcher&&) = delete;
	FD3D11RenderQueryBatcher& operator=(const FD3D11RenderQueryBatcher&) = delete;
	FD3D11RenderQueryBatcher& operator=(FD3D11RenderQueryBatcher&&) = delete;

private:
	struct FRenderQueryBatch
	{
		uint32 Id;
		TArray<FRenderQueryRHIRef> Queries;
	};

	static const int32 NumBufferedFrames = 5;

	uint32 CurrentId;
	TArray<FRenderQueryBatch> ActiveBatches;

	FD3D11RenderQueryBatcher()
		: CurrentId(0)
	{}

	~FD3D11RenderQueryBatcher() = default;
};

void D3D11RHIQueryBatcherPerFrameCleanup()
{
	FD3D11RenderQueryBatcher::Get().PerFrameFlush();
}

void FD3D11DynamicRHI::ReleaseCachedQueries()
{
	FD3D11RenderQueryBatcher::Get().Release();
}

void FD3D11DynamicRHI::RHIPollRenderQueryResults()
{
	if (ShouldNotEnqueueRHICommand())
	{
		FD3D11RenderQueryBatcher::Get().PollQueryResults();
	}
	else
	{
		RunOnRHIThread(
			[]()
		{
			FD3D11RenderQueryBatcher::Get().PollQueryResults();
		});
	}
}

void FD3D11DynamicRHI::RHIBeginOcclusionQueryBatch(uint32 NumQueriesInBatch)
{
	check(RequestedOcclusionQueriesInBatch == 0);
	ActualOcclusionQueriesInBatch = 0;
	RequestedOcclusionQueriesInBatch = NumQueriesInBatch;
	FD3D11RenderQueryBatcher::Get().StartNewBatch();
}

void FD3D11DynamicRHI::RHIEndOcclusionQueryBatch()
{
	checkf(ActualOcclusionQueriesInBatch <= RequestedOcclusionQueriesInBatch, TEXT("Expected %d total occlusion queries per RHIBeginOcclusionQueryBatch(); got %d total; this will break newer APIs"), RequestedOcclusionQueriesInBatch, ActualOcclusionQueriesInBatch);
	RequestedOcclusionQueriesInBatch = 0;
}

FRenderQueryRHIRef FD3D11DynamicRHI::RHICreateRenderQuery(ERenderQueryType QueryType)
{
	TRefCountPtr<ID3D11Query> Query;
	D3D11_QUERY_DESC Desc;

	if(QueryType == RQT_Occlusion)
	{
		Desc.Query = D3D11_QUERY_OCCLUSION;
	}
	else if(QueryType == RQT_AbsoluteTime)
	{
		Desc.Query = D3D11_QUERY_TIMESTAMP;
	}
	else
	{
		check(0);
	}

	Desc.MiscFlags = 0;
	VERIFYD3D11RESULT_EX(Direct3DDevice->CreateQuery(&Desc,Query.GetInitReference()), Direct3DDevice);
	return new FD3D11RenderQuery(Query, QueryType);
}

bool FD3D11DynamicRHI::RHIGetRenderQueryResult(FRHIRenderQuery* QueryRHI, uint64& OutResult, bool bWait, uint32 GPUIndex)
{
	check(IsInRenderingThread());
	FD3D11RenderQuery* Query = ResourceCast(QueryRHI);
	bool bSuccess = true;
	if(Query->QueryType == RQT_AbsoluteTime && !bWait)
	{
		if(!Query->bResultIsCached)
		{
			return false;
		}
	}
	else
	{
		static uint32 LastQueryBatchId = 0xffffffff;
		uint32 CurrentQueryBatchId = FD3D11RenderQueryBatcher::Get().GetCurrentId();
		if (LastQueryBatchId != CurrentQueryBatchId && Query->QueryType == RQT_Occlusion)
		{
			LastQueryBatchId = CurrentQueryBatchId;
			FD3D11RenderQueryBatcher::Get().PollQueryResults_RenderThread();
		}

		if(!Query->bResultIsCached)
		{
			bSuccess = GetQueryData(Query->Resource,&Query->Result,sizeof(Query->Result),Query->QueryType,bWait,true);

			Query->bResultIsCached = bSuccess;
		}
	}

	if(Query->QueryType == RQT_AbsoluteTime)
	{
		// GetTimingFrequency is the number of ticks per second
		uint64 Div = FMath::Max(1llu, FGPUTiming::GetTimingFrequency() / (1000 * 1000));

		// convert from GPU specific timestamp to micro sec (1 / 1 000 000 s) which seems a reasonable resolution
		OutResult = Query->Result / Div;
	}
	else
	{
		OutResult = Query->Result;
	}
	return bSuccess;
}


// Occlusion/Timer queries.
void FD3D11DynamicRHI::RHIBeginRenderQuery(FRHIRenderQuery* QueryRHI)
{
	FD3D11RenderQuery* Query = ResourceCast(QueryRHI);

	if(Query->QueryType == RQT_Occlusion)
	{
		++ActualOcclusionQueriesInBatch;
		Query->bResultIsCached = false;
		Direct3DDeviceIMContext->Begin(Query->Resource.GetReference());
		FD3D11RenderQueryBatcher::Get().Add(QueryRHI);
	}
	else
	{
		// not supported/needed for RQT_AbsoluteTime
		check(0);
	}
}

void FD3D11DynamicRHI::RHIEndRenderQuery(FRHIRenderQuery* QueryRHI)
{
	FD3D11RenderQuery* Query = ResourceCast(QueryRHI);
	Query->bResultIsCached = false; // for occlusion queries, this is redundant with the one in begin
	if(Query->QueryType == RQT_AbsoluteTime)
	{
		FD3D11RenderQueryBatcher::Get().Add(QueryRHI);
	}
	Direct3DDeviceIMContext->End(Query->Resource);

	//@todo - d3d debug spews warnings about OQ's that are being issued but not polled, need to investigate
}

bool FD3D11DynamicRHI::GetQueryData(ID3D11Query* Query, void* Data, SIZE_T DataSize, ERenderQueryType QueryType, bool bWait, bool bStallRHIThread)
{
#define SAFE_GET_QUERY_DATA \
	{ \
		FScopedD3D11RHIThreadStaller StallRHIThread(bStallRHIThread); \
		Result = Direct3DDeviceIMContext->GetData(Query, Data, DataSize, 0); \
	}

	// Request the data from the query.
	HRESULT Result;
	SAFE_GET_QUERY_DATA

	// Isn't the query finished yet, and can we wait for it?
	if ( Result == S_FALSE && bWait )
	{
		SCOPE_CYCLE_COUNTER( STAT_RenderQueryResultTime );
		FRenderThreadIdleScope IdleScope(ERenderThreadIdleTypes::WaitingForGPUQuery);

		double StartTime = FPlatformTime::Seconds();
		double TimeoutWarningLimit = 5.0;
		// timer queries are used for Benchmarks which can stall a bit more
		double TimeoutValue = (QueryType == RQT_AbsoluteTime) ? GD3D11AbsoluteTimeQueryTimeoutValue : GD3D11QueryTimeoutValue;

		do
		{
			SAFE_GET_QUERY_DATA

			if(Result == S_OK)
			{
				return true;
			}

			float DeltaTime = FPlatformTime::Seconds() - StartTime;
			if(DeltaTime > TimeoutWarningLimit)
			{
				HRESULT DeviceRemovedReason = Direct3DDevice->GetDeviceRemovedReason();
				TimeoutWarningLimit += 5.0;
				UE_LOG(LogD3D11RHI, Log, TEXT("GetQueryData is taking a very long time (%.1f s) (%08x)"), DeltaTime, (uint32)DeviceRemovedReason);
			}

			if(DeltaTime > TimeoutValue)
			{
				UE_LOG(LogD3D11RHI, Log, TEXT("Timed out while waiting for GPU query. (Timeout %.1f s) (ErrorCode %08x)"), TimeoutValue, (uint32)Result);

				// Check for GPU status and crash.
				GPUProfilingData.CheckGpuHeartbeat(true);
				VERIFYD3D11RESULT_EX(Result, Direct3DDevice);
				return false;
			}
		} while ( Result == S_FALSE );
	}

	if( Result == S_OK )
	{
		return true;
	}
	else if(Result == S_FALSE && !bWait)
	{
		// Return failure if the query isn't complete, and waiting wasn't requested.
		return false;
	}
	else
	{
		VERIFYD3D11RESULT_EX(Result, Direct3DDevice);
		return false;
	}
#undef SAFE_GET_QUERY_DATA
}

void FD3D11EventQuery::IssueEvent()
{
	if (ShouldNotEnqueueRHICommand())
	{
		D3DRHI->GetDeviceContext()->End(Query);
	}
	else
	{
		RunOnRHIThread(
			[InQuery = Query]()
		{
			D3D11RHI_IMMEDIATE_CONTEXT->End(InQuery);
		});
	}
}

void FD3D11EventQuery::WaitForCompletion()
{
	BOOL bRenderingIsFinished = false;
	while(
		D3DRHI->GetQueryData(Query,&bRenderingIsFinished,sizeof(bRenderingIsFinished),RQT_Undefined,true,true) &&
		!bRenderingIsFinished
		)
	{};
}

FD3D11EventQuery::FD3D11EventQuery(class FD3D11DynamicRHI* InD3DRHI):
	D3DRHI(InD3DRHI)
{
	D3D11_QUERY_DESC QueryDesc;
	QueryDesc.Query = D3D11_QUERY_EVENT;
	QueryDesc.MiscFlags = 0;
	VERIFYD3D11RESULT_EX(D3DRHI->GetDevice()->CreateQuery(&QueryDesc,Query.GetInitReference()), D3DRHI->GetDevice());
}

/*=============================================================================
 * class FD3D11BufferedGPUTiming
 *=============================================================================*/

/**
 * Constructor.
 *
 * @param InD3DRHI			RHI interface
 * @param InBufferSize		Number of buffered measurements
 */
FD3D11BufferedGPUTiming::FD3D11BufferedGPUTiming( FD3D11DynamicRHI* InD3DRHI, int32 InBufferSize )
:	D3DRHI( InD3DRHI )
,	BufferSize( InBufferSize )
,	CurrentTimestamp( -1 )
,	NumIssuedTimestamps( 0 )
,	StartTimestamps( NULL )
,	EndTimestamps( NULL )
,	bIsTiming( false )
{
}

/**
 * Initializes the static variables, if necessary.
 */
void FD3D11BufferedGPUTiming::PlatformStaticInitialize(void* UserData)
{
	// Are the static variables initialized?
	check( !GAreGlobalsInitialized );

	// Get the GPU timestamp frequency.
	SetTimingFrequency(0);
	TRefCountPtr<ID3D11Query> FreqQuery;
	FD3D11DynamicRHI* D3DRHI = (FD3D11DynamicRHI*)UserData;
	ID3D11DeviceContext *D3D11DeviceContext = D3DRHI->GetDeviceContext();
	HRESULT D3DResult;

	D3D11_QUERY_DESC QueryDesc;
	QueryDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
	QueryDesc.MiscFlags = 0;

	{
		// to track down some rare event where GTimingFrequency is 0 or <1000*1000
		uint32 DebugState = 0;
		uint32 DebugCounter = 0;

		D3DResult = D3DRHI->GetDevice()->CreateQuery(&QueryDesc, FreqQuery.GetInitReference());
		if (D3DResult == S_OK)
		{
			DebugState = 1;
			D3D11DeviceContext->Begin(FreqQuery);
			D3D11DeviceContext->End(FreqQuery);

			D3D11_QUERY_DATA_TIMESTAMP_DISJOINT FreqQueryData;

			{
				FScopedD3D11RHIThreadStaller StallRHIThread;
				D3DResult = D3D11DeviceContext->GetData(FreqQuery, &FreqQueryData, sizeof(D3D11_QUERY_DATA_TIMESTAMP_DISJOINT), 0);
			}

			double StartTime = FPlatformTime::Seconds();
			while (D3DResult == S_FALSE && (FPlatformTime::Seconds() - StartTime) < 0.5f)
			{
				++DebugCounter;
				FPlatformProcess::Sleep(0.005f);

				FScopedD3D11RHIThreadStaller StallRHIThread;
				D3DResult = D3D11DeviceContext->GetData(FreqQuery, &FreqQueryData, sizeof(D3D11_QUERY_DATA_TIMESTAMP_DISJOINT), 0);
			}

			if (D3DResult == S_OK)
			{
				DebugState = 2;
				SetTimingFrequency(FreqQueryData.Frequency);
				checkSlow(!FreqQueryData.Disjoint);

				if (FreqQueryData.Disjoint)
				{
					DebugState = 3;
				}
			}
		}

		UE_LOG(LogD3D11RHI, Log, TEXT("GPU Timing Frequency: %f (Debug: %d %d)"), GetTimingFrequency() / (double)(1000 * 1000), DebugState, DebugCounter);
	}

	FreqQuery = NULL;
}

void FD3D11BufferedGPUTiming::CalibrateTimers(FD3D11DynamicRHI* InD3DRHI)
{
	// Attempt to generate a timestamp on GPU and CPU as closely to each other as possible.
	// This works by first flushing any pending GPU work, then writing a GPU timestamp and waiting for GPU to finish.
	// CPU timestamp is continuously captured while we are waiting on GPU.

	HRESULT D3DResult = E_FAIL;

	TRefCountPtr<ID3D11Query> DisjointQuery;
	{
		D3D11_QUERY_DESC QueryDesc;
		QueryDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
		QueryDesc.MiscFlags = 0;
		D3DResult = InD3DRHI->GetDevice()->CreateQuery(&QueryDesc, DisjointQuery.GetInitReference());

		if (D3DResult != S_OK) return;
	}

	TRefCountPtr<ID3D11Query> TimestampQuery;
	{
		D3D11_QUERY_DESC QueryDesc;
		QueryDesc.Query = D3D11_QUERY_TIMESTAMP;
		QueryDesc.MiscFlags = 0;
		D3DResult = InD3DRHI->GetDevice()->CreateQuery(&QueryDesc, TimestampQuery.GetInitReference());

		if (D3DResult != S_OK) return;
	}

	TRefCountPtr<ID3D11Query> PendingWorkDoneQuery;
	TRefCountPtr<ID3D11Query> TimestampDoneQuery;
	{
		D3D11_QUERY_DESC QueryDesc;
		QueryDesc.Query = D3D11_QUERY_EVENT;
		QueryDesc.MiscFlags = 0;

		D3DResult = InD3DRHI->GetDevice()->CreateQuery(&QueryDesc, PendingWorkDoneQuery.GetInitReference());
		if (D3DResult != S_OK) return;

		D3DResult = InD3DRHI->GetDevice()->CreateQuery(&QueryDesc, TimestampDoneQuery.GetInitReference());
		if (D3DResult != S_OK) return;
	}

	ID3D11DeviceContext* D3D11DeviceContext = InD3DRHI->GetDeviceContext();

	// Flush any currently pending GPU work and wait for it to finish
	D3D11DeviceContext->End(PendingWorkDoneQuery);
	D3D11DeviceContext->Flush();

	for(;;)
	{
		BOOL EventComplete = false;
		D3D11DeviceContext->GetData(PendingWorkDoneQuery, &EventComplete, sizeof(EventComplete), 0);
		if (EventComplete) break;
		FPlatformProcess::Sleep(0.001f);
	}

	const uint32 MaxCalibrationAttempts = 10;
	for (uint32 CalibrationAttempt = 0; CalibrationAttempt < MaxCalibrationAttempts; ++CalibrationAttempt)
	{
		D3D11DeviceContext->Begin(DisjointQuery);
		D3D11DeviceContext->End(TimestampQuery);
		D3D11DeviceContext->End(DisjointQuery);
		D3D11DeviceContext->End(TimestampDoneQuery);

		D3D11DeviceContext->Flush();

		uint64 CPUTimestamp = 0;
		uint64 GPUTimestamp = 0;

		// Busy-wait for GPU to finish and capture CPU timestamp approximately when GPU work is done
		for (;;)
		{
			BOOL EventComplete = false;
			CPUTimestamp = FPlatformTime::Cycles64();
			D3D11DeviceContext->GetData(TimestampDoneQuery, &EventComplete, sizeof(EventComplete), 0);
			if (EventComplete) break;
		}

		D3D11_QUERY_DATA_TIMESTAMP_DISJOINT DisjointQueryData = {};
		D3DResult = D3D11DeviceContext->GetData(DisjointQuery, &DisjointQueryData, sizeof(DisjointQueryData), 0);

		// If timestamp was unreliable, try again
		if (D3DResult != S_OK || DisjointQueryData.Disjoint)
		{
			continue;
		}

		D3DResult = D3D11DeviceContext->GetData(TimestampQuery, &GPUTimestamp, sizeof(GPUTimestamp), 0);

		// If we managed to get valid timestamps, save both of them (CPU & GPU) and return
		if (D3DResult == S_OK && GPUTimestamp)
		{
			FGPUTimingCalibrationTimestamp CalibrationTimestamp;
			CalibrationTimestamp.CPUMicroseconds = uint64(FPlatformTime::ToSeconds64(CPUTimestamp) * 1e6);
			CalibrationTimestamp.GPUMicroseconds = uint64(GPUTimestamp * (1e6 / GetTimingFrequency()));
			SetCalibrationTimestamp(CalibrationTimestamp);
			break;
		}
		else
		{
			continue;
		}
	}
}

void FD3D11DynamicRHI::RHICalibrateTimers()
{
	check(IsInRenderingThread());

	FScopedRHIThreadStaller StallRHIThread(FRHICommandListExecutor::GetImmediateCommandList());

	FD3D11BufferedGPUTiming::CalibrateTimers(this);
}

/**
 * Initializes all D3D resources and if necessary, the static variables.
 */
void FD3D11BufferedGPUTiming::InitRHI(FRHICommandListBase& RHICmdList)
{
	StaticInitialize(D3DRHI, PlatformStaticInitialize);

	CurrentTimestamp = 0;
	NumIssuedTimestamps = 0;
	bIsTiming = false;

	// Now initialize the queries for this timing object.
	if ( GIsSupported )
	{
		StartTimestamps = new TRefCountPtr<ID3D11Query>[ BufferSize ];
		EndTimestamps = new TRefCountPtr<ID3D11Query>[ BufferSize ];
		for ( int32 TimestampIndex = 0; TimestampIndex < BufferSize; ++TimestampIndex )
		{
			HRESULT D3DResult;

			D3D11_QUERY_DESC QueryDesc;
			QueryDesc.Query = D3D11_QUERY_TIMESTAMP;
			QueryDesc.MiscFlags = 0;

			CA_SUPPRESS(6385);	// Doesn't like COM
			D3DResult = D3DRHI->GetDevice()->CreateQuery(&QueryDesc,StartTimestamps[TimestampIndex].GetInitReference());
			GIsSupported = GIsSupported && (D3DResult == S_OK);
			CA_SUPPRESS(6385);	// Doesn't like COM
			D3DResult = D3DRHI->GetDevice()->CreateQuery(&QueryDesc,EndTimestamps[TimestampIndex].GetInitReference());
			GIsSupported = GIsSupported && (D3DResult == S_OK);
		}
	}
}

/**
 * Releases all D3D resources.
 */
void FD3D11BufferedGPUTiming::ReleaseRHI()
{
	if ( StartTimestamps && EndTimestamps )
	{
		for ( int32 TimestampIndex = 0; TimestampIndex < BufferSize; ++TimestampIndex )
		{
			StartTimestamps[TimestampIndex] = NULL;
			EndTimestamps[TimestampIndex] = NULL;
		}
		delete [] StartTimestamps;
		delete [] EndTimestamps;
		StartTimestamps = NULL;
		EndTimestamps = NULL;
	}
}

/**
 * Start a GPU timing measurement.
 */
void FD3D11BufferedGPUTiming::StartTiming()
{
	// Issue a timestamp query for the 'start' time.
	if ( GIsSupported && !bIsTiming )
	{
		int32 NewTimestampIndex = (CurrentTimestamp + 1) % BufferSize;
		D3DRHI->GetDeviceContext()->End(StartTimestamps[NewTimestampIndex]);
		CurrentTimestamp = NewTimestampIndex;
		bIsTiming = true;
	}
}

/**
 * End a GPU timing measurement.
 * The timing for this particular measurement will be resolved at a later time by the GPU.
 */
void FD3D11BufferedGPUTiming::EndTiming()
{
	// Issue a timestamp query for the 'end' time.
	if ( GIsSupported && bIsTiming )
	{
		checkSlow( CurrentTimestamp >= 0 && CurrentTimestamp < BufferSize );
		D3DRHI->GetDeviceContext()->End(EndTimestamps[CurrentTimestamp]);
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
uint64 FD3D11BufferedGPUTiming::GetTiming(bool bGetCurrentResultsAndBlock)
{
	if ( GIsSupported )
	{
		checkSlow( CurrentTimestamp >= 0 && CurrentTimestamp < BufferSize );
		uint64 StartTime, EndTime;
		HRESULT D3DResult;

		int32 TimestampIndex = CurrentTimestamp;
		if (!bGetCurrentResultsAndBlock)
		{
			// Quickly check the most recent measurements to see if any of them has been resolved.  Do not flush these queries.
			for ( int32 IssueIndex = 1; IssueIndex < NumIssuedTimestamps; ++IssueIndex )
			{
				D3DResult = D3DRHI->GetDeviceContext()->GetData(EndTimestamps[TimestampIndex],&EndTime,sizeof(EndTime),D3D11_ASYNC_GETDATA_DONOTFLUSH);
				if ( D3DResult == S_OK )
				{
					D3DResult = D3DRHI->GetDeviceContext()->GetData(StartTimestamps[TimestampIndex],&StartTime,sizeof(StartTime),D3D11_ASYNC_GETDATA_DONOTFLUSH);
					if ( D3DResult == S_OK && EndTime > StartTime)
					{
						return EndTime - StartTime;
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
			const uint32 AsyncFlags = bBlocking ? 0 : D3D11_ASYNC_GETDATA_DONOTFLUSH;
			{
				FRenderThreadIdleScope IdleScope(ERenderThreadIdleTypes::WaitingForGPUQuery);
				double StartTimeoutTime = FPlatformTime::Seconds();

				SCOPE_CYCLE_COUNTER( STAT_RenderQueryResultTime );
				// If we are blocking, retry until the GPU processes the time stamp command
				do 
				{
					D3DResult = D3DRHI->GetDeviceContext()->GetData( EndTimestamps[TimestampIndex], &EndTime, sizeof(EndTime), AsyncFlags );

					if ((FPlatformTime::Seconds() - StartTimeoutTime) > 0.5)
					{
						UE_LOG(LogD3D11RHI, Log, TEXT("Timed out while waiting for GPU to catch up. (500 ms)"));
						return 0;
					}
				} while ( D3DResult == S_FALSE && bBlocking );
			}

			if ( D3DResult == S_OK )
			{
				{
					FRenderThreadIdleScope IdleScope(ERenderThreadIdleTypes::WaitingForGPUQuery);
					double StartTimeoutTime = FPlatformTime::Seconds();
					do 
					{
						D3DResult = D3DRHI->GetDeviceContext()->GetData( StartTimestamps[TimestampIndex], &StartTime, sizeof(StartTime), AsyncFlags );

						if ((FPlatformTime::Seconds() - StartTimeoutTime) > 0.5)
						{
							UE_LOG(LogD3D11RHI, Log, TEXT("Timed out while waiting for GPU to catch up. (500 ms)"));
							return 0;
						}
					} while ( D3DResult == S_FALSE && bBlocking );
				}

				if ( D3DResult == S_OK && EndTime > StartTime )
				{
					return EndTime - StartTime;
				}
			}
		}
	}
	return 0;
}

FD3D11DisjointTimeStampQuery::FD3D11DisjointTimeStampQuery(class FD3D11DynamicRHI* InD3DRHI) :
	D3DRHI(InD3DRHI)
{

}

void FD3D11DisjointTimeStampQuery::StartTracking()
{
	ID3D11DeviceContext* D3D11DeviceContext = D3DRHI->GetDeviceContext();
	D3D11DeviceContext->Begin(DisjointQuery);
}

void FD3D11DisjointTimeStampQuery::EndTracking()
{
	ID3D11DeviceContext* D3D11DeviceContext = D3DRHI->GetDeviceContext();
	D3D11DeviceContext->End(DisjointQuery);
}

bool FD3D11DisjointTimeStampQuery::IsResultValid()
{
	return GetResult().Disjoint == 0;
}

D3D11_QUERY_DATA_TIMESTAMP_DISJOINT FD3D11DisjointTimeStampQuery::GetResult()
{
	D3D11_QUERY_DATA_TIMESTAMP_DISJOINT DisjointQueryData;

	ID3D11DeviceContext* D3D11DeviceContext = D3DRHI->GetDeviceContext();
	HRESULT D3DResult = D3D11DeviceContext->GetData(DisjointQuery, &DisjointQueryData, sizeof(D3D11_QUERY_DATA_TIMESTAMP_DISJOINT), 0);

	const double StartTime = FPlatformTime::Seconds();
	while (D3DResult == S_FALSE && (FPlatformTime::Seconds() - StartTime) < 0.5)
	{
		FPlatformProcess::Sleep(0.005f);
		D3DResult = D3D11DeviceContext->GetData(DisjointQuery, &DisjointQueryData, sizeof(D3D11_QUERY_DATA_TIMESTAMP_DISJOINT), 0);
	}

	return DisjointQueryData;
}

void FD3D11DisjointTimeStampQuery::InitRHI(FRHICommandListBase& RHICmdList)
{
	D3D11_QUERY_DESC QueryDesc;
	QueryDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
	QueryDesc.MiscFlags = 0;

	VERIFYD3D11RESULT_EX(D3DRHI->GetDevice()->CreateQuery(&QueryDesc, DisjointQuery.GetInitReference()), D3DRHI->GetDevice());
}

void FD3D11DisjointTimeStampQuery::ReleaseRHI()
{

}
