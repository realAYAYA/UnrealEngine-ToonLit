// Copyright Epic Games, Inc. All Rights Reserved.

#include "VirtualShadowMapCacheManager.h"
#include "VirtualShadowMapClipmap.h"
#include "VirtualShadowMapShaders.h"
#include "RendererModule.h"
#include "RenderGraphUtils.h"
#include "RHIGPUReadback.h"
#include "ScenePrivate.h"
#include "HAL/FileManager.h"
#include "NaniteDefinitions.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "PrimitiveSceneInfo.h"
#include "ShaderPrint.h"
#include "RendererOnScreenNotification.h"
#include "SystemTextures.h"

CSV_DECLARE_CATEGORY_EXTERN(VSM);

static TAutoConsoleVariable<int32> CVarAccumulateStats(
	TEXT("r.Shadow.Virtual.AccumulateStats"),
	0,
	TEXT("When enabled, VSM stats will be collected over multiple frames and written to a CSV file"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarCacheVirtualSMs(
	TEXT("r.Shadow.Virtual.Cache"),
	1,
	TEXT("Turn on to enable caching"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarDrawInvalidatingBounds(
	TEXT("r.Shadow.Virtual.Cache.DrawInvalidatingBounds"),
	0,
	TEXT("Turn on debug render cache invalidating instance bounds, heat mapped by number of pages invalidated.\n")
	TEXT("   1  = Draw all bounds.\n")
	TEXT("   2  = Draw those invalidating static cached pages only\n")
	TEXT("   3  = Draw those invalidating dynamic cached pages only"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarCacheVsmUseHzb(
	TEXT("r.Shadow.Virtual.Cache.InvalidateUseHZB"),
	1,
	TEXT(" When enabled, instances invalidations are tested against the HZB. Instances that are fully occluded will not cause page invalidations."),
	ECVF_RenderThreadSafe);

int32 GClipmapPanning = 1;
FAutoConsoleVariableRef CVarEnableClipmapPanning(
	TEXT("r.Shadow.Virtual.Cache.ClipmapPanning"),
	GClipmapPanning,
	TEXT("Enable support for panning cached clipmap pages for directional lights, allowing re-use of cached data when the camera moves. Keep this enabled outside of debugging."),
	ECVF_RenderThreadSafe
);

static int32 GVSMCacheDeformableMeshesInvalidate = 1;
FAutoConsoleVariableRef CVarCacheInvalidateOftenMoving(
	TEXT("r.Shadow.Virtual.Cache.DeformableMeshesInvalidate"),
	GVSMCacheDeformableMeshesInvalidate,
	TEXT("If enabled, Primitive Proxies that are marked as having deformable meshes (HasDeformableMesh() == true) cause invalidations regardless of whether their transforms are updated."),
	ECVF_RenderThreadSafe);

int32 GForceInvalidateDirectionalVSM = 0;
static FAutoConsoleVariableRef  CVarForceInvalidateDirectionalVSM(
	TEXT("r.Shadow.Virtual.Cache.ForceInvalidateDirectional"),
	GForceInvalidateDirectionalVSM,
	TEXT("Forces the clipmap to always invalidate, useful to emulate a moving sun to avoid misrepresenting cache performance."),
	ECVF_RenderThreadSafe);

// We give a little leeway here as occasionally the scene frame number is incremented multiple times between frames
int32 GVSMMaxPageAgeSinceLastRequest = 3;
FAutoConsoleVariableRef CVarVSMMaxPageAgeSinceLastRequest(
	TEXT("r.Shadow.Virtual.Cache.MaxPageAgeSinceLastRequest"),
	GVSMMaxPageAgeSinceLastRequest,
	TEXT("The maximum number of frames to allow cached pages that aren't requested in the current frame to live. 0=disabled."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarFramesStaticThreshold(
	TEXT("r.Shadow.Virtual.Cache.StaticSeparate.FramesStaticThreshold"),
	100,
	TEXT("Number of frames without an invalidation before an object will transition to static caching."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarVSMReservedResource(
	TEXT("r.Shadow.Virtual.AllocatePagePoolAsReservedResource"),
	1,
	TEXT("Allocate VSM page pool as a reserved/virtual texture, backed by N small physical memory allocations to reduce fragmentation."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarVSMDynamicResolutionMaxLodBias(
	TEXT("r.Shadow.Virtual.DynamicRes.MaxResolutionLodBias"),
	2.0f,
	TEXT("As page allocation approaches the pool capacity, VSM resolution ramps down by biasing the LOD up, similar to 'ResolutionLodBiasDirectional'.\n")
	TEXT("This is the maximum LOD bias to clamp to for global dynamic shadow resolution reduction. 0 = disabled"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarVSMDynamicResolutionMaxPagePoolLoadFactor(
	TEXT("r.Shadow.Virtual.DynamicRes.MaxPagePoolLoadFactor"),
	0.85f,
	TEXT("If allocation exceeds this factor of total page pool capacity, shadow resolution will be biased downwards. 0 = disabled"),
	ECVF_RenderThreadSafe
);

namespace Nanite
{
	extern bool IsStatFilterActive(const FString& FilterName);
}

void FVirtualShadowMapCacheEntry::UpdateClipmapLevel(
	FVirtualShadowMapArray& VirtualShadowMapArray,
	const FVirtualShadowMapPerLightCacheEntry& PerLightEntry,
	int32 VirtualShadowMapId,
	FInt64Point PageSpaceLocation,
	double LevelRadius,
	double ViewCenterZ,
	double ViewRadiusZ,
	double WPODistanceDisableThresholdSquared)
{
	int32 PrevVirtualShadowMapId = CurrentVirtualShadowMapId;
	FInt64Point PrevPageSpaceLocation = Clipmap.PageSpaceLocation;
	PrevHZBMetadata = CurrentHZBMetadata;

	bool bCacheValid = (PrevVirtualShadowMapId != INDEX_NONE);
	
	if (bCacheValid && GClipmapPanning == 0)
	{
		if (PageSpaceLocation.X != PrevPageSpaceLocation.X ||
			PageSpaceLocation.Y != PrevPageSpaceLocation.Y)
		{
			bCacheValid = false;
			//UE_LOG(LogRenderer, Display, TEXT("Invalidated clipmap level (VSM %d) with page space location %d,%d (Prev %d, %d)"),
			//	VirtualShadowMapId, PageSpaceLocation.X, PageSpaceLocation.Y, PrevPageSpaceLocation.X, PrevPageSpaceLocation.Y);
		}
	}

	// Invalidate if the new Z radius strayed too close/outside the guardband of the cached shadow map
	if (bCacheValid)
	{
		double DeltaZ = FMath::Abs(ViewCenterZ - Clipmap.ViewCenterZ);
		if ((DeltaZ + LevelRadius) > 0.9 * Clipmap.ViewRadiusZ)
		{
			bCacheValid = false;
			//UE_LOG(LogRenderer, Display, TEXT("Invalidated clipmap level (VSM %d) due to depth range movement"), VirtualShadowMapId);
		}
	}

	// Not valid if it was never rendered
	bCacheValid = bCacheValid && (PerLightEntry.Prev.RenderedFrameNumber >= 0);

	// Not valid if radius has changed
	bCacheValid = bCacheValid && (ViewRadiusZ == Clipmap.ViewRadiusZ);

	// Not valid if WPO threshold has changed
	if (bCacheValid && (WPODistanceDisableThresholdSquared != Clipmap.WPODistanceDisableThresholdSquared))
	{
		bCacheValid = false;
		// Only warn once per change... when this changes it will hit all of them
		if (PerLightEntry.ShadowMapEntries.Num() > 0 && PerLightEntry.ShadowMapEntries[0].CurrentVirtualShadowMapId == VirtualShadowMapId)
		{
			UE_LOG(LogRenderer, Display, TEXT("Invalidated clipmap due to WPO threshold change. This can occur due to resolution or FOV changes."), VirtualShadowMapId);
		}
	}

	if (!bCacheValid)
	{
		Clipmap.ViewCenterZ = ViewCenterZ;
		Clipmap.ViewRadiusZ = ViewRadiusZ;
		Clipmap.WPODistanceDisableThresholdSquared = WPODistanceDisableThresholdSquared;
	}
	else
	{
		// NOTE: Leave the view center and radius where they were previously for the cached page
		FInt64Point CurrentToPreviousPageOffset(PageSpaceLocation - PrevPageSpaceLocation);
		VirtualShadowMapArray.UpdateNextData(PrevVirtualShadowMapId, VirtualShadowMapId, FInt32Point(CurrentToPreviousPageOffset));
	}
	
	CurrentVirtualShadowMapId = VirtualShadowMapId;
	Clipmap.PageSpaceLocation = PageSpaceLocation;
}

void FVirtualShadowMapCacheEntry::Update(
	FVirtualShadowMapArray& VirtualShadowMapArray,
	const FVirtualShadowMapPerLightCacheEntry& PerLightEntry,
	int32 VirtualShadowMapId)
{
	// Swap previous frame data over.
	int32 PrevVirtualShadowMapId = CurrentVirtualShadowMapId;

	// TODO: This is pretty wrong specifically for unreferenced lights, as the VSM IDs will have changed and not been updated since
	// this gets updated by rendering! Need to figure out a better way to track this data, and probably not here...
	PrevHZBMetadata = CurrentHZBMetadata;
	
	bool bCacheValid = (PrevVirtualShadowMapId != INDEX_NONE);

	// Not valid if it was never rendered
	bCacheValid = bCacheValid && (PerLightEntry.Prev.RenderedFrameNumber >= 0);

	if (bCacheValid)
	{
		// Invalidate on transition between single page and full
		bool bPrevSinglePage = FVirtualShadowMapArray::IsSinglePage(PrevVirtualShadowMapId);
		bool bCurrentSinglePage = FVirtualShadowMapArray::IsSinglePage(VirtualShadowMapId);
		if (bPrevSinglePage != bCurrentSinglePage)
		{
			bCacheValid = false;
		}
	}

	if (bCacheValid)
	{
		// Update previous/next frame mapping if we have a valid cached shadow map
		VirtualShadowMapArray.UpdateNextData(PrevVirtualShadowMapId, VirtualShadowMapId);
	}

	CurrentVirtualShadowMapId = VirtualShadowMapId;
	// Current HZB metadata gets updated during rendering
}

void FVirtualShadowMapCacheEntry::SetHZBViewParams(Nanite::FPackedViewParams& OutParams)
{
	OutParams.PrevTargetLayerIndex = PrevHZBMetadata.TargetLayerIndex;
	OutParams.PrevViewMatrices = PrevHZBMetadata.ViewMatrices;
	OutParams.Flags |= NANITE_VIEW_FLAG_HZBTEST;
}

void FVirtualShadowMapPerLightCacheEntry::UpdateClipmap(const FVector& LightDirection, int FirstLevel)
{
	Prev.RenderedFrameNumber = FMath::Max(Prev.RenderedFrameNumber, Current.RenderedFrameNumber);
	Current.RenderedFrameNumber = -1;

	if (GForceInvalidateDirectionalVSM != 0 ||
		LightDirection != ClipmapCacheKey.LightDirection ||
		FirstLevel != ClipmapCacheKey.FirstLevel)
	{
		Prev.RenderedFrameNumber = -1;
	}
	ClipmapCacheKey.LightDirection = LightDirection;
	ClipmapCacheKey.FirstLevel = FirstLevel;

	bool bNewIsUncached = GForceInvalidateDirectionalVSM != 0 || Prev.RenderedFrameNumber < 0;

	// On transition between uncached <-> cached we must invalidate since the static pages may not be initialized
	if (bNewIsUncached != bIsUncached)
	{
		Prev.RenderedFrameNumber = -1;
	}
	bIsUncached = bNewIsUncached;
}

bool FVirtualShadowMapPerLightCacheEntry::UpdateLocal(const FProjectedShadowInitializer& InCacheKey, bool bNewIsDistantLight, bool bCacheEnabled, bool bAllowInvalidation)
{
	// TODO: The logic in this function is needlessly convoluted... clean up

	Prev.RenderedFrameNumber = FMath::Max(Prev.RenderedFrameNumber, Current.RenderedFrameNumber);
	Prev.ScheduledFrameNumber = FMath::Max(Prev.ScheduledFrameNumber, Current.ScheduledFrameNumber);

	// Check cache validity based of shadow setup
	// If it is a distant light, we want to let the time-share perform the invalidation.
	if (!bCacheEnabled
		|| (bAllowInvalidation && !LocalCacheKey.IsCachedShadowValid(InCacheKey)))
	{
		// TODO: track invalidation state somehow for later.
		Prev.RenderedFrameNumber = -1;
		//UE_LOG(LogRenderer, Display, TEXT("Invalidated!"));
	}
	LocalCacheKey = InCacheKey;

	// On transition between uncached <-> cached we must invalidate since the static pages may not be initialized
	bool bNewIsUncached = Prev.RenderedFrameNumber < 0;
	if (bNewIsUncached != bIsUncached)
	{
		Prev.RenderedFrameNumber = -1;
	}

	// On transition between distant <-> regular we must invalidate
	if (bNewIsDistantLight != bIsDistantLight)
	{
		Prev.RenderedFrameNumber = -1;
	}

	Current.RenderedFrameNumber = -1;
	Current.ScheduledFrameNumber = -1;
	bIsDistantLight = bNewIsDistantLight;
	bIsUncached = bNewIsUncached;

	return Prev.RenderedFrameNumber >= 0;
}

void FVirtualShadowMapArrayCacheManager::FShadowInvalidatingInstancesImplementation::AddPrimitive(const FPrimitiveSceneInfo* PrimitiveSceneInfo)
{
	AddInstanceRange(PrimitiveSceneInfo->GetPersistentIndex(), PrimitiveSceneInfo->GetInstanceSceneDataOffset(), PrimitiveSceneInfo->GetNumInstanceSceneDataEntries());
}

void FVirtualShadowMapArrayCacheManager::FShadowInvalidatingInstancesImplementation::AddInstanceRange(FPersistentPrimitiveIndex PersistentPrimitiveIndex, uint32 InstanceSceneDataOffset, uint32 NumInstanceSceneDataEntries)
{
	PrimitiveInstancesToInvalidate.Add(FVirtualShadowMapInstanceRange{
		PersistentPrimitiveIndex,
		int32(InstanceSceneDataOffset),
		int32(NumInstanceSceneDataEntries)});
}

void FVirtualShadowMapPerLightCacheEntry::Invalidate()
{
	Prev.RenderedFrameNumber = -1;
}

static uint32 EncodeInstanceInvalidationPayload(int32 VirtualShadowMapId, uint32 Flags = VSM_INVALIDATION_PAYLOAD_FLAG_NONE)
{
	check(VirtualShadowMapId >= 0);		// Should not be INDEX_NONE by this point

	uint32 Payload = Flags;
	Payload = Payload | (uint32(VirtualShadowMapId) << VSM_INVALIDATION_PAYLOAD_FLAG_BITS);
	return Payload;
}

FVirtualShadowMapArrayCacheManager::FInvalidatingPrimitiveCollector::FInvalidatingPrimitiveCollector(FVirtualShadowMapArrayCacheManager* InVirtualShadowMapArrayCacheManager)
	: Scene(*InVirtualShadowMapArrayCacheManager->Scene)
	, GPUScene(InVirtualShadowMapArrayCacheManager->Scene->GPUScene)
	, Manager(*InVirtualShadowMapArrayCacheManager)
{
	uint32 Num = Manager.CachePrimitiveAsDynamic.Num();
	InvalidatedPrimitives.SetNum(Num, false);
	RemovedPrimitives.SetNum(Num, false);
}

void FVirtualShadowMapArrayCacheManager::FInvalidatingPrimitiveCollector::AddPrimitivesToInvalidate()
{	
	for (auto& CacheEntry : Manager.CacheEntries)
	{
		for (const auto& SmCacheEntry : CacheEntry.Value->ShadowMapEntries)
		{
			const uint32 Payload = EncodeInstanceInvalidationPayload(SmCacheEntry.CurrentVirtualShadowMapId);

			// Global invalidations
			for (const FVirtualShadowMapInstanceRange& Range : Manager.ShadowInvalidatingInstancesImplementation.PrimitiveInstancesToInvalidate)
			{
				Instances.Add(Range.InstanceSceneDataOffset, Range.NumInstanceSceneDataEntries, Payload);
				if (Range.PersistentPrimitiveIndex.IsValid())
				{
					InvalidatedPrimitives[Range.PersistentPrimitiveIndex.Index] = true;
				}
			}

			// Per-light invalidations
			for (const FVirtualShadowMapInstanceRange& Range : CacheEntry.Value->PrimitiveInstancesToInvalidate)
			{
				Instances.Add(Range.InstanceSceneDataOffset, Range.NumInstanceSceneDataEntries, Payload);
				check(Range.PersistentPrimitiveIndex.IsValid());		// Should always be valid currently in this path
				if (Range.PersistentPrimitiveIndex.IsValid())
				{
					InvalidatedPrimitives[Range.PersistentPrimitiveIndex.Index] = true;
				}
			}
		}
		CacheEntry.Value->PrimitiveInstancesToInvalidate.Reset();
	}
	Manager.ShadowInvalidatingInstancesImplementation.PrimitiveInstancesToInvalidate.Reset();
}

void FVirtualShadowMapArrayCacheManager::FInvalidatingPrimitiveCollector::AddInvalidation(FPrimitiveSceneInfo * PrimitiveSceneInfo, bool bRemovedPrimitive)
{
	const int32 PrimitiveID = PrimitiveSceneInfo->GetIndex();
	const int32 InstanceSceneDataOffset = PrimitiveSceneInfo->GetInstanceSceneDataOffset();
	const int32 NumInstanceSceneDataEntries = PrimitiveSceneInfo->GetNumInstanceSceneDataEntries();

	if (PrimitiveID < 0 || InstanceSceneDataOffset == INDEX_NONE)
	{
		return;
	}

	const FPersistentPrimitiveIndex PersistentPrimitiveIndex = PrimitiveSceneInfo->GetPersistentIndex();
	const EPrimitiveDirtyState DirtyState = GPUScene.GetPrimitiveDirtyState(PersistentPrimitiveIndex);

	if (bRemovedPrimitive)
	{
		RemovedPrimitives[PersistentPrimitiveIndex.Index] = true;
	}

	// TODO: Early out on stuff that doesn't cast shadows on the CPU?

	// Suppress invalidations from moved primitives that are marked to behave as if they were static.
	if (!bRemovedPrimitive && PrimitiveSceneInfo->Proxy->GetShadowCacheInvalidationBehavior() == EShadowCacheInvalidationBehavior::Static)
	{
		return;
	}

	// If it's "added" we can't invalidate pre-update since it will not be in GPUScene yet
	// Similarly if it is removed, we can't invalidate post-update
	// Otherwise we currently add it to both passes as we need to invalidate both the position it came from, and the position
	// it moved *to* if transform/instances changed. Both pages may need to updated.
	// TODO: Filter out one of the updates for things like WPO animation or other cases where the transform/bounds have not changed
	// TODO: Should we be using AddedScenePrimitives now instead of this flag?
	const bool bAddedPrimitive = EnumHasAnyFlags(DirtyState, EPrimitiveDirtyState::Added);
	
	if (bAddedPrimitive)
	{
		return;
	}

	//UE_LOG(LogRenderer, Warning, TEXT("Invalidating instances: %u, %u"), InstanceSceneDataOffset, NumInstanceSceneDataEntries);
	InvalidatedPrimitives[PersistentPrimitiveIndex.Index] = true;

	// Nanite meshes need special handling because they don't get culled on CPU, thus always process invalidations for those
	const bool bIsNaniteMesh = Scene.PrimitiveFlagsCompact[PrimitiveID].bIsNaniteMesh;

	for (auto& CacheEntry : Manager.CacheEntries)
	{
		TBitArray<>& CachedPrimitives = CacheEntry.Value->CachedPrimitives;
		if (bIsNaniteMesh || (PersistentPrimitiveIndex.Index < CachedPrimitives.Num() && CachedPrimitives[PersistentPrimitiveIndex.Index]))
		{
			if (!bIsNaniteMesh)
			{
				// Clear the record as we're wiping it out.
				CachedPrimitives[PersistentPrimitiveIndex.Index] = false;
			}

			// Add item for each shadow map explicitly, inflates host data but improves load balancing,
			for (const auto& SmCacheEntry : CacheEntry.Value->ShadowMapEntries)
			{
				Instances.Add(InstanceSceneDataOffset, NumInstanceSceneDataEntries, EncodeInstanceInvalidationPayload(SmCacheEntry.CurrentVirtualShadowMapId));
			}
		}
	}
}


FVirtualShadowMapFeedback::FVirtualShadowMapFeedback()
{
	for (int32 i = 0; i < MaxBuffers; ++i)
	{
		Buffers[i].Buffer = new FRHIGPUBufferReadback(TEXT("Shadow.Virtual.Readback"));
		Buffers[i].Size = 0;
	}
}

FVirtualShadowMapFeedback::~FVirtualShadowMapFeedback()
{
	for (int32 i = 0; i < MaxBuffers; ++i)
	{
		delete Buffers[i].Buffer;
		Buffers[i].Buffer = nullptr;
		Buffers[i].Size = 0;
	}
}

void FVirtualShadowMapFeedback::SubmitFeedbackBuffer(
	FRDGBuilder& GraphBuilder,
	FRDGBufferRef FeedbackBuffer)
{
	// Source copy usage is required for readback
	check((FeedbackBuffer->Desc.Usage & EBufferUsageFlags::SourceCopy) == EBufferUsageFlags::SourceCopy);

	if (NumPending == MaxBuffers)
	{
		return;
	}

	FRHIGPUBufferReadback* ReadbackBuffer = Buffers[WriteIndex].Buffer;
	Buffers[WriteIndex].Size = FeedbackBuffer->Desc.GetSize();

	AddReadbackBufferPass(GraphBuilder, RDG_EVENT_NAME("Readback"), FeedbackBuffer,
		[ReadbackBuffer, FeedbackBuffer](FRHICommandList& RHICmdList)
		{
			ReadbackBuffer->EnqueueCopy(RHICmdList, FeedbackBuffer->GetRHI(), 0u);
		});

	WriteIndex = (WriteIndex + 1) % MaxBuffers;
	NumPending = FMath::Min(NumPending + 1, MaxBuffers);
}

FVirtualShadowMapFeedback::FReadbackInfo FVirtualShadowMapFeedback::GetLatestReadbackBuffer()
{
	int32 LatestBufferIndex = -1;

	// Find latest buffer that is ready
	while (NumPending > 0)
	{
		uint32 Index = (WriteIndex + MaxBuffers - NumPending) % MaxBuffers;
		if (Buffers[Index].Buffer->IsReady())
		{
			--NumPending;
			LatestBufferIndex = Index;
		}
		else
		{
			break;
		}
	}

	return LatestBufferIndex >= 0 ? Buffers[LatestBufferIndex] : FReadbackInfo();
}


FVirtualShadowMapArrayCacheManager::FVirtualShadowMapArrayCacheManager(FScene* InScene) 
	: Scene(InScene)
	, ShadowInvalidatingInstancesImplementation(*this)
{
	// Handle message with status sent back from GPU
	StatusFeedbackSocket = GPUMessage::RegisterHandler(TEXT("Shadow.Virtual.StatusFeedback"), [this](GPUMessage::FReader Message)
	{
		// Goes negative on underflow
		int32 LastFreePhysicalPages = Message.Read<int32>(0);
		const float LastGlobalResolutionLodBias = FMath::AsFloat(Message.Read<uint32>(0U));
		
		CSV_CUSTOM_STAT(VSM, FreePages, LastFreePhysicalPages, ECsvCustomStatOp::Set);

		// Dynamic resolution
		{
			// Could be cvars if needed, but not clearly something that needs to be tweaked currently
			// NOTE: Should react more quickly when reducing resolution than when increasing again
			// TODO: Possibly something smarter/PID-like rather than simple exponential decay
			const float ResolutionDownExpLerpFactor = 0.5f;
			const float ResolutionUpExpLerpFactor = 0.1f;
			const uint32 FramesBeforeResolutionUp = 10;

			const float MaxPageAllocation = CVarVSMDynamicResolutionMaxPagePoolLoadFactor.GetValueOnRenderThread();
			const float MaxLodBias = CVarVSMDynamicResolutionMaxLodBias.GetValueOnRenderThread();
			
			if (MaxPageAllocation > 0.0f)
			{
				const uint32 SceneFrameNumber = Scene->GetFrameNumberRenderThread();

				// Dynamically bias shadow resolution when we get too near the maximum pool capacity
				// NB: In a perfect world each +1 of resolution bias will drop the allocation in half
				float CurrentAllocation = 1.0f - (LastFreePhysicalPages / static_cast<float>(MaxPhysicalPages));
				float AllocationRatio = CurrentAllocation / MaxPageAllocation;
				float TargetLodBias = FMath::Max(0.0f, LastGlobalResolutionLodBias + FMath::Log2(AllocationRatio));

				if (CurrentAllocation <= MaxPageAllocation &&
					(SceneFrameNumber - LastFrameOverPageAllocationBudget) > FramesBeforeResolutionUp)
				{
					GlobalResolutionLodBias = FMath::Lerp(GlobalResolutionLodBias, TargetLodBias, ResolutionUpExpLerpFactor);
				}
				else if (CurrentAllocation > MaxPageAllocation)
				{
					LastFrameOverPageAllocationBudget = SceneFrameNumber;
					GlobalResolutionLodBias = FMath::Lerp(GlobalResolutionLodBias, TargetLodBias, ResolutionDownExpLerpFactor);
				}
			}

			GlobalResolutionLodBias = FMath::Clamp(GlobalResolutionLodBias, 0.0f, MaxLodBias);
		}

		if (LastFreePhysicalPages < 0)
		{
#if !UE_BUILD_SHIPPING
			if (!bLoggedPageOverflow)
			{
				static const auto* CVarResolutionLodBiasLocalPtr = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.Shadow.Virtual.ResolutionLodBiasLocal"));
				static const auto* CVarResolutionLodBiasDirectionalPtr = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.Shadow.Virtual.ResolutionLodBiasDirectional"));

				UE_LOG(LogRenderer, Warning, TEXT("Virtual Shadow Map Page Pool overflow (%d page allocations were not served), this will produce visual artifacts (missing shadow), increase the page pool limit or reduce resolution bias to avoid.\n")
					TEXT(" See r.Shadow.Virtual.MaxPhysicalPages (%d), r.Shadow.Virtual.ResolutionLodBiasLocal (%.2f), r.Shadow.Virtual.ResolutionLodBiasDirectional (%.2f), Global Resolution Lod Bias (%.2f)"),
					-LastFreePhysicalPages,
					MaxPhysicalPages,
					CVarResolutionLodBiasLocalPtr->GetValueOnRenderThread(),
					CVarResolutionLodBiasDirectionalPtr->GetValueOnRenderThread(),
					GlobalResolutionLodBias);

				bLoggedPageOverflow = true;
			}
			LastOverflowTime = float(FGameTime::GetTimeSinceAppStart().GetRealTimeSeconds());
#endif
		}
#if !UE_BUILD_SHIPPING
		else
		{
			bLoggedPageOverflow = false;
		}
#endif
	});

#if !UE_BUILD_SHIPPING
	// Handle message with stats sent back from GPU whenever stats are enabled
	StatsFeedbackSocket = GPUMessage::RegisterHandler(TEXT("Shadow.Virtual.StatsFeedback"), [this](GPUMessage::FReader Message)
	{
		// Culling stats
		int32 NaniteNumTris = Message.Read<int32>(0);
		int32 NanitePostCullNodeCount = Message.Read<int32>(0);
		int32 NonNanitePostCullInstanceCount = Message.Read<int32>(0);

		CSV_CUSTOM_STAT(VSM, NaniteNumTris, NaniteNumTris, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(VSM, NanitePostCullNodeCount, NanitePostCullNodeCount, ECsvCustomStatOp::Set);
		CSV_CUSTOM_STAT(VSM, NonNanitePostCullInstanceCount, NonNanitePostCullInstanceCount, ECsvCustomStatOp::Set);

		// Large page area items
		LastLoggedPageOverlapAppTime.SetNumZeroed(Scene->GetMaxPersistentPrimitiveIndex());
		float RealTimeSeconds = float(FGameTime::GetTimeSinceAppStart().GetRealTimeSeconds());

		TConstArrayView<uint32> PageAreaDiags = Message.ReadCount(FVirtualShadowMapArray::MaxPageAreaDiagnosticSlots * 2);
		for (int32 Index = 0; Index < PageAreaDiags.Num(); Index += 2)
		{
			uint32 Overlap = PageAreaDiags[Index];
			uint32 PersistentPrimitiveId = PageAreaDiags[Index + 1];
			int32 PrimtiveIndex = Scene->GetPrimitiveIndex(FPersistentPrimitiveIndex{ int32(PersistentPrimitiveId) });
			if (Overlap > 0 && PrimtiveIndex != INDEX_NONE)
			{
				if (RealTimeSeconds - LastLoggedPageOverlapAppTime[PersistentPrimitiveId] > 5.0f)
				{
					LastLoggedPageOverlapAppTime[PersistentPrimitiveId] = RealTimeSeconds;
					UE_LOG(LogRenderer, Warning, TEXT("Non-Nanite VSM page overlap performance Warning, %d, %s, %s"), Overlap, *Scene->Primitives[PrimtiveIndex]->GetOwnerActorNameOrLabelForDebuggingOnly(), *Scene->Primitives[PrimtiveIndex]->GetFullnameForDebuggingOnly());
				}
				LargePageAreaItems.Add(PersistentPrimitiveId, FLargePageAreaItem{ Overlap, RealTimeSeconds });
			}
		}
	});
#endif

#if !UE_BUILD_SHIPPING
	ScreenMessageDelegate = FRendererOnScreenNotification::Get().AddLambda([this](TMultiMap<FCoreDelegates::EOnScreenMessageSeverity, FText >& OutMessages)
	{
		float RealTimeSeconds = float(FGameTime::GetTimeSinceAppStart().GetRealTimeSeconds());

		// Show for ~5s after last overflow
		if (LastOverflowTime >= 0.0f && RealTimeSeconds - LastOverflowTime < 5.0f)
		{
			OutMessages.Add(FCoreDelegates::EOnScreenMessageSeverity::Warning, FText::FromString(FString::Printf(TEXT("Virtual Shadow Map Page Pool overflow detected (%0.0f seconds ago)"), RealTimeSeconds - LastOverflowTime)));
		}

		for (const auto& Item : LargePageAreaItems)
		{
			int32 PrimtiveIndex = Scene->GetPrimitiveIndex(FPersistentPrimitiveIndex{ int32(Item.Key) });
			uint32 Overlap = Item.Value.PageArea;
			if (PrimtiveIndex != INDEX_NONE && RealTimeSeconds - Item.Value.LastTimeSeen < 2.5f)
			{
				OutMessages.Add(FCoreDelegates::EOnScreenMessageSeverity::Warning, FText::FromString(FString::Printf(TEXT("Non-Nanite VSM page overlap performance Warning: Primitive '%s' overlapped %d Pages"), *Scene->Primitives[PrimtiveIndex]->GetOwnerActorNameOrLabelForDebuggingOnly(), Overlap)));
			}
		}
		TrimLoggingInfo();
	});
#endif
}

FVirtualShadowMapArrayCacheManager::~FVirtualShadowMapArrayCacheManager()
{
#if !UE_BUILD_SHIPPING
	FRendererOnScreenNotification::Get().Remove(ScreenMessageDelegate);
#endif
}


void FVirtualShadowMapArrayCacheManager::SetPhysicalPoolSize(FRDGBuilder& GraphBuilder, FIntPoint RequestedSize, int RequestedArraySize, uint32 RequestedMaxPhysicalPages)
{
	bool bInvalidateCache = false;

	// Using ReservedResource|ImmediateCommit flags hint to the RHI that the resource can be allocated using N small physical memory allocations,
	// instead of a single large contighous allocation. This helps Windows video memory manager page allocations in and out of local memory more efficiently.
	ETextureCreateFlags RequestedCreateFlags = (CVarVSMReservedResource.GetValueOnRenderThread() && GRHIGlobals.ReservedResources.Supported)
		? (TexCreate_ReservedResource | TexCreate_ImmediateCommit)
		: TexCreate_None;

	if (!PhysicalPagePool 
		|| PhysicalPagePool->GetDesc().Extent != RequestedSize 
		|| PhysicalPagePool->GetDesc().ArraySize != RequestedArraySize
		|| RequestedMaxPhysicalPages != MaxPhysicalPages
		|| PhysicalPagePoolCreateFlags != RequestedCreateFlags)
	{
		if (PhysicalPagePool)
		{
			UE_LOG(LogRenderer, Display, TEXT("Recreating Shadow.Virtual.PhysicalPagePool due to size or flags change. This will also drop any cached pages."));
		}

		// Track changes to these ourselves instead of from the GetDesc() since that may get manipulated internally
		PhysicalPagePoolCreateFlags = RequestedCreateFlags;
        
        ETextureCreateFlags PoolTexCreateFlags = TexCreate_ShaderResource | TexCreate_UAV;
        
#if PLATFORM_MAC
        if(GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM6)
        {
            PoolTexCreateFlags |= TexCreate_AtomicCompatible;
        }
#endif
        
		FPooledRenderTargetDesc Desc2D = FPooledRenderTargetDesc::Create2DArrayDesc(
			RequestedSize,
			PF_R32_UINT,
			FClearValueBinding::None,
			PhysicalPagePoolCreateFlags,
            PoolTexCreateFlags,
			false,
			RequestedArraySize
		);
		GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, Desc2D, PhysicalPagePool, TEXT("Shadow.Virtual.PhysicalPagePool"));

		MaxPhysicalPages = RequestedMaxPhysicalPages;

		// Allocate page metadata alongside
		FRDGBufferRef PhysicalPageMetaDataRDG = GraphBuilder.CreateBuffer(
			FRDGBufferDesc::CreateStructuredDesc(sizeof(FPhysicalPageMetaData), MaxPhysicalPages),
			TEXT("Shadow.Virtual.PhysicalPageMetaData"));
		// Persistent, so we extract it immediately
		PhysicalPageMetaData = GraphBuilder.ConvertToExternalBuffer(PhysicalPageMetaDataRDG);

		bInvalidateCache = true;
	}

	if (bInvalidateCache)
	{
		Invalidate(GraphBuilder);
	}
}

void FVirtualShadowMapArrayCacheManager::FreePhysicalPool(FRDGBuilder& GraphBuilder)
{
	if (PhysicalPagePool)
	{
		PhysicalPagePool = nullptr;
		PhysicalPageMetaData = nullptr;
		Invalidate(GraphBuilder);
	}
}

TRefCountPtr<IPooledRenderTarget> FVirtualShadowMapArrayCacheManager::SetHZBPhysicalPoolSize(FRDGBuilder& GraphBuilder, FIntPoint RequestedHZBSize, const EPixelFormat Format)
{
	if (!HZBPhysicalPagePool || HZBPhysicalPagePool->GetDesc().Extent != RequestedHZBSize || HZBPhysicalPagePool->GetDesc().Format != Format)
	{
		// TODO: This may need to be an array as well
		FPooledRenderTargetDesc Desc = FPooledRenderTargetDesc::Create2DDesc(
			RequestedHZBSize,
			Format,
			FClearValueBinding::None,
			GFastVRamConfig.HZB,
			TexCreate_ShaderResource | TexCreate_UAV,
			false,
			FVirtualShadowMap::NumHZBLevels);

		GRenderTargetPool.FindFreeElement(GraphBuilder.RHICmdList, Desc, HZBPhysicalPagePool, TEXT("Shadow.Virtual.HZBPhysicalPagePool"));

		// TODO: Clear to black?

		Invalidate(GraphBuilder);
	}

	return HZBPhysicalPagePool;
}

void FVirtualShadowMapArrayCacheManager::FreeHZBPhysicalPool(FRDGBuilder& GraphBuilder)
{
	if (HZBPhysicalPagePool)
	{
		HZBPhysicalPagePool = nullptr;
		Invalidate(GraphBuilder);
	}
}

void FVirtualShadowMapArrayCacheManager::Invalidate(FRDGBuilder& GraphBuilder)
{
	// Clear the cache
	CacheEntries.Reset();

	//UE_LOG(LogRenderer, Display, TEXT("Virtual shadow map cache invalidated."));

	// Clear the physical page metadata (on all GPUs)
	if (PhysicalPageMetaData)
	{
		RDG_GPU_MASK_SCOPE(GraphBuilder, FRHIGPUMask::All());
		FRDGBufferRef PhysicalPageMetaDataRDG = GraphBuilder.RegisterExternalBuffer(PhysicalPageMetaData);
		AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(PhysicalPageMetaDataRDG), 0);
	}
}

bool FVirtualShadowMapArrayCacheManager::IsCacheEnabled()
{
	return CVarCacheVirtualSMs.GetValueOnRenderThread() != 0;
}

bool FVirtualShadowMapArrayCacheManager::IsCacheDataAvailable()
{
	return IsCacheEnabled() &&
		PhysicalPagePool &&
		PhysicalPageMetaData &&
		PrevBuffers.PageTable &&
		PrevBuffers.PageFlags &&
		PrevBuffers.PageRectBounds &&
		PrevBuffers.ProjectionData &&
		PrevBuffers.PhysicalPageLists;
}

bool FVirtualShadowMapArrayCacheManager::IsHZBDataAvailable()
{
	// NOTE: HZB can be used/valid even when physical page caching is disabled
	return HZBPhysicalPagePool && PrevBuffers.PageTable && PrevBuffers.PageFlags;
}

FRDGBufferRef FVirtualShadowMapArrayCacheManager::UploadCachePrimitiveAsDynamic(FRDGBuilder& GraphBuilder) const
{
	const uint32 NumElements = FMath::Max(1, FMath::DivideAndRoundUp(CachePrimitiveAsDynamic.Num(), 32));
	//GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(uint32), BufferElements), 
	
	FRDGBufferRef CachePrimitiveAsDynamicRDG = CreateStructuredBuffer(
		GraphBuilder,
		TEXT("CachePrimitiveAsDynamic"),
		sizeof(uint32),
		NumElements,
		CachePrimitiveAsDynamic.GetData(),
		FMath::DivideAndRoundUp(CachePrimitiveAsDynamic.Num(), 8)	// Size in bytes of initial data
	);

	return CachePrimitiveAsDynamicRDG;
}

TSharedPtr<FVirtualShadowMapPerLightCacheEntry> FVirtualShadowMapArrayCacheManager::FindCreateLightCacheEntry(
	int32 LightSceneId, uint32 ViewUniqueID, uint32 NumShadowMaps)
{
	const FVirtualShadowMapCacheKey CacheKey = { ViewUniqueID, LightSceneId };

	TSharedPtr<FVirtualShadowMapPerLightCacheEntry> *LightEntryKey = CacheEntries.Find(CacheKey);

	if (LightEntryKey)
	{
		TSharedPtr<FVirtualShadowMapPerLightCacheEntry> LightEntry = *LightEntryKey;

		if (LightEntry->ShadowMapEntries.Num() == NumShadowMaps)
		{
			LightEntry->bReferencedThisRender = true;
			LightEntry->LastReferencedFrameNumber = Scene->GetFrameNumberRenderThread();
			return LightEntry;
		}
		else
		{
			// Remove this entry and create a new one below
			// NOTE: This should only happen for clipmaps currently on cvar changes
			UE_LOG(LogRenderer, Display, TEXT("Virtual shadow map cache invalidated for light due to clipmap level count change"));
			CacheEntries.Remove(CacheKey);
		}
	}

	// Make new entry for this light
	TSharedPtr<FVirtualShadowMapPerLightCacheEntry> LightEntry = MakeShared<FVirtualShadowMapPerLightCacheEntry>(Scene->GetMaxPersistentPrimitiveIndex(), NumShadowMaps);
	LightEntry->bReferencedThisRender = true;
	LightEntry->LastReferencedFrameNumber = Scene->GetFrameNumberRenderThread();
	CacheEntries.Add(CacheKey, LightEntry);

	return LightEntry;
}

void FVirtualShadowMapPerLightCacheEntry::OnPrimitiveRendered(const FPrimitiveSceneInfo* PrimitiveSceneInfo)
{
	// Mark as (potentially present in a cached page somehwere, so we'd need to invalidate if it is removed/moved)
	CachedPrimitives[PrimitiveSceneInfo->GetPersistentIndex().Index] = true;

	if (GVSMCacheDeformableMeshesInvalidate != 0)
	{
		// Deformable mesh primitives need to trigger invalidation (even if they did not move) or we get artifacts, for example skinned meshes that are animating but not currently moving.
		// Skip if the invalidation mode is NOT auto (because Always will do it elsewhere & the others should prevent this).
		if (PrimitiveSceneInfo->Proxy->HasDeformableMesh() && PrimitiveSceneInfo->Proxy->GetShadowCacheInvalidationBehavior() == EShadowCacheInvalidationBehavior::Auto)
		{
			PrimitiveInstancesToInvalidate.Add(FVirtualShadowMapInstanceRange{
				PrimitiveSceneInfo->GetPersistentIndex(),
				PrimitiveSceneInfo->GetInstanceSceneDataOffset(),
				PrimitiveSceneInfo->GetNumInstanceSceneDataEntries()
			});
		}
	}
}

void FVirtualShadowMapArrayCacheManager::UpdateUnreferencedCacheEntries(
	FVirtualShadowMapArray& VirtualShadowMapArray)
{
	const uint32 SceneFrameNumber = Scene->GetFrameNumberRenderThread();

	for (FEntryMap::TIterator It = CacheEntries.CreateIterator(); It; ++It)
	{
		TSharedPtr<FVirtualShadowMapPerLightCacheEntry> CacheEntry = (*It).Value;
		// For this test we care if it is active *this render*, not just this scene frame number (which can include multiple renders)
		if (CacheEntry->bReferencedThisRender)
		{
			// Active this render, leave it alone
			check(CacheEntry->ShadowMapEntries.Last().CurrentVirtualShadowMapId < VirtualShadowMapArray.GetNumShadowMapSlots());
		}
		else if (int32(SceneFrameNumber - CacheEntry->LastReferencedFrameNumber) <= GVSMMaxPageAgeSinceLastRequest)
		{
			// Not active this render, but still recent enough to keep it and its pages alive
			int PrevBaseVirtualShadowMapId = CacheEntry->ShadowMapEntries[0].CurrentVirtualShadowMapId;
			bool bIsSinglePage = FVirtualShadowMapArray::IsSinglePage(PrevBaseVirtualShadowMapId);

			// Keep the entry, reallocate new VSM IDs
			int32 NumMaps = CacheEntry->ShadowMapEntries.Num();
			int32 VirtualShadowMapId = VirtualShadowMapArray.Allocate(bIsSinglePage, NumMaps);
			for (int32 Map = 0; Map < NumMaps; ++Map)
			{
				CacheEntry->ShadowMapEntries[Map].Update(VirtualShadowMapArray, *CacheEntry, VirtualShadowMapId + Map);
				// Mark it as inactive for this frame/render
				// NOTE: We currently recompute/overwrite the whole ProjectionData structure for referenced lights, but if that changes we
				// will need to clear this flag again when they become referenced.
				CacheEntry->ShadowMapEntries[Map].ProjectionData.Flags |= VSM_PROJ_FLAG_UNREFERENCED;
			}
		}
		else
		{
			It.RemoveCurrent();
		}
	}
}

void FVirtualShadowMapArrayCacheManager::UploadProjectionData(FRDGScatterUploadBuffer& Uploader) const
{
	// If we get here, this should be non-empty, otherwise we will upload nothing and the destination buffer
	// will end up as not having been written in RDG, which makes it upset even if it will never get referenced on the GPU.
	check(!CacheEntries.IsEmpty());

	for (const auto& LightEntry : CacheEntries)
	{
		TSharedPtr<FVirtualShadowMapPerLightCacheEntry> CacheEntry = LightEntry.Value;
		for (const auto& Entry : CacheEntry->ShadowMapEntries)
		{
			Uploader.Add(Entry.CurrentVirtualShadowMapId, &Entry.ProjectionData);
		}
	}
}

class FVirtualSmCopyStatsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVirtualSmCopyStatsCS);
	SHADER_USE_PARAMETER_STRUCT(FVirtualSmCopyStatsCS, FGlobalShader)
public:
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, InStatsBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FNaniteStats>, NaniteStatsBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, AccumulatedStatsBufferOut)
		SHADER_PARAMETER(uint32, NumStats)
	END_SHADER_PARAMETER_STRUCT()
	
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) &&
			DoesPlatformSupportVirtualShadowMaps(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		OutEnvironment.SetDefine(TEXT("MAX_STAT_FRAMES"), FVirtualShadowMapArrayCacheManager::MaxStatFrames);
	}
};
IMPLEMENT_GLOBAL_SHADER(FVirtualSmCopyStatsCS, "/Engine/Private/VirtualShadowMaps/VirtualShadowMapCopyStats.usf", "CopyStatsCS", SF_Compute);

void FVirtualShadowMapArrayCacheManager::ExtractFrameData(
	FRDGBuilder& GraphBuilder,	
	FVirtualShadowMapArray &VirtualShadowMapArray,
	const FSceneRenderer& SceneRenderer,
	bool bAllowPersistentData)
{
	TrimLoggingInfo();

	const bool bNewShadowData = VirtualShadowMapArray.IsAllocated();
	const bool bDropAll = !bAllowPersistentData;
	const bool bDropPrevBuffers = bDropAll || bNewShadowData;

	if (bDropPrevBuffers)
	{
		PrevBuffers = FVirtualShadowMapArrayFrameData();
		PrevUniformParameters.NumFullShadowMaps = 0;
		PrevUniformParameters.NumSinglePageShadowMaps = 0;
		PrevUniformParameters.NumShadowMapSlots = 0;
	}

	if (bDropAll)
	{
		// We drop the physical page pool here as well to ensure that it disappears in the case where
		// thumbnail rendering or similar creates multiple FSceneRenderers that never get deleted.
		// Caching is disabled on these contexts intentionally to avoid these issues.
		FreePhysicalPool(GraphBuilder);
		FreeHZBPhysicalPool(GraphBuilder);
	}
	else if (bNewShadowData)
	{
		// Page table and associated data are needed by HZB next frame even when VSM physical page caching is disabled
		GraphBuilder.QueueBufferExtraction(VirtualShadowMapArray.PageTableRDG, &PrevBuffers.PageTable);
		GraphBuilder.QueueBufferExtraction(VirtualShadowMapArray.PageRectBoundsRDG, &PrevBuffers.PageRectBounds);
		GraphBuilder.QueueBufferExtraction(VirtualShadowMapArray.PageFlagsRDG, &PrevBuffers.PageFlags);

		if (IsCacheEnabled())
		{
			GraphBuilder.QueueBufferExtraction(VirtualShadowMapArray.ProjectionDataRDG, &PrevBuffers.ProjectionData);
			GraphBuilder.QueueBufferExtraction(VirtualShadowMapArray.PhysicalPageListsRDG, &PrevBuffers.PhysicalPageLists);
						
			// Store but drop any temp references embedded in the uniform parameters this frame
			PrevUniformParameters = VirtualShadowMapArray.UniformParameters;
			PrevUniformParameters.ProjectionData = nullptr;
			PrevUniformParameters.PageTable = nullptr;
			PrevUniformParameters.PageRectBounds = nullptr;
			PrevUniformParameters.PageFlags = nullptr;
			PrevUniformParameters.LightGridData = nullptr;
			PrevUniformParameters.NumCulledLightsGrid = nullptr;
			PrevUniformParameters.CachePrimitiveAsDynamic = nullptr;
		}

		// propagate current-frame primitive state to cache entry
		for (const auto& LightInfo : SceneRenderer.VisibleLightInfos)
		{
			for (const TSharedPtr<FVirtualShadowMapClipmap> &Clipmap : LightInfo.VirtualShadowMapClipmaps)
			{
				// Push data to cache entry
				Clipmap->UpdateCachedFrameData();
			}
		}

		ExtractStats(GraphBuilder, VirtualShadowMapArray);
	}
	
	// Clear out the referenced light flags since this render is finishing
	for (auto& LightEntry : CacheEntries)
	{
		LightEntry.Value->bReferencedThisRender = false;
	}
}

void FVirtualShadowMapArrayCacheManager::ExtractStats(FRDGBuilder& GraphBuilder, FVirtualShadowMapArray &VirtualShadowMapArray)
{
	FRDGBufferRef AccumulatedStatsBufferRDG = nullptr;

	// Note: stats accumulation thing is here because it needs to persist over frames.
	if (AccumulatedStatsBuffer.IsValid())
	{
		AccumulatedStatsBufferRDG = GraphBuilder.RegisterExternalBuffer(AccumulatedStatsBuffer, TEXT("Shadow.Virtual.AccumulatedStatsBuffer"));
	}

	if (IsAccumulatingStats())
	{
		if (!AccumulatedStatsBuffer.IsValid())
		{
			FRDGBufferDesc Desc = FRDGBufferDesc::CreateBufferDesc(4, 1 + FVirtualShadowMapArray::NumStats * MaxStatFrames);
			Desc.Usage = EBufferUsageFlags(Desc.Usage | BUF_SourceCopy);

			AccumulatedStatsBufferRDG = GraphBuilder.CreateBuffer(Desc, TEXT("Shadow.Virtual.AccumulatedStatsBuffer"));	// TODO: Can't be a structured buffer as EnqueueCopy is only defined for vertex buffers
			AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(AccumulatedStatsBufferRDG, PF_R32_UINT), 0);
			AccumulatedStatsBuffer = GraphBuilder.ConvertToExternalBuffer(AccumulatedStatsBufferRDG);
		}

		// Initialize/clear
		if (!bAccumulatingStats)
		{
			AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(AccumulatedStatsBufferRDG, PF_R32_UINT), 0);
			bAccumulatingStats = true;
		}

		FVirtualSmCopyStatsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FVirtualSmCopyStatsCS::FParameters>();

		PassParameters->InStatsBuffer = GraphBuilder.CreateSRV(VirtualShadowMapArray.StatsBufferRDG, PF_R32_UINT);
		PassParameters->AccumulatedStatsBufferOut = GraphBuilder.CreateUAV(AccumulatedStatsBufferRDG, PF_R32_UINT);
		PassParameters->NumStats = FVirtualShadowMapArray::NumStats;

		// Dummy data
		PassParameters->NaniteStatsBuffer = GraphBuilder.CreateSRV(GSystemTextures.GetDefaultStructuredBuffer<FNaniteStats>(GraphBuilder));

		// Optionally pull in some nanite stats too
		// NOTE: This only works if nanite is set to gather stats from the VSM pass!
		// i.e. run "NaniteStats VirtualShadowMaps" before starting accumulation
		if (Nanite::IsStatFilterActive(TEXT("VirtualShadowMaps")))
		{
			TRefCountPtr<FRDGPooledBuffer> NaniteStatsBuffer = Nanite::GGlobalResources.GetStatsBufferRef();
			if (NaniteStatsBuffer)
			{
				PassParameters->NaniteStatsBuffer = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalBuffer(NaniteStatsBuffer));
			}
		}

		auto ComputeShader = GetGlobalShaderMap(Scene->GetFeatureLevel())->GetShader<FVirtualSmCopyStatsCS>();

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("Copy Stats"),
			ComputeShader,
			PassParameters,
			FIntVector(1, 1, 1)
		);
	}
	else if (bAccumulatingStats)
	{
		bAccumulatingStats = false;

		GPUBufferReadback = new FRHIGPUBufferReadback(TEXT("Shadow.Virtual.AccumulatedStatsBufferReadback"));
		AddEnqueueCopyPass(GraphBuilder, GPUBufferReadback, AccumulatedStatsBufferRDG, 0u);
	}
	else if (AccumulatedStatsBuffer.IsValid())
	{
		AccumulatedStatsBuffer.SafeRelease();
	}

	if (GPUBufferReadback && GPUBufferReadback->IsReady())
	{
		TArray<uint32> Tmp;
		Tmp.AddDefaulted(1 + FVirtualShadowMapArray::NumStats * MaxStatFrames);

		{
			const uint32* BufferPtr = (const uint32*)GPUBufferReadback->Lock((1 + FVirtualShadowMapArray::NumStats * MaxStatFrames) * sizeof(uint32));
			FPlatformMemory::Memcpy(Tmp.GetData(), BufferPtr, Tmp.Num() * Tmp.GetTypeSize());
			GPUBufferReadback->Unlock();

			delete GPUBufferReadback;
			GPUBufferReadback = nullptr;
		}

		FString FileName = TEXT("VirtualShadowMapCacheStats.csv");// FString::Printf(TEXT("%s.csv"), *FileNameToUse);
		FArchive * FileToLogTo = IFileManager::Get().CreateFileWriter(*FileName, false);
		ensure(FileToLogTo);
		if (FileToLogTo)
		{
			static const FString StatNames[] =
			{
				TEXT("Requested"),
				TEXT("StaticCached"),
				TEXT("StaticInvalidated"),
				TEXT("DynamicCached"),
				TEXT("DynamicInvalidated"),
				TEXT("Empty"),
				TEXT("NonNaniteInstances"),
				TEXT("NonNaniteInstancesDrawn"),
				TEXT("NonNaniteInstancesHZBCulled"),
				TEXT("NonNaniteInstancesPageMaskCulled"),
				TEXT("NonNaniteInstancesEmptyRectCulled"),
				TEXT("NonNaniteInstancesFrustumCulled"),
				TEXT("Merged"),
				TEXT("Cleared"),
				TEXT("HZBBuilt"),
				TEXT("AllocatedNew"),
				TEXT("NaniteTriangles"),
				TEXT("NaniteInstancesMain"),
				TEXT("NaniteInstancesPost"),
			};

			// Print header
			FString StringToPrint;
			for (int32 Index = 0; Index < FVirtualShadowMapArray::NumStats; ++Index)
			{
				if (!StringToPrint.IsEmpty())
				{
					StringToPrint += TEXT(",");
				}
				if (Index < int32(UE_ARRAY_COUNT(StatNames)))
				{
					StringToPrint.Append(StatNames[Index]);
				}
				else
				{
					StringToPrint.Appendf(TEXT("Stat_%d"), Index);
				}
			}

			StringToPrint += TEXT("\n");
			FileToLogTo->Serialize(TCHAR_TO_ANSI(*StringToPrint), StringToPrint.Len());

			uint32 Num = Tmp[0];
			for (uint32 Ind = 0; Ind < Num; ++Ind)
			{
				StringToPrint.Empty();

				for (uint32 StatInd = 0; StatInd < FVirtualShadowMapArray::NumStats; ++StatInd)
				{
					if (!StringToPrint.IsEmpty())
					{
						StringToPrint += TEXT(",");
					}

					StringToPrint += FString::Printf(TEXT("%d"), Tmp[1 + Ind * FVirtualShadowMapArray::NumStats + StatInd]);
				}

				StringToPrint += TEXT("\n");
				FileToLogTo->Serialize(TCHAR_TO_ANSI(*StringToPrint), StringToPrint.Len());
			}


			FileToLogTo->Close();
		}
	}
}

bool FVirtualShadowMapArrayCacheManager::IsAccumulatingStats()
{
	return CVarAccumulateStats.GetValueOnRenderThread() != 0;
}

static uint32 GetPrimFlagsBufferSizeInDwords(int32 MaxPersistentPrimitiveIndex)
{
	return FMath::RoundUpToPowerOfTwo(FMath::DivideAndRoundUp(MaxPersistentPrimitiveIndex, 32));
}

void FVirtualShadowMapArrayCacheManager::OnSceneChange()
{
	const int32 MaxPersistentPrimitiveIndex = FMath::Max(1, Scene->GetMaxPersistentPrimitiveIndex());

	for (auto& CacheEntry : CacheEntries)
	{
		CacheEntry.Value->CachedPrimitives.SetNum(MaxPersistentPrimitiveIndex, false);
		CacheEntry.Value->RenderedPrimitives.SetNum(MaxPersistentPrimitiveIndex, false);
	}

	// TODO: Initialize new primitives based on their mobility; need a way to know which ones are newly created though
	CachePrimitiveAsDynamic.SetNum(MaxPersistentPrimitiveIndex, false);	
	if (MaxPersistentPrimitiveIndex > LastPrimitiveInvalidatedFrame.Num())
	{
		const uint32 OldSize = LastPrimitiveInvalidatedFrame.Num();
		LastPrimitiveInvalidatedFrame.SetNumUninitialized(MaxPersistentPrimitiveIndex);
		for (int32 It = OldSize; It < MaxPersistentPrimitiveIndex; ++It)
		{
			// Unknown last invalidation
			LastPrimitiveInvalidatedFrame[It] = 0xFFFFFFFF;
		}
	}

	// Do instance-based GPU allocations here too? For now we do them lazily each frame when the FVirtualShadowMapArray gets constructed
}

BEGIN_SHADER_PARAMETER_STRUCT(FInvalidatePagesParameters, )
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FVirtualShadowMapUniformParameters, VirtualShadowMap)
	SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneUniformParameters, Scene)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FPhysicalPageMetaData>, PhysicalPageMetaDataOut)

	// When USE_HZB_OCCLUSION
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, HZBPageTable)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint4>, HZBPageRectBounds)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, HZBTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, HZBSampler)
	SHADER_PARAMETER(FVector2f, HZBSize)
