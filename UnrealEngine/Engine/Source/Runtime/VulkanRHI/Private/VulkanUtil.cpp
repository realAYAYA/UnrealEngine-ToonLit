// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanUtil.cpp: Vulkan Utility implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanUtil.h"
#include "VulkanPendingState.h"
#include "VulkanContext.h"
#include "VulkanMemory.h"
#include "Misc/App.h"
#include "Misc/OutputDeviceRedirector.h"
#include "RHIValidationContext.h"
#include "HAL/FileManager.h"
#include "RenderCore.h"

#if NV_AFTERMATH
#include "GFSDK_Aftermath_GpuCrashDump.h"
#include "GFSDK_Aftermath_GpuCrashDumpDecoding.h"
#endif

FVulkanDynamicRHI*	GVulkanRHI = nullptr;

extern CORE_API bool GIsGPUCrashed;


static FString		EventDeepString(TEXT("EventTooDeep"));
static const uint32	EventDeepCRC = FCrc::StrCrc32<TCHAR>(*EventDeepString);
static const uint32 BUFFERED_TIMING_QUERIES = 1;
static const uint32 TIMING_QUERY_RETRIES = 1;

#if NV_AFTERMATH
	float GVulkanNVAfterMathDumpWaitTime = 10.0f;
	static FAutoConsoleVariableRef CVarVulkanNVAfterMathDumpWaitTime(
		TEXT("r.VulkanNVAfterMathDumpWaitTime"),
		GVulkanNVAfterMathDumpWaitTime,
		TEXT("Amount of time to wait for NV Aftermath to finish processing GPU crash dumps."),
		ECVF_Default
	);
#endif

/**
 * Initializes the static variables, if necessary.
 */
void FVulkanGPUTiming::PlatformStaticInitialize(void* UserData)
{
	GIsSupported = false;

	// Are the static variables initialized?
	check( !GAreGlobalsInitialized );

	FVulkanGPUTiming* Caller = (FVulkanGPUTiming*)UserData;
	if (Caller && Caller->Device && FVulkanPlatform::SupportsTimestampRenderQueries())
	{
		const VkPhysicalDeviceLimits& Limits = Caller->Device->GetDeviceProperties().limits;
		bool bSupportsTimestamps = (Limits.timestampComputeAndGraphics == VK_TRUE);
		if (!bSupportsTimestamps)
		{
			UE_LOG(LogVulkanRHI, Warning, TEXT("Timestamps not supported on Device"));
			return;
		}
		SetTimingFrequency((uint64)((1000.0 * 1000.0 * 1000.0) / Limits.timestampPeriod));
		GIsSupported = true;
	}
}

void FVulkanGPUTiming::CalibrateTimers(FVulkanCommandListContext& InCmdContext)
{
	FVulkanDevice* Device = InCmdContext.GetDevice();
	if (Device->GetOptionalExtensions().HasEXTCalibratedTimestamps)
	{
		FGPUTimingCalibrationTimestamp CalibrationTimestamp = Device->GetCalibrationTimestamp();
		SetCalibrationTimestamp(CalibrationTimestamp);
	}
}

void FVulkanDynamicRHI::RHICalibrateTimers()
{
	check(IsInRenderingThread());

	FScopedRHIThreadStaller StallRHIThread(FRHICommandListExecutor::GetImmediateCommandList());

	FVulkanGPUTiming::CalibrateTimers(GetDevice()->GetImmediateContext());
}


FVulkanStagingBuffer::~FVulkanStagingBuffer()
{
	if (StagingBuffer)
	{
		check(Device);
		Device->GetStagingManager().ReleaseBuffer(nullptr, StagingBuffer);
	}
}

void* FVulkanStagingBuffer::Lock(uint32 Offset, uint32 NumBytes)
{
	check(!bIsLocked);
	bIsLocked = true;
	const uint32 EndOffset = Offset + NumBytes;
	checkf(EndOffset <= QueuedNumBytes, TEXT("Lock at Offset (%u) and NumBytes (%u) reads beyond the allocated size of the staging buffer (%u)"), Offset, NumBytes, QueuedNumBytes);
	// make sure cached memory is invalidated
	StagingBuffer->InvalidateMappedMemory();
	return (void*)((uint8*)StagingBuffer->GetMappedPointer() + Offset);
}

void FVulkanStagingBuffer::Unlock()
{
	check(bIsLocked);
	bIsLocked = false;
}

FStagingBufferRHIRef FVulkanDynamicRHI::RHICreateStagingBuffer()
{
	return new FVulkanStagingBuffer();
}

void* FVulkanDynamicRHI::RHILockStagingBuffer(FRHIStagingBuffer* StagingBufferRHI, FRHIGPUFence* FenceRHI, uint32 Offset, uint32 NumBytes)
{
	FVulkanStagingBuffer* StagingBuffer = ResourceCast(StagingBufferRHI);

	if (FenceRHI && !FenceRHI->Poll())
	{
		Device->SubmitCommandsAndFlushGPU();

		// SubmitCommandsAndFlushGPU might update fence state if it was tied to a previously submitted command buffer.
		// Its state will have been updated from Submitted to NeedReset, and would assert in WaitForCmdBuffer (which is not needed in such a case)
		if (!FenceRHI->Poll())
		{
			FVulkanGPUFence* Fence = ResourceCast(FenceRHI);
			Device->GetImmediateContext().GetCommandBufferManager()->WaitForCmdBuffer(Fence->GetCmdBuffer());
		}
	}

	return StagingBuffer->Lock(Offset, NumBytes);
}

void FVulkanDynamicRHI::RHIUnlockStagingBuffer(FRHIStagingBuffer* StagingBufferRHI)
{
	FVulkanStagingBuffer* StagingBuffer = ResourceCast(StagingBufferRHI);
	StagingBuffer->Unlock();
}

void FVulkanGPUFence::Clear()
{
	CmdBuffer = nullptr;
	FenceSignaledCounter = MAX_uint64;
}

bool FVulkanGPUFence::Poll() const
{
	return (CmdBuffer && (FenceSignaledCounter < CmdBuffer->GetFenceSignaledCounter()));
}

FGPUFenceRHIRef FVulkanDynamicRHI::RHICreateGPUFence(const FName& Name)
{
	return new FVulkanGPUFence(Name);
}

FVulkanGPUTiming::~FVulkanGPUTiming()
{
	check(!Pool);
}

/**
 * Initializes all Vulkan resources and if necessary, the static variables.
 */
