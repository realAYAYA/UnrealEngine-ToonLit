// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanQuery.cpp: Vulkan query RHI implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanResources.h"
#include "VulkanContext.h"
#include "VulkanCommandBuffer.h"
#include "EngineGlobals.h"
#include "RenderCore.h"

#if VULKAN_QUERY_CALLSTACK
#include "HAL/PlatformStackwalk.h"
#endif

static uint32 GTimestampQueryStage = 0;
TAutoConsoleVariable<int32> GTimestampQueryStageCVar(
	TEXT("r.Vulkan.TimestampQueryStage"),
	GTimestampQueryStage,
	TEXT("Defines which pipeline stage is used for timestamp queries.\n")
	TEXT(" 0: Use VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, less precise measures but less likely to alter performance (default)\n")
	TEXT(" 1: Use VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, more precise measures but may alter performance on some platforms"),
	ECVF_RenderThreadSafe
);

constexpr uint32 GMinNumberOfQueriesInPool = 256;

#if PLATFORM_ANDROID
constexpr int32 NUM_FRAMES_TO_WAIT_REUSE_POOL = 5;
// numbers of frame to wait before releasing QueryPool
constexpr uint32 NUM_FRAMES_TO_WAIT_RELEASE_POOL = 10; 
#else
constexpr int32 NUM_FRAMES_TO_WAIT_REUSE_POOL = 10;
constexpr uint32 NUM_FRAMES_TO_WAIT_RELEASE_POOL = MAX_uint32; // never release
#endif

FVulkanQueryPool::FVulkanQueryPool(FVulkanDevice* InDevice, FVulkanCommandBufferManager* CommandBufferManager, uint32 InMaxQueries, VkQueryType InQueryType, bool bInShouldAddReset)
	: VulkanRHI::FDeviceChild(InDevice)
	, QueryPool(VK_NULL_HANDLE)
	, ResetEvent(VK_NULL_HANDLE)
	, MaxQueries(InMaxQueries)
	, QueryType(InQueryType)
{
	INC_DWORD_STAT(STAT_VulkanNumQueryPools);
	VkQueryPoolCreateInfo PoolCreateInfo;
	ZeroVulkanStruct(PoolCreateInfo, VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO);
	PoolCreateInfo.queryType = QueryType;
	PoolCreateInfo.queryCount = MaxQueries;

	VERIFYVULKANRESULT(VulkanRHI::vkCreateQueryPool(Device->GetInstanceHandle(), &PoolCreateInfo, VULKAN_CPU_ALLOCATOR, &QueryPool));
	
	// If host query resets are supported, reset all new query pools on creation
	if (Device->GetOptionalExtensions().HasEXTHostQueryReset)
	{
		VulkanRHI::vkResetQueryPoolEXT(Device->GetInstanceHandle(), QueryPool, 0, MaxQueries);
	}
	else if (bInShouldAddReset && CommandBufferManager)
	{
		CommandBufferManager->AddQueryPoolForReset(QueryPool, InMaxQueries);
	}

	VkEventCreateInfo EventCreateInfo;
	ZeroVulkanStruct(EventCreateInfo, VK_STRUCTURE_TYPE_EVENT_CREATE_INFO);
	VERIFYVULKANRESULT(VulkanRHI::vkCreateEvent(Device->GetInstanceHandle(), &EventCreateInfo, VULKAN_CPU_ALLOCATOR, &ResetEvent));
	
	QueryOutput.AddZeroed(MaxQueries);
}

FVulkanQueryPool::~FVulkanQueryPool()
{
	DEC_DWORD_STAT(STAT_VulkanNumQueryPools);
	VulkanRHI::vkDestroyQueryPool(Device->GetInstanceHandle(), QueryPool, VULKAN_CPU_ALLOCATOR);
	VulkanRHI::vkDestroyEvent(Device->GetInstanceHandle(), ResetEvent, VULKAN_CPU_ALLOCATOR);
	
	QueryPool = VK_NULL_HANDLE;
	ResetEvent = VK_NULL_HANDLE;
}


