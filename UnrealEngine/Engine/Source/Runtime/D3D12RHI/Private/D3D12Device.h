// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
D3D12Device.h: D3D12 Device Interfaces
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "D3D12BindlessDescriptors.h"
#include "D3D12Descriptors.h"

class FD3D12Device;
class FD3D12DynamicRHI;
class FD3D12Buffer;
class FD3D12ExplicitDescriptorHeapCache;
class FD3D12RayTracingPipelineCache;
class FD3D12RayTracingCompactionRequestHandler;
struct FD3D12RayTracingPipelineInfo;

// Counterpart to UEDiagnosticBuffer in D3DCommon.ush
struct FD3D12DiagnosticBufferData
{
	uint32 Counter;
	uint32 MessageID;
	union
	{
		int32  AsInt[4];
		uint32 AsUint[4];
		float  AsFloat[4];
	} Payload;
};

static_assert(sizeof(FD3D12DiagnosticBufferData) == 6 * sizeof(uint32),
	"Remember to change UEDiagnosticBuffer layout in the shaders when changing FD3D12DiagnosticBufferData");

// Helper data used to track GPU progress on this command queue
struct FD3D12DiagnosticBuffer
{
	TRefCountPtr<FD3D12Heap> Heap;
	TRefCountPtr<FD3D12Resource> Resource;

	void* CpuAddress = nullptr;
	D3D12_GPU_VIRTUAL_ADDRESS GpuAddress = 0;

	FD3D12DiagnosticBuffer(TRefCountPtr<FD3D12Heap>&& Heap, TRefCountPtr<FD3D12Resource>&& Resource, void* CpuAddress, D3D12_GPU_VIRTUAL_ADDRESS GpuAddress)
		: Heap(MoveTemp(Heap))
		, Resource(MoveTemp(Resource))
		, CpuAddress(CpuAddress)
		, GpuAddress(GpuAddress)
	{}

	TArray<uint16> FreeContextIds;
	FCriticalSection CriticalSection;
	uint32 BreadCrumbsContextSize = 0;

	uint32 BreadCrumbsOffset = 0;
	uint32 BreadCrumbsSize = 0;

	uint32 DiagnosticsOffset = 0;
	uint32 DiagnosticsSize = 0;

	~FD3D12DiagnosticBuffer();
};

// Encapsulates the state required for tracking GPU queue performance across a frame.
class FD3D12Timing
{
public:
	FD3D12Queue& Queue;

	TArray<uint64> Timestamps;
	int32 TimestampIndex = 0;

	uint64 BusyCycles = 0;

	D3D12_QUERY_DATA_PIPELINE_STATISTICS PipelineStats {};

	uint64 GetCurrentTimestamp()  const { return Timestamps[TimestampIndex]; }
	uint64 GetPreviousTimestamp() const { return Timestamps[TimestampIndex - 1]; }

	bool HasMoreTimestamps() const { return TimestampIndex < Timestamps.Num(); }
	bool IsStartingWork()    const { return (TimestampIndex & 0x01) == 0x00; }

	void AdvanceTimestamp() { TimestampIndex++; }

	FD3D12Timing(FD3D12Queue& Queue)
		: Queue(Queue)
	{}
};

// Encapsulates a single D3D command queue, and maintains the 
// state required by the submission thread for managing the queue.
class FD3D12Queue final
{
public:
	FD3D12Device* const Device;
	ED3D12QueueType const QueueType;

	struct : public TQueue<FD3D12Payload*, EQueueMode::Mpsc>
	{
		FD3D12Payload* Peek()
		{
			if (FD3D12Payload** Result = TQueue::Peek())
				return *Result;

			return nullptr;
		}		
	} PendingSubmission, PendingInterrupt;

	FD3D12Payload*          PayloadToSubmit    = nullptr;
	FD3D12CommandAllocator* BarrierAllocator   = nullptr;
	FD3D12QueryAllocator    BarrierTimestamps;

	uint32 NumCommandListsInBatch = 0;

	// Query ranges/locations to be resolved when the submission thread receives a command list which is still open.
	TArray<FD3D12QueryRange   > PendingQueryRanges;
	TArray<FD3D12QueryLocation> PendingTimestampQueries;
	TArray<FD3D12QueryLocation> PendingOcclusionQueries;
	TArray<FD3D12QueryLocation> PendingPipelineStatsQueries;

	// Executes the current payload, returning the latest fence value signaled for this queue.
	uint64 ExecutePayload();

	bool bRequiresSignal = false;

	// On some hardware, some auxiliary queue types may not support tile mapping and a separate queue must be used
	bool bSupportsTileMapping = true;