void FVulkanGPUTiming::Initialize(uint32 PoolSize)
{
	StaticInitialize(this, PlatformStaticInitialize);

	bIsTiming = false;

	if (FVulkanPlatform::SupportsTimestampRenderQueries() && GIsSupported)
	{
		check(!Pool);
		Pool = new FVulkanTimingQueryPool(Device, CmdContext->GetCommandBufferManager(), PoolSize);
		Pool->ResultsBuffer = Device->GetStagingManager().AcquireBuffer(Pool->GetMaxQueries() * sizeof(uint64) * 2, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		Pool->MappedPointer = (uint64*)Pool->ResultsBuffer->GetMappedPointer();
	}
}

/**
 * Releases all Vulkan resources.
 */
void FVulkanGPUTiming::Release()
{
	if (FVulkanPlatform::SupportsTimestampRenderQueries() && GIsSupported)
	{
		check(Pool);
		Device->GetStagingManager().ReleaseBuffer(nullptr, Pool->ResultsBuffer);
		delete Pool;
		Pool = nullptr;
	}
}

/**
 * Start a GPU timing measurement.
 */
void FVulkanGPUTiming::StartTiming(FVulkanCmdBuffer* CmdBuffer)
{
	// Issue a timestamp query for the 'start' time.
	if (GIsSupported && !bIsTiming)
	{
		if (CmdBuffer == nullptr)
		{
			CmdBuffer = CmdContext->GetCommandBufferManager()->GetActiveCmdBuffer();
		}

		// In case we aren't reading queries, remove oldest
		if (NumPendingQueries >= Pool->BufferSize)
		{
			DiscardExpiredQueries(true);

			if (NumPendingQueries >= Pool->BufferSize)
			{
				return;
			}
		}

		RefreshPendingQueries(*CmdBuffer);

		Pool->CurrentTimestamp = (Pool->CurrentTimestamp + 1) % Pool->BufferSize;
		const uint32 QueryStartIndex = Pool->CurrentTimestamp * 2;

		// Since we use non-blocking queries, we need to reset them before using them
		// Use host query resets if supported since there's no need to consider host/GPU sync
		if (Device->GetOptionalExtensions().HasEXTHostQueryReset)
		{
			VulkanRHI::vkResetQueryPoolEXT(Device->GetInstanceHandle(), Pool->GetHandle(), QueryStartIndex, 2);
		}
		else
		{
			VulkanRHI::vkCmdResetQueryPool(CmdBuffer->GetHandle(), Pool->GetHandle(), QueryStartIndex, 2);
		}

		VulkanRHI::vkCmdWriteTimestamp(CmdBuffer->GetHandle(), VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, Pool->GetHandle(), QueryStartIndex);
		Pool->TimestampListHandles[QueryStartIndex].CmdBuffer = CmdBuffer;
		Pool->TimestampListHandles[QueryStartIndex].FenceCounter = CmdBuffer->GetFenceSignaledCounter();
		Pool->TimestampListHandles[QueryStartIndex].FrameCount = CmdContext->GetFrameCounter();
		Pool->TimestampListHandles[QueryStartIndex].Attempts = 0;
		bIsTiming = true;
	}
}

/**
 * End a GPU timing measurement.
 * The timing for this particular measurement will be resolved at a later time by the GPU.
 */
void FVulkanGPUTiming::EndTiming(FVulkanCmdBuffer* CmdBuffer)
{
	// Issue a timestamp query for the 'end' time.
	if (GIsSupported && bIsTiming)
	{
		if (CmdBuffer == nullptr)
		{
			CmdBuffer = CmdContext->GetCommandBufferManager()->GetActiveCmdBuffer();
		}
		check(Pool->CurrentTimestamp < Pool->BufferSize);
		const uint32 QueryStartIndex = Pool->CurrentTimestamp * 2;
		// Keep Start and End contiguous to fetch them together with a single AddPendingTimestampQuery(QueryStartIndex,2,...)
		const uint32 QueryEndIndex = QueryStartIndex + 1;   

		PendingQueries.Enqueue(Pool->CurrentTimestamp);
		NumPendingQueries++;

		VulkanRHI::vkCmdWriteTimestamp(CmdBuffer->GetHandle(), VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, Pool->GetHandle(), QueryEndIndex);
		CmdBuffer->AddPendingTimestampQuery(QueryStartIndex, 2, Pool->GetHandle(), Pool->ResultsBuffer->GetHandle(), false);
		Pool->TimestampListHandles[QueryStartIndex].CmdBuffer = CmdBuffer;
		Pool->TimestampListHandles[QueryStartIndex].FenceCounter = CmdBuffer->GetFenceSignaledCounter();
		Pool->TimestampListHandles[QueryEndIndex].CmdBuffer = CmdBuffer;
		Pool->TimestampListHandles[QueryEndIndex].FenceCounter = CmdBuffer->GetFenceSignaledCounter();
		Pool->TimestampListHandles[QueryEndIndex].FrameCount = CmdContext->GetFrameCounter();
		Pool->TimestampListHandles[QueryEndIndex].Attempts = 0;
		Pool->NumIssuedTimestamps = FMath::Min<uint32>(Pool->NumIssuedTimestamps + 1, Pool->BufferSize);

		bIsTiming = false;
		bEndTimestampIssued = true;
	}
}

void FVulkanGPUTiming::DiscardExpiredQueries(bool bNeedSpace)
{
	// Returns true if we can safely throw away the query
	auto CanBeDiscarded = [this](uint32 QueryIndex)
	{
		FVulkanTimingQueryPool::FCmdBufferFence& QuerySyncPoint = Pool->TimestampListHandles[QueryIndex];
		if (QuerySyncPoint.CmdBuffer && (QuerySyncPoint.FenceCounter < QuerySyncPoint.CmdBuffer->GetFenceSignaledCounter()))
		{
			const uint64 Availability = Pool->MappedPointer[QueryIndex * 2 + 1];

			if (Availability || (QuerySyncPoint.Attempts < TIMING_QUERY_RETRIES))
			{
				return false;
			}
		}
		return true;
	};

	auto IsQueryExpired = [&](uint32 QueryIndex)
	{
		bool QueryExpired = CanBeDiscarded(QueryIndex);
		if (!QueryExpired && bNeedSpace)
		{
			// If the hold back for a query is the fence and the command buffer was submitted, take a shot at updating its status
			FVulkanTimingQueryPool::FCmdBufferFence& QuerySyncPoint = Pool->TimestampListHandles[QueryIndex];
			if ((QuerySyncPoint.FenceCounter >= QuerySyncPoint.CmdBuffer->GetFenceSignaledCounter()) && QuerySyncPoint.CmdBuffer->IsSubmitted())
			{
				QuerySyncPoint.CmdBuffer->GetOwner()->RefreshFenceStatus();
				if (QuerySyncPoint.FenceCounter < QuerySyncPoint.CmdBuffer->GetFenceSignaledCounter())
				{
					QueryExpired = CanBeDiscarded(QueryIndex);
				}
			}
		}
		return QueryExpired;
	};

	Pool->ResultsBuffer->InvalidateMappedMemory();

	uint32 OldestTimestamp;
	while (PendingQueries.Peek(OldestTimestamp))
	{
		const uint32 QueryStartIndex = OldestTimestamp * 2;
		const uint32 QueryEndIndex = QueryStartIndex + 1;

		const bool StartQueryExpired = IsQueryExpired(QueryStartIndex);
		const bool EndQueryExpired = IsQueryExpired(QueryEndIndex);

		if (StartQueryExpired || EndQueryExpired)
		{
			// If the queries is no good anymore, throw it away and try the next one
			PendingQueries.Pop();
			NumPendingQueries--;
		}
		else
		{
			break;
		}
	}
}

void FVulkanGPUTiming::RefreshPendingQueries(FVulkanCmdBuffer& CmdBuffer)
{
	for (int32 QueryIndex=0; QueryIndex < Pool->TimestampListHandles.Num(); ++QueryIndex)
	{
		FVulkanTimingQueryPool::FCmdBufferFence& QuerySyncPoint = Pool->TimestampListHandles[QueryIndex];

		if (QuerySyncPoint.CmdBuffer && (QuerySyncPoint.FenceCounter < QuerySyncPoint.CmdBuffer->GetFenceSignaledCounter()))
		{
			Pool->ResultsBuffer->InvalidateMappedMemory();
			const uint64 Availability = Pool->MappedPointer[QueryIndex * 2 + 1];

			if (!Availability && (QuerySyncPoint.Attempts < TIMING_QUERY_RETRIES))
			{
				QuerySyncPoint.CmdBuffer = &CmdBuffer;
				QuerySyncPoint.FenceCounter = CmdBuffer.GetFenceSignaledCounter();
				QuerySyncPoint.FrameCount = CmdContext->GetFrameCounter();
				++QuerySyncPoint.Attempts;

				CmdBuffer.AddPendingTimestampQuery(
					QueryIndex, 1, Pool->GetHandle(), Pool->ResultsBuffer->GetHandle(), false);
			}
		}
	}
}


/**
 * Retrieves the most recently resolved timing measurement.
 * The unit is the same as for FPlatformTime::Cycles(). Returns 0 if there are no resolved measurements.
 *
 * @return	Value of the most recently resolved timing, or 0 if no measurements have been resolved by the GPU yet.
 */
uint64 FVulkanGPUTiming::GetTiming(bool bGetCurrentResultsAndBlock)
{
	if (GIsSupported)
	{
		check(Pool->CurrentTimestamp < Pool->BufferSize);
		uint64 StartTime, EndTime, StartTimeAvailability, EndTimeAvailability;
		uint32 TimestampIndex;

		uint64 TotalTime = 0;

		uint64_t FrameCount = CmdContext->GetFrameCounter();

		// If these timings have been processed return the same time
		if (PreviousFrame == FrameCount)
		{
			return PreviousTime;
		}

		PreviousFrame = FrameCount;

		DiscardExpiredQueries(false);

		while (PendingQueries.Peek(TimestampIndex))
		{
			const uint32 QueryStartIndex = TimestampIndex * 2;
			const uint32 QueryEndIndex = QueryStartIndex + 1;

			const FVulkanTimingQueryPool::FCmdBufferFence& StartQuerySyncPoint = Pool->TimestampListHandles[QueryStartIndex];
			const FVulkanTimingQueryPool::FCmdBufferFence& EndQuerySyncPoint = Pool->TimestampListHandles[QueryEndIndex];

			// Block if we require results or we are backed up
			bool bBlocking = bGetCurrentResultsAndBlock;

			if (!bBlocking)
			{
				const uint64_t QueryFrameCount = EndQuerySyncPoint.FrameCount;
				
				// Allow queries to back up if we are non-blocking
				if (FrameCount < BUFFERED_TIMING_QUERIES || FrameCount - BUFFERED_TIMING_QUERIES < QueryFrameCount)
				{
					PreviousTime = TotalTime;
					return TotalTime;
				}
				else
				{
					// Otherwise if we don't have a result we have to block
					bBlocking = true;
				}
			}

			if (EndQuerySyncPoint.FenceCounter < EndQuerySyncPoint.CmdBuffer->GetFenceSignaledCounter() &&
				StartQuerySyncPoint.FenceCounter < StartQuerySyncPoint.CmdBuffer->GetFenceSignaledCounter())
			{
				Pool->ResultsBuffer->InvalidateMappedMemory();
				StartTimeAvailability = Pool->MappedPointer[QueryStartIndex * 2 + 1];
				EndTimeAvailability = Pool->MappedPointer[QueryEndIndex * 2 + 1];
				StartTime = Pool->MappedPointer[QueryStartIndex * 2];
				EndTime = Pool->MappedPointer[QueryEndIndex * 2];

				if (!StartTimeAvailability || !EndTimeAvailability)
				{
					break;
				}

				PendingQueries.Pop();
				NumPendingQueries--;

				if (EndTime > StartTime)
				{
					// Only keep the most recent result
					TotalTime = EndTime - StartTime;
				}

				continue;
			}

			if (bBlocking)
			{
				SCOPE_CYCLE_COUNTER(STAT_RenderQueryResultTime);
				{
					FRenderThreadIdleScope IdleScope(ERenderThreadIdleTypes::WaitingForGPUQuery);

					const bool bWaitForStart = StartQuerySyncPoint.FenceCounter >= StartQuerySyncPoint.CmdBuffer->GetFenceSignaledCounter();
					const bool bWaitForEnd = EndQuerySyncPoint.FenceCounter >= EndQuerySyncPoint.CmdBuffer->GetFenceSignaledCounter();
					if (bWaitForEnd || bWaitForStart)
					{
						// Need to submit the open command lists.
						Device->SubmitCommandsAndFlushGPU();
					}

					// CPU wait for query results to be ready.
					if (bWaitForStart && StartQuerySyncPoint.FenceCounter >= StartQuerySyncPoint.CmdBuffer->GetFenceSignaledCounter())
					{
						CmdContext->GetCommandBufferManager()->WaitForCmdBuffer(StartQuerySyncPoint.CmdBuffer);
					}
					if (bWaitForEnd && EndQuerySyncPoint.FenceCounter >= EndQuerySyncPoint.CmdBuffer->GetFenceSignaledCounter())
					{
						CmdContext->GetCommandBufferManager()->WaitForCmdBuffer(EndQuerySyncPoint.CmdBuffer);
					}
				}

				Pool->ResultsBuffer->InvalidateMappedMemory();
				StartTimeAvailability = Pool->MappedPointer[QueryStartIndex * 2 + 1];
				EndTimeAvailability = Pool->MappedPointer[QueryEndIndex * 2 + 1];
				StartTime = Pool->MappedPointer[QueryStartIndex * 2];
				EndTime = Pool->MappedPointer[QueryEndIndex * 2];

				PendingQueries.Pop();
				NumPendingQueries--;

				if (EndTime > StartTime && StartTimeAvailability && EndTimeAvailability)
				{
					TotalTime += EndTime - StartTime;
				}
			}
		}

		PreviousTime = TotalTime;
		return TotalTime;
	}

	return 0;
}

/** Start this frame of per tracking */
void FVulkanEventNodeFrame::StartFrame()
{
	EventTree.Reset();
	RootEventTiming.StartTiming();
}

/** End this frame of per tracking, but do not block yet */
void FVulkanEventNodeFrame::EndFrame()
{
	RootEventTiming.EndTiming();
}

float FVulkanEventNodeFrame::GetRootTimingResults()
{
	double RootResult = 0.0f;
	if (RootEventTiming.IsSupported())
	{
		const uint64 GPUTiming = RootEventTiming.GetTiming(true);

		// In milliseconds
		RootResult = (double)GPUTiming / (double)RootEventTiming.GetTimingFrequency();
	}

	return (float)RootResult;
}

float FVulkanEventNode::GetTiming()
{
	float Result = 0;

	if (Timing.IsSupported())
	{
		const uint64 GPUTiming = Timing.GetTiming(true);
		// In milliseconds
		Result = (double)GPUTiming / (double)Timing.GetTimingFrequency();
	}

	return Result;
}

FVulkanGPUProfiler::FVulkanGPUProfiler(FVulkanCommandListContext* InCmd, FVulkanDevice* InDevice)
	: bCommandlistSubmitted(false)
	, Device(InDevice)
	, CmdContext(InCmd)
	, LocalTracePointsQueryPool(nullptr)
	, bBeginFrame(false)
{
}

FVulkanGPUProfiler::~FVulkanGPUProfiler()
{
	if (LocalTracePointsQueryPool != nullptr)
	{
		delete LocalTracePointsQueryPool;
	}
}

void FVulkanGPUProfiler::BeginFrame()
{
#if VULKAN_SUPPORTS_GPU_CRASH_DUMPS
	if (GGPUCrashDebuggingEnabled)
	{
		static auto* CrashCollectionEnableCvar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.gpucrash.collectionenable"));
		static auto* CrashCollectionDataDepth = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.gpucrash.datadepth"));
		bTrackingGPUCrashData = CrashCollectionEnableCvar ? CrashCollectionEnableCvar->GetValueOnRenderThread() != 0 : false;
		GPUCrashDataDepth = CrashCollectionDataDepth ? CrashCollectionDataDepth->GetValueOnRenderThread() : -1;
		if (GPUCrashDataDepth == -1 || GPUCrashDataDepth > GMaxCrashBufferEntries)
		{
			if (Device->GetOptionalExtensions().HasAMDBufferMarker)
			{
				static bool bChecked = false;
				if (!bChecked)
				{
					bChecked = true;
					UE_LOG(LogVulkanRHI, Warning, TEXT("Clamping r.gpucrash.datadepth to %d"), GMaxCrashBufferEntries);
				}
				GPUCrashDataDepth = GMaxCrashBufferEntries;
			}
		}

		// Use local tracepoints if no extension is available
		if (!Device->GetOptionalExtensions().HasGPUCrashDumpExtensions() && LocalTracePointsQueryPool == nullptr)
		{
			LocalTracePointsQueryPool = new FVulkanTimingQueryPool(Device, CmdContext->GetCommandBufferManager(), GMaxCrashBufferEntries);
		}
	}
#endif

	bCommandlistSubmitted = false;
	CurrentEventNode = NULL;
	check(!bTrackingEvents);
	check(!CurrentEventNodeFrame); // this should have already been cleaned up and the end of the previous frame

	if (GGPUCrashDebuggingEnabled && !Device->GetOptionalExtensions().HasGPUCrashDumpExtensions())
	{
		VulkanRHI::vkCmdResetQueryPool(Device->GetImmediateContext().GetCommandBufferManager()->GetActiveCmdBuffer()->GetHandle(), LocalTracePointsQueryPool->GetHandle(), 0, GMaxCrashBufferEntries);

		PushPopStack.Reset();
		CrashMarkers.Reset();
		CrashMarkers.AddZeroed(GMaxCrashBufferEntries);
	}

	bBeginFrame = true;

	// latch the bools from the game thread into our private copy
	bLatchedGProfilingGPU = GTriggerGPUProfile;
	bLatchedGProfilingGPUHitches = GTriggerGPUHitchProfile;
	if (bLatchedGProfilingGPUHitches)
	{
		bLatchedGProfilingGPU = false; // we do NOT permit an ordinary GPU profile during hitch profiles
	}

	// if we are starting a hitch profile or this frame is a gpu profile, then save off the state of the draw events
	if (bLatchedGProfilingGPU || (!bPreviousLatchedGProfilingGPUHitches && bLatchedGProfilingGPUHitches))
	{
		bOriginalGEmitDrawEvents = GetEmitDrawEvents();
	}

	if (bLatchedGProfilingGPU || bLatchedGProfilingGPUHitches)
	{
		if (bLatchedGProfilingGPUHitches && GPUHitchDebounce)
		{
			// if we are doing hitches and we had a recent hitch, wait to recover
			// the reasoning is that collecting the hitch report may itself hitch the GPU
			GPUHitchDebounce--; 
		}
		else
		{
			SetEmitDrawEvents(true);  // thwart an attempt to turn this off on the game side
			bTrackingEvents = true;
			CurrentEventNodeFrame = new FVulkanEventNodeFrame(CmdContext, Device);
			CurrentEventNodeFrame->StartFrame();
		}
	}
	else if (bPreviousLatchedGProfilingGPUHitches)
	{
		// hitch profiler is turning off, clear history and restore draw events
		GPUHitchEventNodeFrames.Empty();
		SetEmitDrawEvents(bOriginalGEmitDrawEvents);
	}
	bPreviousLatchedGProfilingGPUHitches = bLatchedGProfilingGPUHitches;

	if (GetEmitDrawEvents())
	{
		PushEvent(TEXT("FRAME"), FColor(0, 255, 0, 255));
	}
}

void FVulkanGPUProfiler::EndFrameBeforeSubmit()
{
	if (GetEmitDrawEvents())
	{
		// Finish all open nodes
		// This is necessary because timestamps must be issued before SubmitDone(), and SubmitDone() happens in RHIEndDrawingViewport instead of RHIEndFrame
		while (CurrentEventNode)
		{
			UE_LOG(LogRHI, Warning, TEXT("POPPING BEFORE SUB"));
			PopEvent();
		}

		bCommandlistSubmitted = true;
	}

	// if we have a frame open, close it now.
	if (CurrentEventNodeFrame)
	{
		CurrentEventNodeFrame->EndFrame();
	}
}

void FVulkanGPUProfiler::EndFrame()
{
	EndFrameBeforeSubmit();

	check(!bTrackingEvents || bLatchedGProfilingGPU || bLatchedGProfilingGPUHitches);
	if (bLatchedGProfilingGPU)
	{
		if (bTrackingEvents)
		{
			CmdContext->GetDevice()->SubmitCommandsAndFlushGPU();

			SetEmitDrawEvents(bOriginalGEmitDrawEvents);
			UE_LOG(LogRHI, Warning, TEXT(""));
			UE_LOG(LogRHI, Warning, TEXT(""));
			check(CurrentEventNodeFrame);
			CurrentEventNodeFrame->DumpEventTree();
			GTriggerGPUProfile = false;
			bLatchedGProfilingGPU = false;
		}
	}
	else if (bLatchedGProfilingGPUHitches)
	{
		UE_LOG(LogRHI, Warning, TEXT("GPU hitch tracking not implemented on Vulkan"));
	}
	bTrackingEvents = false;
	if (CurrentEventNodeFrame)
	{
		delete CurrentEventNodeFrame;
		CurrentEventNodeFrame = nullptr;
	}

	bBeginFrame = false;
}

#if VULKAN_SUPPORTS_GPU_CRASH_DUMPS
void FVulkanGPUProfiler::PushMarkerForCrash(VkCommandBuffer CmdBuffer, VkBuffer DestBuffer, const TCHAR* Name)
{
	if (!Device->GetOptionalExtensions().HasGPUCrashDumpExtensions() && !bBeginFrame)
	{
		// If using local trace points, ignore any markers pushed before begin frame or after end frame.
		return;
	}

	uint32 CRC = 0;
	if (GPUCrashDataDepth < 0 || PushPopStack.Num() < GPUCrashDataDepth)
	{
		CRC = FCrc::StrCrc32<TCHAR>(Name);

		if (CachedStrings.Num() > 10000)
		{
			CachedStrings.Empty(10000);
			CachedStrings.Emplace(EventDeepCRC, EventDeepString);
		}

		if (CachedStrings.Find(CRC) == nullptr)
		{
			CachedStrings.Emplace(CRC, FString(Name));
		}
	}
	else
	{
		CRC = EventDeepCRC;
	}

	PushPopStack.Push(CRC);
	FVulkanPlatform::WriteCrashMarker(Device->GetOptionalExtensions(), CmdBuffer, DestBuffer, TArrayView<uint32>(PushPopStack), true);

	if (GGPUCrashDebuggingEnabled && !Device->GetOptionalExtensions().HasGPUCrashDumpExtensions())
	{
		VulkanRHI::vkCmdWriteTimestamp(CmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, LocalTracePointsQueryPool->GetHandle(), PushPopStack.Num() - 1);
	}
}

void FVulkanGPUProfiler::PopMarkerForCrash(VkCommandBuffer CmdBuffer, VkBuffer DestBuffer)
{
	if (!Device->GetOptionalExtensions().HasGPUCrashDumpExtensions() && !bBeginFrame)
	{
		// If using local trace points, ignore any markers popped before begin frame or after end frame.
		return;
	}

	if (PushPopStack.Num() > 0)
	{
		if (Device->GetOptionalExtensions().HasGPUCrashDumpExtensions())
		{
			PushPopStack.Pop(EAllowShrinking::No);
			FVulkanPlatform::WriteCrashMarker(Device->GetOptionalExtensions(), CmdBuffer, DestBuffer, TArrayView<uint32>(PushPopStack), false);
		}
		else if (GGPUCrashDebuggingEnabled)
		{
			VulkanRHI::vkGetQueryPoolResults(Device->GetInstanceHandle(), LocalTracePointsQueryPool->GetHandle(), 0, PushPopStack.Num(), sizeof(uint64) * GMaxCrashBufferEntries, CrashMarkers.GetData(), sizeof(uint64), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
		}
	}
}

void FVulkanGPUProfiler::DumpCrashMarkers(void* BufferData)
{
	if (Device->GetOptionalExtensions().HasAMDBufferMarker)
	{
		uint32* Entries = (uint32*)BufferData;
		uint32 NumCRCs = *Entries++;
		for (uint32 Index = 0; Index < NumCRCs; ++Index)
		{
			const FString* Frame = CachedStrings.Find(*Entries);
			UE_LOG(LogVulkanRHI, Error, TEXT("[VK_AMD_buffer_info] %i: %s (CRC 0x%x)"), Index, Frame ? *(*Frame) : TEXT("<undefined>"), *Entries);
			++Entries;
		}
	}

	if (Device->GetOptionalExtensions().HasNVDiagnosticCheckpoints)
	{
		struct FCheckpointDataNV : public VkCheckpointDataNV
		{
			FCheckpointDataNV()
			{
				ZeroVulkanStruct(*this, VK_STRUCTURE_TYPE_CHECKPOINT_DATA_NV);
			}
		};
		TArray<FCheckpointDataNV> Data;
		uint32 Num = 0;
		VkQueue QueueHandle = Device->GetGraphicsQueue()->GetHandle();
		VulkanDynamicAPI::vkGetQueueCheckpointDataNV(QueueHandle, &Num, nullptr);
		if (Num > 0)
		{
			Data.AddDefaulted(Num);
			VulkanDynamicAPI::vkGetQueueCheckpointDataNV(QueueHandle, &Num, &Data[0]);
			check(Num == Data.Num());
			for (uint32 Index = 0; Index < Num; ++Index)
			{
				check(Data[Index].sType == VK_STRUCTURE_TYPE_CHECKPOINT_DATA_NV);
				uint32 Value = (uint32)(size_t)Data[Index].pCheckpointMarker;
				const FString* Frame = CachedStrings.Find(Value);
				UE_LOG(LogVulkanRHI, Error, TEXT("[VK_NV_device_diagnostic_checkpoints] %i: Stage %s (0x%08x), %s (CRC 0x%x)"), 
					Index, VK_TYPE_TO_STRING(VkPipelineStageFlagBits, Data[Index].stage), Data[Index].stage, Frame ? *(*Frame) : TEXT("<undefined>"), Value);
			}
			GLog->Panic();
		}
	}

	if (!Device->GetOptionalExtensions().HasGPUCrashDumpExtensions())
	{
		UE_LOG(LogVulkanRHI, Warning, TEXT("Printing trace points."));

		for (int32 i = 0; i < PushPopStack.Num(); ++i)
		{
			const FString* InsertedFrame = CachedStrings.Find(PushPopStack[i]);
			const FString FrameName = InsertedFrame ? *InsertedFrame : TEXT("<undefined>");

			UE_LOG(LogVulkanRHI, Warning, TEXT("[gpu_crash_markers] %s"), (CrashMarkers[i] != 0) ? *FrameName : TEXT("unavailable"));
		}

		GLog->Panic();
	}
}
#endif // VULKAN_SUPPORTS_GPU_CRASH_DUMPS

#if NV_AFTERMATH
void AftermathGpuCrashDumpCallback(const void* CrashDump, const uint32 CrashDumpSize, void* UserData)
{
	const FString DateTimeString = FDateTime::Now().ToString();

	// Create a GPU crash dump decoder object for the GPU crash dump.
	GFSDK_Aftermath_GpuCrashDump_Decoder Decoder = {};
	{
		GFSDK_Aftermath_Result Result = GFSDK_Aftermath_GpuCrashDump_CreateDecoder(GFSDK_Aftermath_Version_API, CrashDump, CrashDumpSize, &Decoder);
		if (Result != GFSDK_Aftermath_Result_Success)
		{
			UE_LOG(LogVulkanRHI, Warning, TEXT("Unable to initialize create Aftermath decoder (Result %d, CrashDumpSize=%d)"), (int32)Result, CrashDumpSize);
		}
	}

	// Use the decoder object to read basic information, like application
	// name, PID, etc. from the GPU crash dump.
	GFSDK_Aftermath_GpuCrashDump_BaseInfo BaseInfo = {};
	{
		GFSDK_Aftermath_Result Result = GFSDK_Aftermath_GpuCrashDump_GetBaseInfo(Decoder, &BaseInfo);
		if (Result != GFSDK_Aftermath_Result_Success)
		{
			UE_LOG(LogVulkanRHI, Warning, TEXT("Unable to get Aftermath base info (Result %d)"), (int32)Result);
		}
	}

	{
		const FString Filename = FPaths::ProjectLogDir() / FString::Printf(TEXT("vulkan.%s.nv-gpudmp"), *DateTimeString);
		FArchive* Writer = IFileManager::Get().CreateFileWriter(*Filename);
		if (Writer)
		{
			Writer->Serialize((void*)CrashDump, CrashDumpSize);
			Writer->Close();
			UE_LOG(LogVulkanRHI, Warning, TEXT("Generated Aftermath crash dump at '%s'"), *Filename);
		}
	}

	// Decode the crash dump to a JSON string.
	// Step 1: Generate the JSON and get the size.
	uint32 JsonSize = 0;
	{
		GFSDK_Aftermath_Result Result = GFSDK_Aftermath_GpuCrashDump_GenerateJSON(
			Decoder,
			GFSDK_Aftermath_GpuCrashDumpDecoderFlags_ALL_INFO,
			GFSDK_Aftermath_GpuCrashDumpFormatterFlags_NONE,
			nullptr/*ShaderDebugInfoLookupCallback*/,
			nullptr/*ShaderLookupCallback*/,
			nullptr/*ShaderSourceDebugInfoLookupCallback*/,
			UserData,
			&JsonSize);

			if (Result == GFSDK_Aftermath_Result_Success)
			{
				// Step 2: Allocate a buffer and fetch the generated JSON.
				TArray<ANSICHAR> Json;
				Json.AddZeroed(JsonSize);
				GFSDK_Aftermath_Result ResultJson = GFSDK_Aftermath_GpuCrashDump_GetJSON(Decoder, (uint32)Json.Num(), Json.GetData());
				if (ResultJson == GFSDK_Aftermath_Result_Success)
				{
					const FString Filename = FPaths::ProjectLogDir() / FString::Printf(TEXT("vulkan.%s.nv-gpudmp.json"), *DateTimeString);
					FArchive* Writer = IFileManager::Get().CreateFileWriter(*Filename);
					if (Writer)
					{
						Writer->Serialize((void*)Json.GetData(), Json.Num());
						Writer->Close();
						UE_LOG(LogVulkanRHI, Warning, TEXT("Generated Aftermath crash dump json at '%s'"), *Filename);
					}
				}
				else
				{
					UE_LOG(LogVulkanRHI, Warning, TEXT("Unable to get Aftermath JSON (Result %d)"), (int32)Result);
				}
			}
			else
			{
				UE_LOG(LogVulkanRHI, Warning, TEXT("Unable to get Aftermath JSON Size (Result %d)"), (int32)Result);
			}
	}
}

void AftermathShaderDebugInfoCallback(const void* pShaderDebugInfo, const uint32 shaderDebugInfoSize, void* pUserData)
{
}

void AftermathCrashDumpDescriptionCallback(PFN_GFSDK_Aftermath_AddGpuCrashDumpDescription AddDescription, void* pUserData)
{
	// Add some basic description about the crash. This is called after the GPU crash happens, but before
	// the actual GPU crash dump callback. The provided data is included in the crash dump and can be
	// retrieved using GFSDK_Aftermath_GpuCrashDump_GetDescription().
	FTCHARToUTF8 ProjectNameConverter(FApp::GetProjectName());
	FTCHARToUTF8 VersionConverter(FApp::GetBuildVersion());
	AddDescription(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationName, ProjectNameConverter.Get());
	AddDescription(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_ApplicationVersion, VersionConverter.Get());
	AddDescription(GFSDK_Aftermath_GpuCrashDumpDescriptionKey_UserDefined, "Vulkan GPU crash");
}

void AftermathResolveMarkerCallback(const void* pMarker, void* pUserData, void** resolvedMarkerData, uint32_t* markerSize)
{
#if VULKAN_SUPPORTS_NV_DIAGNOSTICS
	FVulkanDevice* VulkanDevice = (FVulkanDevice*)pUserData;
	if (VulkanDevice->GetOptionalExtensions().HasNVDiagnosticCheckpoints)
	{
		const uint32 Value = (uint32)(size_t)pMarker;
		const FString* MarkerName = VulkanDevice->GetImmediateContext().GetGPUProfiler().CachedStrings.Find(Value);
		UE_LOG(LogVulkanRHI, Display, TEXT("[AftermathResolveMarkerCallback] Requested %u [%s]"), Value, MarkerName ? *(*MarkerName) : TEXT("<undefined>"));
		if (MarkerName && !MarkerName->IsEmpty() && resolvedMarkerData && markerSize)
		{
			const TArray<TCHAR, FString::AllocatorType>& CharArray = MarkerName->GetCharArray();
			(*resolvedMarkerData) = (void*)CharArray.GetData();
			(*markerSize) = CharArray.Num() * CharArray.GetTypeSize();
		}
	}
#endif // VULKAN_SUPPORTS_NV_DIAGNOSTICS
}
#endif

namespace VulkanRHI
{
	VkBuffer CreateBuffer(FVulkanDevice* InDevice, VkDeviceSize Size, VkBufferUsageFlags BufferUsageFlags, VkMemoryRequirements& OutMemoryRequirements)
	{
		VkDevice Device = InDevice->GetInstanceHandle();
		VkBuffer Buffer = VK_NULL_HANDLE;

		VkBufferCreateInfo BufferCreateInfo;
		ZeroVulkanStruct(BufferCreateInfo, VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
		BufferCreateInfo.size = Size;
		BufferCreateInfo.usage = BufferUsageFlags;
		VERIFYVULKANRESULT_EXPANDED(VulkanRHI::vkCreateBuffer(Device, &BufferCreateInfo, VULKAN_CPU_ALLOCATOR, &Buffer));

		VulkanRHI::vkGetBufferMemoryRequirements(Device, Buffer, &OutMemoryRequirements);

		return Buffer;
	}

	void CheckDeviceFault(FVulkanDevice* InDevice)
	{
		if (InDevice->GetOptionalExtensions().HasEXTDeviceFault)
		{
			const VkDevice DeviceHandle = InDevice->GetInstanceHandle();
			VkResult Result;

			VkDeviceFaultCountsEXT FaultCounts;
			ZeroVulkanStruct(FaultCounts, VK_STRUCTURE_TYPE_DEVICE_FAULT_COUNTS_EXT);
			Result = vkGetDeviceFaultInfoEXT(DeviceHandle, &FaultCounts, nullptr);
			if (Result == VK_SUCCESS)
			{
				VkDeviceFaultInfoEXT FaultInfo;
				ZeroVulkanStruct(FaultInfo, VK_STRUCTURE_TYPE_DEVICE_FAULT_INFO_EXT);

				TArray<VkDeviceFaultAddressInfoEXT> AddressInfos;
				AddressInfos.SetNumZeroed(FaultCounts.addressInfoCount);
				FaultInfo.pAddressInfos = AddressInfos.GetData();

				TArray<VkDeviceFaultVendorInfoEXT> VendorInfos;
				VendorInfos.SetNumZeroed(FaultCounts.vendorInfoCount);
				FaultInfo.pVendorInfos = VendorInfos.GetData();

				TArray<uint8> VendorBinaryData;
				VendorBinaryData.SetNumZeroed(FaultCounts.vendorBinarySize);
				FaultInfo.pVendorBinaryData = VendorBinaryData.GetData();

				Result = vkGetDeviceFaultInfoEXT(DeviceHandle, &FaultCounts, &FaultInfo);
				if (Result == VK_SUCCESS)
				{
					// :todo-jn: match these up to resources

					auto ReportAddrToStr = [&AddressInfos]() {
						FString AddrStr;
						for (const VkDeviceFaultAddressInfoEXT& AddrInfo : AddressInfos)
						{
							const uint64_t LowerAddress = (AddrInfo.reportedAddress & ~(AddrInfo.addressPrecision - 1));
							const uint64_t UpperAddress = (AddrInfo.reportedAddress | (AddrInfo.addressPrecision - 1));

							AddrStr += FString::Printf(TEXT("\n    - %s : 0x%016llX (range:0x%016llX-0x%016llX)"), 
								VK_TYPE_TO_STRING(VkDeviceFaultAddressTypeEXT, AddrInfo.addressType),
								AddrInfo.reportedAddress, LowerAddress, UpperAddress);
						}
						return AddrStr;
					};

					auto ReportVendorToStr = [&VendorInfos]() {
						FString VendorStr;
						for (const VkDeviceFaultVendorInfoEXT& VendorInfo : VendorInfos)
						{
							VendorStr += FString::Printf(TEXT("\n    - %s (code:0x%016llX data:0x%016llX)"),
								StringCast<TCHAR>((UTF8CHAR*)VendorInfo.description).Get(), VendorInfo.vendorFaultCode, VendorInfo.vendorFaultData);
						}
						return VendorStr;
					};

					UE_LOG(LogVulkanRHI, Error, 
						TEXT("\nDEVICE FAULT REPORT:\n")
						TEXT("* Description: %s\n")
						TEXT("* Address Info: %s\n")
						TEXT("* Vendor Info: %s\n")
						TEXT("* Vendor Binary Size: %llu\n"),

						StringCast<TCHAR>((UTF8CHAR*)FaultInfo.description).Get(), *ReportAddrToStr(), *ReportVendorToStr(), FaultCounts.vendorBinarySize
					);
				}
			}
		}
	}

	/**
	 * Checks that the given result isn't a failure.  If it is, the application exits with an appropriate error message.
	 * @param	Result - The result code to check
	 * @param	Code - The code which yielded the result.
	 * @param	VkFunction - Tested function name.
	 * @param	Filename - The filename of the source file containing Code.
	 * @param	Line - The line number of Code within Filename.
	 */
	void VerifyVulkanResult(VkResult Result, const ANSICHAR* VkFunction, const ANSICHAR* Filename, uint32 Line)
	{
		bool bDumpMemory = false;
		FString ErrorString;
		switch (Result)
		{
#define VKERRORCASE(x)	case x: ErrorString = TEXT(#x)
		VKERRORCASE(VK_NOT_READY); break;
		VKERRORCASE(VK_TIMEOUT); break;
		VKERRORCASE(VK_EVENT_SET); break;
		VKERRORCASE(VK_EVENT_RESET); break;
		VKERRORCASE(VK_INCOMPLETE); break;
		VKERRORCASE(VK_ERROR_OUT_OF_HOST_MEMORY); bDumpMemory = true; break;
		VKERRORCASE(VK_ERROR_OUT_OF_DEVICE_MEMORY); bDumpMemory = true; break;
		VKERRORCASE(VK_ERROR_INITIALIZATION_FAILED); break;
		VKERRORCASE(VK_ERROR_DEVICE_LOST); GIsGPUCrashed = true; break;
		VKERRORCASE(VK_ERROR_MEMORY_MAP_FAILED); break;
		VKERRORCASE(VK_ERROR_LAYER_NOT_PRESENT); break;
		VKERRORCASE(VK_ERROR_EXTENSION_NOT_PRESENT); break;
		VKERRORCASE(VK_ERROR_FEATURE_NOT_PRESENT); break;
		VKERRORCASE(VK_ERROR_INCOMPATIBLE_DRIVER); break;
		VKERRORCASE(VK_ERROR_TOO_MANY_OBJECTS); break;
		VKERRORCASE(VK_ERROR_FORMAT_NOT_SUPPORTED); break;
		VKERRORCASE(VK_ERROR_SURFACE_LOST_KHR); break;
		VKERRORCASE(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR); break;
		VKERRORCASE(VK_SUBOPTIMAL_KHR); break;
		VKERRORCASE(VK_ERROR_OUT_OF_DATE_KHR); break;
		VKERRORCASE(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR); break;
		VKERRORCASE(VK_ERROR_VALIDATION_FAILED_EXT); break;
		VKERRORCASE(VK_ERROR_INVALID_SHADER_NV); break;
		VKERRORCASE(VK_ERROR_FRAGMENTED_POOL); break;
		VKERRORCASE(VK_ERROR_OUT_OF_POOL_MEMORY_KHR); break;
		VKERRORCASE(VK_ERROR_INVALID_EXTERNAL_HANDLE_KHR); break;
		VKERRORCASE(VK_ERROR_NOT_PERMITTED_EXT); break;
#undef VKERRORCASE
		default:
			break;
		}

#if VULKAN_HAS_DEBUGGING_ENABLED
		if (Result == VK_ERROR_VALIDATION_FAILED_EXT)
		{
			if (GValidationCvar.GetValueOnRenderThread() == 0)
			{
				UE_LOG(LogVulkanRHI, Fatal, TEXT("Failed with Validation error. Try running with r.Vulkan.EnableValidation=1 or -vulkandebug to get information from the validation layers."));
			}
		}
#endif

		UE_LOG(LogVulkanRHI, Error, TEXT("%s failed, VkResult=%d\n at %s:%u \n with error %s"),
			ANSI_TO_TCHAR(VkFunction), (int32)Result, ANSI_TO_TCHAR(Filename), Line, *ErrorString);

		if (GIsGPUCrashed)
		{
			FVulkanDevice* Device = GVulkanRHI->GetDevice();

#if VULKAN_SUPPORTS_GPU_CRASH_DUMPS
			if (GGPUCrashDebuggingEnabled)
			{
				Device->GetImmediateContext().GetGPUProfiler().DumpCrashMarkers(Device->GetCrashMarkerMappedPointer());
			}
#endif

			CheckDeviceFault(Device);

			// Make sure we wait on the Aftermath crash dump before we crash.
#if NV_AFTERMATH
			if (GGPUCrashDebuggingEnabled && GVulkanNVAftermathModuleLoaded)
			{
				GFSDK_Aftermath_CrashDump_Status AftermathStatus{};
				GFSDK_Aftermath_GetCrashDumpStatus(&AftermathStatus);
				if (AftermathStatus != GFSDK_Aftermath_CrashDump_Status_Unknown)
				{
					const float StartTime = FPlatformTime::Seconds();
					const float EndTime = StartTime + GVulkanNVAfterMathDumpWaitTime;
					while (
						((AftermathStatus == GFSDK_Aftermath_CrashDump_Status_NotStarted)
						|| (AftermathStatus != GFSDK_Aftermath_CrashDump_Status_CollectingDataFailed
						&& AftermathStatus != GFSDK_Aftermath_CrashDump_Status_Finished))
						&& FPlatformTime::Seconds() < EndTime)
					{
						FPlatformProcess::Sleep(0.01f);
						GFSDK_Aftermath_GetCrashDumpStatus(&AftermathStatus);
					}
				}
				UE_LOG(LogVulkanRHI, Warning, TEXT("Final Aftermath status was %d."), (int32)AftermathStatus);
			}
#endif
		}

#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
		if (bDumpMemory)
		{
			GVulkanRHI->DumpMemory();
		}
#endif

		UE_LOG(LogVulkanRHI, Fatal, TEXT("%s failed, VkResult=%d\n at %s:%u \n with error %s"),
			ANSI_TO_TCHAR(VkFunction), (int32)Result, ANSI_TO_TCHAR(Filename), Line, *ErrorString);
	}
}


DEFINE_STAT(STAT_VulkanNumPSOs);
DEFINE_STAT(STAT_VulkanNumGraphicsPSOs);
DEFINE_STAT(STAT_VulkanNumPSOLRU);
DEFINE_STAT(STAT_VulkanNumPSOLRUSize);
DEFINE_STAT(STAT_VulkanPSOLookupTime);
DEFINE_STAT(STAT_VulkanPSOCreationTime);
DEFINE_STAT(STAT_VulkanPSOHeaderInitTime);
DEFINE_STAT(STAT_VulkanPSOVulkanCreationTime);
DEFINE_STAT(STAT_VulkanNumComputePSOs);
DEFINE_STAT(STAT_VulkanPSOKeyMemory);


DEFINE_STAT(STAT_VulkanDrawCallTime);
DEFINE_STAT(STAT_VulkanDispatchCallTime);
DEFINE_STAT(STAT_VulkanDrawCallPrepareTime);
DEFINE_STAT(STAT_VulkanCustomPresentTime);
DEFINE_STAT(STAT_VulkanDispatchCallPrepareTime);
DEFINE_STAT(STAT_VulkanGetOrCreatePipeline);
DEFINE_STAT(STAT_VulkanGetDescriptorSet);
DEFINE_STAT(STAT_VulkanPipelineBind);
DEFINE_STAT(STAT_VulkanNumCmdBuffers);
DEFINE_STAT(STAT_VulkanNumRenderPasses);
DEFINE_STAT(STAT_VulkanNumFrameBuffers);
DEFINE_STAT(STAT_VulkanNumBufferViews);
DEFINE_STAT(STAT_VulkanNumImageViews);
DEFINE_STAT(STAT_VulkanNumPhysicalMemAllocations);
DEFINE_STAT(STAT_VulkanTempFrameAllocationBuffer);
DEFINE_STAT(STAT_VulkanDynamicVBSize);
DEFINE_STAT(STAT_VulkanDynamicIBSize);
DEFINE_STAT(STAT_VulkanDynamicVBLockTime);
DEFINE_STAT(STAT_VulkanDynamicIBLockTime);
DEFINE_STAT(STAT_VulkanUPPrepTime);
DEFINE_STAT(STAT_VulkanUniformBufferCreateTime);
DEFINE_STAT(STAT_VulkanApplyDSUniformBuffers);
DEFINE_STAT(STAT_VulkanApplyPackedUniformBuffers);
DEFINE_STAT(STAT_VulkanBarrierTime);
DEFINE_STAT(STAT_VulkanSRVUpdateTime);
DEFINE_STAT(STAT_VulkanUAVUpdateTime);
DEFINE_STAT(STAT_VulkanDeletionQueue);
DEFINE_STAT(STAT_VulkanQueueSubmit);
DEFINE_STAT(STAT_VulkanQueuePresent);
DEFINE_STAT(STAT_VulkanNumQueries);
DEFINE_STAT(STAT_VulkanNumQueryPools);
DEFINE_STAT(STAT_VulkanWaitQuery);
DEFINE_STAT(STAT_VulkanWaitFence);
DEFINE_STAT(STAT_VulkanResetQuery);
DEFINE_STAT(STAT_VulkanWaitSwapchain);
DEFINE_STAT(STAT_VulkanAcquireBackBuffer);
DEFINE_STAT(STAT_VulkanStagingBuffer);
DEFINE_STAT(STAT_VulkanVkCreateDescriptorPool);
DEFINE_STAT(STAT_VulkanNumDescPools);
DEFINE_STAT(STAT_VulkanUpdateUniformBuffers);
DEFINE_STAT(STAT_VulkanUpdateUniformBuffersRename);
#if VULKAN_ENABLE_AGGRESSIVE_STATS
DEFINE_STAT(STAT_VulkanUpdateDescriptorSets);
DEFINE_STAT(STAT_VulkanNumUpdateDescriptors);
DEFINE_STAT(STAT_VulkanNumDescSets);
DEFINE_STAT(STAT_VulkanSetUniformBufferTime);
DEFINE_STAT(STAT_VulkanVkUpdateDS);
DEFINE_STAT(STAT_VulkanBindVertexStreamsTime);
#endif
DEFINE_STAT(STAT_VulkanNumDescSetsTotal);