bool FVulkanOcclusionQueryPool::CanBeReused()
{
	const uint64 NumWords = NumUsedQueries / 64;
	for (int32 Index = 0; Index < NumWords; ++Index)
	{
		if (AcquiredIndices[Index])
		{
			return false;
		}
	}

	const uint64 Remaining = NumUsedQueries % 64;
	const uint64 Mask = ((uint64)1 << (uint64)Remaining) - 1;
	return Mask == 0 || (AcquiredIndices[NumWords] & Mask) == 0;
}

bool FVulkanOcclusionQueryPool::InternalTryGetResults(bool bWait)
{
	check(CmdBuffer);
	check(State == EState::RHIT_PostEndBatch);

	VkResult Result = VK_NOT_READY;
	if (VulkanRHI::vkGetEventStatus(Device->GetInstanceHandle(), ResetEvent) == VK_EVENT_SET)
	{
		Result = VulkanRHI::vkGetQueryPoolResults(Device->GetInstanceHandle(), QueryPool, 0, NumUsedQueries, NumUsedQueries * sizeof(uint64), QueryOutput.GetData(), sizeof(uint64), VK_QUERY_RESULT_64_BIT);
		if (Result == VK_SUCCESS)
		{
			State = EState::RT_PostGetResults;
			return true;
		}
	}

	if (Result == VK_NOT_READY)
	{
		if (bWait)
		{
			FRenderThreadIdleScope IdleScope(ERenderThreadIdleTypes::WaitingForGPUQuery);
			SCOPE_CYCLE_COUNTER(STAT_VulkanWaitQuery);

			// We'll do manual wait
			double StartTime = FPlatformTime::Seconds();

			ENamedThreads::Type RenderThread_Local = ENamedThreads::GetRenderThread_Local();
			bool bSuccess = false;
			int32 NumLoops = 0;
			while (!bSuccess)
			{
				FPlatformProcess::SleepNoStats(0);

				// pump RHIThread to make sure these queries have actually been submitted to the GPU.
				if (IsInActualRenderingThread())
				{
					FTaskGraphInterface::Get().ProcessThreadUntilIdle(RenderThread_Local);
				}

				if (VulkanRHI::vkGetEventStatus(Device->GetInstanceHandle(), ResetEvent) == VK_EVENT_SET)
				{
					Result = VulkanRHI::vkGetQueryPoolResults(Device->GetInstanceHandle(), QueryPool, 0, NumUsedQueries, NumUsedQueries * sizeof(uint64), QueryOutput.GetData(), sizeof(uint64), VK_QUERY_RESULT_64_BIT);
				}

				if (Result == VK_SUCCESS)
				{
					bSuccess = true;
					break;
				}
				else if (Result == VK_NOT_READY)
				{
					bSuccess = false;
				}
				else
				{
					bSuccess = false;
					VERIFYVULKANRESULT(Result);
				}

				// timer queries are used for Benchmarks which can stall a bit more
				const double TimeoutValue = (QueryType == VK_QUERY_TYPE_TIMESTAMP) ? 2.0 : 0.5;
				// look for gpu stuck/crashed
				if ((FPlatformTime::Seconds() - StartTime) > TimeoutValue)
				{
					if (QueryType == VK_QUERY_TYPE_OCCLUSION)
					{
						UE_LOG(LogRHI, Log, TEXT("Timed out while waiting for GPU to catch up on occlusion results. (%.1f s)"), TimeoutValue);
					}
					else
					{
						UE_LOG(LogRHI, Log, TEXT("Timed out while waiting for GPU to catch up on occlusion/timer results. (%.1f s)"), TimeoutValue);
					}
					return false;
				}

				++NumLoops;
			}

			State = EState::RT_PostGetResults;
			return true;
		}
	}
	else
	{
		VERIFYVULKANRESULT(Result);
	}

	return false;
}

void FVulkanOcclusionQueryPool::SetFence(FVulkanCmdBuffer* InCmdBuffer)
{
	check(InCmdBuffer);
	CmdBuffer = InCmdBuffer;
	FenceCounter = InCmdBuffer->GetFenceSignaledCounter();
}

