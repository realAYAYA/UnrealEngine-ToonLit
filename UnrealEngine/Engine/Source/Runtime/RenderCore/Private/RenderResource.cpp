// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RenderResource.cpp: Render resource implementation.
=============================================================================*/

#include "RenderResource.h"
#include "Misc/ScopedEvent.h"
#include "Misc/App.h"
#include "RenderingThread.h"
#include "ProfilingDebugging/LoadTimeTracker.h"
#include "CoreGlobals.h"
#include "RayTracingGeometryManager.h"

/** Whether to enable mip-level fading or not: +1.0f if enabled, -1.0f if disabled. */
float GEnableMipLevelFading = 1.0f;

// The maximum number of transient vertex buffer bytes to allocate before we start panic logging who is doing the allocations
int32 GMaxVertexBytesAllocatedPerFrame = 32 * 1024 * 1024;

FAutoConsoleVariableRef CVarMaxVertexBytesAllocatedPerFrame(
	TEXT("r.MaxVertexBytesAllocatedPerFrame"),
	GMaxVertexBytesAllocatedPerFrame,
	TEXT("The maximum number of transient vertex buffer bytes to allocate before we start panic logging who is doing the allocations"));

int32 GGlobalBufferNumFramesUnusedThresold = 30;
FAutoConsoleVariableRef CVarReadBufferNumFramesUnusedThresold(
	TEXT("r.NumFramesUnusedBeforeReleasingGlobalResourceBuffers"),
	GGlobalBufferNumFramesUnusedThresold,
	TEXT("Number of frames after which unused global resource allocations will be discarded. Set 0 to ignore. (default=30)"));

bool GFreeStructuresOnRHIBufferCreation = true;
FAutoConsoleVariableRef CVarFreeStructuresOnRHIBufferCreation(
	TEXT("r.FreeStructuresOnRHIBufferCreation"),
	GFreeStructuresOnRHIBufferCreation,
	TEXT("Toggles experimental method for freeing helper structures that own the resource arrays after submitting to RHI instead of in the callback sink."));

int32 GVarDebugForceRuntimeBLAS = 0;
FAutoConsoleVariableRef CVarDebugForceRuntimeBLAS(
	TEXT("r.Raytracing.DebugForceRuntimeBLAS"),
	GVarDebugForceRuntimeBLAS,
	TEXT("Force building BLAS at runtime."),
	ECVF_ReadOnly);

/** Tracks render resources in a list. The implementation is optimized to allow fast allocation / deallocation from any thread,
 *  at the cost of period coalescing of thread-local data at a sync point each frame. Furthermore, iteration is not thread safe
 *  and must be performed at sync points.
 */
class FRenderResourceList
{
public:
	static FRenderResourceList& Get()
	{
		static FRenderResourceList Instance;
		return Instance;
	}

	~FRenderResourceList()
	{
		for (FFreeList* FreeList : LocalFreeLists)
		{
			delete FreeList;
		}
		FPlatformTLS::FreeTlsSlot(TLSSlot);
	}

	int32 Allocate(FRenderResource* Resource)
	{
		if (bIsIterating)
		{
			// This part is not thread safe. Iteration requires that no adds / removals are happening concurrently. The only
			// supported case is recursive adds on the same thread (i.e. a parent resource initializes a child resource). In
			// this case, we need to add the resource to the end so that it gets iterated as well.
			check(IsInRenderingThread());
			return ResourceList.AddElement(Resource);
		}

		FFreeList& LocalFreeList = GetLocalFreeList();

		if (LocalFreeList.IsEmpty())
		{
			FScopeLock Lock(&CS);

			// Try to allocate free slots from the global free list.
			const int32 OldFreeListSize = GlobalFreeList.Num();
			const int32 NewFreeListSize = FMath::Max(OldFreeListSize - ChunkSize, 0);
			int32 NumElements = OldFreeListSize - NewFreeListSize;

			if (NumElements > 0)
			{
				LocalFreeList.Append(GlobalFreeList.GetData() + NewFreeListSize, NumElements);
				GlobalFreeList.SetNum(NewFreeListSize, false);
			}

			// Allocate more if we didn't get a full chunk from the global list.
			while (NumElements < ChunkSize)
			{
				LocalFreeList.Emplace(ResourceList.AddElement(nullptr));
				NumElements++;
			}
		}

		int32 Index = LocalFreeList.Pop(false);
		ResourceList[Index] = Resource;
		return Index;
	}

	void Deallocate(int32 Index)
	{
		GetLocalFreeList().Emplace(Index);
		ResourceList[Index] = nullptr;
	}

	//////////////////////////////////////////////////////////////////////////////
	// These methods must be called at sync points where allocations / deallocations can't occur from another thread.

	void Clear()
	{
		check(IsInRenderingThread());

		for (FFreeList* FreeList : LocalFreeLists)
		{
			FreeList->Empty();
		}
		ResourceList.Empty();
	}

	void Coalesce()
	{
		check(IsInRenderingThread());

		for (FFreeList* FreeList : LocalFreeLists)
		{
			GlobalFreeList.Append(*FreeList);
			FreeList->Empty();
		}
	}

