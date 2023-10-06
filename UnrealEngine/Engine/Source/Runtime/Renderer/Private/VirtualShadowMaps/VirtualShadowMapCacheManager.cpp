// Copyright Epic Games, Inc. All Rights Reserved.

#include "VirtualShadowMapCacheManager.h"
#include "VirtualShadowMapClipmap.h"
#include "RendererModule.h"
#include "RenderGraphUtils.h"
#include "RHIGPUReadback.h"
#include "ScenePrivate.h"
#include "HAL/FileManager.h"

#include "DataDrivenShaderPlatformInfo.h"
#include "PrimitiveSceneInfo.h"
#include "ShaderPrint.h"
#include "RendererOnScreenNotification.h"
#include "SystemTextures.h"

#define VSM_LOG_STATIC_CACHING 0

CSV_DECLARE_CATEGORY_EXTERN(VSM);
DECLARE_DWORD_COUNTER_STAT(TEXT("VSM Unreferenced Lights"), STAT_VSMUnreferencedLights, STATGROUP_ShadowRendering);

static TAutoConsoleVariable<int32> CVarAccumulateStats(
	TEXT("r.Shadow.Virtual.AccumulateStats"),
	0,
	TEXT("AccumulateStats"),
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
	TEXT("Enables testing HZB for Virtual Shadow Map invalidations."),
	ECVF_RenderThreadSafe);

int32 GClipmapPanning = 1;
FAutoConsoleVariableRef CVarEnableClipmapPanning(
	TEXT("r.Shadow.Virtual.Cache.ClipmapPanning"),
	GClipmapPanning,
	TEXT("Enable support for panning cached clipmap pages for directional lights."),
	ECVF_RenderThreadSafe
);

static int32 GVSMCacheDeformableMeshesInvalidate = 1;
FAutoConsoleVariableRef CVarCacheInvalidateOftenMoving(
	TEXT("r.Shadow.Virtual.Cache.DeformableMeshesInvalidate"),
	GVSMCacheDeformableMeshesInvalidate,
	TEXT("If enabled, Primitive Proxies that are marked as having deformable meshes (HasDeformableMesh() == true) causes invalidations regardless of whether their transforms are updated."),
	ECVF_RenderThreadSafe);