END_SHADER_PARAMETER_STRUCT()

class FInvalidateInstancePagesLoadBalancerCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FInvalidateInstancePagesLoadBalancerCS);
	SHADER_USE_PARAMETER_STRUCT(FInvalidateInstancePagesLoadBalancerCS, FGlobalShader)

	class FUseHzbDim : SHADER_PERMUTATION_BOOL("USE_HZB_OCCLUSION");
	using FPermutationDomain = TShaderPermutationDomain<FUseHzbDim>;

public:
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FInvalidatePagesParameters, InvalidatePagesParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FGPUScene::FInstanceGPULoadBalancer::FShaderParameters, LoadBalancerParameters)
	END_SHADER_PARAMETER_STRUCT()

	// This is probably fine even in instance list mode
	static constexpr int Cs1dGroupSizeX = FVirtualShadowMapArrayCacheManager::FInstanceGPULoadBalancer::ThreadGroupSize;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) &&
			DoesPlatformSupportVirtualShadowMaps(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		FVirtualShadowMapArray::SetShaderDefines(OutEnvironment);
		OutEnvironment.SetDefine(TEXT("CS_1D_GROUP_SIZE_X"), Cs1dGroupSizeX);
		OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), 1);
		FGPUScene::FInstanceGPULoadBalancer::SetShaderDefines(OutEnvironment);
	}
};
IMPLEMENT_GLOBAL_SHADER(FInvalidateInstancePagesLoadBalancerCS, "/Engine/Private/VirtualShadowMaps/VirtualShadowMapCacheLoadBalancer.usf", "InvalidateInstancePagesLoadBalancerCS", SF_Compute);