	template<typename FunctionType>
	void ForEach(const FunctionType& Function)
	{
		check(IsInRenderingThread());
		check(!bIsIterating);
		bIsIterating = true;
		for (int32 Index = 0; Index < ResourceList.Num(); ++Index)
		{
			FRenderResource* Resource = ResourceList[Index];
			if (Resource)
			{
				check(Resource->GetListIndex() == Index);
				Function(Resource);
			}
		}
		bIsIterating = false;
	}

	template<typename FunctionType>
	void ForEachReverse(const FunctionType& Function)
	{
		check(IsInRenderingThread());
		check(!bIsIterating);
		bIsIterating = true;
		for (int32 Index = ResourceList.Num() - 1; Index >= 0; --Index)
		{
			FRenderResource* Resource = ResourceList[Index];
			if (Resource)
			{
				check(Resource->GetListIndex() == Index);
				Function(Resource);
			}
		}
		bIsIterating = false;
	}

	//////////////////////////////////////////////////////////////////////////////

private:
	FRenderResourceList()
	{
		TLSSlot = FPlatformTLS::AllocTlsSlot();
	}

	const int32 ChunkSize = 1024;

	using FFreeList = TArray<int32>;

	FFreeList& GetLocalFreeList()
	{
		void* TLSValue = FPlatformTLS::GetTlsValue(TLSSlot);
		if (TLSValue == nullptr)
		{
			FFreeList* TLSCache = new FFreeList();
			FPlatformTLS::SetTlsValue(TLSSlot, (void*)(TLSCache));
			FScopeLock S(&CS);
			LocalFreeLists.Add(TLSCache);
			return *TLSCache;
		}
		return *((FFreeList*)TLSValue);
	}

	uint32 TLSSlot;
	FCriticalSection CS;
	TArray<FFreeList*> LocalFreeLists;
	FFreeList GlobalFreeList;
	TChunkedArray<FRenderResource*> ResourceList;
	bool bIsIterating = false;
};

void FRenderResource::CoalesceResourceList()
{
	FRenderResourceList::Get().Coalesce();
}

void FRenderResource::ReleaseRHIForAllResources()
{
	FRenderResourceList& ResourceList = FRenderResourceList::Get();
	ResourceList.ForEachReverse([](FRenderResource* Resource) { check(Resource->IsInitialized()); Resource->ReleaseRHI(); });
	ResourceList.ForEachReverse([](FRenderResource* Resource) { Resource->ReleaseDynamicRHI(); });
}

/** Initialize all resources initialized before the RHI was initialized */
void FRenderResource::InitPreRHIResources()
{
	FRenderResourceList& ResourceList = FRenderResourceList::Get();

	// Notify all initialized FRenderResources that there's a valid RHI device to create their RHI resources for now.
	ResourceList.ForEach([](FRenderResource* Resource) { Resource->InitRHI(); });
	// Dynamic resources can have dependencies on static resources (with uniform buffers) and must initialized last!
	ResourceList.ForEach([](FRenderResource* Resource) { Resource->InitDynamicRHI(); });

#if !PLATFORM_NEEDS_RHIRESOURCELIST
	ResourceList.Clear();
#endif
}

void FRenderResource::ChangeFeatureLevel(ERHIFeatureLevel::Type NewFeatureLevel)
{
	ENQUEUE_RENDER_COMMAND(FRenderResourceChangeFeatureLevel)(
		[NewFeatureLevel](FRHICommandList& RHICmdList)
	{
		FRenderResourceList::Get().ForEach([NewFeatureLevel](FRenderResource* Resource)
		{
			// Only resources configured for a specific feature level need to be updated
			if (Resource->HasValidFeatureLevel() && (Resource->FeatureLevel != NewFeatureLevel))
			{
				Resource->ReleaseRHI();
				Resource->ReleaseDynamicRHI();
				Resource->FeatureLevel = NewFeatureLevel;
				Resource->InitDynamicRHI();
				Resource->InitRHI();
			}
		});
	});
}

void FRenderResource::InitResource()
{
	if (ListIndex == INDEX_NONE)
	{
		int32 LocalListIndex = INDEX_NONE;

		if (PLATFORM_NEEDS_RHIRESOURCELIST || !GIsRHIInitialized)
		{
			LLM_SCOPE(ELLMTag::SceneRender);
			LocalListIndex = FRenderResourceList::Get().Allocate(this);
		}
		else
		{
			// Mark this resource as initialized
			LocalListIndex = 0;
		}

		if (GIsRHIInitialized)
		{
			CSV_SCOPED_TIMING_STAT_EXCLUSIVE(InitRenderResource);
			InitDynamicRHI();
			InitRHI();
		}

		FPlatformMisc::MemoryBarrier(); // there are some multithreaded reads of ListIndex
		ListIndex = LocalListIndex;
	}
}

void FRenderResource::ReleaseResource()
{
	if ( !GIsCriticalError )
	{
		if(ListIndex != INDEX_NONE)
		{
			if(GIsRHIInitialized)
			{
				ReleaseRHI();
				ReleaseDynamicRHI();
			}

#if PLATFORM_NEEDS_RHIRESOURCELIST
			FRenderResourceList::Get().Deallocate(ListIndex);
#endif
			ListIndex = INDEX_NONE;
		}
	}
}

