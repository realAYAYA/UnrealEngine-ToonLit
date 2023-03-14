// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
D3D12CommandContext.cpp: RHI  Command Context implementation.
=============================================================================*/

#include "D3D12RHIPrivate.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include "amd_ags.h"
#include "Windows/HideWindowsPlatformTypes.h"
#endif

#include "D3D12RayTracing.h"

int32 GD3D12MaxCommandsPerCommandList = 10000;
static FAutoConsoleVariableRef CVarMaxCommandsPerCommandList(
	TEXT("D3D12.MaxCommandsPerCommandList"),
	GD3D12MaxCommandsPerCommandList,
	TEXT("Flush command list to GPU after certain amount of enqueued commands (draw, dispatch, copy, ...) (default value 10000)"),
	ECVF_RenderThreadSafe
);

// We don't yet have a way to auto-detect that the Radeon Developer Panel is running
// with profiling enabled, so for now, we have to manually toggle this console var.
// It needs to be set before device creation, so it's read only.
int32 GEmitRgpFrameMarkers = 0;
static FAutoConsoleVariableRef CVarEmitRgpFrameMarkers(
	TEXT("D3D12.EmitRgpFrameMarkers"),
	GEmitRgpFrameMarkers,
	TEXT("Enables/Disables frame markers for AMD's RGP tool."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe
);

// jhoerner TODO 10/4/2022:  This setting is a hack to improve performance by reverting cross GPU transfer synchronization behavior to
// what it was in 5.0, at a cost in validation correctness (D3D debug errors related to using a cross GPU transferred resource in an
// incorrect transition state, or when possibly still being written).  In practice, these errors haven't caused artifacts or stability
// issues, but if you run into an artifact suspected to be related to a cross GPU transfer, or want to run with validation for
// debugging, you can disable the hack.  A future refactor in 5.2 will clean this up and provide validation correctness without any
// performance loss.
//
bool GD3D12UnsafeCrossGPUTransfers = true;
static FAutoConsoleVariableRef CVarD3D12UnsafeCrossGPUTransfers(
	TEXT("D3D12.UnsafeCrossGPUTransfers"),
	GD3D12UnsafeCrossGPUTransfers,
	TEXT("Disables cross GPU synchronization correctness, for a gain in performance (Default: true)."),
	ECVF_RenderThreadSafe
);

FD3D12CommandContextBase::FD3D12CommandContextBase(class FD3D12Adapter* InParentAdapter, FRHIGPUMask InGPUMask)
	: FD3D12AdapterChild(InParentAdapter)
	, GPUMask(InGPUMask)
	, PhysicalGPUMask(InGPUMask)
{
}

static D3D12_RESOURCE_STATES GetValidResourceStates(ED3D12QueueType CommandListType)
{
	// For reasons, we can't just list the allowed states, we have to list the disallowed states.
	// For reference on allowed/disallowed states, see:
	//    https://microsoft.github.io/DirectX-Specs/d3d/CPUEfficiency.html#state-support-by-command-list-type

	const D3D12_RESOURCE_STATES DisallowedDirectStates =
		static_cast<D3D12_RESOURCE_STATES>(0);

	const D3D12_RESOURCE_STATES DisallowedComputeStates =
		DisallowedDirectStates |
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER |
		D3D12_RESOURCE_STATE_INDEX_BUFFER |
		D3D12_RESOURCE_STATE_RENDER_TARGET |
		D3D12_RESOURCE_STATE_DEPTH_WRITE |
		D3D12_RESOURCE_STATE_DEPTH_READ |
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
		D3D12_RESOURCE_STATE_STREAM_OUT |
		D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT |
		D3D12_RESOURCE_STATE_RESOLVE_DEST |
		D3D12_RESOURCE_STATE_RESOLVE_SOURCE;

	const D3D12_RESOURCE_STATES DisallowedCopyStates =
		DisallowedComputeStates |
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS |
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;


	if (CommandListType == ED3D12QueueType::Copy)
	{
		return ~DisallowedCopyStates;
	}

	if (CommandListType == ED3D12QueueType::Async)
	{
		return ~DisallowedComputeStates;
	}

	return ~DisallowedDirectStates;
}

FD3D12CommandContext::FD3D12CommandContext(FD3D12Device* InParent, ED3D12QueueType QueueType, bool InIsDefaultContext)
	: FD3D12ContextCommon(InParent, QueueType, InIsDefaultContext)
	, FD3D12CommandContextBase(InParent->GetParentAdapter(), InParent->GetGPUMask())
	, FD3D12DeviceChild(InParent)
	, ConstantsAllocator(InParent, InParent->GetGPUMask())
	, StateCache(*this, InParent->GetGPUMask())
	, ValidResourceStates(GetValidResourceStates(QueueType))
	, VSConstantBuffer(InParent, ConstantsAllocator)
	, MSConstantBuffer(InParent, ConstantsAllocator)
	, ASConstantBuffer(InParent, ConstantsAllocator)
	, PSConstantBuffer(InParent, ConstantsAllocator)
	, GSConstantBuffer(InParent, ConstantsAllocator)
	, CSConstantBuffer(InParent, ConstantsAllocator)
{
	StaticUniformBuffers.AddZeroed(FUniformBufferStaticSlotRegistry::Get().GetSlotCount());
	ClearState();
}

FD3D12CommandContext::~FD3D12CommandContext()
{
	ClearState();
}


/** Write out the event stack to the bread crumb resource if available */
void FD3D12CommandContext::WriteGPUEventStackToBreadCrumbData(bool bBeginEvent)
{
	// Write directly to command list if breadcrumb resource is available
	TUniquePtr<FD3D12DiagnosticBuffer>& DiagnosticBuffer = Device->GetQueue(QueueType).DiagnosticBuffer;
	if (!DiagnosticBuffer)
		return;

	if (!GraphicsCommandList2())
		return;

	// Find the max parameter count from the resource
	const int32 MaxParameterCount = DiagnosticBuffer->BreadCrumbsSize / sizeof(uint32);

	// allocate the parameters on the stack if smaller than 4K
	int32 ParameterCount = GPUEventStack.Num() < (MaxParameterCount - 2) ? GPUEventStack.Num() + 2 : MaxParameterCount;
	size_t MemSize = ParameterCount * (sizeof(D3D12_WRITEBUFFERIMMEDIATE_PARAMETER) + sizeof(D3D12_WRITEBUFFERIMMEDIATE_PARAMETER));
	const bool bAllocateOnStack = (MemSize < 4096);
	void* Mem = bAllocateOnStack ? FMemory_Alloca(MemSize) : FMemory::Malloc(MemSize);

	if (Mem)
	{
		D3D12_WRITEBUFFERIMMEDIATE_PARAMETER* Parameters = (D3D12_WRITEBUFFERIMMEDIATE_PARAMETER*)Mem;
		D3D12_WRITEBUFFERIMMEDIATE_MODE* Modes = (D3D12_WRITEBUFFERIMMEDIATE_MODE*)(Parameters + ParameterCount);
		for (int i = 0; i < ParameterCount; ++i)
		{
			Parameters[i].Dest = DiagnosticBuffer->Resource->GetGPUVirtualAddress() + 4 * i;

		#if 1 // This is more accurate, but may have a higher theoretical GPU overhead
			if (bBeginEvent)
			{
				// The write operation is guaranteed to occur after all preceding commands in the command stream have started, including previous.
				// We use this mode because we want to know which ops have started on the GPU.
				Modes[i] = D3D12_WRITEBUFFERIMMEDIATE_MODE_MARKER_IN;
			}
			else
			{
				// The write operation is deferred until all previous commands in the command stream have completed through the GPU pipeline, including previous WriteBufferImmediate operations.
				// We want this mode when ending breadcrumb scopes because the GPU might start the next scope before we hit a problem in the current one in some cases (when there are no barriers).
				Modes[i] = D3D12_WRITEBUFFERIMMEDIATE_MODE_MARKER_OUT;
			}
		#else // This is less accurate (we don't know for sure if the event has finished), but has lower theoretical GPU overhead
			Modes[i] = D3D12_WRITEBUFFERIMMEDIATE_MODE_MARKER_IN;
		#endif

			// Write event stack count first
			if (i == 0)
			{
				Parameters[i].Value = GPUEventStack.Num();
			}
			// Then if it's the begin or end event
			else if (i == 1)
			{
				Parameters[i].Value = bBeginEvent ? 1 : 0;
			}
			// Otherwise the actual stack value
			else
			{
				Parameters[i].Value = GPUEventStack[i - 2];
			}
		}
		GraphicsCommandList2()->WriteBufferImmediate(ParameterCount, Parameters, Modes);
	}

	if (!bAllocateOnStack)
	{
		FMemory::Free(Mem);
	}
}


void FD3D12CommandContext::RHIPushEvent(const TCHAR* Name, FColor Color)
{
	D3D12RHI::FD3DGPUProfiler& GPUProfiler = GetParentDevice()->GetGPUProfiler();

	// forward event to profiler if it's the default context
	if (IsDefaultContext() && !IsAsyncComputeContext())
	{
		GPUProfiler.PushEvent(Name, Color);
	}

	// If we are tracking GPU crashes then retrieve the hash of the name and track in the command list somewhere
	if (GPUProfiler.bTrackingGPUCrashData)
	{
		// Get the CRC of the event (handle case when depth is too big)
		const TCHAR* EventName = (GPUProfiler.GPUCrashDataDepth < 0 || GPUEventStack.Num() < GPUProfiler.GPUCrashDataDepth) ? Name : *D3D12RHI::FD3DGPUProfiler::EventDeepString;
		uint32 CRC = GPUProfiler.GetOrAddEventStringHash(Name);

		GPUEventStack.Push(CRC);
		WriteGPUEventStackToBreadCrumbData(true);

#if NV_AFTERMATH
		// Only track aftermath for default context?
		if (IsDefaultContext() && GDX12NVAfterMathEnabled && GDX12NVAfterMathMarkers)
			GFSDK_Aftermath_SetEventMarker(AftermathHandle(), &GPUEventStack[0], GPUEventStack.Num() * sizeof(uint32));
#endif // NV_AFTERMATH		
	}

#if PLATFORM_WINDOWS
	AGSContext* const AmdAgsContext = FD3D12DynamicRHI::GetD3DRHI()->GetAmdAgsContext();
	if (GEmitRgpFrameMarkers && AmdAgsContext)
	{
		agsDriverExtensionsDX12_PushMarker(AmdAgsContext, GraphicsCommandList().Get(), TCHAR_TO_ANSI(Name));
	}
#endif

#if USE_PIX
	if (FD3D12DynamicRHI::GetD3DRHI()->IsPixEventEnabled())
	{
		PIXBeginEvent(GraphicsCommandList().Get(), PIX_COLOR(Color.R, Color.G, Color.B), Name);
	}
#endif // USE_PIX
}

void FD3D12CommandContext::RHIPopEvent()
{
	D3D12RHI::FD3DGPUProfiler& GPUProfiler = GetParentDevice()->GetGPUProfiler();

	if (IsDefaultContext() && !IsAsyncComputeContext())
	{
		GPUProfiler.PopEvent();
	}

	if (GPUProfiler.bTrackingGPUCrashData)
	{
		WriteGPUEventStackToBreadCrumbData(false);

		// need to look for unbalanced push/pop
		if (GPUEventStack.Num() > 0)
		{
			GPUEventStack.Pop(false);
		}
	}

#if PLATFORM_WINDOWS
	AGSContext* const AmdAgsContext = FD3D12DynamicRHI::GetD3DRHI()->GetAmdAgsContext();
	if (GEmitRgpFrameMarkers && AmdAgsContext)
	{
		agsDriverExtensionsDX12_PopMarker(AmdAgsContext, GraphicsCommandList().Get());
	}
#endif

#if USE_PIX
	if (FD3D12DynamicRHI::GetD3DRHI()->IsPixEventEnabled())
	{
		PIXEndEvent(GraphicsCommandList().Get());
	}
#endif
}

FD3D12ContextCommon::FD3D12ContextCommon(FD3D12Device* Device, ED3D12QueueType QueueType, bool bIsDefaultContext)
	: Device(Device)
	, QueueType(QueueType)
	, bIsDefaultContext(bIsDefaultContext)
	, TimestampQueries(Device, QueueType, D3D12_QUERY_TYPE_TIMESTAMP)
	, OcclusionQueries(Device, QueueType, D3D12_QUERY_TYPE_OCCLUSION)
{
}

void FD3D12ContextCommon::WaitSyncPoint(FD3D12SyncPoint* SyncPoint)
{
	if (IsOpen())
	{
		CloseCommandList();
	}

	GetPayload(EPhase::Wait)->SyncPointsToWait.Add(SyncPoint);
}

void FD3D12ContextCommon::SignalSyncPoint(FD3D12SyncPoint* SyncPoint)
{
	if (IsOpen())
	{
		CloseCommandList();
	}

	GetPayload(EPhase::Signal)->SyncPointsToSignal.Add(SyncPoint);
}

void FD3D12ContextCommon::SignalManualFence(ID3D12Fence* Fence, uint64 Value)
{
	if (IsOpen())
	{
		CloseCommandList();
	}

	GetPayload(EPhase::Signal)->FencesToSignal.Emplace(Fence, Value);
}

void FD3D12ContextCommon::WaitManualFence(ID3D12Fence* Fence, uint64 Value)
{
	if (IsOpen())
	{
		CloseCommandList();
	}

	GetPayload(EPhase::Wait)->FencesToWait.Emplace(Fence, Value);
}

FD3D12QueryLocation FD3D12ContextCommon::AllocateQuery(ED3D12QueryType Type, uint64* Target)
{
	switch (Type)
	{
	default:
		checkNoEntry();
		[[fallthrough]];

	case ED3D12QueryType::AdjustedRaw:
	case ED3D12QueryType::AdjustedMicroseconds:
		return TimestampQueries.Allocate(Type, Target);

	case ED3D12QueryType::Occlusion:
		return OcclusionQueries.Allocate(Type, Target);
	}
}

FD3D12QueryLocation FD3D12ContextCommon::InsertTimestamp(ED3D12Units Units, uint64* Target)
{
	ED3D12QueryType Type;
	switch (Units)
	{
	default:
		checkNoEntry();
		[[fallthrough]];

	case ED3D12Units::Microseconds: Type = ED3D12QueryType::AdjustedMicroseconds; break;
	case ED3D12Units::Raw:          Type = ED3D12QueryType::AdjustedRaw;          break;
	}

	FD3D12QueryLocation Location = AllocateQuery(Type, Target);
	EndQuery(Location);

	return Location;
}

void FD3D12ContextCommon::OpenCommandList()
{
	LLM_SCOPE_BYNAME(TEXT("RHIMisc/OpenCommandList"));
	checkf(!IsOpen(), TEXT("Command list is already open."));

	if (CommandAllocator == nullptr)
	{
		// Obtain a command allocator if the context doesn't already have one.
		CommandAllocator = Device->ObtainCommandAllocator(QueueType);
	}

	// Get a new command list
	CommandList = Device->ObtainCommandList(CommandAllocator, &TimestampQueries);
	GetPayload(EPhase::Execute)->CommandListsToExecute.Add(CommandList);

	check(ActiveQueries == 0);
}

void FD3D12CommandContext::OpenCommandList()
{
	FD3D12ContextCommon::OpenCommandList();

	// Notify the descriptor cache about the new command list
	// This will set the descriptor cache's current heaps on the new command list.
	StateCache.GetDescriptorCache()->OpenCommandList();
}

void FD3D12ContextCommon::CloseCommandList()
{
	checkf(IsOpen(), TEXT("Command list is not open."));
	checkf(Payloads.Num() && CurrentPhase == EPhase::Execute, TEXT("Expected the current payload to be in the execute phase."));
	
	checkf(ActiveQueries == 0, TEXT("All queries must be completed before the command list is closed."));

	FD3D12Payload* Payload = GetPayload(EPhase::Execute);

	// Do this before we insert the final timestamp to ensure we're timing all the work on the command list.
	FlushResourceBarriers();

	CommandList->Close();
	CommandList = nullptr;

	TimestampQueries.CloseAndReset(Payload->QueryRanges);
	OcclusionQueries.CloseAndReset(Payload->QueryRanges);
}

void FD3D12CommandContext::CloseCommandList()
{
	StateCache.GetDescriptorCache()->CloseCommandList();
	FD3D12ContextCommon::CloseCommandList();
	// Mark state as dirty now, because ApplyState may be called before OpenCommandList(), and it needs to know that the state has
	// become invalid, so it can set it up again (which opens a new command list if necessary).
	StateCache.DirtyStateForNewCommandList();
}

void FD3D12ContextCommon::Finalize(TArray<FD3D12Payload*>& OutPayloads)
{
	if (IsOpen())
	{
		CloseCommandList();
	}

	// Collect the context's batch of sync points to wait/signal
	if (BatchedSyncPoints.ToWait.Num())
	{
		FD3D12Payload* Payload = Payloads.Num()
			? Payloads[0]
			: GetPayload(EPhase::Wait);

		Payload->SyncPointsToWait.Append(BatchedSyncPoints.ToWait);
		BatchedSyncPoints.ToWait.Reset();
	}

	if (BatchedSyncPoints.ToSignal.Num())
	{
		GetPayload(EPhase::Signal)->SyncPointsToSignal.Append(BatchedSyncPoints.ToSignal);
		BatchedSyncPoints.ToSignal.Reset();
	}

	// Attach the command allocator and query heaps to the last payload.
	// The interrupt thread will release these back to the device object pool.
	if (CommandAllocator)
	{
		GetPayload(EPhase::Signal)->AllocatorsToRelease.Add(CommandAllocator);
		CommandAllocator = nullptr;
	}

	check(!TimestampQueries.HasQueries());
	check(!OcclusionQueries.HasQueries());

	ContextSyncPoint = nullptr;

	// Move the list of payloads out of this context
	OutPayloads.Append(MoveTemp(Payloads));
}

FD3D12QueryLocation FD3D12QueryAllocator::Allocate(ED3D12QueryType Type, uint64* Target)
{
	check(Type != ED3D12QueryType::None);

	// Allocate a new heap if needed
	if (Ranges.Num() == 0 || Ranges.Last().IsFull())
	{
		TRefCountPtr<FD3D12QueryHeap> Heap = Device->ObtainQueryHeap(QueueType, QueryType);
		if (!Heap)
		{
			// Unsupported query type
			return {};
		}

		FD3D12QueryRange& Range = Ranges.Emplace_GetRef();
		Range.Heap = MoveTemp(Heap);
	}

	FD3D12QueryRange& Range = Ranges.Last();
	return FD3D12QueryLocation(
		Range.Heap,
		Range.End++,
		Type,
		Target
	);
}

void FD3D12QueryAllocator::CloseAndReset(TArray<FD3D12QueryRange>& OutRanges)
{
	if (HasQueries())
	{
		OutRanges.Append(Ranges);

		if (Ranges.Last().IsFull())
		{
			// No space in any heap. Reset the whole array.
			Ranges.Reset();
		}
		else
		{
			// The last heap still has space. Reuse it for the next batch of command lists.
			FD3D12QueryRange LastRange = MoveTemp(Ranges.Last());
			LastRange.Start = LastRange.End;

			Ranges.Reset();
			Ranges.Emplace(MoveTemp(LastRange));
		}
	}
}

FD3D12CopyScope::FD3D12CopyScope(FD3D12Device* Device, ED3D12SyncPointType SyncPointType, FD3D12SyncPointRef const& WaitSyncPoint)
	: Device(Device)
	, SyncPoint(FD3D12SyncPoint::Create(SyncPointType))
	, Context(*Device->ObtainContextCopy())
{
	if (WaitSyncPoint)
	{
		Context.BatchedSyncPoints.ToWait.Add(WaitSyncPoint);
	}
}

FD3D12CopyScope::~FD3D12CopyScope()
{
	checkf(bSyncPointRetrieved, TEXT("The copy sync point must be retrieved before the end of the scope."));

	Context.SignalSyncPoint(SyncPoint);

	TArray<FD3D12Payload*> Payloads;
	Context.Finalize(Payloads);

	Context.ClearState();
	Device->ReleaseContext(&Context);

	FD3D12DynamicRHI::GetD3DRHI()->SubmitPayloads(Payloads);
}

FD3D12SyncPoint* FD3D12CopyScope::GetSyncPoint() const
{
#if DO_CHECK
	bSyncPointRetrieved = true;
#endif

	return SyncPoint;
}

void FD3D12ContextCommon::FlushCommands(ED3D12FlushFlags FlushFlags)
{
	// We should only be flushing the default context
	check(IsDefaultContext());

	if (IsOpen())
	{
		CloseCommandList();
	}

	FD3D12SyncPointRef SyncPoint;
	FGraphEventRef SubmissionEvent;

	if (EnumHasAnyFlags(FlushFlags, ED3D12FlushFlags::WaitForCompletion))
	{
		SyncPoint = FD3D12SyncPoint::Create(ED3D12SyncPointType::GPUAndCPU);
		SignalSyncPoint(SyncPoint);
	}

	if (EnumHasAnyFlags(FlushFlags, ED3D12FlushFlags::WaitForSubmission))
	{
		SubmissionEvent = FGraphEvent::CreateGraphEvent();
		GetPayload(EPhase::Signal)->SubmissionEvent = SubmissionEvent;
	}

	{
		TArray<FD3D12Payload*> LocalPayloads;
		Finalize(LocalPayloads);
		FD3D12DynamicRHI::GetD3DRHI()->SubmitPayloads(LocalPayloads);
	}

	if (SyncPoint)
	{
		SyncPoint->Wait();
	}

	if (SubmissionEvent && !SubmissionEvent->IsComplete())
	{
		SCOPED_NAMED_EVENT_TEXT("Submission_Wait", FColor::Turquoise);

		FRenderThreadIdleScope Scope(ERenderThreadIdleTypes::WaitingForAllOtherSleep);
		SubmissionEvent->Wait();
	}
}

void FD3D12ContextCommon::ConditionalSplitCommandList()
{
	// Start a new command list if the total number of commands exceeds the threshold. Too many commands in a single command list can cause TDRs.
	if (IsOpen() && ActiveQueries == 0 && GD3D12MaxCommandsPerCommandList > 0 && CommandList->State.NumCommands > (uint32)GD3D12MaxCommandsPerCommandList)
	{
		UE_LOG(LogD3D12RHI, Verbose, TEXT("Splitting command lists because too many commands have been enqueued already (%d commands)"), CommandList->State.NumCommands);
		CloseCommandList();
	}
}

void FD3D12CommandContext::RHIBeginFrame()
{
	Device->GetGPUProfiler().BeginFrame();

	Device->GetDefaultBufferAllocator().BeginFrame();
	Device->GetTextureAllocator().BeginFrame();

	bTrackingEvents = IsDefaultContext() && Device->GetGPUProfiler().bTrackingEvents;

#if D3D12_RHI_RAYTRACING
	Device->GetRayTracingCompactionRequestHandler()->Update(*this);
#endif // D3D12_RHI_RAYTRACING
}

void FD3D12CommandContext::ClearState(EClearStateMode Mode)
{
	StateCache.ClearState();

	bDiscardSharedGraphicsConstants = false;
	bDiscardSharedComputeConstants = false;

	FMemory::Memzero(BoundUniformBuffers, sizeof(BoundUniformBuffers));
	FMemory::Memzero(DirtyUniformBuffers, sizeof(DirtyUniformBuffers));

	if (Mode == EClearStateMode::All)
	{
		FMemory::Memzero(StaticUniformBuffers.GetData(), StaticUniformBuffers.Num() * sizeof(FRHIUniformBuffer*));
	}

	for (int i = 0; i < UE_ARRAY_COUNT(BoundUniformBufferRefs); i++)
	{
		for (int j = 0; j < UE_ARRAY_COUNT(BoundUniformBufferRefs[i]); j++)
		{
			BoundUniformBufferRefs[i][j] = NULL;
		}
	}
}

void FD3D12CommandContext::ConditionalClearShaderResource(FD3D12ResourceLocation* Resource)
{
	check(Resource);
	StateCache.ClearShaderResourceViews<SF_Vertex>(Resource);
	StateCache.ClearShaderResourceViews<SF_Mesh>(Resource);
	StateCache.ClearShaderResourceViews<SF_Amplification>(Resource);
	StateCache.ClearShaderResourceViews<SF_Pixel>(Resource);
	StateCache.ClearShaderResourceViews<SF_Geometry>(Resource);
	StateCache.ClearShaderResourceViews<SF_Compute>(Resource);
}

void FD3D12CommandContext::ClearAllShaderResources()
{
	StateCache.ClearSRVs();
}

void FD3D12CommandContextBase::RHIEndFrame()
{
	FD3D12Device* Device = ParentAdapter->GetDevice(0);

	ParentAdapter->EndFrame();

	for (uint32 GPUIndex : GPUMask)
	{
		Device = ParentAdapter->GetDevice(GPUIndex);

		FD3D12CommandContext& DefaultContext = Device->GetDefaultCommandContext();
		DefaultContext.FlushResourceBarriers();

		DefaultContext.ClearState();
		DefaultContext.FlushCommands();

		Device->GetTextureAllocator().CleanUpAllocations();

		// Only delete free blocks when not used in the last 2 frames, to make sure we are not allocating and releasing
		// the same blocks every frame.
		uint64 BufferPoolDeletionFrameLag = 20;
		Device->GetDefaultBufferAllocator().CleanupFreeBlocks(BufferPoolDeletionFrameLag);

		uint64 FastAllocatorDeletionFrameLag = 10;
		Device->GetDefaultFastAllocator().CleanupPages(FastAllocatorDeletionFrameLag);
	}

	UpdateMemoryStats();

	// Stop Timing at the very last moment
	for (uint32 GPUIndex : GPUMask)
	{
		Device = ParentAdapter->GetDevice(GPUIndex);
		Device->GetGPUProfiler().EndFrame();
	}

	// Close the previous frame's timing and start a new one
	FD3D12DynamicRHI::GetD3DRHI()->FlushTiming(true);

	// Pump the interrupt queue to gather completed events
	// (required if we're not using an interrupt thread).
	FD3D12DynamicRHI::GetD3DRHI()->ProcessInterruptQueueUntil(nullptr);
}

void FD3D12CommandContextBase::UpdateMemoryStats()
{
#if PLATFORM_WINDOWS && STATS
	// Refresh captured memory info.
	ParentAdapter->UpdateMemoryInfo();

	const FD3D12MemoryInfo& MemoryInfo = ParentAdapter->GetMemoryInfo();

	SET_MEMORY_STAT(STAT_D3D12UsedVideoMemory, MemoryInfo.LocalMemoryInfo.CurrentUsage);
	SET_MEMORY_STAT(STAT_D3D12UsedSystemMemory, MemoryInfo.NonLocalMemoryInfo.CurrentUsage);
	SET_MEMORY_STAT(STAT_D3D12AvailableVideoMemory, MemoryInfo.AvailableLocalMemory);
	SET_MEMORY_STAT(STAT_D3D12DemotedVideoMemory, MemoryInfo.DemotedLocalMemory);
	SET_MEMORY_STAT(STAT_D3D12TotalVideoMemory, MemoryInfo.LocalMemoryInfo.Budget);

	uint64 MaxTexAllocWastage = 0;
	for (uint32 GPUIndex : GPUMask)
	{
		FD3D12Device* Device = ParentAdapter->GetDevice(GPUIndex);

#if D3D12RHI_SEGREGATED_TEXTURE_ALLOC && D3D12RHI_SEGLIST_ALLOC_TRACK_WASTAGE
		uint64 TotalAllocated;
		uint64 TotalUnused;
		Device->GetTextureAllocator().GetMemoryStats(TotalAllocated, TotalUnused);
		MaxTexAllocWastage = FMath::Max(MaxTexAllocWastage, TotalUnused);
		SET_MEMORY_STAT(STAT_D3D12TextureAllocatorAllocated, TotalAllocated);
		SET_MEMORY_STAT(STAT_D3D12TextureAllocatorUnused, TotalUnused);
#endif

		Device->GetDefaultBufferAllocator().UpdateMemoryStats();
		ParentAdapter->GetUploadHeapAllocator(GPUIndex).UpdateMemoryStats();
	}
#endif
}

void FD3D12CommandContext::RHIBeginScene()
{
	ensure(!bDrawingScene);
	bDrawingScene = true;
}

void FD3D12CommandContext::RHIEndScene()
{
	ensure(bDrawingScene);
	bDrawingScene = false;
}

IRHIComputeContext* FD3D12DynamicRHI::RHIGetCommandContext(ERHIPipeline Pipeline, FRHIGPUMask GPUMask)
{
	if (GPUMask.HasSingleIndex())
	{
		FD3D12Device* Device = GetAdapter().GetDevice(GPUMask.ToIndex());

		FD3D12CommandContext* CmdContext;
		switch (Pipeline)
		{
		default: checkNoEntry(); // fallthrough
		case ERHIPipeline::Graphics    : CmdContext = Device->ObtainContextGraphics(); break;
		case ERHIPipeline::AsyncCompute: CmdContext = Device->ObtainContextCompute();  break;
		}

		check(CmdContext->GetPhysicalGPUMask() == GPUMask);

		return CmdContext;
	}
	else
	{
		FD3D12CommandContextRedirector* CmdContextRedirector = new FD3D12CommandContextRedirector(&GetAdapter(), GetD3DCommandQueueType(Pipeline), false);
		CmdContextRedirector->SetPhysicalGPUMask(GPUMask);

		for (uint32 GPUIndex : GPUMask)
		{
			FD3D12Device* Device = GetAdapter().GetDevice(GPUIndex);

			FD3D12CommandContext* CmdContext;
			switch (Pipeline)
			{
			default: checkNoEntry(); // fallthrough
			case ERHIPipeline::Graphics    : CmdContext = Device->ObtainContextGraphics(); break;
			case ERHIPipeline::AsyncCompute: CmdContext = Device->ObtainContextCompute();  break;
			}

			CmdContextRedirector->SetPhysicalContext(CmdContext);
		}

		return CmdContextRedirector;
	}
}

void FD3D12DynamicRHI::RHICreateTransition(FRHITransition* Transition, const FRHITransitionCreateInfo& CreateInfo)
{
	// Construct the data in-place on the transition instance
	FD3D12TransitionData* Data = new (Transition->GetPrivateData<FD3D12TransitionData>()) FD3D12TransitionData;

	Data->SrcPipelines = CreateInfo.SrcPipelines;
	Data->DstPipelines = CreateInfo.DstPipelines;
	Data->CreateFlags = CreateInfo.Flags;

	const bool bCrossPipeline = (CreateInfo.SrcPipelines != CreateInfo.DstPipelines) && (!EnumHasAnyFlags(Data->CreateFlags, ERHITransitionCreateFlags::NoFence));
	if (bCrossPipeline)
	{
		// Create one sync point per device, per source pipe
		for (uint32 Index : FRHIGPUMask::All())
		{
			TRHIPipelineArray<FD3D12SyncPointRef>& DeviceSyncPoints = Data->SyncPoints.Emplace_GetRef();
			EnumerateRHIPipelines(CreateInfo.SrcPipelines, [&](ERHIPipeline Pipeline)
			{
				DeviceSyncPoints[Pipeline] = FD3D12SyncPoint::Create(ED3D12SyncPointType::GPUOnly);
			});
		}
	}

	Data->bCrossPipeline = bCrossPipeline;
	Data->TransitionInfos = CreateInfo.TransitionInfos;
	Data->AliasingInfos = CreateInfo.AliasingInfos;

	uint32 AliasingOverlapCount = 0;

	for (const FRHITransientAliasingInfo& AliasingInfo : Data->AliasingInfos)
	{
		AliasingOverlapCount += AliasingInfo.Overlaps.Num();
	}

	Data->AliasingOverlaps.Reserve(AliasingOverlapCount);

	for (FRHITransientAliasingInfo& AliasingInfo : Data->AliasingInfos)
	{
		const int32 OverlapCount = AliasingInfo.Overlaps.Num();

		if (OverlapCount > 0)
		{
			const int32 OverlapOffset = Data->AliasingOverlaps.Num();
			Data->AliasingOverlaps.Append(AliasingInfo.Overlaps.GetData(), OverlapCount);
			AliasingInfo.Overlaps = MakeArrayView(&Data->AliasingOverlaps[OverlapOffset], OverlapCount);
		}
	}
}

void FD3D12DynamicRHI::RHIReleaseTransition(FRHITransition* Transition)
{
	// Destruct the transition data
	Transition->GetPrivateData<FD3D12TransitionData>()->~FD3D12TransitionData();
}

IRHITransientResourceAllocator* FD3D12DynamicRHI::RHICreateTransientResourceAllocator()
{
	return new FD3D12TransientResourceHeapAllocator(GetAdapter().GetOrCreateTransientHeapCache());
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// FD3D12CommandContextRedirector
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////

FD3D12CommandContextRedirector::FD3D12CommandContextRedirector(class FD3D12Adapter* InParent, ED3D12QueueType QueueType, bool InIsDefaultContext)
	: FD3D12CommandContextBase(InParent, FRHIGPUMask::All())
	, QueueType(QueueType)
	, bIsDefaultContext(InIsDefaultContext)
{
	for (FD3D12CommandContext*& Context : PhysicalContexts)
		Context = nullptr;
}

void FD3D12CommandContextRedirector::RHITransferResources(const TArrayView<const FTransferResourceParams> Params)
{
#if WITH_MGPU
	if (Params.Num() == 0)
		return;

	auto MGPUSync = [this](FRHIGPUMask SignalMask, TOptional<FRHIGPUMask> WaitMask = {})
	{
		FRHIGPUMask CombinedMask = SignalMask;
		if (WaitMask.IsSet())
		{
			CombinedMask |= WaitMask.GetValue();
		}

		// Signal a sync point on each source GPU
		TStaticArray<FD3D12SyncPointRef, MAX_NUM_GPUS> SyncPoints;
		for (uint32 GPUIndex : SignalMask)
		{
			SyncPoints[GPUIndex] = FD3D12SyncPoint::Create(ED3D12SyncPointType::GPUOnly);
			PhysicalContexts[GPUIndex]->SignalSyncPoint(SyncPoints[GPUIndex]);
		}

		// Wait for sync points
		if (WaitMask.IsSet())
		{
			for (uint32 WaitGPUIndex : WaitMask.GetValue())
			{
				for (uint32 SignalGPUIndex : SignalMask)
				{
					PhysicalContexts[WaitGPUIndex]->WaitSyncPoint(SyncPoints[SignalGPUIndex]);
				}
			}
		}

		return SyncPoints;
	};

	// Note that by default it is not empty, but GPU0
	FRHIGPUMask SrcMask, DstMask;
	bool bLockstep = GD3D12UnsafeCrossGPUTransfers == false; // @todo mgpu - fix synchronization
	bool bDelayFence = false;

	{
		bool bFirst = true;
		for (const FTransferResourceParams& Param : Params)
		{
			FD3D12CommandContext* SrcContext = PhysicalContexts[Param.SrcGPUIndex];
			FD3D12CommandContext* DstContext = PhysicalContexts[Param.DestGPUIndex];
			if (!ensure(SrcContext && DstContext))
			{
				continue;
			}

			// @todo mgpu - fix synchronization
			bLockstep |= Param.bLockStepGPUs;

			// If it's the first time we set the mask.
			if (bFirst)
			{
				SrcMask = FRHIGPUMask::FromIndex(Param.SrcGPUIndex);
				DstMask = FRHIGPUMask::FromIndex(Param.DestGPUIndex);
				bDelayFence = Param.DelayedFence != nullptr;
				bFirst = false;
			}
			else
			{
				SrcMask |= FRHIGPUMask::FromIndex(Param.SrcGPUIndex);
				DstMask |= FRHIGPUMask::FromIndex(Param.DestGPUIndex);
				check(bDelayFence == (Param.DelayedFence != nullptr));
			}

			FD3D12Resource* SrcResource;
			FD3D12Resource* DstResource;

			if (Param.Texture)
			{
				check(Param.Buffer == nullptr);

				SrcResource = FD3D12CommandContext::RetrieveTexture(Param.Texture, Param.SrcGPUIndex )->GetResource();
				DstResource = FD3D12CommandContext::RetrieveTexture(Param.Texture, Param.DestGPUIndex)->GetResource();
			}
			else
			{
				check(Param.Buffer != nullptr);

				SrcResource = FD3D12DynamicRHI::ResourceCast(Param.Buffer.GetReference(), Param.SrcGPUIndex )->GetResource();
				DstResource = FD3D12DynamicRHI::ResourceCast(Param.Buffer.GetReference(), Param.DestGPUIndex)->GetResource();
			}

			SrcContext->TransitionResource(SrcResource, D3D12_RESOURCE_STATE_TBD, D3D12_RESOURCE_STATE_COPY_SOURCE, 0);
			DstContext->TransitionResource(DstResource, D3D12_RESOURCE_STATE_TBD, D3D12_RESOURCE_STATE_COPY_DEST  , 0);
		}
	}

	// Wait on any pre-transfer fences first
	for (const FTransferResourceParams& Param : Params)
	{
		if (Param.PreTransferFence)
		{
			FTransferResourceFenceData* FenceData = Param.PreTransferFence;
			for (uint32 GPUIndex : FenceData->Mask)
			{
				FD3D12SyncPoint* SyncPoint = static_cast<FD3D12SyncPoint*>(FenceData->SyncPoints[GPUIndex]);

				PhysicalContexts[GPUIndex]->WaitSyncPoint(SyncPoint);

				SyncPoint->Release();
			}

			delete FenceData;
		}
	}
	
	// Pre-copy synchronization
	if (bLockstep)
	{
		// Everyone waits for completion of everyone one else.
		MGPUSync(SrcMask | DstMask, SrcMask | DstMask);
	}
	else
	{
		for (const FTransferResourceParams& Param : Params)
		{
			if (Param.bPullData)
			{
				// Destination GPUs wait for source GPUs
				MGPUSync(SrcMask, DstMask);
				break;
			}
		}
	}

	// Enqueue the copy work
	for (const FTransferResourceParams& Param : Params)
	{
		FD3D12CommandContext* SrcContext = PhysicalContexts[Param.SrcGPUIndex];
		FD3D12CommandContext* DstContext = PhysicalContexts[Param.DestGPUIndex];
		if (!ensure(SrcContext && DstContext))
		{
			continue;
		}

		FD3D12CommandContext* CopyContext = Param.bPullData ? DstContext : SrcContext;

		if (Param.Texture)
		{
			FD3D12Texture* SrcTexture = FD3D12CommandContext::RetrieveTexture(Param.Texture, Param.SrcGPUIndex);
			FD3D12Texture* DstTexture = FD3D12CommandContext::RetrieveTexture(Param.Texture, Param.DestGPUIndex);

			// If the texture size is zero (Max.Z == 0, set in the constructor), copy the whole resource
			if (Param.Max.Z == 0)
			{
				CopyContext->GraphicsCommandList()->CopyResource(DstTexture->GetResource()->GetResource(), SrcTexture->GetResource()->GetResource());
			}
			else
			{
				// Must be a 2D texture for this code path
				check(Param.Texture->GetTexture2D() != nullptr);

				ensureMsgf(
					Param.Min.X >= 0 && Param.Min.Y >= 0 && Param.Min.Z >= 0 &&
					Param.Max.X >= 0 && Param.Max.Y >= 0 && Param.Max.Z >= 0,
					TEXT("Invalid rect for texture transfer: %i, %i, %i, %i"), Param.Min.X, Param.Min.Y, Param.Min.Z, Param.Max.X, Param.Max.Y, Param.Max.Z);

				D3D12_BOX Box = { (UINT)Param.Min.X, (UINT)Param.Min.Y, (UINT)Param.Min.Z, (UINT)Param.Max.X, (UINT)Param.Max.Y, (UINT)Param.Max.Z };

				CD3DX12_TEXTURE_COPY_LOCATION SrcLocation(SrcTexture->GetResource()->GetResource(), 0);
				CD3DX12_TEXTURE_COPY_LOCATION DstLocation(DstTexture->GetResource()->GetResource(), 0);

				CopyContext->GraphicsCommandList()->CopyTextureRegion(&DstLocation, Box.left, Box.top, Box.front, &SrcLocation, &Box);
			}
		}
		else
		{
			FD3D12Resource* SrcResource = FD3D12DynamicRHI::ResourceCast(Param.Buffer.GetReference(), Param.SrcGPUIndex)->GetResource();
			FD3D12Resource* DstResource = FD3D12DynamicRHI::ResourceCast(Param.Buffer.GetReference(), Param.DestGPUIndex)->GetResource();

			CopyContext->GraphicsCommandList()->CopyResource(DstResource->GetResource(), SrcResource->GetResource());
		}
	}

	// Post-copy synchronization
	if (bLockstep)
	{
		// Complete the lockstep by ensuring the GPUs don't start doing something else before the copy completes.
		MGPUSync(SrcMask | DstMask, SrcMask | DstMask);
	}
	else if (bDelayFence)
	{
		auto SyncPoints = MGPUSync(SrcMask | DstMask);

		for (const FTransferResourceParams& Param : Params)
		{
			check(Param.DelayedFence);
			Param.DelayedFence->Mask = SrcMask | DstMask;

			// Copy the sync points into the delayed fence struct. These will be awaited later in RHITransferResourceWait().
			for (int32 Index = 0; Index < SyncPoints.Num(); ++Index)
			{
				FD3D12SyncPointRef& SyncPoint = SyncPoints[Index];

				if (SyncPoint)
				{
					SyncPoint->AddRef();
					Param.DelayedFence->SyncPoints[Index] = SyncPoint.GetReference();
				}
				else
				{
					Param.DelayedFence->SyncPoints[Index] = nullptr;
				}
			}
		}
	}
	else
	{
		// The dest waits for the src to be at this place in the frame before using the data.
		MGPUSync(SrcMask, DstMask);
	}
#endif // WITH_MGPU
}

void FD3D12CommandContextRedirector::RHITransferResourceSignal(const TArrayView<FTransferResourceFenceData* const> FenceDatas, FRHIGPUMask SrcGPUMask)
{
	check(FenceDatas.Num() == SrcGPUMask.GetNumActive());

	uint32 FenceIndex = 0;
	for (uint32 SrcGPUIndex : SrcGPUMask)
	{
		FD3D12SyncPointRef SyncPoint = FD3D12SyncPoint::Create(ED3D12SyncPointType::GPUOnly);
		SyncPoint->AddRef();

		PhysicalContexts[SrcGPUIndex]->SignalSyncPoint(SyncPoint);

		FTransferResourceFenceData* FenceData = FenceDatas[FenceIndex++];
		FenceData->Mask = FRHIGPUMask::FromIndex(SrcGPUIndex);
		FenceData->SyncPoints[SrcGPUIndex] = SyncPoint;
	}
}

void FD3D12CommandContextRedirector::RHITransferResourceWait(const TArrayView<FTransferResourceFenceData* const> FenceDatas)
{
#if WITH_MGPU
	FRHIGPUMask AllMasks;
	for (int32 Index = 0; Index < FenceDatas.Num(); ++Index)
	{
		AllMasks = Index == 0
			? FenceDatas[Index]->Mask
			: FenceDatas[Index]->Mask | AllMasks;
	}

	for (FTransferResourceFenceData* FenceData : FenceDatas)
	{
		// Wait for sync points
		for (uint32 WaitGPUIndex : FenceData->Mask)
		{
			for (void* SyncPointPtr : FenceData->SyncPoints)
			{
				if (SyncPointPtr)
				{	
					FD3D12SyncPoint* SyncPoint = static_cast<FD3D12SyncPoint*>(SyncPointPtr);
					PhysicalContexts[WaitGPUIndex]->WaitSyncPoint(SyncPoint);
				}
			}
		}

		// Release sync points
		for (void* SyncPointPtr : FenceData->SyncPoints)
		{
			if (SyncPointPtr)
			{
				static_cast<FD3D12SyncPoint*>(SyncPointPtr)->Release();
			}
		}

		delete FenceData;
	}

#endif // WITH_MGPU
}