void FVirtualShadowMapArrayCacheManager::UpdateCachePrimitiveAsDynamic(FInvalidatingPrimitiveCollector& InvalidatingPrimitiveCollector)
{
	//UE_LOG(LogRenderer, Display, TEXT("VirtualShadowMapCacheManager: UpdateCachePrimitiveAsDynamic"));

	const uint32 SceneFrameNumber = Scene->GetFrameNumberRenderThread();
	const uint32 FramesStaticThreshold = CVarFramesStaticThreshold.GetValueOnRenderThread();

	// Update the cache states of things that are being invalidated
	for (TConstSetBitIterator<> BitIt(InvalidatingPrimitiveCollector.InvalidatedPrimitives); BitIt; ++BitIt)
	{
		int32 PersistentPrimitiveIndex = BitIt.GetIndex();
		// Any invalidations mean we set this primitive to dynamic. We already added an invalidation otherwise
		// we wouldn't be here, so no need to add another.
		CachePrimitiveAsDynamic[PersistentPrimitiveIndex] = true;
		LastPrimitiveInvalidatedFrame[PersistentPrimitiveIndex] = SceneFrameNumber;
	}

	// Zero out anything that was being removed
	// NOTE: This will be redundant with the invalidated stuff, but shouldn't be a big deal
	for (TConstSetBitIterator<> BitIt(InvalidatingPrimitiveCollector.RemovedPrimitives); BitIt; ++BitIt)
	{
		int32 PersistentPrimitiveIndex = BitIt.GetIndex();
		// TODO: We probably want to start new primitives as dynamic by default instead, but we don't want to have
		// to loop over all of them and try and get their PrimitiveSceneInfo every frame for invalid ones
		CachePrimitiveAsDynamic[PersistentPrimitiveIndex] = false;
		LastPrimitiveInvalidatedFrame[PersistentPrimitiveIndex] = 0xFFFFFFFF;

		//UE_LOG(LogRenderer, Display, TEXT("VirtualShadowMapCacheManager: Removing primitive %d!"), PersistentPrimitiveIndex);
	}

	// Finally check anything that is currently dynamic to see if it has not invalidated for long enough that
	// we should move it back to static
	for (TConstSetBitIterator<> BitIt(CachePrimitiveAsDynamic); BitIt; ++BitIt)
	{
		int32 PersistentPrimitiveIndex = BitIt.GetIndex();

		const uint32 LastInvalidationFrame = LastPrimitiveInvalidatedFrame[PersistentPrimitiveIndex];
		// Note: cleared to MAX_uint32; treated as "unknown/no invalidations"
		const uint32 InvalidationAge = 
			SceneFrameNumber >= LastInvalidationFrame ?
			(SceneFrameNumber - LastInvalidationFrame) :
			0xFFFFFFFF;

		const bool bWantStatic = InvalidationAge > FramesStaticThreshold;
		if (bWantStatic)
		{
			// Add invalidation and swap it to static
			FPersistentPrimitiveIndex WrappedIndex;
			WrappedIndex.Index = PersistentPrimitiveIndex;
			FPrimitiveSceneInfo* PrimitiveSceneInfo = Scene->GetPrimitiveSceneInfo(WrappedIndex);
			if (PrimitiveSceneInfo)
			{
				// Add an invalidation for every light
				for (auto& CacheEntry : CacheEntries)
				{
					for (const auto& SmCacheEntry : CacheEntry.Value->ShadowMapEntries)
					{
						const uint32 PayloadForceStatic = EncodeInstanceInvalidationPayload(SmCacheEntry.CurrentVirtualShadowMapId, VSM_INVALIDATION_PAYLOAD_FLAG_FORCE_STATIC);
						InvalidatingPrimitiveCollector.Instances.Add(
							PrimitiveSceneInfo->GetInstanceSceneDataOffset(),
							PrimitiveSceneInfo->GetNumInstanceSceneDataEntries(),
							PayloadForceStatic);
					}
				}
			}
			else
			{
				// This seems to still happen very occasionally... presumably a remove gets "missed" somehow and thus we try and transition
				// something that is no longer valid back to static. This could also potentially mean we incorrect transition a new thing that
				// grabbed this slot back to static, but that is less likely as the addition would trigger a separate invalidation.
				// Not much we can do here currently other than ignore it and move on
				UE_LOG(LogRenderer, Display, TEXT("VirtualShadowMapCacheManager: Invalid persistent primitive index %d, age %u!"), PersistentPrimitiveIndex, InvalidationAge);
				LastPrimitiveInvalidatedFrame[PersistentPrimitiveIndex] = 0xFFFFFFFF;
			}
			// NOTE: This is safe with the current set bit iterator, but should maybe use a temp array for future safety?
			CachePrimitiveAsDynamic[PersistentPrimitiveIndex] = false;
		}
	}
}