void FRenderResource::UpdateRHI()
{
	check(IsInRenderingThread());
	if(IsInitialized() && GIsRHIInitialized)
	{
		ReleaseRHI();
		ReleaseDynamicRHI();
		InitDynamicRHI();
		InitRHI();
	}
}

FRenderResource::~FRenderResource()
{
	checkf(ResourceState == ERenderResourceState::Default, TEXT(" Invalid Resource State: %s"), ResourceState == ERenderResourceState::BatchReleased ? TEXT("BatchReleased") : TEXT("Deleted"));
	ResourceState = ERenderResourceState::Deleted;
	if (IsInitialized() && !GIsCriticalError)
	{
		// Deleting an initialized FRenderResource will result in a crash later since it is still linked
		UE_LOG(LogRendererCore, Fatal,TEXT("A FRenderResource was deleted without being released first!"));
	}
}

void BeginInitResource(FRenderResource* Resource)
{
	LLM_SCOPE(ELLMTag::SceneRender);
	ENQUEUE_RENDER_COMMAND(InitCommand)(
		[Resource](FRHICommandListImmediate& RHICmdList)
		{
			Resource->InitResource();
		});
}

void BeginUpdateResourceRHI(FRenderResource* Resource)
{
	ENQUEUE_RENDER_COMMAND(UpdateCommand)(
		[Resource](FRHICommandListImmediate& RHICmdList)
		{
			Resource->UpdateRHI();
		});
}

struct FBatchedReleaseResources
{
	enum 
	{
		NumPerBatch = 16
	};
	TArray<FRenderResource*, TInlineAllocator<NumPerBatch>> Resources;

	void Flush()
	{
		if (Resources.Num())
		{
			ENQUEUE_RENDER_COMMAND(BatchReleaseCommand)(
			[BatchedReleaseResources = MoveTemp(Resources)](FRHICommandList& RHICmdList)
			{
				for (FRenderResource* Resource : BatchedReleaseResources)
				{
					check(Resource->ResourceState == ERenderResourceState::BatchReleased);
					Resource->ReleaseResource();
					Resource->ResourceState = ERenderResourceState::Default;
				}
			});
		}
	}

	void Add(FRenderResource* Resource)
	{
		if (Resources.Num() >= NumPerBatch)
		{
			Flush();
		}
		check(Resources.Num() < NumPerBatch);
		check(Resource->ResourceState == ERenderResourceState::Default);
		Resource->ResourceState = ERenderResourceState::BatchReleased;
		Resources.Push(Resource);
	}

	bool IsEmpty()
	{
		return Resources.Num() == 0;
	}
};

static bool GBatchedReleaseIsActive = false;
static FBatchedReleaseResources GBatchedRelease;

void StartBatchedRelease()
{
	check(IsInGameThread() && !GBatchedReleaseIsActive && GBatchedRelease.IsEmpty());
	GBatchedReleaseIsActive = true;
}
void EndBatchedRelease()
{
	check(IsInGameThread() && GBatchedReleaseIsActive);
	GBatchedRelease.Flush();
	GBatchedReleaseIsActive = false;
}

void BeginReleaseResource(FRenderResource* Resource)
{
	if (GBatchedReleaseIsActive && IsInGameThread())
	{
		GBatchedRelease.Add(Resource);
		return;
	}
	ENQUEUE_RENDER_COMMAND(ReleaseCommand)(
		[Resource](FRHICommandList& RHICmdList)
		{
			Resource->ReleaseResource();
		});
}

void ReleaseResourceAndFlush(FRenderResource* Resource)
{
	// Send the release message.
	ENQUEUE_RENDER_COMMAND(ReleaseCommand)(
		[Resource](FRHICommandList& RHICmdList)
		{
			Resource->ReleaseResource();
		});

	FlushRenderingCommands();
}

FTextureReference::FTextureReference()
	: TextureReferenceRHI(NULL)
{
}

FTextureReference::~FTextureReference()
{
}

void FTextureReference::BeginInit_GameThread()
{
	bInitialized_GameThread = true;
	BeginInitResource(this);
}

void FTextureReference::BeginRelease_GameThread()
{
	BeginReleaseResource(this);
	bInitialized_GameThread = false;
}

double FTextureReference::GetLastRenderTime() const
{
	if (bInitialized_GameThread && TextureReferenceRHI)
	{
		return TextureReferenceRHI->GetLastRenderTime();
	}

	return FLastRenderTimeContainer().GetLastRenderTime();
}

void FTextureReference::InvalidateLastRenderTime()
{
	if (bInitialized_GameThread && TextureReferenceRHI)
	{
		TextureReferenceRHI->SetLastRenderTime(-FLT_MAX);
	}
}

void FTextureReference::InitRHI()
{
	SCOPED_LOADTIMER(FTextureReference_InitRHI);
	TextureReferenceRHI = RHICreateTextureReference();
}
	
int32 GTextureReferenceRevertsLastRenderContainer = 1;
FAutoConsoleVariableRef CVarTextureReferenceRevertsLastRenderContainer(
	TEXT("r.TextureReferenceRevertsLastRenderContainer"),
	GTextureReferenceRevertsLastRenderContainer,
	TEXT(""));

void FTextureReference::ReleaseRHI()
{
	TextureReferenceRHI.SafeRelease();
}