int32 GForceInvalidateDirectionalVSM = 0;
static FAutoConsoleVariableRef  CVarForceInvalidateDirectionalVSM(
	TEXT("r.Shadow.Virtual.Cache.ForceInvalidateDirectional"),
	GForceInvalidateDirectionalVSM,
	TEXT("Forces the clipmap to always invalidate, useful to emulate a moving sun to avoid misrepresenting cache performance."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarCacheMaxUnreferencedLightAge(
	TEXT("r.Shadow.Virtual.Cache.MaxUnreferencedLightAge"),
	0,
	TEXT("The number of frames to keep around cached pages from lights that are unreferenced (usually due to being offscreen or otherwise culled). 0=disabled.\n")
	TEXT("Higher values potentially allow for more physical page reuse when cache space is available, but setting this too high can add overhead due to maintaining the extra entries."),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarVSMReservedResource(
	TEXT("r.Shadow.Virtual.AllocatePagePoolAsReservedResource"),
	1,
	TEXT("Allocate VSM page pool as a reserved/virtual texture, backed by N small physical memory allocations to reduce fragmentation."),
	ECVF_RenderThreadSafe
);

void FVirtualShadowMapCacheEntry::UpdateClipmapLevel(
	FVirtualShadowMapArray& VirtualShadowMapArray,
	const FVirtualShadowMapPerLightCacheEntry& PerLightEntry,
	int32 VirtualShadowMapId,
	FInt64Point PageSpaceLocation,
	double LevelRadius,
	double ViewCenterZ,
	double ViewRadiusZ)
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

	if (!bCacheValid)
	{
		Clipmap.ViewCenterZ = ViewCenterZ;
		Clipmap.ViewRadiusZ = ViewRadiusZ;
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
	Prev.bIsUncached = Current.bIsUncached;
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

	Current.bIsUncached = GForceInvalidateDirectionalVSM != 0 || Prev.RenderedFrameNumber < 0;

	// On transition between uncached <-> cached we must invalidate since the static pages may not be initialized
	if (Current.bIsUncached != Prev.bIsUncached)
	{
		Prev.RenderedFrameNumber = -1;
	}
}

bool FVirtualShadowMapPerLightCacheEntry::UpdateLocal(const FProjectedShadowInitializer& InCacheKey, bool bIsDistantLight, bool bCacheEnabled, bool bAllowInvalidation)
{
	Prev.bIsUncached = Current.bIsUncached;
	Prev.bIsDistantLight = Current.bIsDistantLight;
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

	Current.bIsDistantLight = bIsDistantLight;
	Current.RenderedFrameNumber = -1;
	Current.ScheduledFrameNumber = -1;
	Current.bIsUncached = Prev.RenderedFrameNumber < 0;

	// On transition between uncached <-> cached we must invalidate since the static pages may not be initialized
	if (Current.bIsUncached != Prev.bIsUncached)
	{
		Prev.RenderedFrameNumber = -1;
	}

	return Prev.RenderedFrameNumber >= 0;
}

void FVirtualShadowMapPerLightCacheEntry::Invalidate()
{
	Prev.RenderedFrameNumber = -1;
}

static inline uint32 EncodeInstanceInvalidationPayload(bool bInvalidateStaticPage, int32 ClipmapVirtualShadowMapId = INDEX_NONE)
{
	uint32 Payload = 0;

	if (bInvalidateStaticPage)
	{
		Payload = Payload | 0x2;
	}

	if (ClipmapVirtualShadowMapId != INDEX_NONE)
	{
		// Do a single clipmap level
		Payload = Payload | 0x1;
		Payload = Payload | (((uint32)ClipmapVirtualShadowMapId) << 2);
	}

	return Payload;
}

FVirtualShadowMapArrayCacheManager::FInvalidatingPrimitiveCollector::FInvalidatingPrimitiveCollector(FVirtualShadowMapArrayCacheManager* InVirtualShadowMapArrayCacheManager)
	: Scene(*InVirtualShadowMapArrayCacheManager->Scene)
	, GPUScene(InVirtualShadowMapArrayCacheManager->Scene->GPUScene)
	, Manager(*InVirtualShadowMapArrayCacheManager)
{
}

void FVirtualShadowMapArrayCacheManager::FInvalidatingPrimitiveCollector::AddDynamicAndGPUPrimitives()
{
	// Add and clear pending invalidations enqueued on the GPU Scene from dynamic primitives added since last invalidation
	for (const FGPUScene::FInstanceRange& Range : GPUScene.DynamicPrimitiveInstancesToInvalidate)
	{
		// Dynamic primitives are never cached as static; see  FUploadDataSourceAdapterDynamicPrimitives::GetPrimitiveInfo
		// TODO: Do we ever need to invalidate these "post" update?
		Instances.Add(Range.InstanceSceneDataOffset, Range.NumInstanceSceneDataEntries, EncodeInstanceInvalidationPayload(false));
	}

	// SIDE EFFECT GLOBAL
	GPUScene.DynamicPrimitiveInstancesToInvalidate.Reset();

	for (auto& CacheEntry : Manager.CacheEntries)
	{
		for (const FVirtualShadowMapPerLightCacheEntry::FInstanceRange& Range : CacheEntry.Value->PrimitiveInstancesToInvalidate)
		{
			// Add item for each shadow map explicitly, inflates host data but improves load balancing,
			// TODO: maybe add permutation so we can strip the loop completely.
			for (const auto& SmCacheEntry : CacheEntry.Value->ShadowMapEntries)
			{
				// TODO: Do we ever need to invalidate these "post" update?
				Instances.Add(Range.InstanceSceneDataOffset, Range.NumInstanceSceneDataEntries,
					EncodeInstanceInvalidationPayload(Range.bInvalidateStaticPage, SmCacheEntry.CurrentVirtualShadowMapId));
			}
		}
		CacheEntry.Value->PrimitiveInstancesToInvalidate.Reset();
	}

	// Process any GPU readback static invalidations
	{
		FVirtualShadowMapFeedback::FReadbackInfo Readback = Manager.StaticGPUInvalidationsFeedback.GetLatestReadbackBuffer();
		if (Readback.Size > 0)
		{
			TBitArray<SceneRenderingAllocator> Primitives;
			{
				const uint32* Data = (const uint32*)Readback.Buffer->Lock(Readback.Size);			
				Primitives.AddRange(Data, Readback.Size * 8);
				Readback.Buffer->Unlock();
				// TODO: Mark that we've done this buffer? Not really harmful to redo it
			}

			for (TConstSetBitIterator<SceneRenderingAllocator> PrimitivesIt(Primitives); PrimitivesIt; ++PrimitivesIt)
			{
				const FPersistentPrimitiveIndex PersistentPrimitiveIndex = FPersistentPrimitiveIndex{PrimitivesIt.GetIndex()};

				// NOTE: Have to be a bit careful as this primitive index came from a few frames ago... thus it may no longer
				// be valid, or possibly even replaced by a new primitive.
				// TODO: Dual iterator and avoid any primitives that have been removed recently (might resuse the slot)
				const int32 PrimitiveIndex = Scene.GetPrimitiveIndex(PersistentPrimitiveIndex);
				FPrimitiveSceneInfo* PrimitiveSceneInfo = Scene.GetPrimitiveSceneInfo(PrimitiveIndex);
				if (PrimitiveSceneInfo)
				{
					if (Manager.WasRecentlyRemoved(PersistentPrimitiveIndex))
					{
						// Do nothing for now, as this slot may have been reused after a previous removal
#if VSM_LOG_STATIC_CACHING
						UE_LOG(LogRenderer, Warning, TEXT("Ignoring GPU invalidation due to recent primitive removal: %u"), PersistentPrimitiveIndex.Index);
#endif
					}
					else
					{
#if VSM_LOG_STATIC_CACHING
						UE_LOG(LogRenderer, Warning, TEXT("Transitioning GPU invalidation to dynamic caching: %u"), PersistentPrimitiveIndex.Index);
#endif
						AddInvalidation(PrimitiveSceneInfo, false);
					}
				}
			}
		}
	}
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
	const EPrimitiveDirtyState DirtyState = GPUScene.GetPrimitiveDirtyState(PrimitiveID);

	// suppress invalidations from moved primitives that are marked to behave as if they were static.
	if (!bRemovedPrimitive && PrimitiveSceneInfo->Proxy->GetShadowCacheInvalidationBehavior() == EShadowCacheInvalidationBehavior::Static)
	{
		return;
	}

	// Nanite meshes need special handling because they don't get culled on CPU, thus always process invalidations for those
	const bool bIsNaniteMesh = Scene.PrimitiveFlagsCompact[PrimitiveID].bIsNaniteMesh;
	const bool bPreviouslyCachedAsStatic = PrimitiveSceneInfo->ShouldCacheShadowAsStatic();

	if (bRemovedPrimitive)
	{
		RemovedPrimitives.PadToNum(PersistentPrimitiveIndex.Index + 1, false);
		RemovedPrimitives[PersistentPrimitiveIndex.Index] = true;
	}
	else
	{
		// Swap to dynamic caching if it was already static
		if (bPreviouslyCachedAsStatic)
		{
			// SIDE EFFECT, GLOBAL
			// TODO: Verify the timing on this...
			PrimitiveSceneInfo->SetCacheShadowAsStatic(false);
#if VSM_LOG_STATIC_CACHING
			UE_LOG(LogRenderer, Warning, TEXT("FVirtualShadowMapArrayCacheManager: '%s' switched to dynamic caching"), *PrimitiveSceneInfo->GetFullnameForDebuggingOnly());
#endif
		}
	}

	// If it's "added" we can't invalidate pre-update since it will not be in GPUScene yet
	// Similarly if it is removed, we can't invalidate post-update
	// Otherwise we currently add it to both passes as we need to invalidate both the position it came from, and the position
	// it moved *to* if transform/instances changed. Both pages may need to updated.
	// TODO: Filter out one of the updates for things like WPO animation or other cases where the transform/bounds have not changed
	const bool bInvalidatePreUpdate = !EnumHasAnyFlags(DirtyState, EPrimitiveDirtyState::Added);
	const bool bInvalidatePostUpdate = !bRemovedPrimitive;

	// TODO
	if (!bInvalidatePreUpdate)
	{
		return;
	}

	for (auto& CacheEntry : Manager.CacheEntries)
	{
		TBitArray<>& CachedPrimitives = CacheEntry.Value->CachedPrimitives;
		if (bIsNaniteMesh || (PersistentPrimitiveIndex.Index < CachedPrimitives.Num() && CachedPrimitives[PersistentPrimitiveIndex.Index]))
		{
			if (!bIsNaniteMesh)
			{
				// Clear the record as we're wiping it out.
				// SIDE EFFECT, per cache manager
				CachedPrimitives[PersistentPrimitiveIndex.Index] = false;
			}

			// Add item for each shadow map explicitly, inflates host data but improves load balancing,
			// TODO: maybe add permutation so we can strip the loop completely.
			for (const auto& SmCacheEntry : CacheEntry.Value->ShadowMapEntries)
			{
				checkSlow(SmCacheEntry.CurrentVirtualShadowMapId != INDEX_NONE);
				uint32 Payload = EncodeInstanceInvalidationPayload(bPreviouslyCachedAsStatic, SmCacheEntry.CurrentVirtualShadowMapId);
				Instances.Add(InstanceSceneDataOffset, NumInstanceSceneDataEntries, Payload);
			}
		}
	}
}

void FVirtualShadowMapArrayCacheManager::FInvalidatingPrimitiveCollector::Finalize()
{
	Instances.FinalizeBatches();
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
#if VSM_LOG_STATIC_CACHING
		UE_LOG(LogRenderer, Warning, TEXT("FVirtualShadowMapFeedback ran out of feedback buffers!"));
#endif
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
{
	// Handle message with status sent back from GPU
	StatusFeedbackSocket = GPUMessage::RegisterHandler(TEXT("Shadow.Virtual.StatusFeedback"), [this](GPUMessage::FReader Message)
	{
		// Goes negative on underflow
		int32 NumPagesFree = Message.Read<int32>(0);
			
		CSV_CUSTOM_STAT(VSM, FreePages, NumPagesFree, ECsvCustomStatOp::Set);

		if (NumPagesFree < 0)
		{
			static const auto* CVarResolutionLodBiasLocalPtr = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.Shadow.Virtual.ResolutionLodBiasLocal"));
			const float LodBiasLocal = CVarResolutionLodBiasLocalPtr->GetValueOnRenderThread();

			static const auto* CVarResolutionLodBiasDirectionalPtr = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.Shadow.Virtual.ResolutionLodBiasDirectional"));
			const float LodBiasDirectional = CVarResolutionLodBiasDirectionalPtr->GetValueOnRenderThread();

			static const auto* CVarMaxPhysicalPagesPtr = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Shadow.Virtual.MaxPhysicalPages"));
			const int32 MaxPhysicalPages = CVarMaxPhysicalPagesPtr->GetValueOnRenderThread();

			static const auto* CVarMaxPhysicalPagesSceneCapturePtr = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Shadow.Virtual.MaxPhysicalPagesSceneCapture"));
			const int32 MaxPhysicalPagesSceneCapture = CVarMaxPhysicalPagesSceneCapturePtr->GetValueOnRenderThread();

#if !UE_BUILD_SHIPPING
			if (!bLoggedPageOverflow)
			{
				UE_LOG(LogRenderer, Warning, TEXT("Virtual Shadow Map Page Pool overflow (%d page allocations were not served), this will produce visual artifacts (missing shadow), increase the page pool limit or reduce resolution bias to avoid.\n")
					TEXT(" See r.Shadow.Virtual.MaxPhysicalPages (%d), r.Shadow.Virtual.MaxPhysicalPagesSceneCapture (%d), r.Shadow.Virtual.ResolutionLodBiasLocal (%.2f), and r.Shadow.Virtual.ResolutionLodBiasDirectional (%.2f)"),
					-NumPagesFree,
					MaxPhysicalPages,
					MaxPhysicalPagesSceneCapture,
					LodBiasLocal,
					LodBiasDirectional);
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
		int32 CurrentFrameNumber = Scene->GetFrameNumber();
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


void FVirtualShadowMapArrayCacheManager::SetPhysicalPoolSize(FRDGBuilder& GraphBuilder, FIntPoint RequestedSize, int RequestedArraySize, uint32 MaxPhysicalPages)
{
	// Using ReservedResource|ImmediateCommit flags hint to the RHI that the resource can be allocated using N small physical memory allocations,
	// instead of a single large contighous allocation. This helps Windows video memory manager page allocations in and out of local memory more efficiently.
	ETextureCreateFlags RequestedCreateFlags = (CVarVSMReservedResource.GetValueOnRenderThread() && GRHISupportsReservedResources)
		? (TexCreate_ReservedResource | TexCreate_ImmediateCommit)
		: TexCreate_None;

	if (!PhysicalPagePool 
		|| PhysicalPagePool->GetDesc().Extent != RequestedSize 
		|| PhysicalPagePool->GetDesc().ArraySize != RequestedArraySize
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

		// Allocate page metadata alongside
		FRDGBufferRef PhysicalPageMetaDataRDG = GraphBuilder.CreateBuffer(
			FRDGBufferDesc::CreateStructuredDesc(sizeof(FPhysicalPageMetaData), MaxPhysicalPages),
			TEXT("Shadow.Virtual.PhysicalPageMetaData"));
		// Persistent, so we extract it immediately
		PhysicalPageMetaData = GraphBuilder.ConvertToExternalBuffer(PhysicalPageMetaDataRDG);

		Invalidate();
	}
}

void FVirtualShadowMapArrayCacheManager::FreePhysicalPool()
{
	if (PhysicalPagePool)
	{
		PhysicalPagePool = nullptr;
		Invalidate();
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
	}

	return HZBPhysicalPagePool;
}

void FVirtualShadowMapArrayCacheManager::FreeHZBPhysicalPool()
{
	if (HZBPhysicalPagePool)
	{
		HZBPhysicalPagePool = nullptr;
		Invalidate();
	}
}

void FVirtualShadowMapArrayCacheManager::Invalidate()
{
	// Clear the cache
	CacheEntries.Reset();

	// Mark globally invalid until the next GPU allocation/metadata update
	bCacheDataValid = false;

	//UE_LOG(LogRenderer, Display, TEXT("Virtual shadow map cache invalidated."));
}

void FVirtualShadowMapArrayCacheManager::MarkCacheDataValid()
{
	bCacheDataValid = true;
}

bool FVirtualShadowMapArrayCacheManager::IsCacheEnabled()
{
	return CVarCacheVirtualSMs.GetValueOnRenderThread() != 0;
}

bool FVirtualShadowMapArrayCacheManager::IsCacheDataAvailable()
{
	return IsCacheEnabled() &&
		bCacheDataValid &&
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

TSharedPtr<FVirtualShadowMapPerLightCacheEntry> FVirtualShadowMapArrayCacheManager::FindCreateLightCacheEntry(
	int32 LightSceneId, uint32 ViewUniqueID, uint32 NumShadowMaps)
{
	const uint64 CacheKey = (uint64(ViewUniqueID) << 32U) | uint64(LightSceneId);

	TSharedPtr<FVirtualShadowMapPerLightCacheEntry> *LightEntryKey = CacheEntries.Find(CacheKey);

	if (LightEntryKey)
	{
		TSharedPtr<FVirtualShadowMapPerLightCacheEntry> LightEntry = *LightEntryKey;

		if (LightEntry->ShadowMapEntries.Num() == NumShadowMaps)
		{
			LightEntry->ReferencedRenderSequenceNumber = RenderSequenceNumber;
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
	LightEntry->ReferencedRenderSequenceNumber = RenderSequenceNumber;	
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
		if (PrimitiveSceneInfo->Proxy->HasDeformableMesh())
		{
			PrimitiveInstancesToInvalidate.Add(FInstanceRange{ 
				PrimitiveSceneInfo->GetInstanceSceneDataOffset(),
				PrimitiveSceneInfo->GetNumInstanceSceneDataEntries(),
				PrimitiveSceneInfo->ShouldCacheShadowAsStatic()
			});
		}
	}
}

void FVirtualShadowMapArrayCacheManager::UpdateUnreferencedCacheEntries(
	FVirtualShadowMapArray& VirtualShadowMapArray)
{
	const int64 MaxAge = CVarCacheMaxUnreferencedLightAge.GetValueOnRenderThread();
	const bool bAllowUnreferencedEntries = IsCacheEnabled() && MaxAge > 0;

	TArray<uint64, SceneRenderingAllocator> EntriesToRemove;
	for (auto& LightEntry : CacheEntries)
	{
		TSharedPtr<FVirtualShadowMapPerLightCacheEntry> CacheEntry = LightEntry.Value;
		// NOTE: We probably want to decouple the age from render calls at some point, but regardless we need to know
		// in the first branch that the given light was referenced *this* render, and therefore has already been
		// (re)allocated a current VSM ID.
		int64 Age = RenderSequenceNumber - CacheEntry->ReferencedRenderSequenceNumber;
		check(CacheEntry->ReferencedRenderSequenceNumber >= 0);
		check(Age >= 0);
		if (Age == 0)
		{
			// Referenced this frame still, leave it alone
			check(CacheEntry->ShadowMapEntries.Last().CurrentVirtualShadowMapId < VirtualShadowMapArray.GetNumShadowMapSlots());
		}
		else if (bAllowUnreferencedEntries && Age < MaxAge)
		{
			INC_DWORD_STAT(STAT_VSMUnreferencedLights);

			int PrevBaseVirtualShadowMapId = CacheEntry->ShadowMapEntries[0].CurrentVirtualShadowMapId;
			bool bIsSinglePage = FVirtualShadowMapArray::IsSinglePage(PrevBaseVirtualShadowMapId);

			// Keep the entry, reallocate new VSM IDs
			int32 NumMaps = CacheEntry->ShadowMapEntries.Num();
			int32 VirtualShadowMapId = VirtualShadowMapArray.Allocate(bIsSinglePage, NumMaps);
			for (int32 Map = 0; Map < NumMaps; ++Map)
			{
				CacheEntry->ShadowMapEntries[Map].Update(VirtualShadowMapArray, *CacheEntry, VirtualShadowMapId + Map);
				// NOTE: Leave the ProjectionData as whatever it was before
				// TODO: We may want to add a flag that this is unreferenced so we can prune it from the light grid and skip it in page marking, etc...?
				// Except in theory if we are marking things from onscreen pixels then we wouldn't have culled it... (small light culling though)?
			}
		}
		else
		{
			// Enqueue for remove
			EntriesToRemove.Add(LightEntry.Key);
			//UE_LOG(LogRenderer, Display, TEXT("Removed VSM light cache entry (%d, Age %d)"), LightEntry.Key, Age);
		}
	}

	for (uint64 Entry : EntriesToRemove)
	{
		int32 NumRemoved = CacheEntries.Remove(Entry);
		check(NumRemoved > 0);
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
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer< uint >, InStatsBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer< uint >, AccumulatedStatsBufferOut)
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
	bool bEnableCaching)
{
	TrimLoggingInfo();

	const bool bNewShadowData = VirtualShadowMapArray.IsAllocated();
	const bool bDropAll = !bEnableCaching;
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
		FreePhysicalPool();
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

			// Enqueue readback
			StaticGPUInvalidationsFeedback.SubmitFeedbackBuffer(GraphBuilder, VirtualShadowMapArray.StaticInvalidatingPrimitivesRDG);
			
			// Store but drop any temp references embedded in the uniform parameters this frame.
			// We'll reestablish them when we reimport the extracted resources next frame
			PrevUniformParameters = VirtualShadowMapArray.UniformParameters;
			PrevUniformParameters.ProjectionData = nullptr;
			PrevUniformParameters.PageTable = nullptr;
			PrevUniformParameters.PhysicalPagePool = nullptr;
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
	else
	{
		// Do nothing; maintain the data that we had
		// This allows us to work around some cases where the renderer gets called multiple times in a given frame
		// - such as scene captures - but does no shadow-related work in all but one of them. We do not want to drop
		// all the cached data in this case otherwise we effectively get no caching at all.
		// Ideally in the long run we want the cache itself to be more robust against rendering multiple views. but
		// for now this at least provides a work-around for some common cases where only one view is rendering VSMs.
	}

	// Every once in a while zero out our recently removed primitive flags. This lets us ignore slots that
	// may have been reused since they were flagged from GPU invalidations.
	++RecentlyRemovedFrameCounter;
	if (RecentlyRemovedFrameCounter >= 3)
	{
		RecentlyRemovedPrimitives[RecentlyRemovedReadIndex].Reset();
		RecentlyRemovedReadIndex = 1 - RecentlyRemovedReadIndex;
		RecentlyRemovedFrameCounter = 0;
	}

	++RenderSequenceNumber;
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
				TEXT("Allocated"),
				TEXT("StaticCached"),
				TEXT("StaticInvalidated"),
				TEXT("DynamicCached"),
				TEXT("DynamicInvalidated"),
				TEXT("NumSms"),
				TEXT("NonNaniteInstances"),
				TEXT("NonNaniteInstancesDrawn"),
				TEXT("NonNaniteInstancesHZBCulled"),
				TEXT("NonNaniteInstancesPageMaskCulled"),
				TEXT("NonNaniteInstancesEmptyRectCulled"),
				TEXT("NonNaniteInstancesFrustumCulled"),
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

void FVirtualShadowMapArrayCacheManager::ProcessInvalidations(FRDGBuilder& GraphBuilder, FSceneUniformBuffer &SceneUniformBuffer, const FInvalidatingPrimitiveCollector& InvalidatingPrimitiveCollector)
{
	// Always incorporate any scene removals into the "recently removed" list
	UpdateRecentlyRemoved(InvalidatingPrimitiveCollector.GetRemovedPrimitives());

	if (IsCacheDataAvailable())
	{
		RDG_EVENT_SCOPE(GraphBuilder, "Shadow.Virtual.ProcessInvalidations");

		if (!InvalidatingPrimitiveCollector.Instances.IsEmpty())
		{
			ProcessInvalidations(GraphBuilder, SceneUniformBuffer, InvalidatingPrimitiveCollector.Instances);
		}
	}
}

void FVirtualShadowMapArrayCacheManager::OnSceneChange()
{
	{
		const int32 MaxPersistentPrimitiveIndex = FMath::Max(1, Scene->GetMaxPersistentPrimitiveIndex());

		for (auto& CacheEntry : CacheEntries)
		{
			CacheEntry.Value->CachedPrimitives.SetNum(MaxPersistentPrimitiveIndex, false);
			CacheEntry.Value->RenderedPrimitives.SetNum(MaxPersistentPrimitiveIndex, false);
		}
	}
}

void FVirtualShadowMapArrayCacheManager::OnLightRemoved(int32 LightId)
{
	CacheEntries.Remove(LightId);
}

/**
 * Compute shader to project and invalidate the rectangles of given instances.
 */
class FVirtualSmInvalidateInstancePagesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVirtualSmInvalidateInstancePagesCS);
	SHADER_USE_PARAMETER_STRUCT(FVirtualSmInvalidateInstancePagesCS, FGlobalShader)

	class FDebugDim : SHADER_PERMUTATION_BOOL("ENABLE_DEBUG_MODE");
	class FUseHzbDim : SHADER_PERMUTATION_BOOL("USE_HZB_OCCLUSION");
	using FPermutationDomain = TShaderPermutationDomain<FUseHzbDim, FDebugDim>;

public:
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FVirtualShadowMapUniformParameters, VirtualShadowMap)
		SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrintUniformBuffer)
		SHADER_PARAMETER(uint32, bDrawBounds)

		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer< FPhysicalPageMetaData >, PhysicalPageMetaDataOut)

		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneUniformParameters, Scene)

		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer< uint >, HZBPageTable)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer< uint4 >, HZBPageRectBounds)

		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, HZBTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, HZBSampler)
		SHADER_PARAMETER( FVector2f,	HZBSize )

		// GPU instances parameters
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint>, InvalidatingInstances)
		SHADER_PARAMETER(uint32, NumInvalidatingInstanceSlots)
		RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<uint>, OutStaticInvalidatingPrimitives)

		SHADER_PARAMETER_STRUCT_INCLUDE(FGPUScene::FInstanceGPULoadBalancer::FShaderParameters, LoadBalancerParameters)
	END_SHADER_PARAMETER_STRUCT()

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
IMPLEMENT_GLOBAL_SHADER(FVirtualSmInvalidateInstancePagesCS, "/Engine/Private/VirtualShadowMaps/VirtualShadowMapCacheManagement.usf", "VirtualSmInvalidateInstancePagesCS", SF_Compute);


TRDGUniformBufferRef<FVirtualShadowMapUniformParameters> FVirtualShadowMapArrayCacheManager::GetPreviousUniformBuffer(FRDGBuilder& GraphBuilder) const
{
	FVirtualShadowMapUniformParameters* VersionedParameters = GraphBuilder.AllocParameters<FVirtualShadowMapUniformParameters>();
	*VersionedParameters = PrevUniformParameters;
	return GraphBuilder.CreateUniformBuffer(VersionedParameters);
}

#if WITH_MGPU
void FVirtualShadowMapArrayCacheManager::UpdateGPUMask(FRHIGPUMask GPUMask)
{
	if (LastGPUMask != GPUMask)
	{
		LastGPUMask = GPUMask;
		Invalidate();
	}
}
#endif  // WITH_MGPU


void FVirtualShadowMapArrayCacheManager::ProcessInvalidations(
	FRDGBuilder& GraphBuilder,
	FSceneUniformBuffer &SceneUniformBuffer,
	const FInstanceGPULoadBalancer& Instances) const
{
	if (Instances.IsEmpty())
	{
		return;
	}

	RDG_EVENT_SCOPE(GraphBuilder, "ProcessInvalidations [%d batches]", Instances.GetBatches().Num());

	FVirtualSmInvalidateInstancePagesCS::FPermutationDomain PermutationVector;
	FVirtualSmInvalidateInstancePagesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FVirtualSmInvalidateInstancePagesCS::FParameters>();
	{
		// Construct a uniform buffer based on the previous frame data, reimported into this graph builder
		FVirtualShadowMapUniformParameters* UniformParameters = GraphBuilder.AllocParameters<FVirtualShadowMapUniformParameters>();
		*UniformParameters = GetPreviousUniformParameters();
		{
			auto RegExtCreateSrv = [&GraphBuilder](const TRefCountPtr<FRDGPooledBuffer>& Buffer, const TCHAR* Name) -> FRDGBufferSRVRef
			{
				return GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalBuffer(Buffer, Name));
			};

			UniformParameters->ProjectionData = RegExtCreateSrv(PrevBuffers.ProjectionData, TEXT("Shadow.Virtual.PrevProjectionData"));
			UniformParameters->PageTable = RegExtCreateSrv(PrevBuffers.PageTable, TEXT("Shadow.Virtual.PrevPageTable"));
			UniformParameters->PageFlags = RegExtCreateSrv(PrevBuffers.PageFlags, TEXT("Shadow.Virtual.PrevPageFlags"));
			UniformParameters->PageRectBounds = RegExtCreateSrv(PrevBuffers.PageRectBounds, TEXT("Shadow.Virtual.PrevPageRectBounds"));
			// Unused in this path
			UniformParameters->PhysicalPagePool = GSystemTextures.GetZeroUIntArrayDummy(GraphBuilder);
		}
		PassParameters->VirtualShadowMap = GraphBuilder.CreateUniformBuffer(UniformParameters);

		PassParameters->Scene = SceneUniformBuffer.GetBuffer(GraphBuilder);

		PassParameters->PhysicalPageMetaDataOut = GraphBuilder.CreateUAV(GraphBuilder.RegisterExternalBuffer(PhysicalPageMetaData));
		PassParameters->bDrawBounds = CVarDrawInvalidatingBounds.GetValueOnRenderThread() != 0;

		// Note: this disables the whole debug permutation since the parameters must be bound.
		const bool bUseDebugPermutation = PassParameters->bDrawBounds && ShaderPrint::IsDefaultViewEnabled();
		if (bUseDebugPermutation)
		{		
			ShaderPrint::SetEnabled(true);
			ShaderPrint::RequestSpaceForLines(Instances.GetTotalNumInstances() * 12);
			ShaderPrint::SetParameters(GraphBuilder, PassParameters->ShaderPrintUniformBuffer);
		}

		const bool bUseHZB = (CVarCacheVsmUseHzb.GetValueOnRenderThread() != 0);
		const TRefCountPtr<IPooledRenderTarget> HZBPhysical = (bUseHZB && HZBPhysicalPagePool) ? HZBPhysicalPagePool : nullptr;
		if (HZBPhysical)
		{
			// Same, since we are not producing a new frame just yet
			PassParameters->HZBPageTable = UniformParameters->PageTable;
			PassParameters->HZBPageRectBounds = UniformParameters->PageRectBounds;
			PassParameters->HZBTexture = GraphBuilder.RegisterExternalTexture(HZBPhysical);
			PassParameters->HZBSize = HZBPhysical->GetDesc().Extent;
			PassParameters->HZBSampler = TStaticSamplerState< SF_Point, AM_Clamp, AM_Clamp, AM_Clamp >::GetRHI();

		}
		PermutationVector.Set<FVirtualSmInvalidateInstancePagesCS::FDebugDim>(bUseDebugPermutation);
		PermutationVector.Set<FVirtualSmInvalidateInstancePagesCS::FUseHzbDim>(HZBPhysical != nullptr);
	}
	
	Instances.UploadFinalized(GraphBuilder).GetShaderParameters(GraphBuilder, PassParameters->LoadBalancerParameters);

	auto ComputeShader = GetGlobalShaderMap(Scene->GetFeatureLevel())->GetShader<FVirtualSmInvalidateInstancePagesCS>(PermutationVector);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("VirtualSmInvalidateInstancePagesCS"),
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