	// The underlying D3D queue object
	TRefCountPtr<ID3D12CommandQueue> D3DCommandQueue;

	// A single D3D fence to manage completion of work on this queue
	FD3D12Fence Fence;

	// Tracks what fence values this queue has awaited on other queues.
	struct FRemoteFenceState
	{
		uint64 MaxValueAwaited = 0;
		uint64 NextValueToAwait = 0;
	};
	TMap<FD3D12Fence*, FRemoteFenceState> RemoteFenceStates;

	uint64 SignalFence()
	{
		if (bRequiresSignal)
		{
			bRequiresSignal = false;
			uint64 ValueToSignal = ++Fence.LastSignaledValue;
			VERIFYD3D12RESULT(D3DCommandQueue->Signal(
				Fence.D3DFence,
				ValueToSignal
			));

			return ValueToSignal;
		}
		else
		{
			return Fence.LastSignaledValue;
		}
	}

	TArray<FD3D12Fence*, TInlineAllocator<GD3D12MaxNumQueues>> FencesToAwait;
	void EnqueueFenceWait(FD3D12Fence* RemoteFence, uint64 Value)
	{
		uint64& NextValueToAwait = RemoteFenceStates.FindOrAdd(RemoteFence).NextValueToAwait;
		NextValueToAwait = FMath::Max(NextValueToAwait, Value);
		FencesToAwait.AddUnique(RemoteFence);
	}

	void FlushFenceWaits()
	{
		for (FD3D12Fence* FenceToAwait : FencesToAwait)
		{
			FRemoteFenceState& RemoteFenceState = RemoteFenceStates.FindChecked(FenceToAwait);

			// Skip issuing the fence wait if we've previously awaited the same fence with a higher value.
			if (RemoteFenceState.NextValueToAwait > RemoteFenceState.MaxValueAwaited)
			{
				VERIFYD3D12RESULT(D3DCommandQueue->Wait(
					FenceToAwait->D3DFence,
					RemoteFenceState.NextValueToAwait
				));

				RemoteFenceState.MaxValueAwaited = FMath::Max(
					RemoteFenceState.MaxValueAwaited,
					RemoteFenceState.NextValueToAwait
				);
			}
		}

		FencesToAwait.Reset();
	}

	// A pool of reusable command list/allocator/context objects
	struct
	{
		TD3D12ObjectPool<FD3D12ContextCommon   > Contexts;
		TD3D12ObjectPool<FD3D12CommandAllocator> Allocators;
		TD3D12ObjectPool<FD3D12CommandList     > Lists;
	} ObjectPool;

	TUniquePtr<FD3D12DiagnosticBuffer> DiagnosticBuffer;

	const D3D12_GPU_VIRTUAL_ADDRESS GetDiagnosticBufferGPUAddress() const
	{
		return DiagnosticBuffer
			? DiagnosticBuffer->GpuAddress + DiagnosticBuffer->DiagnosticsOffset
			: 0;
	}

	const FD3D12DiagnosticBufferData* GetDiagnosticBufferData() const
	{
		const uint8* Address = DiagnosticBuffer
			? reinterpret_cast<const uint8*>(DiagnosticBuffer->CpuAddress) + DiagnosticBuffer->DiagnosticsOffset
			: nullptr;
		return reinterpret_cast<const FD3D12DiagnosticBufferData*>(Address);
	}

	// Get the CPU readable data from the breadcrumb data - this data is still valid after the Device is Lost
	const void* GetBreadCrumbBufferData() const
	{
		return DiagnosticBuffer
			? reinterpret_cast<const uint8*>(DiagnosticBuffer->CpuAddress) + DiagnosticBuffer->BreadCrumbsOffset
			: nullptr;
	}

	// The active timing struct on this queue. Updated / accessed by the interrupt thread.
	FD3D12Timing* Timing = nullptr;

	uint64 CumulativeIdleTicks = 0;
	uint64 LastEndTime = 0;

	FD3D12Queue(FD3D12Device* Device, ED3D12QueueType QueueType);
	~FD3D12Queue();

	void SetupAfterDeviceCreation();
};

class FD3D12Device final : public FD3D12SingleNodeGPUObject, public FNoncopyable, public FD3D12AdapterChild
{
public:
	FD3D12Device(FRHIGPUMask InGPUMask, FD3D12Adapter* InAdapter);
	~FD3D12Device();

	ID3D12Device* GetDevice();

	// GPU Profiler
	FORCEINLINE D3D12RHI::FD3DGPUProfiler& GetGPUProfiler() { return GPUProfilingData; }

	void RegisterGPUWork(uint32 NumPrimitives = 0, uint32 NumVertices = 0);
	void RegisterGPUDispatch(FIntVector GroupCount);