FString FTextureReference::GetFriendlyName() const
{
	return TEXT("FTextureReference");
}

/** The global null color vertex buffer, which is set with a stride of 0 on meshes without a color component. */
TGlobalResource<FNullColorVertexBuffer> GNullColorVertexBuffer;

/** The global null vertex buffer, which is set with a stride of 0 on meshes */
TGlobalResource<FNullVertexBuffer> GNullVertexBuffer;

/*------------------------------------------------------------------------------
	FRayTracingGeometry implementation.
------------------------------------------------------------------------------*/

#if RHI_RAYTRACING

void FRayTracingGeometry::CreateRayTracingGeometryFromCPUData(TResourceArray<uint8>& OfflineData)
{
	check(OfflineData.Num() == 0 || Initializer.OfflineData == nullptr);
	if (OfflineData.Num())
	{
		Initializer.OfflineData = &OfflineData;
	}

	if (GVarDebugForceRuntimeBLAS && Initializer.OfflineData != nullptr)
	{
		Initializer.OfflineData->Discard();
		Initializer.OfflineData = nullptr;
	}

	bRequiresBuild = Initializer.OfflineData == nullptr;		
	RayTracingGeometryRHI = RHICreateRayTracingGeometry(Initializer);
}

void FRayTracingGeometry::RequestBuildIfNeeded(ERTAccelerationStructureBuildPriority InBuildPriority)
{
	RayTracingGeometryRHI->SetInitializer(Initializer);

	if (bRequiresBuild)
	{
		RayTracingBuildRequestIndex = GRayTracingGeometryManager.RequestBuildAccelerationStructure(this, InBuildPriority);
		bRequiresBuild = false;
	}	
}

void FRayTracingGeometry::CreateRayTracingGeometry(ERTAccelerationStructureBuildPriority InBuildPriority)
{
	// Release previous RHI object if any
	ReleaseRHI();

	check(RawData.Num() == 0 || Initializer.OfflineData == nullptr);
	if (RawData.Num())
	{
		Initializer.OfflineData = &RawData;
	}

	if (GVarDebugForceRuntimeBLAS && Initializer.OfflineData != nullptr)
	{
		Initializer.OfflineData->Discard();
		Initializer.OfflineData = nullptr;
	}

	bool bAllSegmentsAreValid = Initializer.Segments.Num() > 0 || Initializer.OfflineData;
	for (const FRayTracingGeometrySegment& Segment : Initializer.Segments)
	{
		if (!Segment.VertexBuffer)
		{
			bAllSegmentsAreValid = false;
			break;
		}
	}

	const bool bWithoutNativeResource = Initializer.Type == ERayTracingGeometryInitializerType::StreamingDestination;
	if (bAllSegmentsAreValid)
	{
		bValid = !bWithoutNativeResource;
		RayTracingGeometryRHI = RHICreateRayTracingGeometry(Initializer);
		if (Initializer.OfflineData == nullptr)
		{
			// Request build if not skip
			if (InBuildPriority != ERTAccelerationStructureBuildPriority::Skip)
			{
				RayTracingBuildRequestIndex = GRayTracingGeometryManager.RequestBuildAccelerationStructure(this, InBuildPriority);
				bRequiresBuild = false;
			}
			else
			{
				bRequiresBuild = true;
			}
		}
		else
		{
			bRequiresBuild = false;

			// Offline data ownership is transferred to the RHI, which discards it after use.
			// It is no longer valid to use it after this point.
			Initializer.OfflineData = nullptr;
		}
	}
}

bool FRayTracingGeometry::IsValid() const
{
	return RayTracingGeometryRHI != nullptr && Initializer.TotalPrimitiveCount > 0 && bValid;
}

void FRayTracingGeometry::InitRHI()
{
	if (!IsRayTracingEnabled())
		return;

	ERTAccelerationStructureBuildPriority BuildPriority = Initializer.Type != ERayTracingGeometryInitializerType::Rendering
		? ERTAccelerationStructureBuildPriority::Skip
		: ERTAccelerationStructureBuildPriority::Normal;
	CreateRayTracingGeometry(BuildPriority);
}

void FRayTracingGeometry::ReleaseRHI()
{
	RemoveBuildRequest();
	RayTracingGeometryRHI.SafeRelease();
}

void FRayTracingGeometry::RemoveBuildRequest()
{
	if (HasPendingBuildRequest())
	{
		GRayTracingGeometryManager.RemoveBuildRequest(RayTracingBuildRequestIndex);
		RayTracingBuildRequestIndex = INDEX_NONE;
	}
}

void FRayTracingGeometry::ReleaseResource()
{
	// Release any resource references held by the initializer.
	// This includes index and vertex buffers used for building the BLAS.
	Initializer = FRayTracingGeometryInitializer {};

	FRenderResource::ReleaseResource();
}

void FRayTracingGeometry::BoostBuildPriority(float InBoostValue) const
{
	check(HasPendingBuildRequest());
	GRayTracingGeometryManager.BoostPriority(RayTracingBuildRequestIndex, InBoostValue);
}

#endif // RHI_RAYTRACING

/*------------------------------------------------------------------------------
	FGlobalDynamicVertexBuffer implementation.
------------------------------------------------------------------------------*/