void FVulkanOcclusionQueryPool::Reset(FVulkanCmdBuffer* InCmdBuffer, uint32 InFrameNumber)
{
/*
	check(!InCmdBuffer || InCmdBuffer->GetFenceSignaledCounter() > FenceCounter);
*/
	//ensure(State == EState::Undefined || State == EState::RT_PostGetResults);
	FMemory::Memzero(AcquiredIndices.GetData(), AcquiredIndices.GetAllocatedSize());
/*
	CmdBuffer = nullptr;
	FenceCounter = UINT32_MAX;
*/
	NumUsedQueries = 0;
	FrameNumber = InFrameNumber;
/*
	bHasResults = false;
*/
	VulkanRHI::vkResetEvent(Device->GetInstanceHandle(), ResetEvent);
	VulkanRHI::vkCmdResetQueryPool(InCmdBuffer->GetHandle(), QueryPool, 0, MaxQueries);
	
	// workaround for apparent cache-flush bug in AMD driver implementation of vkCmdResetQueryPool
	if (Device->GetVendorId() == EGpuVendorId::Amd)
	{
		VkMemoryBarrier barrier;
		ZeroVulkanStruct(barrier, VK_STRUCTURE_TYPE_MEMORY_BARRIER);
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
		VulkanRHI::vkCmdPipelineBarrier(InCmdBuffer->GetHandle(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &barrier, 0, 0, 0, 0);
	}
	
	VulkanRHI::vkCmdSetEvent(InCmdBuffer->GetHandle(), ResetEvent, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
	//FVulkanQueryPool::ResetAll(InCmdBuffer);

	State = EState::RHIT_PostBeginBatch;
}

void FVulkanCommandListContext::BeginOcclusionQueryBatch(FVulkanCmdBuffer* CmdBuffer, uint32 NumQueriesInBatch)
{
	ensure(IsImmediate());
/*
	FScopeLock ScopeLock(&GOcclusionQueryCS);
*/
	checkf(!CurrentOcclusionQueryPool, TEXT("BeginOcclusionQueryBatch called without corresponding EndOcclusionQueryBatch!"));
	CurrentOcclusionQueryPool = Device->AcquireOcclusionQueryPool(GetCommandBufferManager(), NumQueriesInBatch);
	ensure(CmdBuffer->IsOutsideRenderPass());
	CurrentOcclusionQueryPool->Reset(CmdBuffer, GFrameNumberRenderThread);
}


inline bool FVulkanOcclusionQueryPool::IsStalePool() const
{
	return FrameNumber + NUM_FRAMES_TO_WAIT_REUSE_POOL < GFrameNumberRenderThread;
}

void FVulkanOcclusionQueryPool::FlushAllocatedQueries()
{
	for (int32 Index = 0; Index < AcquiredIndices.Num(); ++Index)
	{
		uint32 QueryIndex = 0;
		uint64 Acquired = AcquiredIndices[Index];
		while (Acquired)
		{
			if (Acquired & 1)
			{
				FVulkanOcclusionQuery* Query =AllocatedQueries[QueryIndex + (Index * 64)];
				Query->State = Query->State == FVulkanOcclusionQuery::EState::RT_GotResults ? FVulkanOcclusionQuery::EState::FlushedFromPoolHadResults : FVulkanOcclusionQuery::EState::Undefined;
				Query->Pool = nullptr;
				Query->IndexInPool = UINT32_MAX;

				AllocatedQueries[QueryIndex + (Index * 64)] = nullptr;
			}
			Acquired = Acquired >> (uint64)1;
			QueryIndex++;
		}

		AcquiredIndices[Index] = Acquired;
	}
}

FVulkanOcclusionQueryPool* FVulkanDevice::AcquireOcclusionQueryPool(FVulkanCommandBufferManager* CommandBufferManager, uint32 NumQueries)
{
	// At least add one query
	NumQueries = FMath::Max(1u, NumQueries);
	NumQueries = AlignArbitrary(NumQueries, GMinNumberOfQueriesInPool);

	bool bChanged = false;
	for (int32 Index = UsedOcclusionQueryPools.Num() - 1; Index >= 0; --Index)
	{
		FVulkanOcclusionQueryPool* Pool = UsedOcclusionQueryPools[Index];
		if (Pool->CanBeReused())
		{
			UsedOcclusionQueryPools.RemoveAtSwap(Index);
			FreeOcclusionQueryPools.Add(Pool);
			Pool->FreedFrameNumber = GFrameNumberRenderThread;
			bChanged = true;
		}
		else if (Pool->IsStalePool())
		{
			Pool->FlushAllocatedQueries();
			UsedOcclusionQueryPools.RemoveAtSwap(Index);
			FreeOcclusionQueryPools.Add(Pool);
			Pool->FreedFrameNumber = GFrameNumberRenderThread;
			bChanged = true;
		}
	}

	if (FreeOcclusionQueryPools.Num() > 0)
	{
		if (bChanged)
		{
			FreeOcclusionQueryPools.Sort([](FVulkanOcclusionQueryPool& A, FVulkanOcclusionQueryPool& B)
			{
				return A.GetMaxQueries() < B.GetMaxQueries();
			});
		}

		for (int32 Index = 0; Index < FreeOcclusionQueryPools.Num(); ++Index)
		{
			if (NumQueries <= FreeOcclusionQueryPools[Index]->GetMaxQueries())
			{
				FVulkanOcclusionQueryPool* Pool = FreeOcclusionQueryPools[Index];
				FreeOcclusionQueryPools.RemoveAt(Index);
				UsedOcclusionQueryPools.Add(Pool);
				return Pool;
			}
		}
	}

	FVulkanOcclusionQueryPool* Pool = new FVulkanOcclusionQueryPool(this, CommandBufferManager, NumQueries);
	UsedOcclusionQueryPools.Add(Pool);
	return Pool;
}

void FVulkanDevice::ReleaseUnusedOcclusionQueryPools()
{
	if (GFrameNumberRenderThread < NUM_FRAMES_TO_WAIT_RELEASE_POOL)
	{
		return;
	}
	
	uint32 ReleaseFrame = (GFrameNumberRenderThread - NUM_FRAMES_TO_WAIT_RELEASE_POOL);

	for (int32 Index = FreeOcclusionQueryPools.Num() - 1; Index >= 0; --Index)
	{
		FVulkanOcclusionQueryPool* Pool = FreeOcclusionQueryPools[Index];
		if (ReleaseFrame > Pool->FreedFrameNumber) //-V547
		{
			delete Pool;
			FreeOcclusionQueryPools.RemoveAt(Index);
		}
	}
}

void FVulkanCommandListContext::EndOcclusionQueryBatch(FVulkanCmdBuffer* CmdBuffer)
{
	ensure(IsImmediate());
	check(CmdBuffer);
	checkf(CurrentOcclusionQueryPool, TEXT("EndOcclusionQueryBatch called without corresponding BeginOcclusionQueryBatch!"));
	CurrentOcclusionQueryPool->EndBatch(CmdBuffer);
	CurrentOcclusionQueryPool = nullptr;
}


FVulkanOcclusionQuery::FVulkanOcclusionQuery()
	: FVulkanRenderQuery(RQT_Occlusion)
{
	INC_DWORD_STAT(STAT_VulkanNumQueries);
}

FVulkanOcclusionQuery::~FVulkanOcclusionQuery()
{
	if (State != EState::Undefined)
	{
		if (IndexInPool != UINT32_MAX)
		{
			ReleaseFromPool();
		}
		else
		{
			ensure(State == EState::RT_GotResults);
		}
	}

	DEC_DWORD_STAT(STAT_VulkanNumQueries);
}

void FVulkanOcclusionQuery::ReleaseFromPool()
{
	Pool->ReleaseIndex(IndexInPool);
	State = EState::Undefined;
	IndexInPool = UINT32_MAX;
}

void FVulkanCommandListContext::ReadAndCalculateGPUFrameTime()
{
	check(IsImmediate());

	if (FVulkanPlatform::SupportsTimestampRenderQueries() && FrameTiming)
	{
		uint64 Delta = 0;
		
		// If we support profile command buffers then use this timing for GPU
		if (GVulkanUseCmdBufferTimingForGPUTime)
		{
			Delta = CommandBufferManager->CalculateGPUTime();
		}
		else
		{
			Delta = FrameTiming->GetTiming(false);
		}
	
		const double SecondsPerCycle = FPlatformTime::GetSecondsPerCycle();
		const double Frequency = double(FVulkanGPUTiming::GetTimingFrequency());
		GGPUFrameTime = FMath::TruncToInt(double(Delta) / Frequency / SecondsPerCycle);
	}
	else if(!FVulkanPlatform::HasCustomFrameTiming())
	{
		GGPUFrameTime = 0;
	}
}


FRenderQueryRHIRef FVulkanDynamicRHI::RHICreateRenderQuery(ERenderQueryType QueryType)
{
	if (QueryType == RQT_Occlusion)
	{
		FVulkanRenderQuery* Query = new FVulkanOcclusionQuery();
		return Query;
	}
	else if (QueryType == RQT_AbsoluteTime)
	{
		FVulkanTimingQuery* Query = new FVulkanTimingQuery();
		return Query;
	}
	else
	{
		// Dummy!
		ensureMsgf(0, TEXT("Unknown QueryType %d"), QueryType);
	}
	FVulkanRenderQuery* Query = new FVulkanRenderQuery(QueryType);
	return Query;
}

bool FVulkanDynamicRHI::RHIGetRenderQueryResult(FRHIRenderQuery* QueryRHI, uint64& OutNumPixels, bool bWait, uint32 GPUIndex)
{
	auto ToMicroseconds = [](uint64 Timestamp)
	{
		const double Frequency = double(FVulkanGPUTiming::GetTimingFrequency());
		uint64 Microseconds = (uint64)((double(Timestamp) / Frequency) * 1000.0 * 1000.0);
		return Microseconds;
	};
	check(IsInRenderingThread());
	FVulkanRenderQuery* BaseQuery = ResourceCast(QueryRHI);
	if (BaseQuery->QueryType == RQT_Occlusion)
	{
		FVulkanOcclusionQuery* Query = static_cast<FVulkanOcclusionQuery*>(BaseQuery);
		if (Query->State == FVulkanOcclusionQuery::EState::RT_GotResults || Query->State == FVulkanOcclusionQuery::EState::FlushedFromPoolHadResults)
		{
			OutNumPixels = Query->Result;
			return true;
		}

		if (Query->State == FVulkanOcclusionQuery::EState::Undefined)
		{
			UE_LOG(LogVulkanRHI, Verbose, TEXT("Stale query asking for result!"));
			return false;
		}

		ensure(Query->State == FVulkanOcclusionQuery::EState::RHI_PostEnd);
		if (Query->Pool->TryGetResults(bWait))
		{
			Query->Result = Query->Pool->GetResultValue(Query->IndexInPool);
			Query->ReleaseFromPool();
			Query->State = FVulkanOcclusionQuery::EState::RT_GotResults;
			OutNumPixels = Query->Result;
			return true;
		}
	}
	else if (BaseQuery->QueryType == RQT_AbsoluteTime)
	{
		FVulkanTimingQuery* Query = static_cast<FVulkanTimingQuery*>(BaseQuery);
		if (Query->Pool == nullptr)
		{
			// RHIEndRenderQuery hasn't been executed yet for this query, so it's not ready yet. If it's a non-blocking wait, we can return false right away.
			if (!bWait)
			{
				return false;
			}

			// Blocking was requested, so, we need to wait for the RHI thread to catch up.
			FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThread);
			check(Query->Pool != nullptr);
		}

		check(Query->Pool->CurrentTimestamp < Query->Pool->BufferSize);
		int32 TimestampIndex = Query->Pool->CurrentTimestamp;
		if (!bWait)
		{
			// Quickly check the most recent measurements to see if any of them has been resolved.  Do not flush these queries.
			for (uint32 IssueIndex = 1; IssueIndex < Query->Pool->NumIssuedTimestamps; ++IssueIndex)
			{
				const FVulkanTimingQueryPool::FCmdBufferFence& StartQuerySyncPoint = Query->Pool->TimestampListHandles[TimestampIndex];
				if (StartQuerySyncPoint.FenceCounter < StartQuerySyncPoint.CmdBuffer->GetFenceSignaledCounter())
				{
					Query->Pool->ResultsBuffer->InvalidateMappedMemory();
					const uint64* Data = Query->Pool->MappedPointer;
					OutNumPixels = ToMicroseconds(Data[TimestampIndex]);
					return true;
				}

				TimestampIndex = (TimestampIndex + Query->Pool->BufferSize - 1) % Query->Pool->BufferSize;
			}
		}

		if (Query->Pool->NumIssuedTimestamps > 0 || bWait)
		{
			// None of the (NumIssuedTimestamps - 1) measurements were ready yet,
			// so check the oldest measurement more thoroughly.
			// This really only happens if occlusion and frame sync event queries are disabled, otherwise those will block until the GPU catches up to 1 frame behind

			const bool bBlocking = (Query->Pool->NumIssuedTimestamps == Query->Pool->BufferSize) || bWait;
			{
				FRenderThreadIdleScope IdleScope(ERenderThreadIdleTypes::WaitingForGPUQuery);
				SCOPE_CYCLE_COUNTER(STAT_RenderQueryResultTime);

				if (bBlocking)
				{
					const FVulkanTimingQueryPool::FCmdBufferFence& StartQuerySyncPoint = Query->Pool->TimestampListHandles[TimestampIndex];
					bool bWaitForStart = StartQuerySyncPoint.FenceCounter == StartQuerySyncPoint.CmdBuffer->GetFenceSignaledCounter();
					if (bWaitForStart)
					{
						FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThread);

						// Need to submit the open command lists.
						Device->SubmitCommandsAndFlushGPU();
					}

					// CPU wait for query results to be ready.
					if (bWaitForStart && StartQuerySyncPoint.FenceCounter == StartQuerySyncPoint.CmdBuffer->GetFenceSignaledCounter())
					{
						Device->GetImmediateContext().GetCommandBufferManager()->WaitForCmdBuffer(StartQuerySyncPoint.CmdBuffer);
					}
				}

				Query->Pool->ResultsBuffer->InvalidateMappedMemory();
			}

			const uint64* Data = Query->Pool->MappedPointer;
			OutNumPixels = ToMicroseconds(Data[TimestampIndex]);
			return true;
		}
	}

	return false;
}

