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
#include "RenderGraphResources.h"
#include "Containers/ResourceArray.h"
#include "RenderCore.h"

/** Whether to enable mip-level fading or not: +1.0f if enabled, -1.0f if disabled. */
float GEnableMipLevelFading = 1.0f;

bool GFreeStructuresOnRHIBufferCreation = true;
FAutoConsoleVariableRef CVarFreeStructuresOnRHIBufferCreation(
	TEXT("r.FreeStructuresOnRHIBufferCreation"),
	GFreeStructuresOnRHIBufferCreation,
	TEXT("Toggles experimental method for freeing helper structures that own the resource arrays after submitting to RHI instead of in the callback sink."));

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

FRenderResource::FRenderResource()
	: ListIndex(INDEX_NONE)
	, FeatureLevel(ERHIFeatureLevel::Num)
{
}

FRenderResource::FRenderResource(ERHIFeatureLevel::Type InFeatureLevel)
	: ListIndex(INDEX_NONE)
	, FeatureLevel(InFeatureLevel)
{
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

bool FRenderResource::ShouldFreeResourceObject(void* ResourceObject, FResourceArrayInterface* ResourceArray)
{
	return GFreeStructuresOnRHIBufferCreation && ResourceObject && (!ResourceArray || !ResourceArray->GetResourceDataSize());
}

FBufferRHIRef FRenderResource::CreateRHIBufferInternal(
	const TCHAR* InDebugName,
	const FName& InOwnerName,
	uint32 ResourceCount,
	EBufferUsageFlags InBufferUsageFlags,
	FResourceArrayInterface* ResourceArray,
	bool bRenderThread,
	bool bWithoutNativeResource)
{
	const uint32 SizeInBytes = ResourceArray ? ResourceArray->GetResourceDataSize() : 0;
	FRHIResourceCreateInfo CreateInfo(InDebugName, ResourceArray);
	CreateInfo.bWithoutNativeResource = bWithoutNativeResource;

	FBufferRHIRef Buffer;
	if (bRenderThread)
	{
		Buffer = RHICreateVertexBuffer(SizeInBytes, InBufferUsageFlags, CreateInfo);
	}
	else
	{
		FRHIAsyncCommandList CommandList;
		Buffer = CommandList->CreateBuffer(SizeInBytes, InBufferUsageFlags | EBufferUsageFlags::VertexBuffer, 0, ERHIAccess::SRVMask, CreateInfo);
	}

	Buffer->SetOwnerName(InOwnerName);
	return Buffer;
}

void FRenderResource::SetOwnerName(const FName& InOwnerName)
{
#if RHI_ENABLE_RESOURCE_INFO
	OwnerName = InOwnerName;
#endif
}

FName FRenderResource::GetOwnerName() const
{
#if RHI_ENABLE_RESOURCE_INFO
	return OwnerName;
#else
	return NAME_None;
#endif
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

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FTextureSamplerStateCache

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

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FTexture

FTexture::FTexture() = default;
FTexture::~FTexture() = default;

uint32 FTexture::GetSizeX() const
{
	return 0;
}

uint32 FTexture::GetSizeY() const
{
	return 0;
}

uint32 FTexture::GetSizeZ() const
{
	return 0;
}

void FTexture::ReleaseRHI()
{
	TextureRHI.SafeRelease();
	SamplerStateRHI.SafeRelease();
	DeferredPassSamplerStateRHI.SafeRelease();
}

FString FTexture::GetFriendlyName() const
{
	return TEXT("FTexture");
}

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

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FTextureWithSRV

FTextureWithSRV::FTextureWithSRV() = default;
FTextureWithSRV::~FTextureWithSRV() = default;

void FTextureWithSRV::ReleaseRHI()
{
	ShaderResourceViewRHI.SafeRelease();
	FTexture::ReleaseRHI();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FTextureReference

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

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FVertexBuffer

FVertexBuffer::FVertexBuffer() = default;
FVertexBuffer::~FVertexBuffer() = default;

void FVertexBuffer::ReleaseRHI()
{
	VertexBufferRHI.SafeRelease();
}

FString FVertexBuffer::GetFriendlyName() const
{
	return TEXT("FVertexBuffer");
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FVertexBufferWithSRV

FVertexBufferWithSRV::FVertexBufferWithSRV() = default;
FVertexBufferWithSRV::~FVertexBufferWithSRV() = default;

void FVertexBufferWithSRV::ReleaseRHI()
{
	ShaderResourceViewRHI.SafeRelease();
	UnorderedAccessViewRHI.SafeRelease();
	FVertexBuffer::ReleaseRHI();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FIndexBuffer

FIndexBuffer::FIndexBuffer() = default;
FIndexBuffer::~FIndexBuffer() = default;

void FIndexBuffer::ReleaseRHI()
{
	IndexBufferRHI.SafeRelease();
}

FString FIndexBuffer::GetFriendlyName() const
{
	return TEXT("FIndexBuffer");
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FBufferWithRDG

FBufferWithRDG::FBufferWithRDG() = default;
FBufferWithRDG::FBufferWithRDG(const FBufferWithRDG& Other) = default;
FBufferWithRDG& FBufferWithRDG::operator=(const FBufferWithRDG& Other) = default;
FBufferWithRDG::~FBufferWithRDG() = default;

void FBufferWithRDG::ReleaseRHI()
{
	Buffer = nullptr;
	FRenderResource::ReleaseRHI();
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