/**
 * An individual dynamic vertex buffer.
 */
class FDynamicVertexBuffer : public FVertexBuffer
{
public:
	/** The aligned size of all dynamic vertex buffers. */
	enum { ALIGNMENT = (1 << 16) }; // 64KB
	/** Pointer to the vertex buffer mapped in main memory. */
	uint8* MappedBuffer;
	/** Size of the vertex buffer in bytes. */
	uint32 BufferSize;
	/** Number of bytes currently allocated from the buffer. */
	uint32 AllocatedByteCount;
	/** Number of successive frames for which AllocatedByteCount == 0. Used as a metric to decide when to free the allocation. */
	int32 NumFramesUnused = 0;

	/** Default constructor. */
	explicit FDynamicVertexBuffer(uint32 InMinBufferSize)
		: MappedBuffer(NULL)
		, BufferSize(FMath::Max<uint32>(Align(InMinBufferSize,ALIGNMENT),ALIGNMENT))
		, AllocatedByteCount(0)
	{
	}

	/**
	 * Locks the vertex buffer so it may be written to.
	 */
	void Lock()
	{
		check(MappedBuffer == NULL);
		check(AllocatedByteCount == 0);
		check(IsValidRef(VertexBufferRHI));
		MappedBuffer = (uint8*)RHILockBuffer(VertexBufferRHI, 0, BufferSize, RLM_WriteOnly);
	}

	/**
	 * Unocks the buffer so the GPU may read from it.
	 */
	void Unlock()
	{
		check(MappedBuffer != NULL);
		check(IsValidRef(VertexBufferRHI));
		RHIUnlockBuffer(VertexBufferRHI);
		MappedBuffer = NULL;
		AllocatedByteCount = 0;
		NumFramesUnused = 0;
	}

	// FRenderResource interface.
	virtual void InitRHI() override
	{
		check(!IsValidRef(VertexBufferRHI));
		FRHIResourceCreateInfo CreateInfo(TEXT("FDynamicVertexBuffer"));
		VertexBufferRHI = RHICreateVertexBuffer(BufferSize, BUF_Volatile, CreateInfo);
		MappedBuffer = NULL;
		AllocatedByteCount = 0;
	}

	virtual void ReleaseRHI() override
	{
		FVertexBuffer::ReleaseRHI();
		MappedBuffer = NULL;
		AllocatedByteCount = 0;
	}

	virtual FString GetFriendlyName() const override
	{
		return TEXT("FDynamicVertexBuffer");
	}
};

/**
 * A pool of dynamic vertex buffers.
 */
struct FDynamicVertexBufferPool
{
	/** List of vertex buffers. */
	TIndirectArray<FDynamicVertexBuffer> VertexBuffers;
	/** The current buffer from which allocations are being made. */
	FDynamicVertexBuffer* CurrentVertexBuffer;

	/** Default constructor. */
	FDynamicVertexBufferPool()
		: CurrentVertexBuffer(NULL)
	{
	}

	/** Destructor. */
	~FDynamicVertexBufferPool()
	{
		int32 NumVertexBuffers = VertexBuffers.Num();
		for (int32 BufferIndex = 0; BufferIndex < NumVertexBuffers; ++BufferIndex)
		{
			VertexBuffers[BufferIndex].ReleaseResource();
		}
	}
};

FGlobalDynamicVertexBuffer::FGlobalDynamicVertexBuffer()
	: TotalAllocatedSinceLastCommit(0)
{
	Pool = new FDynamicVertexBufferPool();
}

FGlobalDynamicVertexBuffer::~FGlobalDynamicVertexBuffer()
{
	delete Pool;
	Pool = NULL;
}

FGlobalDynamicVertexBuffer::FAllocation FGlobalDynamicVertexBuffer::Allocate(uint32 SizeInBytes)
{
	FAllocation Allocation;

	TotalAllocatedSinceLastCommit += SizeInBytes;
	if (IsRenderAlarmLoggingEnabled())
	{
		UE_LOG(LogRendererCore, Warning, TEXT("FGlobalDynamicVertexBuffer::Allocate(%u), will have allocated %u total this frame"), SizeInBytes, TotalAllocatedSinceLastCommit);
	}

	FDynamicVertexBuffer* VertexBuffer = Pool->CurrentVertexBuffer;
	if (VertexBuffer == NULL || VertexBuffer->AllocatedByteCount + SizeInBytes > VertexBuffer->BufferSize)
	{
		// Find a buffer in the pool big enough to service the request.
		VertexBuffer = NULL;
		for (int32 BufferIndex = 0, NumBuffers = Pool->VertexBuffers.Num(); BufferIndex < NumBuffers; ++BufferIndex)
		{
			FDynamicVertexBuffer& VertexBufferToCheck = Pool->VertexBuffers[BufferIndex];
			if (VertexBufferToCheck.AllocatedByteCount + SizeInBytes <= VertexBufferToCheck.BufferSize)
			{
				VertexBuffer = &VertexBufferToCheck;
				break;
			}
		}

		// Create a new vertex buffer if needed.
		if (VertexBuffer == NULL)
		{
			VertexBuffer = new FDynamicVertexBuffer(SizeInBytes);
			Pool->VertexBuffers.Add(VertexBuffer);
			VertexBuffer->InitResource();
		}

		// Lock the buffer if needed.
		if (VertexBuffer->MappedBuffer == NULL)
		{
			VertexBuffer->Lock();
		}

		// Remember this buffer, we'll try to allocate out of it in the future.
		Pool->CurrentVertexBuffer = VertexBuffer;
	}

	check(VertexBuffer != NULL);
	checkf(VertexBuffer->AllocatedByteCount + SizeInBytes <= VertexBuffer->BufferSize, TEXT("Global vertex buffer allocation failed: BufferSize=%d AllocatedByteCount=%d SizeInBytes=%d"), VertexBuffer->BufferSize, VertexBuffer->AllocatedByteCount, SizeInBytes);
	Allocation.Buffer = VertexBuffer->MappedBuffer + VertexBuffer->AllocatedByteCount;
	Allocation.VertexBuffer = VertexBuffer;
	Allocation.VertexOffset = VertexBuffer->AllocatedByteCount;
	VertexBuffer->AllocatedByteCount += SizeInBytes;

	return Allocation;
}