void FVulkanCommandListContext::RHIBeginRenderQuery(FRHIRenderQuery* QueryRHI)
{
	FVulkanRenderQuery* BaseQuery = ResourceCast(QueryRHI);
	if (BaseQuery->QueryType == RQT_Occlusion)
	{
		//#todo-rco: Temp until we get the merge straightened out
		if (!CurrentOcclusionQueryPool)
		{
			return;
		}
		check(CurrentOcclusionQueryPool);
		FVulkanOcclusionQuery* Query = static_cast<FVulkanOcclusionQuery*>(BaseQuery);
		if (Query->State == FVulkanOcclusionQuery::EState::RHI_PostEnd)
		{
			Query->ReleaseFromPool();
		}
		else if (Query->State == FVulkanOcclusionQuery::EState::RT_GotResults)
		{
			// Nothing to do here...
		}
		else
		{
			ensure(Query->State == FVulkanOcclusionQuery::EState::Undefined || Query->State == FVulkanOcclusionQuery::EState::FlushedFromPoolHadResults);
		}
		Query->State = FVulkanOcclusionQuery::EState::RHI_PostBegin;
		const uint32 IndexInPool = CurrentOcclusionQueryPool->AcquireIndex(Query);
		Query->Pool = CurrentOcclusionQueryPool;
		Query->IndexInPool = IndexInPool;
		FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
		VulkanRHI::vkCmdBeginQuery(CmdBuffer->GetHandle(), CurrentOcclusionQueryPool->GetHandle(), IndexInPool, VK_QUERY_CONTROL_PRECISE_BIT);
	}
	else if (BaseQuery->QueryType == RQT_AbsoluteTime)
	{
		ensureMsgf(0, TEXT("Timing queries should NOT call RHIBeginRenderQuery()!"));
	}
}