void FVirtualShadowMapArrayCacheManager::ProcessInvalidations(FRDGBuilder& GraphBuilder, FSceneUniformBuffer &SceneUniformBuffer, FInvalidatingPrimitiveCollector& InvalidatingPrimitiveCollector)
{
	if (IsCacheDataAvailable() && PrevUniformParameters.NumFullShadowMaps > 0)
	{
		RDG_EVENT_SCOPE(GraphBuilder, "Shadow.Virtual.ProcessInvalidations");

		// NOTE: Important that we get some of these parameters (ex. CachePrimitiveAsDynamic) before
		// we update them as the shader needs to know the previous cache states for invalidation.
		FInvalidationPassCommon InvalidationPassCommon = GetUniformParametersForInvalidation(GraphBuilder, SceneUniformBuffer);

		// Add invalidations for skeletal meshes, CPU culling changes, dynamic primitives, etc.
		InvalidatingPrimitiveCollector.AddPrimitivesToInvalidate();

		// Check whether we want to swap any cache states and add any invalidations to that end as well
		UpdateCachePrimitiveAsDynamic(InvalidatingPrimitiveCollector);

		InvalidatingPrimitiveCollector.Instances.FinalizeBatches();

		if (!InvalidatingPrimitiveCollector.Instances.IsEmpty())
		{
			ProcessInvalidations(GraphBuilder, InvalidationPassCommon, InvalidatingPrimitiveCollector.Instances);
		}
	}
	else
	{
		// Clear any queued-up invalidations
		ShadowInvalidatingInstancesImplementation.PrimitiveInstancesToInvalidate.Reset();
		for (auto& CacheEntry : CacheEntries)
		{
			CacheEntry.Value->PrimitiveInstancesToInvalidate.Reset();
		}
	}
}