bool FGlobalDynamicVertexBuffer::IsRenderAlarmLoggingEnabled() const
{
	return GMaxVertexBytesAllocatedPerFrame > 0 && TotalAllocatedSinceLastCommit >= (size_t)GMaxVertexBytesAllocatedPerFrame;
}

void FGlobalDynamicVertexBuffer::Commit()
{
	for (int32 BufferIndex = 0, NumBuffers = Pool->VertexBuffers.Num(); BufferIndex < NumBuffers; ++BufferIndex)
	{
		FDynamicVertexBuffer& VertexBuffer = Pool->VertexBuffers[BufferIndex];
		if (VertexBuffer.MappedBuffer != NULL)
		{
			VertexBuffer.Unlock();
		}
		else if (GGlobalBufferNumFramesUnusedThresold && !VertexBuffer.AllocatedByteCount)
		{
			++VertexBuffer.NumFramesUnused;
			if (VertexBuffer.NumFramesUnused >= GGlobalBufferNumFramesUnusedThresold)
			{
				// Remove the buffer, assumes they are unordered.
				VertexBuffer.ReleaseResource();
				Pool->VertexBuffers.RemoveAtSwap(BufferIndex);
				--BufferIndex;
				--NumBuffers;
			}
		}
	}
	Pool->CurrentVertexBuffer = NULL;
	TotalAllocatedSinceLastCommit = 0;
}

FGlobalDynamicVertexBuffer InitViewDynamicVertexBuffer;
FGlobalDynamicVertexBuffer InitShadowViewDynamicVertexBuffer;

/*------------------------------------------------------------------------------
	FGlobalDynamicIndexBuffer implementation.
------------------------------------------------------------------------------*/

/**
 * An individual dynamic index buffer.
 */
class FDynamicIndexBuffer : public FIndexBuffer
{
public:
	/** The aligned size of all dynamic index buffers. */
	enum { ALIGNMENT = (1 << 16) }; // 64KB
	/** Pointer to the index buffer mapped in main memory. */
	uint8* MappedBuffer;
	/** Size of the index buffer in bytes. */
	uint32 BufferSize;
	/** Number of bytes currently allocated from the buffer. */
	uint32 AllocatedByteCount;
	/** Stride of the buffer in bytes. */
	uint32 Stride;
	/** Number of successive frames for which AllocatedByteCount == 0. Used as a metric to decide when to free the allocation. */
	int32 NumFramesUnused = 0;

	/** Initialization constructor. */
	explicit FDynamicIndexBuffer(uint32 InMinBufferSize, uint32 InStride)
		: MappedBuffer(NULL)
		, BufferSize(FMath::Max<uint32>(Align(InMinBufferSize,ALIGNMENT),ALIGNMENT))
		, AllocatedByteCount(0)
		, Stride(InStride)
	{
	}

	/**
	 * Locks the vertex buffer so it may be written to.
	 */
	void Lock()
	{
		check(MappedBuffer == NULL);
		check(AllocatedByteCount == 0);
		check(IsValidRef(IndexBufferRHI));
		MappedBuffer = (uint8*)RHILockBuffer(IndexBufferRHI, 0, BufferSize, RLM_WriteOnly);
	}

	/**
	 * Unocks the buffer so the GPU may read from it.
	 */
	void Unlock()
	{
		check(MappedBuffer != NULL);
		check(IsValidRef(IndexBufferRHI));
		RHIUnlockBuffer(IndexBufferRHI);
		MappedBuffer = NULL;
		AllocatedByteCount = 0;
		NumFramesUnused = 0;
	}

	// FRenderResource interface.
	virtual void InitRHI() override
	{
		check(!IsValidRef(IndexBufferRHI));
		FRHIResourceCreateInfo CreateInfo(TEXT("FDynamicIndexBuffer"));
		IndexBufferRHI = RHICreateIndexBuffer(Stride, BufferSize, BUF_Volatile, CreateInfo);
		MappedBuffer = NULL;
		AllocatedByteCount = 0;
	}

	virtual void ReleaseRHI() override
	{
		FIndexBuffer::ReleaseRHI();
		MappedBuffer = NULL;
		AllocatedByteCount = 0;
	}