	uint64 GetTimestampFrequency(ED3D12QueueType QueueType);
	FGPUTimingCalibrationTimestamp GetCalibrationTimestamp(ED3D12QueueType QueueType);

	// Misc
	void BlockUntilIdle();
	D3D12_RESOURCE_ALLOCATION_INFO GetResourceAllocationInfoUncached(const FD3D12ResourceDesc& InDesc);
	D3D12_RESOURCE_ALLOCATION_INFO GetResourceAllocationInfo(const FD3D12ResourceDesc& InDesc);
	TUniquePtr<FD3D12DiagnosticBuffer> CreateDiagnosticBuffer(const D3D12_RESOURCE_DESC& Desc, const TCHAR* Name);

	void									  InitExplicitDescriptorHeap();
	FD3D12ExplicitDescriptorHeapCache*		  GetExplicitDescriptorHeapCache() { return ExplicitDescriptorHeapCache; }

	// Ray Tracing
#if D3D12_RHI_RAYTRACING
	void									  InitRayTracing();
	void									  CleanupRayTracing();

	ID3D12Device5*							  GetDevice5();
	ID3D12Device7*							  GetDevice7();

	FD3D12RayTracingPipelineCache*			  GetRayTracingPipelineCache           () { return RayTracingPipelineCache;            }
	FD3D12Buffer*							  GetRayTracingDispatchRaysDescBuffer  () { return RayTracingDispatchRaysDescBuffer;   }
	FD3D12RayTracingCompactionRequestHandler* GetRayTracingCompactionRequestHandler() { return RayTracingCompactionRequestHandler; }

	TRefCountPtr<ID3D12StateObject>			  DeserializeRayTracingStateObject(D3D12_SHADER_BYTECODE Bytecode, ID3D12RootSignature* RootSignature);

	void GetRaytracingAccelerationStructurePrebuildInfo(
		const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS* pDesc,
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO* pInfo);

	// Queries ray tracing pipeline state object metrics such as VGPR usage (if available/supported). Returns true if query succeeded.
	bool GetRayTracingPipelineInfo(ID3D12StateObject* Pipeline, FD3D12RayTracingPipelineInfo* OutInfo);
#endif // D3D12_RHI_RAYTRACING

	// Heaps
	inline FD3D12GlobalOnlineSamplerHeap& GetGlobalSamplerHeap() { return GlobalSamplerHeap; }

	inline const D3D12_HEAP_PROPERTIES& GetConstantBufferPageProperties() { return ConstantBufferPageProperties; }

	// Descriptor Managers
	inline FD3D12DescriptorHeapManager&     GetDescriptorHeapManager    () { return DescriptorHeapManager;     }
#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
	inline FD3D12BindlessDescriptorManager& GetBindlessDescriptorManager() { return BindlessDescriptorManager; }
#endif
	inline FD3D12OnlineDescriptorManager&   GetOnlineDescriptorManager  () { return OnlineDescriptorManager;   }
	inline FD3D12OfflineDescriptorManager&  GetOfflineDescriptorManager (ERHIDescriptorHeapType InType)
	{
		check(InType < ERHIDescriptorHeapType::Count);
		return OfflineDescriptorManagers[static_cast<int>(InType)];
	}

	const FD3D12DefaultViews& GetDefaultViews() const { return DefaultViews; }

	// Memory Allocators
	inline FD3D12DefaultBufferAllocator& GetDefaultBufferAllocator() { return DefaultBufferAllocator; }
	inline FD3D12FastAllocator&          GetDefaultFastAllocator  () { return DefaultFastAllocator;   }
	inline FD3D12TextureAllocatorPool&   GetTextureAllocator      () { return TextureAllocator;       }

	// Residency
	inline FD3D12ResidencyManager& GetResidencyManager() { return ResidencyManager; }

	// Samplers
	FD3D12SamplerState* CreateSampler(const FSamplerStateInitializerRHI& Initializer);
	void CreateSamplerInternal(const D3D12_SAMPLER_DESC& Desc, D3D12_CPU_DESCRIPTOR_HANDLE Descriptor);

	// Command Allocators
	FD3D12CommandAllocator* ObtainCommandAllocator (ED3D12QueueType QueueType);
	void                    ReleaseCommandAllocator(FD3D12CommandAllocator* Allocator);