void FVirtualShadowMapArrayCacheManager::OnLightRemoved(int32 LightId)
{
	const FVirtualShadowMapCacheKey CacheKey = { /* TODO: this is broken for directional lights! ViewUniqueID */0, LightId };
	CacheEntries.Remove(CacheKey);
}

FVirtualShadowMapArrayCacheManager::FInvalidationPassCommon FVirtualShadowMapArrayCacheManager::GetUniformParametersForInvalidation(
	FRDGBuilder& GraphBuilder,
	FSceneUniformBuffer &SceneUniformBuffer) const
{
	// Construct a uniform buffer based on the previous frame data, reimported into this graph builder
	FVirtualShadowMapUniformParameters* UniformParameters = GraphBuilder.AllocParameters<FVirtualShadowMapUniformParameters>();
	*UniformParameters = PrevUniformParameters;
	{
		auto RegExtCreateSrv = [&GraphBuilder](const TRefCountPtr<FRDGPooledBuffer>& Buffer, const TCHAR* Name) -> FRDGBufferSRVRef
		{
			return GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalBuffer(Buffer, Name));
		};

		UniformParameters->ProjectionData = RegExtCreateSrv(PrevBuffers.ProjectionData, TEXT("Shadow.Virtual.PrevProjectionData"));
		UniformParameters->PageTable = RegExtCreateSrv(PrevBuffers.PageTable, TEXT("Shadow.Virtual.PrevPageTable"));
		UniformParameters->PageFlags = RegExtCreateSrv(PrevBuffers.PageFlags, TEXT("Shadow.Virtual.PrevPageFlags"));
		UniformParameters->PageRectBounds = RegExtCreateSrv(PrevBuffers.PageRectBounds, TEXT("Shadow.Virtual.PrevPageRectBounds"));
		UniformParameters->CachePrimitiveAsDynamic = GraphBuilder.CreateSRV(UploadCachePrimitiveAsDynamic(GraphBuilder));

		// Unused in this path... may be a better way to handle this
		UniformParameters->PhysicalPagePool = GSystemTextures.GetZeroUIntArrayAtomicCompatDummy(GraphBuilder);
		FRDGBufferSRVRef Uint32SRVDummy = GraphBuilder.CreateSRV(GSystemTextures.GetDefaultStructuredBuffer(GraphBuilder, sizeof(uint32)));
		UniformParameters->LightGridData = Uint32SRVDummy;
		UniformParameters->NumCulledLightsGrid = Uint32SRVDummy;
	}
	
	FInvalidationPassCommon Result;
	Result.UniformParameters = UniformParameters;
	Result.VirtualShadowMapUniformBuffer = GraphBuilder.CreateUniformBuffer(UniformParameters);
	Result.SceneUniformBuffer = SceneUniformBuffer.GetBuffer(GraphBuilder);
	return Result;
}