	virtual FString GetFriendlyName() const override
	{
		return TEXT("FDynamicIndexBuffer");
	}
};

/**
 * A pool of dynamic index buffers.
 */
struct FDynamicIndexBufferPool
{
	/** List of index buffers. */
	TIndirectArray<FDynamicIndexBuffer> IndexBuffers;
	/** The current buffer from which allocations are being made. */
	FDynamicIndexBuffer* CurrentIndexBuffer;
	/** Stride of buffers in this pool. */
	uint32 BufferStride;

	/** Initialization constructor. */
	explicit FDynamicIndexBufferPool(uint32 InBufferStride)
		: CurrentIndexBuffer(NULL)
		, BufferStride(InBufferStride)
	{
	}

	/** Destructor. */
	~FDynamicIndexBufferPool()
	{
		int32 NumIndexBuffers = IndexBuffers.Num();
		for (int32 BufferIndex = 0; BufferIndex < NumIndexBuffers; ++BufferIndex)
		{
			IndexBuffers[BufferIndex].ReleaseResource();
		}
	}
};

FGlobalDynamicIndexBuffer::FGlobalDynamicIndexBuffer()
{
	Pools[0] = new FDynamicIndexBufferPool(sizeof(uint16));
	Pools[1] = new FDynamicIndexBufferPool(sizeof(uint32));
}

FGlobalDynamicIndexBuffer::~FGlobalDynamicIndexBuffer()
{
	for (int32 i = 0; i < 2; ++i)
	{
		delete Pools[i];
		Pools[i] = NULL;
	}
}

FGlobalDynamicIndexBuffer::FAllocation FGlobalDynamicIndexBuffer::Allocate(uint32 NumIndices, uint32 IndexStride)
{
	FAllocation Allocation;

	if (IndexStride != 2 && IndexStride != 4)
	{
		return Allocation;
	}

	FDynamicIndexBufferPool* Pool = Pools[IndexStride >> 2]; // 2 -> 0, 4 -> 1

	uint32 SizeInBytes = NumIndices * IndexStride;
	FDynamicIndexBuffer* IndexBuffer = Pool->CurrentIndexBuffer;
	if (IndexBuffer == NULL || IndexBuffer->AllocatedByteCount + SizeInBytes > IndexBuffer->BufferSize)
	{
		// Find a buffer in the pool big enough to service the request.
		IndexBuffer = NULL;
		for (int32 BufferIndex = 0, NumBuffers = Pool->IndexBuffers.Num(); BufferIndex < NumBuffers; ++BufferIndex)
		{
			FDynamicIndexBuffer& IndexBufferToCheck = Pool->IndexBuffers[BufferIndex];
			if (IndexBufferToCheck.AllocatedByteCount + SizeInBytes <= IndexBufferToCheck.BufferSize)
			{
				IndexBuffer = &IndexBufferToCheck;
				break;
			}
		}

		// Create a new index buffer if needed.
		if (IndexBuffer == NULL)
		{
			IndexBuffer = new FDynamicIndexBuffer(SizeInBytes, Pool->BufferStride);
			Pool->IndexBuffers.Add(IndexBuffer);
			IndexBuffer->InitResource();
		}

		// Lock the buffer if needed.
		if (IndexBuffer->MappedBuffer == NULL)
		{
			IndexBuffer->Lock();
		}
		Pool->CurrentIndexBuffer = IndexBuffer;
	}

	check(IndexBuffer != NULL);
	checkf(IndexBuffer->AllocatedByteCount + SizeInBytes <= IndexBuffer->BufferSize, TEXT("Global index buffer allocation failed: BufferSize=%d AllocatedByteCount=%d SizeInBytes=%d"), IndexBuffer->BufferSize, IndexBuffer->AllocatedByteCount, SizeInBytes);

	Allocation.Buffer = IndexBuffer->MappedBuffer + IndexBuffer->AllocatedByteCount;
	Allocation.IndexBuffer = IndexBuffer;
	Allocation.FirstIndex = IndexBuffer->AllocatedByteCount / IndexStride;
	IndexBuffer->AllocatedByteCount += SizeInBytes;

	return Allocation;
}

void FGlobalDynamicIndexBuffer::Commit()
{
	for (int32 i = 0; i < 2; ++i)
	{
		FDynamicIndexBufferPool* Pool = Pools[i];

		for (int32 BufferIndex = 0, NumBuffers = Pool->IndexBuffers.Num(); BufferIndex < NumBuffers; ++BufferIndex)
		{
			FDynamicIndexBuffer& IndexBuffer = Pool->IndexBuffers[BufferIndex];
			if (IndexBuffer.MappedBuffer != NULL)
			{
				IndexBuffer.Unlock();
			}
			else if (GGlobalBufferNumFramesUnusedThresold && !IndexBuffer.AllocatedByteCount)
			{
				++IndexBuffer.NumFramesUnused;
				if (IndexBuffer.NumFramesUnused >= GGlobalBufferNumFramesUnusedThresold)
				{
					// Remove the buffer, assumes they are unordered.
					IndexBuffer.ReleaseResource();
					Pool->IndexBuffers.RemoveAtSwap(BufferIndex);
					--BufferIndex;
					--NumBuffers;
				}
			}
		}
		Pool->CurrentIndexBuffer = NULL;
	}
}