void FVulkanCommandListContext::RHIEndRenderQuery(FRHIRenderQuery* QueryRHI)
{
	FVulkanRenderQuery* BaseQuery = ResourceCast(QueryRHI);
	if (BaseQuery->QueryType == RQT_Occlusion)
	{
		//#todo-rco: Temp until we get the merge straightened out
		if (!CurrentOcclusionQueryPool)
		{
			return;
		}

		FVulkanOcclusionQuery* Query = static_cast<FVulkanOcclusionQuery*>(BaseQuery);
		ensure(Query->State == FVulkanOcclusionQuery::EState::RHI_PostBegin);
		Query->State = FVulkanOcclusionQuery::EState::RHI_PostEnd;
		FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
		VulkanRHI::vkCmdEndQuery(CmdBuffer->GetHandle(), CurrentOcclusionQueryPool->GetHandle(), Query->IndexInPool);
	}
	else if (BaseQuery->QueryType == RQT_AbsoluteTime)
	{
		FVulkanTimingQuery* Query = static_cast<FVulkanTimingQuery*>(BaseQuery);

		if (Query->Pool == nullptr)
		{
			Query->Pool = new FVulkanTimingQueryPool(Device, CommandBufferManager, 4);
			Query->Pool->ResultsBuffer = Device->GetStagingManager().AcquireBuffer(Query->Pool->BufferSize * sizeof(uint64), VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
			Query->Pool->MappedPointer = (uint64*)Query->Pool->ResultsBuffer->GetMappedPointer();
		}

		Query->Pool->CurrentTimestamp = (Query->Pool->CurrentTimestamp + 1) % Query->Pool->BufferSize;
		const uint32 QueryEndIndex = Query->Pool->CurrentTimestamp;
		const VkPipelineStageFlagBits QueryPipelineStage = GTimestampQueryStage ? VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		FVulkanCmdBuffer* CmdBuffer = CommandBufferManager->GetActiveCmdBuffer();
		VulkanRHI::vkCmdWriteTimestamp(CmdBuffer->GetHandle(), QueryPipelineStage, Query->Pool->GetHandle(), QueryEndIndex);
		CmdBuffer->AddPendingTimestampQuery(QueryEndIndex, 1, Query->Pool->GetHandle(), Query->Pool->ResultsBuffer->GetHandle(), true);
		Query->Pool->TimestampListHandles[QueryEndIndex].CmdBuffer = CmdBuffer;
		Query->Pool->TimestampListHandles[QueryEndIndex].FenceCounter = CmdBuffer->GetFenceSignaledCounter();
		Query->Pool->TimestampListHandles[QueryEndIndex].FrameCount = GetFrameCounter();
		Query->Pool->NumIssuedTimestamps = FMath::Min<uint32>(Query->Pool->NumIssuedTimestamps + 1, Query->Pool->BufferSize);
	}
}

void FVulkanCommandListContext::RHICalibrateTimers(FRHITimestampCalibrationQuery* CalibrationQuery)
{
	if (Device->GetOptionalExtensions().HasEXTCalibratedTimestamps)
	{
		FGPUTimingCalibrationTimestamp CalibrationTimestamp = Device->GetCalibrationTimestamp();
		CalibrationQuery->CPUMicroseconds[0] = CalibrationTimestamp.CPUMicroseconds;
		CalibrationQuery->GPUMicroseconds[0] = CalibrationTimestamp.GPUMicroseconds;
	}
}

void FVulkanCommandListContext::WriteBeginTimestamp(FVulkanCmdBuffer* CmdBuffer)
{
	FrameTiming->StartTiming(CmdBuffer);
}

void FVulkanCommandListContext::WriteEndTimestamp(FVulkanCmdBuffer* CmdBuffer)
{
	FrameTiming->EndTiming(CmdBuffer);
}

FVulkanTimingQuery::FVulkanTimingQuery()
	: FVulkanRenderQuery(RQT_AbsoluteTime)
{
}

FVulkanTimingQuery::~FVulkanTimingQuery()
{
	if (Pool != nullptr)
	{
		Pool->GetParent()->GetStagingManager().ReleaseBuffer(nullptr, Pool->ResultsBuffer);
		delete Pool;
	}
}