void FVirtualShadowMapArrayCacheManager::SetInvalidateInstancePagesParameters(
	FRDGBuilder& GraphBuilder,
	const FInvalidationPassCommon& InvalidationPassCommon,
	FInvalidatePagesParameters* PassParameters) const
{
	// TODO: We should make this UBO once and reuse it for all the passes
	PassParameters->VirtualShadowMap = InvalidationPassCommon.VirtualShadowMapUniformBuffer;
	PassParameters->Scene = InvalidationPassCommon.SceneUniformBuffer;
	PassParameters->PhysicalPageMetaDataOut = GraphBuilder.CreateUAV(GraphBuilder.RegisterExternalBuffer(PhysicalPageMetaData));
	
	const bool bUseHZB = (CVarCacheVsmUseHzb.GetValueOnRenderThread() != 0);
	const TRefCountPtr<IPooledRenderTarget> HZBPhysical = (bUseHZB && HZBPhysicalPagePool) ? HZBPhysicalPagePool : nullptr;
	if (HZBPhysical)
	{
		// Same, since we are not producing a new frame just yet
		PassParameters->HZBPageTable = InvalidationPassCommon.UniformParameters->PageTable;
		PassParameters->HZBPageRectBounds = InvalidationPassCommon.UniformParameters->PageRectBounds;
		PassParameters->HZBTexture = GraphBuilder.RegisterExternalTexture(HZBPhysical);
		PassParameters->HZBSize = HZBPhysical->GetDesc().Extent;
		PassParameters->HZBSampler = TStaticSamplerState< SF_Point, AM_Clamp, AM_Clamp, AM_Clamp >::GetRHI();
	}
}