/*=============================================================================
	FMipBiasFade class
=============================================================================*/

/** Global mip fading settings, indexed by EMipFadeSettings. */
FMipFadeSettings GMipFadeSettings[MipFade_NumSettings] =
{ 
	FMipFadeSettings(0.3f, 0.1f),	// MipFade_Normal
	FMipFadeSettings(2.0f, 1.0f)	// MipFade_Slow
};

/** How "old" a texture must be to be considered a "new texture", in seconds. */
float GMipLevelFadingAgeThreshold = 0.5f;

/**
 *	Sets up a new interpolation target for the mip-bias.
 *	@param ActualMipCount	Number of mip-levels currently in memory
 *	@param TargetMipCount	Number of mip-levels we're changing to
 *	@param LastRenderTime	Timestamp when it was last rendered (FApp::CurrentTime time space)
 *	@param FadeSetting		Which fade speed settings to use
 */
void FMipBiasFade::SetNewMipCount( float ActualMipCount, float TargetMipCount, double LastRenderTime, EMipFadeSettings FadeSetting )
{
	check( ActualMipCount >=0 && TargetMipCount <= ActualMipCount );

	float TimeSinceLastRendered = float(FApp::GetCurrentTime() - LastRenderTime);

	// Is this a new texture or is this not in-game?
	if ( TotalMipCount == 0 || TimeSinceLastRendered >= GMipLevelFadingAgeThreshold || GEnableMipLevelFading < 0.0f )
	{
		// No fading.
		TotalMipCount = ActualMipCount;
		MipCountDelta = 0.0f;
		MipCountFadingRate = 0.0f;
		StartTime = GRenderingRealtimeClock.GetCurrentTime();
		BiasOffset = 0.0f;
		return;
	}

	// Calculate the mipcount we're interpolating towards.
	float CurrentTargetMipCount = TotalMipCount - BiasOffset + MipCountDelta;

	// Is there no change?
	if ( FMath::IsNearlyEqual(TotalMipCount, ActualMipCount) && FMath::IsNearlyEqual(TargetMipCount, CurrentTargetMipCount) )
	{
		return;
	}

	// Calculate the mip-count at our current interpolation point.
	float CurrentInterpolatedMipCount = TotalMipCount - CalcMipBias();

	// Clamp it against the available mip-levels.
	CurrentInterpolatedMipCount = FMath::Clamp<float>(CurrentInterpolatedMipCount, 0, ActualMipCount);

	// Set up a new interpolation from CurrentInterpolatedMipCount to TargetMipCount.
	StartTime = GRenderingRealtimeClock.GetCurrentTime();
	TotalMipCount = ActualMipCount;
	MipCountDelta = TargetMipCount - CurrentInterpolatedMipCount;

	// Don't fade if we're already at the target mip-count.
	if ( FMath::IsNearlyZero(MipCountDelta) )
	{
		MipCountDelta = 0.0f;
		BiasOffset = 0.0f;
		MipCountFadingRate = 0.0f;
	}
	else
	{
		BiasOffset = TotalMipCount - CurrentInterpolatedMipCount;
		if ( MipCountDelta > 0.0f )
		{
			MipCountFadingRate = 1.0f / (GMipFadeSettings[FadeSetting].FadeInSpeed * MipCountDelta);
		}
		else
		{
			MipCountFadingRate = -1.0f / (GMipFadeSettings[FadeSetting].FadeOutSpeed * MipCountDelta);
		}
	}
}

class FTextureSamplerStateCache : public FRenderResource
{
public:
	TMap<FSamplerStateInitializerRHI, FRHISamplerState*> Samplers;

	virtual void ReleaseRHI() override
	{
		for (auto Pair : Samplers)
		{
			Pair.Value->Release();
		}
		Samplers.Empty();
	}
};

TGlobalResource<FTextureSamplerStateCache> GTextureSamplerStateCache;

FRHISamplerState* FTexture::GetOrCreateSamplerState(const FSamplerStateInitializerRHI& Initializer)
{
	// This sampler cache is supposed to be used only from RT
	// Add a lock here if it's used from multiple threads
	check(IsInRenderingThread());
	
	FRHISamplerState** Found = GTextureSamplerStateCache.Samplers.Find(Initializer);
	if (Found)
	{
		return *Found;
	}
	
	FSamplerStateRHIRef NewState = RHICreateSamplerState(Initializer);
	
	// Add an extra reference so we don't have TRefCountPtr in the maps
	NewState->AddRef();
	GTextureSamplerStateCache.Samplers.Add(Initializer, NewState);
	return NewState;
}

bool IsRayTracingEnabled()
{
	checkf(GIsRHIInitialized, TEXT("IsRayTracingEnabled() may only be called once RHI is initialized."));

#if DO_CHECK && WITH_EDITOR
	{
		// This function must not be called while cooking
		if (IsRunningCookCommandlet())
		{
			return false;
		}
	}
#endif // DO_CHECK && WITH_EDITOR

	extern RENDERCORE_API bool GUseRayTracing;
	return GUseRayTracing;
}

bool IsRayTracingEnabled(EShaderPlatform ShaderPlatform)
{
	return IsRayTracingEnabled() && RHISupportsRayTracing(ShaderPlatform);
}