	// Contexts
	FD3D12CommandContext&   GetDefaultCommandContext() { return *ImmediateCommandContext; }
	FD3D12ContextCopy*      ObtainContextCopy       () { return static_cast<FD3D12ContextCopy*   >(ObtainContext(ED3D12QueueType::Copy  )); }
	FD3D12CommandContext*   ObtainContextCompute    () { return static_cast<FD3D12CommandContext*>(ObtainContext(ED3D12QueueType::Async )); }
	FD3D12CommandContext*   ObtainContextGraphics   () { return static_cast<FD3D12CommandContext*>(ObtainContext(ED3D12QueueType::Direct)); }
	FD3D12ContextCommon*    ObtainContext           (ED3D12QueueType QueueType);
	void                    ReleaseContext          (FD3D12ContextCommon* Context);

	// Queries
	TRefCountPtr<FD3D12QueryHeap> ObtainQueryHeap (ED3D12QueueType QueueType, D3D12_QUERY_TYPE QueryType);
	void                          ReleaseQueryHeap(FD3D12QueryHeap* QueryHeap);
	
	// Command Lists
	FD3D12CommandList* ObtainCommandList (FD3D12CommandAllocator* CommandAllocator, FD3D12QueryAllocator* TimestampAllocator, FD3D12QueryAllocator* PipelineStatsAllocator);
	void               ReleaseCommandList(FD3D12CommandList* CommandList);

	// Queues
	FD3D12Queue& GetQueue(ED3D12QueueType QueueType) { return Queues[(uint32)QueueType]; }
	TArrayView<FD3D12Queue> GetQueues() { return Queues; }

	// shared code for different D3D12  devices (e.g. PC DirectX12 and XboxOne) called
	// after device creation and GRHISupportsAsyncTextureCreation was set and before resource init
	void SetupAfterDeviceCreation();
	void CleanupResources();

	TRefCountPtr<ID3D12CommandQueue> TileMappingQueue;
	FD3D12Fence TileMappingFence;

private:
	// called by SetupAfterDeviceCreation() when the device gets initialized
	void CreateDefaultViews();
	void UpdateMSAASettings();
	void UpdateConstantBufferPageProperties();

	D3D12RHI::FD3DGPUProfiler GPUProfilingData;

	struct FResidencyManager : public FD3D12ResidencyManager
	{
		FResidencyManager(FD3D12Device& Parent);
		~FResidencyManager();
	} ResidencyManager;

	FD3D12DescriptorHeapManager     DescriptorHeapManager;
#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
	FD3D12BindlessDescriptorManager BindlessDescriptorManager;
#endif
	TArray<FD3D12OfflineDescriptorManager, TInlineAllocator<(uint32)ERHIDescriptorHeapType::Count>> OfflineDescriptorManagers;

	FD3D12GlobalOnlineSamplerHeap GlobalSamplerHeap;
	FD3D12OnlineDescriptorManager OnlineDescriptorManager;

	FD3D12DefaultViews DefaultViews;

	TStaticArray<TD3D12ObjectPool<FD3D12QueryHeap>, 4> QueryHeapPool { InPlace };
	
	FD3D12CommandContext* ImmediateCommandContext = nullptr;

	TArray<FD3D12Queue, TInlineAllocator<(uint32)ED3D12QueueType::Count>> Queues;

	TMap<D3D12_SAMPLER_DESC, TRefCountPtr<FD3D12SamplerState>> SamplerMap;
	uint32 SamplerID = 0;

	/** Hashmap used to cache resource allocation size information */
	FRWLock ResourceAllocationInfoMapMutex;
	TMap<uint64, D3D12_RESOURCE_ALLOCATION_INFO> ResourceAllocationInfoMap;

	// set by UpdateMSAASettings(), get by GetMSAAQuality()
	// [SampleCount] = Quality, 0xffffffff if not supported
	uint32 AvailableMSAAQualities[DX_MAX_MSAA_COUNT + 1];

	// set by UpdateConstantBufferPageProperties, get by GetConstantBufferPageProperties
	D3D12_HEAP_PROPERTIES ConstantBufferPageProperties;

	FD3D12DefaultBufferAllocator DefaultBufferAllocator;
	FD3D12FastAllocator          DefaultFastAllocator;
	FD3D12TextureAllocatorPool   TextureAllocator;

#if D3D12_RHI_RAYTRACING
	FD3D12RayTracingPipelineCache* RayTracingPipelineCache = nullptr;
	FD3D12RayTracingCompactionRequestHandler* RayTracingCompactionRequestHandler = nullptr;
	FD3D12Buffer* RayTracingDispatchRaysDescBuffer = nullptr;
// #dxr_todo UE-72158: unify RT descriptor cache with main FD3D12DescriptorCache
#endif

	FD3D12ExplicitDescriptorHeapCache* ExplicitDescriptorHeapCache = nullptr;
	void DestroyExplicitDescriptorCache();
};