void FVirtualShadowMapArrayCacheManager::ProcessInvalidations(
	FRDGBuilder& GraphBuilder,
	const FInvalidationPassCommon& InvalidationPassCommon,
	const FInstanceGPULoadBalancer& Instances) const
{
	check(InvalidationPassCommon.UniformParameters->NumFullShadowMaps > 0);
	check(!Instances.IsEmpty());

	FInvalidateInstancePagesLoadBalancerCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FInvalidateInstancePagesLoadBalancerCS::FParameters>();

	SetInvalidateInstancePagesParameters(GraphBuilder, InvalidationPassCommon, &PassParameters->InvalidatePagesParameters);
	Instances.UploadFinalized(GraphBuilder).GetShaderParameters(GraphBuilder, PassParameters->LoadBalancerParameters);

	FInvalidateInstancePagesLoadBalancerCS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FInvalidateInstancePagesLoadBalancerCS::FUseHzbDim>(PassParameters->InvalidatePagesParameters.HZBTexture != nullptr);
	
	auto ComputeShader = GetGlobalShaderMap(Scene->GetFeatureLevel())->GetShader<FInvalidateInstancePagesLoadBalancerCS>(PermutationVector);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("InvalidateInstancePagesLoadBalancerCS (%d batches)", Instances.GetBatches().Num()),
		ComputeShader,
		PassParameters,
		Instances.GetWrappedCsGroupCount()
	);
}

// Remove old info used to track logging.
void FVirtualShadowMapArrayCacheManager::TrimLoggingInfo()
{
#if !UE_BUILD_SHIPPING
	// Remove old items
	float RealTimeSeconds = float(FGameTime::GetTimeSinceAppStart().GetRealTimeSeconds());
	LargePageAreaItems = LargePageAreaItems.FilterByPredicate([RealTimeSeconds](const TMap<uint32, FLargePageAreaItem>::ElementType& Element)
	{
		return RealTimeSeconds - Element.Value.LastTimeSeen < 5.0f;
	});
#endif
}
