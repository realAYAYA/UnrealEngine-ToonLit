// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SkeletalRenderGPUSkin.cpp: GPU skinned skeletal mesh rendering code.
=============================================================================*/

#include "SkeletalRenderGPUSkin.h"
#include "Animation/MeshDeformerInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "RenderUtils.h"
#include "SkeletalRender.h"
#include "GPUSkinCache.h"
#include "RayTracingSkinnedGeometry.h"
#include "Rendering/RenderCommandPipes.h"
#include "ShaderParameterUtils.h"
#include "SceneInterface.h"
#include "SkeletalMeshSceneProxy.h"
#include "RenderGraphUtils.h"
#include "RenderCore.h"

DEFINE_LOG_CATEGORY_STATIC(LogSkeletalGPUSkinMesh, Warning, All);

// 0/1
#define UPDATE_PER_BONE_DATA_ONLY_FOR_OBJECT_BEEN_VISIBLE 1

DECLARE_CYCLE_STAT(TEXT("Morph Vertex Buffer Update"),STAT_MorphVertexBuffer_Update,STATGROUP_MorphTarget);
DECLARE_CYCLE_STAT(TEXT("Morph Vertex Buffer Init"),STAT_MorphVertexBuffer_Init,STATGROUP_MorphTarget);
DECLARE_CYCLE_STAT(TEXT("Morph Vertex Buffer Apply Delta"),STAT_MorphVertexBuffer_ApplyDelta,STATGROUP_MorphTarget);
DECLARE_CYCLE_STAT(TEXT("Morph Vertex Buffer Alloc"), STAT_MorphVertexBuffer_Alloc, STATGROUP_MorphTarget);
DECLARE_CYCLE_STAT(TEXT("Morph Vertex Buffer RHI Lock and copy"), STAT_MorphVertexBuffer_RhiLockAndCopy, STATGROUP_MorphTarget);
DECLARE_CYCLE_STAT(TEXT("Morph Vertex Buffer RHI Unlock"), STAT_MorphVertexBuffer_RhiUnlock, STATGROUP_MorphTarget);
DECLARE_GPU_STAT_NAMED(MorphTargets, TEXT("Morph Target Compute"));

static TAutoConsoleVariable<int32> CVarMotionBlurDebug(
	TEXT("r.MotionBlurDebug"),
	0,
	TEXT("Defines if we log debugging output for motion blur rendering.\n")
	TEXT(" 0: off (default)\n")
	TEXT(" 1: on"),
	ECVF_Cheat | ECVF_RenderThreadSafe);

static int32 GUseGPUMorphTargets = 1;
static FAutoConsoleVariableRef CVarUseGPUMorphTargets(
	TEXT("r.MorphTarget.Mode"),
	GUseGPUMorphTargets,
	TEXT("Use GPU for computing morph targets.\n")
	TEXT(" 0: Use original CPU method (loop per morph then by vertex)\n")
	TEXT(" 1: Enable GPU method (default)\n"),
	ECVF_ReadOnly
	);

static int32 GForceUpdateMorphTargets = 0;
static FAutoConsoleVariableRef CVarForceUpdateMorphTargets(
	TEXT("r.MorphTarget.ForceUpdate"),
	GForceUpdateMorphTargets,
	TEXT("Force morph target deltas to be calculated every frame.\n")
	TEXT(" 0: Default\n")
	TEXT(" 1: Force Update\n"),
	ECVF_Default
	);

static bool UseGPUMorphTargets(ERHIFeatureLevel::Type FeatureLevel)
{
	return GUseGPUMorphTargets != 0 && FeatureLevel >= ERHIFeatureLevel::SM5;
}

static float GMorphTargetWeightThreshold = UE_SMALL_NUMBER;
static FAutoConsoleVariableRef CVarMorphTargetWeightThreshold(
	TEXT("r.MorphTarget.WeightThreshold"),
	GMorphTargetWeightThreshold,
	*FString::Printf(TEXT("Set MorphTarget Weight Threshold (Default : %f).\n"), UE_SMALL_NUMBER), 
	ECVF_Default
);

static int32 GetRayTracingSkeletalMeshGlobalLODBias()
{
	static const TConsoleVariableData<int32>* const RayTracingSkeletalMeshLODBiasVar =
		IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.RayTracing.Geometry.SkeletalMeshes.LODBias"));

	return !RayTracingSkeletalMeshLODBiasVar ? 0 :
		FMath::Max(0, RayTracingSkeletalMeshLODBiasVar->GetValueOnAnyThread());  // Only allows positive bias to narrow cloth mapping requirements
}

/*-----------------------------------------------------------------------------
FMorphVertexBuffer
-----------------------------------------------------------------------------*/

void FMorphVertexBuffer::InitRHI(FRHICommandListBase& RHICmdList)
{
	// LOD of the skel mesh is used to find number of vertices in buffer
	FSkeletalMeshLODRenderData& LodData = SkelMeshRenderData->LODRenderData[LODIdx];

	// Create the buffer rendering resource
	uint32 Size = LodData.GetNumVertices() * sizeof(FMorphGPUSkinVertex);
	FRHIResourceCreateInfo CreateInfo(TEXT("MorphVertexBuffer"));

	const EShaderPlatform ShaderPlatform = GetFeatureLevelShaderPlatform(FeatureLevel);
	const bool bUseGPUMorphTargets = UseGPUMorphTargets(FeatureLevel);
	bUsesComputeShader = bUseGPUMorphTargets;

	EBufferUsageFlags Flags = bUseGPUMorphTargets ? (EBufferUsageFlags)(BUF_Static | BUF_UnorderedAccess) : BUF_Dynamic;

	// BUF_ShaderResource is needed for Morph support of the SkinCache
	Flags = (EBufferUsageFlags)(Flags | BUF_ShaderResource);

	VertexBufferRHI = RHICmdList.CreateVertexBuffer(Size, Flags, CreateInfo);
	VertexBufferRHI->SetOwnerName(GetOwnerName());
	SRVValue = RHICmdList.CreateShaderResourceView(VertexBufferRHI, 4, PF_R32_FLOAT);

	if (!bUseGPUMorphTargets)
	{
		// Lock the buffer.
		void* BufferData = RHICmdList.LockBuffer(VertexBufferRHI, 0, sizeof(FMorphGPUSkinVertex)*LodData.GetNumVertices(), RLM_WriteOnly);
		FMorphGPUSkinVertex* Buffer = (FMorphGPUSkinVertex*)BufferData;
		FMemory::Memzero(&Buffer[0], sizeof(FMorphGPUSkinVertex)*LodData.GetNumVertices());
		// Unlock the buffer.
		RHICmdList.UnlockBuffer(VertexBufferRHI);
		bNeedsInitialClear = false;
	}
	else
	{
		UAVValue = RHICmdList.CreateUnorderedAccessView(VertexBufferRHI, PF_R32_UINT);
		bNeedsInitialClear = true;
	}

	// hasn't been updated yet
	bHasBeenUpdated = false;
}

void FMorphVertexBuffer::ReleaseRHI()
{
	UAVValue.SafeRelease();
	VertexBufferRHI.SafeRelease();
	SRVValue.SafeRelease();
}

/*-----------------------------------------------------------------------------
FMorphVertexBufferPool
-----------------------------------------------------------------------------*/
void FMorphVertexBufferPool::InitResources(const FName& OwnerName)
{
	// InitResources may be called again when morph vertex data is persisted during render state re-creation.
	if (!bInitializedResources)
	{
		MorphVertexBuffers[0].SetOwnerName(OwnerName);
		MorphVertexBuffers[1].SetOwnerName(OwnerName);

		check(!MorphVertexBuffers[0].VertexBufferRHI.IsValid());
		check(!MorphVertexBuffers[1].VertexBufferRHI.IsValid());
		BeginInitResource(&MorphVertexBuffers[0], &UE::RenderCommandPipe::SkeletalMesh);
		if (bDoubleBuffer)
		{
			BeginInitResource(&MorphVertexBuffers[1], &UE::RenderCommandPipe::SkeletalMesh);
		}

		bInitializedResources = true;
	}
}

void FMorphVertexBufferPool::ReleaseResources()
{
	check(bInitializedResources);
	MorphVertexBuffers[0].ReleaseResource();
	MorphVertexBuffers[1].ReleaseResource();
	bInitializedResources = false;
}

SIZE_T FMorphVertexBufferPool::GetResourceSize() const
{
	SIZE_T ResourceSize = sizeof(*this);
	ResourceSize += MorphVertexBuffers[0].GetResourceSize();
	ResourceSize += MorphVertexBuffers[1].GetResourceSize();
	return ResourceSize;
}

void FMorphVertexBufferPool::EnableDoubleBuffer(FRHICommandListBase& RHICmdList)
{
	bDoubleBuffer = true;
	if (!MorphVertexBuffers[1].VertexBufferRHI.IsValid())
	{
		MorphVertexBuffers[1].InitResource(RHICmdList);
	}
}

void FMorphVertexBufferPool::SetCurrentRevisionNumber(uint32 RevisionNumber)
{
	if (bDoubleBuffer)
	{
		// Flip revision number to previous if this is new, otherwise keep current version.
		if (CurrentRevisionNumber != RevisionNumber)
		{
			PreviousRevisionNumber = CurrentRevisionNumber;
			CurrentRevisionNumber = RevisionNumber;
			CurrentBuffer = 1 - CurrentBuffer;
		}
	}
}

const FMorphVertexBuffer& FMorphVertexBufferPool::GetMorphVertexBufferForReading(bool bPrevious) const
{
	uint32 Index = 0;
	if (bDoubleBuffer)
	{
		if ((CurrentRevisionNumber - PreviousRevisionNumber) > 1)
		{
			// If the revision number has incremented too much, ignore the request and use the current buffer.
			// With ClearMotionVector calls, we intentionally increment revision number to retrieve current buffer for bPrevious true.
			bPrevious = false;
		}

		Index = CurrentBuffer ^ (uint32)bPrevious;

		if (!MorphVertexBuffers[Index].bHasBeenUpdated)
		{
			// this should only happen the first time updating, in which case the previous buffer hasn't been written into yet.
			check(Index == 1);
			check(MorphVertexBuffers[0].bHasBeenUpdated);
			Index = 0;
		}
	}

	checkf(MorphVertexBuffers[Index].VertexBufferRHI.IsValid(), TEXT("Index: %i Buffer0: %s Buffer1: %s"), Index, MorphVertexBuffers[0].VertexBufferRHI.IsValid() ? TEXT("true") : TEXT("false"), MorphVertexBuffers[1].VertexBufferRHI.IsValid() ? TEXT("true") : TEXT("false"));
	return MorphVertexBuffers[Index];
}

FMorphVertexBuffer& FMorphVertexBufferPool::GetMorphVertexBufferForWriting()
{
	return MorphVertexBuffers[CurrentBuffer];
}

/*-----------------------------------------------------------------------------
FSkeletalMeshObjectGPUSkin
-----------------------------------------------------------------------------*/
FSkeletalMeshObjectGPUSkin::FSkeletalMeshObjectGPUSkin(USkinnedMeshComponent* InMeshComponent, FSkeletalMeshRenderData* InSkelMeshRenderData, ERHIFeatureLevel::Type InFeatureLevel)
	: FSkeletalMeshObject(InMeshComponent, InSkelMeshRenderData, InFeatureLevel)
	,	DynamicData(NULL)
	,	bNeedsUpdateDeferred(false)
	,	bMorphNeedsUpdateDeferred(false)
	,	bMorphResourcesInitialized(false)
	, 	LastBoneTransformRevisionNumber(0)
	,	bAlwaysUpdateMorphVertexBuffer(false)
{
	FSkeletalMeshObjectGPUSkin* PreviousMeshObject = nullptr;
	if (InMeshComponent->PreviousMeshObject && InMeshComponent->PreviousMeshObject->IsGPUSkinMesh())
	{
		PreviousMeshObject = (FSkeletalMeshObjectGPUSkin*)InMeshComponent->PreviousMeshObject;

		// Don't use re-create data if the mesh or feature level changed
		if (PreviousMeshObject->SkeletalMeshRenderData != InSkelMeshRenderData || PreviousMeshObject->FeatureLevel != InFeatureLevel)
		{
			PreviousMeshObject = nullptr;
		}
	}

	if (PreviousMeshObject)
	{
		// Transfer GPU skin cache from PreviousMeshObject -- needs to happen on render thread.  PreviousMeshObject is defer deleted, so it's safe to access it there.
		ENQUEUE_RENDER_COMMAND(ReleaseSkeletalMeshSkinCacheResources)(UE::RenderCommandPipe::SkeletalMesh,
			[this, PreviousMeshObject](FRHICommandList& RHICmdList)
			{
				SkinCacheEntry = PreviousMeshObject->SkinCacheEntry;
				SkinCacheEntryForRayTracing = PreviousMeshObject->SkinCacheEntryForRayTracing;

				// patch entries to point to new GPUSkin
				FGPUSkinCache::SetEntryGPUSkin(SkinCacheEntry, this);
				FGPUSkinCache::SetEntryGPUSkin(SkinCacheEntryForRayTracing, this);

				PreviousMeshObject->SkinCacheEntry = nullptr;
				PreviousMeshObject->SkinCacheEntryForRayTracing = nullptr;
			}
		);
	}

	// create LODs to match the base mesh
	LODs.Empty(SkeletalMeshRenderData->LODRenderData.Num());
	for( int32 LODIndex=0;LODIndex < SkeletalMeshRenderData->LODRenderData.Num();LODIndex++ )
	{
		FMorphVertexBufferPool* RecreateMorphVertexBuffer = nullptr;
		if (PreviousMeshObject)
		{
			RecreateMorphVertexBuffer = PreviousMeshObject->LODs[LODIndex].MorphVertexBufferPool;
		}

		new(LODs) FSkeletalMeshObjectLOD(SkeletalMeshRenderData, LODIndex, InFeatureLevel, RecreateMorphVertexBuffer);
	}

	InitResources(InMeshComponent);

#if RHI_RAYTRACING
	RayTracingUpdateQueue = InMeshComponent->GetScene()->GetRayTracingSkinnedGeometryUpdateQueue();
#endif
}


FSkeletalMeshObjectGPUSkin::~FSkeletalMeshObjectGPUSkin()
{
	if (DynamicData)
	{
		FDynamicSkelMeshObjectDataGPUSkin::FreeDynamicSkelMeshObjectDataGPUSkin(DynamicData);
	}
	DynamicData = nullptr;
}


void FSkeletalMeshObjectGPUSkin::InitResources(USkinnedMeshComponent* InMeshComponent)
{
	for( int32 LODIndex=0;LODIndex < LODs.Num();LODIndex++ )
	{
		FSkeletalMeshObjectLOD& SkelLOD = LODs[LODIndex];
		
		// Skip LODs that have their render data stripped
		if (SkelLOD.SkelMeshRenderData && SkelLOD.SkelMeshRenderData->LODRenderData.IsValidIndex(LODIndex) && SkelLOD.SkelMeshRenderData->LODRenderData[LODIndex].GetNumVertices() > 0)
		{
			const FSkelMeshObjectLODInfo& MeshLODInfo = LODInfo[LODIndex];

			FSkelMeshComponentLODInfo* CompLODInfo = nullptr;
			if (InMeshComponent->LODInfo.IsValidIndex(LODIndex))
			{
				CompLODInfo = &InMeshComponent->LODInfo[LODIndex];
			}

			SkelLOD.InitResources(MeshLODInfo, CompLODInfo, FeatureLevel);
		}
	}

#if RHI_RAYTRACING
	if (IsRayTracingAllowed() && bSupportRayTracing)
	{
		BeginInitResource(&RayTracingGeometry, &UE::RenderCommandPipe::SkeletalMesh);
	}
#endif
}
void FSkeletalMeshObjectGPUSkin::ReleaseResources()
{
	for( int32 LODIndex=0;LODIndex < LODs.Num();LODIndex++ )
	{
		FSkeletalMeshObjectLOD& SkelLOD = LODs[LODIndex];
		SkelLOD.ReleaseResources();
	}
	// also release morph resources
	ReleaseMorphResources();
	FSkeletalMeshObjectGPUSkin* MeshObject = this;
	FGPUSkinCacheEntry** PtrSkinCacheEntry = &SkinCacheEntry;
	ENQUEUE_RENDER_COMMAND(ReleaseSkeletalMeshSkinCacheResources)(UE::RenderCommandPipe::SkeletalMesh,
		[MeshObject, PtrSkinCacheEntry, &SkinCacheEntryForRayTracing = SkinCacheEntryForRayTracing](FRHICommandList& RHICmdList)
		{
			FGPUSkinCacheEntry*& LocalSkinCacheEntry = *PtrSkinCacheEntry;
			FGPUSkinCache::Release(LocalSkinCacheEntry);
			FGPUSkinCacheEntry* LocalSkinCacheEntryForRayTracing = SkinCacheEntryForRayTracing;
			FGPUSkinCache::Release(LocalSkinCacheEntryForRayTracing);

			*PtrSkinCacheEntry = nullptr;
			SkinCacheEntryForRayTracing = nullptr;
		}
	);

#if RHI_RAYTRACING
	if (bSupportRayTracing)
	{
		BeginReleaseResource(&RayTracingGeometry, &UE::RenderCommandPipe::SkeletalMesh);
	}

	// Only enqueue when initialized
	if (RayTracingUpdateQueue != nullptr || RayTracingDynamicVertexBuffer.NumBytes > 0)
	{
		ENQUEUE_RENDER_COMMAND(ReleaseRayTracingDynamicVertexBuffer)(UE::RenderCommandPipe::SkeletalMesh,
			[RayTracingUpdateQueue = RayTracingUpdateQueue, RayTracingGeometryPtr = &RayTracingGeometry, &RayTracingDynamicVertexBuffer = RayTracingDynamicVertexBuffer](FRHICommandList& RHICmdList) mutable
			{
				if (RayTracingUpdateQueue != nullptr)
				{
					RayTracingUpdateQueue->Remove(RayTracingGeometryPtr);
				}
				if (RayTracingDynamicVertexBuffer.NumBytes > 0)
				{
					RayTracingDynamicVertexBuffer.Release();
				}
			});
	}
#endif // RHI_RAYTRACING
}

void FSkeletalMeshObjectGPUSkin::InitMorphResources(bool bInUsePerBoneMotionBlur, const TArray<float>& MorphTargetWeights)
{
	if( bMorphResourcesInitialized )
	{
		// release first if already initialized
		ReleaseMorphResources();
	}

	for( int32 LODIndex=0;LODIndex < LODs.Num();LODIndex++ )
	{
		FSkeletalMeshObjectLOD& SkelLOD = LODs[LODIndex];

		// Check the LOD render data for verts, if it's been stripped we don't create morph buffers
		const int32 LodIndexInMesh = SkelLOD.LODIndex;
		const FSkeletalMeshLODRenderData& RenderData = SkelLOD.SkelMeshRenderData->LODRenderData[LodIndexInMesh];

		if(RenderData.GetNumVertices() > 0)
		{
			// init any morph vertex buffers for each LOD
			const FSkelMeshObjectLODInfo& MeshLODInfo = LODInfo[LODIndex];
			SkelLOD.InitMorphResources(MeshLODInfo, bInUsePerBoneMotionBlur, FeatureLevel);
		}
	}
	bMorphResourcesInitialized = true;
}

void FSkeletalMeshObjectGPUSkin::ReleaseMorphResources()
{
	for( int32 LODIndex=0;LODIndex < LODs.Num();LODIndex++ )
	{
		FSkeletalMeshObjectLOD& SkelLOD = LODs[LODIndex];

		// release morph vertex buffers and factories if they were created
		SkelLOD.ReleaseMorphResources();
	}

	bMorphResourcesInitialized = false;
}

void FSkeletalMeshObjectGPUSkin::Update(
	int32 LODIndex,
	USkinnedMeshComponent* InMeshComponent,
	const FMorphTargetWeightMap& InActiveMorphTargets,
	const TArray<float>& InMorphTargetWeights,
	EPreviousBoneTransformUpdateMode PreviousBoneTransformUpdateMode,
	const FExternalMorphWeightData& InExternalMorphWeightData)
{
	LLM_SCOPE(ELLMTag::SkeletalMesh);

	// make sure morph data has been initialized for each LOD
	if(InMeshComponent && !bMorphResourcesInitialized && (!InActiveMorphTargets.IsEmpty() || !InExternalMorphWeightData.MorphSets.IsEmpty()))
	{
		// initialized on-the-fly in order to avoid creating extra vertex streams for each skel mesh instance
		InitMorphResources(InMeshComponent->bPerBoneMotionBlur, InMorphTargetWeights);		
	}

	// create the new dynamic data for use by the rendering thread
	// this data is only deleted when another update is sent
	FDynamicSkelMeshObjectDataGPUSkin* NewDynamicData = FDynamicSkelMeshObjectDataGPUSkin::AllocDynamicSkelMeshObjectDataGPUSkin();		
	NewDynamicData->InitDynamicSkelMeshObjectDataGPUSkin(
		InMeshComponent,
		SkeletalMeshRenderData,
		this,
		LODIndex,
		InActiveMorphTargets,
		InMorphTargetWeights,
		PreviousBoneTransformUpdateMode,
		InExternalMorphWeightData);

	// The goal is to mirror the exact component state to the render state, including the frame number the component was
	// updated (as opposed to the current frame number), unless there's no associated component.
	uint64 FrameNumberToPrepare = InMeshComponent ? InMeshComponent->GetCurrentBoneTransformFrame() : GFrameCounter;
	uint32 RevisionNumber = 0;
	uint32 PreviousRevisionNumber = 0;

	FGPUSkinCache* GPUSkinCache = nullptr;
	FSceneInterface* Scene = nullptr;
	if (InMeshComponent && InMeshComponent->GetScene())
	{
		// We allow caching of per-frame, per-scene data
		Scene = InMeshComponent->GetScene();
		GPUSkinCache = Scene->GetGPUSkinCache();
		RevisionNumber = InMeshComponent->GetBoneTransformRevisionNumber();
		PreviousRevisionNumber = InMeshComponent->GetPreviousBoneTransformRevisionNumber();
	}

	// queue a call to update this data
	FSkeletalMeshObjectGPUSkin* MeshObject = this;
	bool bRecreating = InMeshComponent ? InMeshComponent->IsRenderStateRecreating() : false;
	ENQUEUE_RENDER_COMMAND(SkelMeshObjectUpdateDataCommand)(UE::RenderCommandPipe::SkeletalMesh,
		[MeshObject, FrameNumberToPrepare, RevisionNumber, PreviousRevisionNumber, NewDynamicData, GPUSkinCache, Scene, bRecreating](FRHICommandList& RHICmdList)
		{
			FScopeCycleCounter Context(MeshObject->GetStatId());
			MeshObject->UpdateDynamicData_RenderThread(GPUSkinCache, RHICmdList, NewDynamicData, Scene, FrameNumberToPrepare, RevisionNumber, PreviousRevisionNumber, bRecreating);
		}
	);
}

void FSkeletalMeshObjectGPUSkin::UpdateSkinWeightBuffer(USkinnedMeshComponent* InMeshComponent)
{
	for (int32 LODIndex = 0; LODIndex < LODs.Num(); LODIndex++)
	{
		FSkeletalMeshObjectLOD& SkelLOD = LODs[LODIndex];

		// Skip LODs that have their render data stripped
		if (InMeshComponent && SkelLOD.SkelMeshRenderData->LODRenderData[LODIndex].GetNumVertices() > 0)
		{
			FSkelMeshComponentLODInfo* CompLODInfo = nullptr;
			if (InMeshComponent->LODInfo.IsValidIndex(LODIndex))
			{
				CompLODInfo = &InMeshComponent->LODInfo[LODIndex];
			}

			SkelLOD.UpdateSkinWeights(CompLODInfo);

			if (InMeshComponent && InMeshComponent->SceneProxy)
			{
				FGPUSkinCacheEntry* SkinCacheEntryToUpdate = SkinCacheEntry;
				if (SkinCacheEntryToUpdate)
				{
					ENQUEUE_RENDER_COMMAND(UpdateSkinCacheSkinWeightBuffer)(UE::RenderCommandPipe::SkeletalMesh,
						[SkinCacheEntryToUpdate](FRHICommandList& RHICmdList)
					{
						FGPUSkinCache::UpdateSkinWeightBuffer(SkinCacheEntryToUpdate);
					});
				}

				if (SkinCacheEntryForRayTracing)
				{
					ENQUEUE_RENDER_COMMAND(UpdateSkinCacheSkinWeightBuffer)(UE::RenderCommandPipe::SkeletalMesh,
						[SkinCacheEntryForRayTracing = SkinCacheEntryForRayTracing](FRHICommandList& RHICmdList)
					{
						FGPUSkinCache::UpdateSkinWeightBuffer(SkinCacheEntryForRayTracing);
					});
				}
			}
		}
	}

}

static TAutoConsoleVariable<int32> CVarDeferSkeletalDynamicDataUpdateUntilGDME(
	TEXT("r.DeferSkeletalDynamicDataUpdateUntilGDME"),
	0,
	TEXT("If > 0, then do skeletal mesh dynamic data updates will be deferred until GDME. Experimental option."));

FORCEINLINE bool IsDeferredSkeletalDynamicDataUpdateEnabled()
{
	return CVarDeferSkeletalDynamicDataUpdateUntilGDME.GetValueOnRenderThread() > 0 && !IsParallelGatherDynamicMeshElementsEnabled();
}

void FSkeletalMeshObjectGPUSkin::UpdateDynamicData_RenderThread(FGPUSkinCache* GPUSkinCache, FRHICommandList& RHICmdList, FDynamicSkelMeshObjectDataGPUSkin* InDynamicData, FSceneInterface* Scene, uint64 FrameNumberToPrepare, uint32 RevisionNumber, uint32 PreviousRevisionNumber, bool bRecreating)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GPUSkin::UpdateDynamicData_RT);

	SCOPE_CYCLE_COUNTER(STAT_GPUSkinUpdateRTTime);
	check(InDynamicData != nullptr);
	CA_ASSUME(InDynamicData != nullptr);
	bool bMorphNeedsUpdate=false;
	// figure out if the morphing vertex buffer needs to be updated. compare old vs new active morphs
	bMorphNeedsUpdate = 
		bAlwaysUpdateMorphVertexBuffer ||
		(bMorphNeedsUpdateDeferred && bNeedsUpdateDeferred) || // the need for an update sticks
		InDynamicData->ExternalMorphWeightData.HasActiveMorphs() ||
		(DynamicData ? (DynamicData->LODIndex != InDynamicData->LODIndex ||
		!DynamicData->ActiveMorphTargetsEqual(InDynamicData->ActiveMorphTargets, InDynamicData->MorphTargetWeights))
		: true);

#if RHI_RAYTRACING
	if (bMorphNeedsUpdate && Scene)
	{
		Scene->InvalidatePathTracedOutput();
	}
#endif

	if (DynamicData)
	{
		FDynamicSkelMeshObjectDataGPUSkin::FreeDynamicSkelMeshObjectDataGPUSkin(DynamicData);
	}
	// update with new data
	DynamicData = InDynamicData;
	LastBoneTransformRevisionNumber = RevisionNumber;

	if (IsDeferredSkeletalDynamicDataUpdateEnabled() && !DynamicData->bForceUpdateDynamicDataImmediately)
	{
		bMorphNeedsUpdateDeferred = bMorphNeedsUpdate;
		bNeedsUpdateDeferred = true;
	}
	else
	{
		ProcessUpdatedDynamicData(EGPUSkinCacheEntryMode::Raster, GPUSkinCache, RHICmdList, FrameNumberToPrepare, RevisionNumber, PreviousRevisionNumber, bMorphNeedsUpdate, DynamicData->LODIndex, bRecreating);

	#if RHI_RAYTRACING
		if (ShouldUseSeparateSkinCacheEntryForRayTracing() && FGPUSkinCache::IsGPUSkinCacheRayTracingSupported() && GPUSkinCache && SkeletalMeshRenderData->bSupportRayTracing)
		{
			// Morph delta is updated in raster pass above, no need to update again for ray tracing
			ProcessUpdatedDynamicData(EGPUSkinCacheEntryMode::RayTracing, GPUSkinCache, RHICmdList, FrameNumberToPrepare, RevisionNumber, PreviousRevisionNumber, /*bMorphNeedsUpdate=*/false, DynamicData->RayTracingLODIndex, bRecreating);
		}
		else
		{
			// Immediately release any stale entry if we decide to share with the raster path
			FGPUSkinCache::Release(SkinCacheEntryForRayTracing);
		}
	#endif
	}

#if RHI_RAYTRACING
	if (FGPUSkinCache::IsGPUSkinCacheRayTracingSupported() && SkeletalMeshRenderData->bSupportRayTracing && !GetSkinCacheEntryForRayTracing() && !DynamicData->bHasMeshDeformer)
	{
		// When SkinCacheEntry is gone, clear geometry
		RayTracingGeometry.ReleaseRHI();
		RayTracingGeometry.SetInitializer(FRayTracingGeometryInitializer{});
	}
#endif
}

void FSkeletalMeshObjectGPUSkin::PreGDMECallback(FRHICommandList& RHICmdList, FGPUSkinCache* GPUSkinCache, uint32 FrameNumber)
{
	if (bNeedsUpdateDeferred)
	{
		ProcessUpdatedDynamicData(EGPUSkinCacheEntryMode::Raster, GPUSkinCache, RHICmdList, FrameNumber, LastBoneTransformRevisionNumber, 0, bMorphNeedsUpdateDeferred, DynamicData->LODIndex, false);
	}
}

void FSkeletalMeshObjectGPUSkin::ProcessUpdatedDynamicData(EGPUSkinCacheEntryMode Mode, FGPUSkinCache* GPUSkinCache, FRHICommandList& RHICmdList, uint32 FrameNumberToPrepare, uint32 RevisionNumber, uint32 PreviousRevisionNumber, bool bMorphNeedsUpdate, int32 LODIndex, bool bRecreating)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FSkeletalMeshObjectGPUSkin_ProcessUpdatedDynamicData);
	bNeedsUpdateDeferred = false;
	bMorphNeedsUpdateDeferred = false;

	FSkeletalMeshObjectLOD& LOD = LODs[LODIndex];
	const FSkeletalMeshLODRenderData& LODData = SkeletalMeshRenderData->LODRenderData[LODIndex];
	const TArray<FSkelMeshRenderSection>& Sections = GetRenderSections(LODIndex);

	// Only consider morphs with active curves and data to deform.
	const bool bMorph = 
		bAlwaysUpdateMorphVertexBuffer || 
		(DynamicData->NumWeightedActiveMorphTargets > 0 && LODData.GetNumVertices() > 0) || 
		(DynamicData->ExternalMorphWeightData.HasActiveMorphs() && LODData.GetNumVertices() > 0);

	bMorphNeedsUpdate |= (GForceUpdateMorphTargets != 0);
	bMorphNeedsUpdate |= bAlwaysUpdateMorphVertexBuffer;
	if (bMorph && bMorphNeedsUpdate)
	{
		// Morph vertex buffer needs updating so advance the revision number
		LOD.MorphVertexBufferPool->SetCurrentRevisionNumber(RevisionNumber);
	}

	FMorphVertexBuffer& MorphVertexBuffer = LOD.MorphVertexBufferPool->GetMorphVertexBufferForWriting();
	// if morph buffer hasn't been updated, force update it
	bMorphNeedsUpdate = MorphVertexBuffer.bHasBeenUpdated ? bMorphNeedsUpdate : true;

	// use correct vertex factories based on alternate weights usage
	FVertexFactoryData& VertexFactoryData = LOD.GPUSkinVertexFactories;

	bool bDataPresent = false;

	bool bGPUSkinCacheEnabled = GPUSkinCache && GEnableGPUSkinCache && (DynamicData->bIsSkinCacheAllowed || Mode == EGPUSkinCacheEntryMode::RayTracing) && !DynamicData->bHasMeshDeformer;

	// Immediately release any stale entry if we've recently switched to a LOD level that disallows skin cache
	// This saves memory and avoids confusing ShouldUseSeparateSkinCacheEntryForRayTracing() which checks SkinCacheEntry == nullptr
	if (!bGPUSkinCacheEnabled)
	{
#if RHI_RAYTRACING
		if (Mode == EGPUSkinCacheEntryMode::Raster)
#endif
		{
			if (SkinCacheEntry)
			{
				FGPUSkinCache::Release(SkinCacheEntry);
			}
		}
#if RHI_RAYTRACING
		else
		{
			check(Mode == EGPUSkinCacheEntryMode::RayTracing);
			if (SkinCacheEntryForRayTracing)
			{
				FGPUSkinCache::Release(SkinCacheEntryForRayTracing);
			}
		}
#endif
	}

	// We need to clear the external morph buffers when the weights are zero.
	const bool bExternalMorphsNeedClear = !DynamicData->ExternalMorphWeightData.MorphSets.IsEmpty() && !DynamicData->ExternalMorphWeightData.HasActiveMorphs();

	if ((MorphVertexBuffer.bNeedsInitialClear && !(bMorph && bMorphNeedsUpdate)) || bExternalMorphsNeedClear)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_FSkeletalMeshObjectGPUSkin_ProcessUpdatedDynamicData_ClearMorphBuffer);
		if (MorphVertexBuffer.GetUAV())
		{
			RHICmdList.Transition(FRHITransitionInfo(MorphVertexBuffer.GetUAV(), ERHIAccess::Unknown, ERHIAccess::UAVCompute));
			RHICmdList.ClearUAVUint(MorphVertexBuffer.GetUAV(), FUintVector4(0, 0, 0, 0));
			RHICmdList.Transition(FRHITransitionInfo(MorphVertexBuffer.GetUAV(), ERHIAccess::UAVCompute, ERHIAccess::SRVMask));
		}
	}
	MorphVertexBuffer.bNeedsInitialClear = false;

	if(bMorph) 
	{
		bDataPresent = true;
		checkSlow((VertexFactoryData.MorphVertexFactories.Num() == Sections.Num()));
		
		// only update if the morph data changed and there are weighted morph targets
		if(bMorphNeedsUpdate)
		{
			UpdateMorphVertexBuffer(RHICmdList, Mode, LOD, LODData, bGPUSkinCacheEnabled, MorphVertexBuffer);
			LOD.MorphVertexBufferPool->SetUpdatedFrameNumber(FrameNumberToPrepare);
		}
	}
	else
	{
//		checkSlow(VertexFactoryData.MorphVertexFactories.Num() == 0);
		bDataPresent = VertexFactoryData.VertexFactories.Num() > 0;
	}

	const EShaderPlatform ShaderPlatform = GetFeatureLevelShaderPlatform(FeatureLevel);
	const bool bIsMobile = IsMobilePlatform(ShaderPlatform);
	const bool bClothEnabled = FGPUBaseSkinAPEXClothVertexFactory::IsClothEnabled(ShaderPlatform);
	if (bDataPresent)
	{
		bool bSkinCacheResult = true;
		for (int32 SectionIdx = 0; SectionIdx < Sections.Num(); SectionIdx++)
		{
			const FSkelMeshRenderSection& Section = Sections[SectionIdx];
			const FClothSimulData* const SimData = DynamicData->ClothingSimData.Find(Section.CorrespondClothAssetIndex);
			const bool bClothFactory = bClothEnabled && SimData && Section.HasClothingData();

			FGPUBaseSkinVertexFactory* VertexFactory;
			{
				if(bClothFactory)
				{
					VertexFactory = VertexFactoryData.ClothVertexFactories[SectionIdx]->GetVertexFactory();
				}
				else
				{
					if (bAlwaysUpdateMorphVertexBuffer || 
						(DynamicData->NumWeightedActiveMorphTargets > 0 && DynamicData->SectionIdsUseByActiveMorphTargets.Contains(SectionIdx)) || 
						!DynamicData->ExternalMorphWeightData.MorphSets.IsEmpty())
					{
						VertexFactory = VertexFactoryData.MorphVertexFactories[SectionIdx].Get();
					}
					else
					{
						VertexFactory = VertexFactoryData.VertexFactories[SectionIdx].Get();
					}
				}
			}

			FGPUBaseSkinVertexFactory::FShaderDataType& ShaderData = VertexFactory->GetShaderData();

			bool bUseSkinCache = bGPUSkinCacheEnabled;
			if (bUseSkinCache)
			{
				if (Section.MaxBoneInfluences == 0)
				{
					//INC_DWORD_STAT(STAT_GPUSkinCache_SkippedForZeroInfluences);
					bUseSkinCache = false;
				}
			}

		#if RHI_RAYTRACING
			bool bShouldUseSeparateMatricesForRayTracing = Mode == EGPUSkinCacheEntryMode::RayTracing && DynamicData->RayTracingLODIndex != DynamicData->LODIndex;
		#else
			bool bShouldUseSeparateMatricesForRayTracing = false;
		#endif

			const FName OwnerName = GetAssetPathName(LODIndex);

			// If we have previous reference to local, we also update to previous frame.  We technically are storing the previous data to "current" first,
			// then the second call to UpdateBoneData immediately below swaps that to become "previous".
			if (DynamicData->PreviousReferenceToLocal.Num() > 0)
			{
				TArray<FMatrix44f>& PreviousReferenceToLocalMatrices = bShouldUseSeparateMatricesForRayTracing ? DynamicData->PreviousReferenceToLocalForRayTracing : DynamicData->PreviousReferenceToLocal;
				ShaderData.UpdateBoneData(RHICmdList, PreviousReferenceToLocalMatrices, Section.BoneMap, PreviousRevisionNumber, FeatureLevel, bUseSkinCache, DynamicData->bForceUpdateDynamicDataImmediately, OwnerName);
			}

			// Create a uniform buffer from the bone transforms.
			TArray<FMatrix44f>& ReferenceToLocalMatrices = bShouldUseSeparateMatricesForRayTracing ? DynamicData->ReferenceToLocalForRayTracing : DynamicData->ReferenceToLocal;
			ShaderData.UpdateBoneData(RHICmdList, ReferenceToLocalMatrices, Section.BoneMap, RevisionNumber, FeatureLevel, bUseSkinCache, DynamicData->bForceUpdateDynamicDataImmediately, OwnerName);
			ShaderData.UpdatedFrameNumber = FrameNumberToPrepare;

			// Update uniform buffer for APEX cloth simulation mesh positions and normals
			if( bClothFactory )
			{
				FGPUBaseSkinAPEXClothVertexFactory::ClothShaderType& ClothShaderData = VertexFactoryData.ClothVertexFactories[SectionIdx]->GetClothShaderData();
				ClothShaderData.ClothBlendWeight = DynamicData->ClothBlendWeight;
				ClothShaderData.WorldScale = (FVector3f)WorldScale;

				ClothShaderData.UpdateClothSimulData(RHICmdList, SimData->Positions, SimData->Normals, RevisionNumber, FeatureLevel, DynamicData->bForceUpdateDynamicDataImmediately, OwnerName);
				// Transform from cloth space to local space. Cloth space is relative to cloth root bone, local space is component space.
				ClothShaderData.GetClothToLocalForWriting() = FMatrix44f(SimData->ComponentRelativeTransform.ToMatrixWithScale());
			}

			// Try to use the GPU skinning cache if possible
			if (bUseSkinCache)
			{
				FMatrix44f ClothToLocal = bClothFactory ? VertexFactoryData.ClothVertexFactories[SectionIdx]->GetClothShaderData().GetClothToLocalForWriting() : FMatrix44f::Identity;

				// ProcessEntry returns false if not enough memory is left in skin cache to allocate for the mesh, if that happens don't try to process subsequent sections because they will also fail.
				if (bSkinCacheResult)
				{
					bSkinCacheResult = GPUSkinCache->ProcessEntry(
						Mode,
						RHICmdList,
						VertexFactory,
						VertexFactoryData.PassthroughVertexFactories[SectionIdx].Get(),
						Section,
						this,
						bMorph ? &MorphVertexBuffer : 0,
						bClothFactory ? &LODData.ClothVertexBuffer : 0,
						bClothFactory ? DynamicData->ClothingSimData.Find(Section.CorrespondClothAssetIndex) : 0,
						ClothToLocal,
						DynamicData->ClothBlendWeight,
						(FVector3f)WorldScale,
						RevisionNumber,
						SectionIdx,
						LODIndex,
						bRecreating,
						Mode == EGPUSkinCacheEntryMode::RayTracing ? SkinCacheEntryForRayTracing : SkinCacheEntry
						);
				}
			}

			// Mobile doesn't support motion blur so no need to double buffer cloth data.
			// Skin cache doesn't need double buffering, if failed to enter skin cache then the fall back GPU skinned VF needs double buffering.
			if (bClothFactory && !bIsMobile && !SkinCacheEntry && Mode == EGPUSkinCacheEntryMode::Raster)
			{
				FGPUBaseSkinAPEXClothVertexFactory::ClothShaderType& ClothShaderData = VertexFactoryData.ClothVertexFactories[SectionIdx]->GetClothShaderData();
				ClothShaderData.EnableDoubleBuffer();
			}
		}
	}

	// Mobile doesn't support motion blur so no need to double buffer morph deltas
	if (bMorph && !bIsMobile && !SkinCacheEntry && Mode == EGPUSkinCacheEntryMode::Raster && !LOD.MorphVertexBufferPool->IsDoubleBuffered())
	{
		// Going through GPU skinned vertex factory, turn on double buffering for motion blur
		LOD.MorphVertexBufferPool->EnableDoubleBuffer(RHICmdList);
	}
}

#if RHI_RAYTRACING

void FSkeletalMeshObjectGPUSkin::UpdateRayTracingGeometry(FRHICommandListBase& RHICmdList, FSkeletalMeshLODRenderData& LODModel, uint32 LODIndex, TArray<FBufferRHIRef>& VertexBuffers)
{
	if (IsRayTracingEnabled() && bSupportRayTracing)
	{
		const bool bAnySegmentUsesWorldPositionOffset = DynamicData != nullptr ? DynamicData->bAnySegmentUsesWorldPositionOffset : false;

		bool bRequireRecreatingRayTracingGeometry = LODIndex != RayTracingGeometry.LODIndex
			|| bHiddenMaterialVisibilityDirtyForRayTracing
			|| RayTracingGeometry.Initializer.Segments.Num() == 0;
		if (!bRequireRecreatingRayTracingGeometry)
		{
			for (FRayTracingGeometrySegment& Segment : RayTracingGeometry.Initializer.Segments)
			{
				if (Segment.VertexBuffer == nullptr)
				{
					bRequireRecreatingRayTracingGeometry = true;
					break;
				}
			}
		}
		bHiddenMaterialVisibilityDirtyForRayTracing = false;

		if (bRequireRecreatingRayTracingGeometry)
		{
			uint32 MemoryEstimation = 0;

			FBufferRHIRef IndexBufferRHI = LODModel.MultiSizeIndexContainer.GetIndexBuffer()->IndexBufferRHI;
			MemoryEstimation += IndexBufferRHI->GetSize();
			uint32 VertexBufferStride = LODModel.StaticVertexBuffers.PositionVertexBuffer.GetStride();
			MemoryEstimation += LODModel.StaticVertexBuffers.PositionVertexBuffer.VertexBufferRHI->GetSize();

			//#dxr_todo: do we need support for separate sections in FRayTracingGeometryData?
			uint32 TotalNumTriangles = 0;
			uint32 TotalNumVertices = 0;
			for (int32 SectionIndex = 0; SectionIndex < LODModel.RenderSections.Num(); SectionIndex++)
			{
				const FSkelMeshRenderSection& Section = LODModel.RenderSections[SectionIndex];
				TotalNumTriangles += Section.NumTriangles;
				TotalNumVertices += Section.GetNumVertices();
			}

			FRayTracingGeometryInitializer Initializer;

#if !UE_BUILD_SHIPPING
			if (DebugName.IsValid())
			{
				Initializer.DebugName = DebugName;
			}
			else
#endif
			{
				static const FName DefaultDebugName("FSkeletalMeshObjectGPUSkin");
				static int32 DebugNumber = 0;
				Initializer.DebugName = FName(DefaultDebugName, DebugNumber++);
			}

			Initializer.OwnerName = GetAssetPathName(LODIndex);
			Initializer.IndexBuffer = IndexBufferRHI;
			Initializer.TotalPrimitiveCount = TotalNumTriangles;
			Initializer.GeometryType = RTGT_Triangles;
			Initializer.bFastBuild = true;
			Initializer.bAllowUpdate = true;

			Initializer.Segments.Reserve(LODModel.RenderSections.Num());

			for (int32 SectionIndex = 0; SectionIndex < LODModel.RenderSections.Num(); ++SectionIndex)
			{
				const FSkelMeshRenderSection& Section = LODModel.RenderSections[SectionIndex];

				FRayTracingGeometrySegment Segment;
				Segment.VertexBuffer = VertexBuffers[SectionIndex];
				Segment.VertexBufferElementType = VET_Float3;
				Segment.VertexBufferStride = VertexBufferStride;
				Segment.VertexBufferOffset = 0;
				Segment.MaxVertices = TotalNumVertices;
				Segment.FirstPrimitive = Section.BaseIndex / 3;
				Segment.NumPrimitives = Section.NumTriangles;

				// TODO: If we are at a dropped LOD, route material index through the LODMaterialMap in the LODInfo struct.
				Segment.bEnabled = !IsMaterialHidden(LODIndex, Section.MaterialIndex) && !Section.bDisabled && Section.bVisibleInRayTracing;
				Initializer.Segments.Add(Segment);
			}

			if (RayTracingGeometry.RayTracingGeometryRHI.IsValid())
			{
				// RayTracingGeometry.ReleaseRHI() releases the old RT geometry, however due to the deferred deletion nature of RHI resources
				// they will not be released until the end of the frame. We may get OOM in the middle of batched updates if not flushing.
				// We pass MemoryEstimation, based on vertex & index buffer size, to the update queue so that it can schedule flushes if necessary.

				// Release the old data (make sure it's not pending build anymore either)
				RayTracingUpdateQueue->Remove(&RayTracingGeometry, MemoryEstimation);
				RayTracingGeometry.RayTracingGeometryRHI->DisableLifetimeExtension();
				RayTracingGeometry.ReleaseRHI();
			}

			Initializer.SourceGeometry = LODModel.SourceRayTracingGeometry.RayTracingGeometryRHI;

			// Get the scratch sizes used for build & update
			RayTracingGeometryStructureSize = RHICmdList.CalcRayTracingGeometrySize(Initializer);

			RayTracingGeometry.LODIndex = LODIndex;

			// Update the new init data
			RayTracingGeometry.SetInitializer(MoveTemp(Initializer));

			// Only create RHI object but enqueue actual BLAS creation so they can be accumulated
			RayTracingGeometry.CreateRayTracingGeometry(RHICmdList, ERTAccelerationStructureBuildPriority::Skip);
		}
		else if (!bAnySegmentUsesWorldPositionOffset)
		{
			check(LODModel.RenderSections.Num() == RayTracingGeometry.Initializer.Segments.Num());

			// Refit BLAS with new vertex buffer data
			for (int32 SectionIndex = 0; SectionIndex < LODModel.RenderSections.Num(); ++SectionIndex)
			{
				FRayTracingGeometrySegment& Segment = RayTracingGeometry.Initializer.Segments[SectionIndex];
				Segment.VertexBuffer = VertexBuffers[SectionIndex];
				Segment.VertexBufferOffset = 0;
			}
		}

		// If we are not using world position offset in material, handle BLAS build/refit here
		if (!bAnySegmentUsesWorldPositionOffset)
		{
			check(bRequireRecreatingRayTracingGeometry == RayTracingGeometry.GetRequiresBuild());
			RayTracingUpdateQueue->Add(&RayTracingGeometry, RayTracingGeometryStructureSize);
		}
		else
		{
			// Otherwise, we will run the dynamic ray tracing geometry path, i.e. running VSinCS and build/refit geometry there, so do nothing here
		}
	}
}

#endif // RHI_RAYTRACING


int32 FSkeletalMeshObjectGPUSkin::CalcNumActiveGPUMorphSets(FMorphVertexBuffer& MorphVertexBuffer, const FExternalMorphSets& ExternalMorphSets) const
{
	if (!UseGPUMorphTargets(FeatureLevel) || !IsValidRef(MorphVertexBuffer.VertexBufferRHI))
	{
		return 0;
	}

	// Count all active external morph sets.
	int32 NumMorphSets = 1; // Start at one, as we have our standard morph targets as well.
	for (const auto& MorphSet : ExternalMorphSets)
	{
		if (IsExternalMorphSetActive(MorphSet.Key, *MorphSet.Value))
		{
			NumMorphSets++;
		}
	}
	return NumMorphSets;
}

bool FSkeletalMeshObjectGPUSkin::IsExternalMorphSetActive(int32 MorphSetID, const FExternalMorphSet& MorphSet) const
{
	const FMorphTargetVertexInfoBuffers& CompressedBuffers = MorphSet.MorphBuffers;
	FExternalMorphSetWeights* WeightData = DynamicData->ExternalMorphWeightData.MorphSets.Find(MorphSetID);
	return (WeightData &&
			WeightData->Weights.Num() == CompressedBuffers.GetNumMorphs() &&
			WeightData->NumActiveMorphTargets > 0);
}

static void CalculateMorphDeltaBoundsAccum(
	const TArray<float>& MorphTargetWeights,
	const FMorphTargetVertexInfoBuffers& MorphTargetVertexInfoBuffers,
	FVector4& MinAccumScale,
	FVector4& MaxAccumScale,
	FVector4& MaxScale)
{
	for (uint32 i = 0; i < MorphTargetVertexInfoBuffers.GetNumMorphs(); i++)
	{
		FVector4f MinMorphScale = MorphTargetVertexInfoBuffers.GetMinimumMorphScale(i);
		FVector4f MaxMorphScale = MorphTargetVertexInfoBuffers.GetMaximumMorphScale(i);

		for (uint32 j = 0; j < 4; j++)
		{
			if (MorphTargetWeights.IsValidIndex(i))
			{
				MinAccumScale[j] += MorphTargetWeights[i] * MinMorphScale[j];
				MaxAccumScale[j] += MorphTargetWeights[i] * MaxMorphScale[j];
			}

			double AbsMorphScale = FMath::Max<double>(FMath::Abs(MinMorphScale[j]), FMath::Abs(MaxMorphScale[j]));
			double AbsAccumScale = FMath::Max<double>(FMath::Abs(MinAccumScale[j]), FMath::Abs(MaxAccumScale[j]));

			// The maximum accumulated and the maximum local value have to fit into out int24.
			MaxScale[j] = FMath::Max(MaxScale[j], FMath::Max(AbsMorphScale, AbsAccumScale));
		}
	}
}

static void CalculateMorphDeltaBoundsIncludingExternalMorphs(
	const TArray<float>& MorphTargetWeights,
	const FMorphTargetVertexInfoBuffers& MorphTargetVertexInfoBuffers,
	const FExternalMorphSets& ExternalMorphSets,
	const TMap<int32, FExternalMorphSetWeights>& ExternalWeights,
	FVector4& MorphScale,
	FVector4& InvMorphScale)
{
	FVector4 MinAccumScale(0, 0, 0, 0);
	FVector4 MaxAccumScale(0, 0, 0, 0);
	FVector4 MaxScale(0, 0, 0, 0);

	// Include the standard morph targets.
	CalculateMorphDeltaBoundsAccum(MorphTargetWeights, MorphTargetVertexInfoBuffers, MinAccumScale, MaxAccumScale, MaxScale);

	// Include all external morph targets.
	for (const auto& MorphSet : ExternalMorphSets)
	{
		const int32 MorphSetID = MorphSet.Key;
		const FMorphTargetVertexInfoBuffers& CompressedBuffers = MorphSet.Value->MorphBuffers;
		const FExternalMorphSetWeights* WeightData = ExternalWeights.Find(MorphSetID);
		check(WeightData);
		CalculateMorphDeltaBoundsAccum(WeightData->Weights, CompressedBuffers, MinAccumScale, MaxAccumScale, MaxScale);
	}

	MaxScale[0] = FMath::Max<double>(MaxScale[0], 1.0);
	MaxScale[1] = FMath::Max<double>(MaxScale[1], 1.0);
	MaxScale[2] = FMath::Max<double>(MaxScale[2], 1.0);
	MaxScale[3] = FMath::Max<double>(MaxScale[3], 1.0);

	const double ScaleToInt24 = 16777216.0;

	MorphScale = FVector4
	(
		ScaleToInt24 / (MaxScale[0]),
		ScaleToInt24 / (MaxScale[1]),
		ScaleToInt24 / (MaxScale[2]),
		ScaleToInt24 / (MaxScale[3])
	);

	InvMorphScale = FVector4
	(
		MaxScale[0] / ScaleToInt24,
		MaxScale[1] / ScaleToInt24,
		MaxScale[2] / ScaleToInt24,
		MaxScale[3] / ScaleToInt24
	);
}

void FSkeletalMeshObjectGPUSkin::UpdateMorphVertexBuffer(FRHICommandList& RHICmdList, EGPUSkinCacheEntryMode Mode, FSkeletalMeshObjectLOD& LOD, const FSkeletalMeshLODRenderData& LODData, 
															bool bGPUSkinCacheEnabled, FMorphVertexBuffer& MorphVertexBuffer)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FSkeletalMeshObjectGPUSkin_ProcessUpdatedDynamicData_UpdateMorphBuffer);
	
	if (UseGPUMorphTargets(FeatureLevel) && IsValidRef(MorphVertexBuffer.VertexBufferRHI))
	{
		const int32 NumMorphSets = CalcNumActiveGPUMorphSets(MorphVertexBuffer, DynamicData->ExternalMorphSets);
		int32 MorphSetIndex = 0;

		// Calculate the delta bounds.
		FVector4 MorphScale;
		FVector4 InvMorphScale;
		{
			SCOPE_CYCLE_COUNTER(STAT_MorphVertexBuffer_ApplyDelta);
			CalculateMorphDeltaBoundsIncludingExternalMorphs(
				DynamicData->MorphTargetWeights,
				LODData.MorphTargetVertexInfoBuffers,
				DynamicData->ExternalMorphSets,
				DynamicData->ExternalMorphWeightData.MorphSets,
				MorphScale,
				InvMorphScale);
		}

		// Sometimes this goes out of bound, we'll ensure here.
		ensureAlways(DynamicData->MorphTargetWeights.Num() == LODData.MorphTargetVertexInfoBuffers.GetNumMorphs());
		LOD.UpdateMorphVertexBufferGPU(
			RHICmdList, 
			DynamicData->MorphTargetWeights, 
			LODData.MorphTargetVertexInfoBuffers, 
			DynamicData->SectionIdsUseByActiveMorphTargets,
			GetDebugName(), 
			Mode, 
			MorphVertexBuffer, 
			true,	// Only clear the morph vertex buffer at the first morph set.
			(MorphSetIndex == NumMorphSets-1),
			MorphScale,
			InvMorphScale);	// Normalize only after the last morph set.

		MorphSetIndex++;

		// Process all external morph targets.
		for (const auto& MorphSet : DynamicData->ExternalMorphSets)
		{
			const int32 MorphSetID = MorphSet.Key;
			const FMorphTargetVertexInfoBuffers& CompressedBuffers = MorphSet.Value->MorphBuffers;
			FExternalMorphSetWeights* WeightData = DynamicData->ExternalMorphWeightData.MorphSets.Find(MorphSetID);
			check(WeightData);
			if (IsExternalMorphSetActive(MorphSetID, *MorphSet.Value))
			{
				LOD.UpdateMorphVertexBufferGPU(
					RHICmdList, WeightData->Weights,
					CompressedBuffers,
					DynamicData->SectionIdsUseByActiveMorphTargets,
					GetDebugName(),
					Mode,
					MorphVertexBuffer,
					false,	// Don't clear the vertex buffer as we already did with the standard morph targets above.
					(MorphSetIndex == NumMorphSets - 1),
					MorphScale,
					InvMorphScale);	// Normalize only after the last morph set.

				MorphSetIndex++;
			}
		}

		// If this hits, the CalcNumActiveGPUMorphSets most likely returns the wrong number.
		check(NumMorphSets == MorphSetIndex);
	}
	else
	{
		// update the morph data for the lod (before SkinCache)
		LOD.UpdateMorphVertexBufferCPU(RHICmdList, DynamicData->ActiveMorphTargets, DynamicData->MorphTargetWeights, DynamicData->SectionIdsUseByActiveMorphTargets, bGPUSkinCacheEnabled, MorphVertexBuffer);
	}

	if (LOD.MorphVertexBufferPool->IsDoubleBuffered())
	{
		// Update vertex stream binding when morphed delta data is double buffered
		for (int32 SectionIdx = 0; SectionIdx < LODData.RenderSections.Num(); ++SectionIdx)
		{
			if (FGPUBaseSkinVertexFactory* GPUSkinVertexFactory = LOD.GPUSkinVertexFactories.MorphVertexFactories[SectionIdx].Get())
			{
				GPUSkinVertexFactory->UpdateMorphVertexStream(&MorphVertexBuffer);
			}
		}
	}
}

TArray<float> FSkeletalMeshObjectGPUSkin::FSkeletalMeshObjectLOD::MorphAccumulatedWeightArray;

FGPUMorphUpdateCS::FGPUMorphUpdateCS() = default;

FGPUMorphUpdateCS::FGPUMorphUpdateCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	: FGlobalShader(Initializer)
{
	MorphVertexBufferParameter.Bind(Initializer.ParameterMap, TEXT("MorphVertexBuffer"));

	MorphTargetWeightsParameter.Bind(Initializer.ParameterMap, TEXT("MorphTargetWeights"));
	MorphTargetBatchOffsetsParameter.Bind(Initializer.ParameterMap, TEXT("MorphTargetBatchOffsets"));
	MorphTargetGroupOffsetsParameter.Bind(Initializer.ParameterMap, TEXT("MorphTargetGroupOffsets"));
	PositionScaleParameter.Bind(Initializer.ParameterMap, TEXT("PositionScale"));
	PrecisionParameter.Bind(Initializer.ParameterMap, TEXT("Precision"));
	NumGroupsParameter.Bind(Initializer.ParameterMap, TEXT("NumGroups"));

	MorphDataBufferParameter.Bind(Initializer.ParameterMap, TEXT("MorphDataBuffer"));
}

void FGPUMorphUpdateCS::SetParameters(
	FRHIBatchedShaderParameters& BatchedParameters,
	const FVector4& LocalScale,
	const FMorphTargetVertexInfoBuffers& MorphTargetVertexInfoBuffers,
	FMorphVertexBuffer& MorphVertexBuffer,
	uint32 NumGroups,
	uint32 BatchOffsets[MorphTargetDispatchBatchSize],
	uint32 GroupOffsets[MorphTargetDispatchBatchSize],
	float Weights[MorphTargetDispatchBatchSize])
{
	SetUAVParameter(BatchedParameters, MorphVertexBufferParameter, MorphVertexBuffer.GetUAV());

	SetShaderValue(BatchedParameters, PositionScaleParameter, (FVector4f)LocalScale);
	FVector2f Precision = { MorphTargetVertexInfoBuffers.GetPositionPrecision(), MorphTargetVertexInfoBuffers.GetTangentZPrecision() };
	SetShaderValue(BatchedParameters, PrecisionParameter, Precision);
	SetShaderValue(BatchedParameters, NumGroupsParameter, NumGroups);

	SetSRVParameter(BatchedParameters, MorphDataBufferParameter, MorphTargetVertexInfoBuffers.MorphDataSRV);

	SetShaderValue(BatchedParameters, MorphTargetBatchOffsetsParameter, *(uint32(*)[MorphTargetDispatchBatchSize]) BatchOffsets);
	SetShaderValue(BatchedParameters, MorphTargetGroupOffsetsParameter, *(uint32(*)[MorphTargetDispatchBatchSize]) GroupOffsets);
	SetShaderValue(BatchedParameters, MorphTargetWeightsParameter, *(float(*)[MorphTargetDispatchBatchSize]) Weights);
}

void FGPUMorphUpdateCS::Dispatch(FRHICommandList& RHICmdList, uint32 Size)
{
	const FIntVector DispatchSize = FComputeShaderUtils::GetGroupCountWrapped(Size);
	RHICmdList.DispatchComputeShader(DispatchSize.X, DispatchSize.Y, DispatchSize.Z);
}

void FGPUMorphUpdateCS::UnsetParameters(FRHIBatchedShaderUnbinds& BatchedUnbinds)
{
	UnsetUAVParameter(BatchedUnbinds, MorphVertexBufferParameter);
}

bool FGPUMorphUpdateCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

IMPLEMENT_SHADER_TYPE(, FGPUMorphUpdateCS, TEXT("/Engine/Private/MorphTargets.usf"), TEXT("GPUMorphUpdateCS"), SF_Compute);

FGPUMorphNormalizeCS::FGPUMorphNormalizeCS() = default;

FGPUMorphNormalizeCS::FGPUMorphNormalizeCS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
	: FGlobalShader(Initializer)
{
	MorphVertexBufferParameter.Bind(Initializer.ParameterMap, TEXT("MorphVertexBuffer"));
	PositionScaleParameter.Bind(Initializer.ParameterMap, TEXT("PositionScale"));
	NumVerticesParameter.Bind(Initializer.ParameterMap, TEXT("NumVertices"));
}

bool FGPUMorphNormalizeCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

void FGPUMorphNormalizeCS::SetParameters(FRHIBatchedShaderParameters& BatchedParameters, const FVector4& InvLocalScale, const FMorphTargetVertexInfoBuffers& MorphTargetVertexInfoBuffers, FMorphVertexBuffer& MorphVertexBuffer, uint32 NumVertices)
{
	SetUAVParameter(BatchedParameters, MorphVertexBufferParameter, MorphVertexBuffer.GetUAV());
	SetShaderValue(BatchedParameters, PositionScaleParameter, (FVector4f)InvLocalScale);
	SetShaderValue(BatchedParameters, NumVerticesParameter, NumVertices);
}

void FGPUMorphNormalizeCS::Dispatch(FRHICommandList& RHICmdList, uint32 NumVertices)
{
	FIntVector DispatchSize = FComputeShaderUtils::GetGroupCountWrapped(NumVertices, 64);
	RHICmdList.DispatchComputeShader(DispatchSize.X, DispatchSize.Y, DispatchSize.Z);
}

void FGPUMorphNormalizeCS::UnsetParameters(FRHIBatchedShaderUnbinds& BatchedUnbinds)
{
	UnsetUAVParameter(BatchedUnbinds, MorphVertexBufferParameter);
}

IMPLEMENT_SHADER_TYPE(, FGPUMorphNormalizeCS, TEXT("/Engine/Private/MorphTargets.usf"), TEXT("GPUMorphNormalizeCS"), SF_Compute);

void FSkeletalMeshObjectGPUSkin::FSkeletalMeshObjectLOD::UpdateMorphVertexBufferGPU(
	FRHICommandList& RHICmdList, 
	const TArray<float>& MorphTargetWeights,
	const FMorphTargetVertexInfoBuffers& MorphTargetVertexInfoBuffers,
	const TArray<int32>& SectionIdsUseByActiveMorphTargets,
	const FName& OwnerName,
	EGPUSkinCacheEntryMode Mode,
	FMorphVertexBuffer& MorphVertexBuffer,
	bool bClearMorphVertexBuffer,
	bool bNormalizePass,
	const FVector4& MorphScale,
	const FVector4& InvMorphScale)
{
	if (IsValidRef(MorphVertexBuffer.VertexBufferRHI))
	{
		SCOPE_CYCLE_COUNTER(STAT_MorphVertexBuffer_Update);

		// LOD of the skel mesh is used to find number of vertices in buffer
		FSkeletalMeshLODRenderData& LodData = SkelMeshRenderData->LODRenderData[LODIndex];

		const bool bUseGPUMorphTargets = UseGPUMorphTargets(FeatureLevel);
		MorphVertexBuffer.RecreateResourcesIfRequired(RHICmdList, bUseGPUMorphTargets);

		SCOPED_GPU_STAT(RHICmdList, MorphTargets);

		const FString RayTracingTag = (Mode == EGPUSkinCacheEntryMode::RayTracing ? TEXT("[RT]") : TEXT(""));
		const FString LODName = OwnerName.ToString() + TEXT("_LOD") + FString::FromInt(LODIndex);
		SCOPED_DRAW_EVENTF(RHICmdList, MorphUpdate,
			TEXT("MorphUpdate%s_%s LodVertices=%d Batches=%d"), *RayTracingTag, *LODName,
			LodData.GetNumVertices(),
			MorphTargetVertexInfoBuffers.GetNumBatches());

		RHICmdList.Transition(FRHITransitionInfo(MorphVertexBuffer.GetUAV(), ERHIAccess::Unknown, ERHIAccess::UAVCompute));
		if (bClearMorphVertexBuffer)
		{
			RHICmdList.ClearUAVUint(MorphVertexBuffer.GetUAV(), FUintVector4(0, 0, 0, 0));
		}

		if (MorphTargetVertexInfoBuffers.IsRHIInitialized() && (MorphTargetVertexInfoBuffers.GetNumMorphs() > 0))
		{
			{
				SCOPED_DRAW_EVENTF(RHICmdList, MorphUpdateScatter, TEXT("Scatter"));

				RHICmdList.Transition(FRHITransitionInfo(MorphVertexBuffer.GetUAV(), ERHIAccess::UAVCompute, ERHIAccess::UAVCompute));
				RHICmdList.BeginUAVOverlap(MorphVertexBuffer.GetUAV());

				//the first pass scatters all morph targets into the vertexbuffer using atomics
				//multiple morph targets can be batched by a single shader where the shader will rely on
				//binary search to find the correct target weight within the batch.
				TShaderMapRef<FGPUMorphUpdateCS> GPUMorphUpdateCS(GetGlobalShaderMap(FeatureLevel));

				uint32 InputMorphStartIndex = 0;
				while (InputMorphStartIndex < MorphTargetVertexInfoBuffers.GetNumMorphs())
				{
					uint32 BatchOffsets[FGPUMorphUpdateCS::MorphTargetDispatchBatchSize];
					uint32 GroupOffsets[FGPUMorphUpdateCS::MorphTargetDispatchBatchSize];
					float Weights[FGPUMorphUpdateCS::MorphTargetDispatchBatchSize];

					uint32 NumBatches = 0;
					uint32 NumOutputMorphs = 0;
					while (InputMorphStartIndex < MorphTargetVertexInfoBuffers.GetNumMorphs() && NumOutputMorphs < FGPUMorphUpdateCS::MorphTargetDispatchBatchSize)
					{
						if (MorphTargetWeights.IsValidIndex(InputMorphStartIndex) && MorphTargetWeights[InputMorphStartIndex] != 0.0f) 	// Omit morphs with zero weight
						{
							BatchOffsets[NumOutputMorphs] = MorphTargetVertexInfoBuffers.GetBatchStartOffset(InputMorphStartIndex);
							GroupOffsets[NumOutputMorphs] = NumBatches;
							Weights[NumOutputMorphs] = MorphTargetWeights[InputMorphStartIndex];
							NumOutputMorphs++;

							NumBatches += MorphTargetVertexInfoBuffers.GetNumBatches(InputMorphStartIndex);
						}
						InputMorphStartIndex++;
					}

					for (uint32 i = NumOutputMorphs; i < FGPUMorphUpdateCS::MorphTargetDispatchBatchSize; i++)
					{
						BatchOffsets[i] = 0;
						GroupOffsets[i] = NumBatches;
						Weights[i] = 0.0f;
					}

					SetComputePipelineState(RHICmdList, GPUMorphUpdateCS.GetComputeShader());

					SetShaderParametersLegacyCS(
						RHICmdList,
						GPUMorphUpdateCS,
						MorphScale,
						MorphTargetVertexInfoBuffers,
						MorphVertexBuffer,
						NumBatches,
						BatchOffsets,
						GroupOffsets,
						Weights);

					GPUMorphUpdateCS->Dispatch(RHICmdList, NumBatches);
				}

				UnsetShaderParametersLegacyCS(RHICmdList, GPUMorphUpdateCS);
				
				RHICmdList.EndUAVOverlap(MorphVertexBuffer.GetUAV());
			}

			if (bNormalizePass)
			{
				SCOPED_DRAW_EVENTF(RHICmdList, MorphUpdateNormalize, TEXT("Normalize"));

				RHICmdList.Transition(FRHITransitionInfo(MorphVertexBuffer.GetUAV(), ERHIAccess::UAVCompute, ERHIAccess::UAVCompute));

				//The second pass normalizes the scattered result and converts it back into floats.
				//The dispatches are split by morph permutation (and their accumulated weight) .
				//Every vertex is touched only by a single permutation. 
				//multiple permutations can be batched by a single shader where the shader will rely on
				//binary search to find the correct target weight within the batch.
				TShaderMapRef<FGPUMorphNormalizeCS> GPUMorphNormalizeCS(GetGlobalShaderMap(FeatureLevel));

				SetComputePipelineState(RHICmdList, GPUMorphNormalizeCS.GetComputeShader());
				SetShaderParametersLegacyCS(RHICmdList, GPUMorphNormalizeCS, InvMorphScale, MorphTargetVertexInfoBuffers, MorphVertexBuffer, MorphVertexBuffer.GetNumVerticies());
				GPUMorphNormalizeCS->Dispatch(RHICmdList, MorphVertexBuffer.GetNumVerticies());
				UnsetShaderParametersLegacyCS(RHICmdList, GPUMorphNormalizeCS);

				RHICmdList.Transition(FRHITransitionInfo(MorphVertexBuffer.GetUAV(), ERHIAccess::UAVCompute, ERHIAccess::VertexOrIndexBuffer | ERHIAccess::SRVMask));
			}
		}

		// Copy the section Ids use by all active morph targets
		MorphVertexBuffer.SectionIds = SectionIdsUseByActiveMorphTargets;
		
		// set update flag
		MorphVertexBuffer.bHasBeenUpdated = true;
	}
}

void FSkeletalMeshObjectGPUSkin::FSkeletalMeshObjectLOD::UpdateSkinWeights(FSkelMeshComponentLODInfo* CompLODInfo)
{	
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FSkeletalMeshObjectLOD_UpdateSkinWeights);

	check(SkelMeshRenderData);
	check(SkelMeshRenderData->LODRenderData.IsValidIndex(LODIndex));

	// If we have a skin weight override buffer (and it's the right size) use it
	FSkeletalMeshLODRenderData& LODData = SkelMeshRenderData->LODRenderData[LODIndex];	
	if (CompLODInfo)
	{		
		FSkinWeightVertexBuffer* NewMeshObjectWeightBuffer = FSkeletalMeshObject::GetSkinWeightVertexBuffer(LODData, CompLODInfo);
		if (MeshObjectWeightBuffer != NewMeshObjectWeightBuffer)
		{
			MeshObjectWeightBuffer = NewMeshObjectWeightBuffer;

			FVertexFactoryBuffers VertexBuffers;
			GetVertexBuffers(VertexBuffers, LODData);

			FSkeletalMeshObjectLOD* Self = this;
			ENQUEUE_RENDER_COMMAND(UpdateSkinWeightsGPUSkin)(UE::RenderCommandPipe::SkeletalMesh,
				[NewMeshObjectWeightBuffer, VertexBuffers, Self](FRHICommandList& RHICmdList)
			{
				Self->GPUSkinVertexFactories.UpdateVertexFactoryData(VertexBuffers);
			});
		}
	}
	
}

void FSkeletalMeshObjectGPUSkin::FSkeletalMeshObjectLOD::UpdateMorphVertexBufferCPU(FRHICommandList& RHICmdList, const FMorphTargetWeightMap& InActiveMorphTargets, const TArray<float>& MorphTargetWeights, 
																					const TArray<int32>& SectionIdsUseByActiveMorphTargets, bool bGPUSkinCacheEnabled, FMorphVertexBuffer& MorphVertexBuffer)
{
	SCOPE_CYCLE_COUNTER(STAT_MorphVertexBuffer_Update);

	if (IsValidRef(MorphVertexBuffer.VertexBufferRHI))
	{
		// LOD of the skel mesh is used to find number of vertices in buffer
		FSkeletalMeshLODRenderData& LodData = SkelMeshRenderData->LODRenderData[LODIndex];

		// Whether all sections of the LOD perform GPU recompute tangent
		bool bAllSectionsDoGPURecomputeTangent = bGPUSkinCacheEnabled && GSkinCacheRecomputeTangents > 0;
		if (bAllSectionsDoGPURecomputeTangent && GSkinCacheRecomputeTangents == 2)
		{
			for (int32 i = 0; i < LodData.RenderSections.Num(); ++i)
			{
				const FSkelMeshRenderSection& RenderSection = LodData.RenderSections[i];
				if (!RenderSection.bRecomputeTangent)
				{
					bAllSectionsDoGPURecomputeTangent = false;
					break;
				}
			}
		}

		// If the LOD performs GPU skin cache recompute tangent, then there is no need to update tangents here
		bool bBlendTangentsOnCPU = !bAllSectionsDoGPURecomputeTangent;
		
		const bool bUseGPUMorphTargets = UseGPUMorphTargets(FeatureLevel);
		MorphVertexBuffer.RecreateResourcesIfRequired(RHICmdList, bUseGPUMorphTargets);

		uint32 Size = LodData.GetNumVertices() * sizeof(FMorphGPUSkinVertex);

		FMorphGPUSkinVertex* Buffer = nullptr;
		{
			SCOPE_CYCLE_COUNTER(STAT_MorphVertexBuffer_Alloc);
			Buffer = (FMorphGPUSkinVertex*)FMemory::Malloc(Size);
		}

		{
			SCOPE_CYCLE_COUNTER(STAT_MorphVertexBuffer_Init);

			if (bBlendTangentsOnCPU)
			{
				// zero everything
				int32 vertsToAdd = static_cast<int32>(LodData.GetNumVertices()) - MorphAccumulatedWeightArray.Num();
				if (vertsToAdd > 0)
				{
					MorphAccumulatedWeightArray.AddUninitialized(vertsToAdd);
				}

				FMemory::Memzero(MorphAccumulatedWeightArray.GetData(), sizeof(float)*LodData.GetNumVertices());
			}

			// PackedNormals will be wrong init with 0, but they'll be overwritten later
			FMemory::Memzero(&Buffer[0], sizeof(FMorphGPUSkinVertex)*LodData.GetNumVertices());
		}

		{
			SCOPE_CYCLE_COUNTER(STAT_MorphVertexBuffer_ApplyDelta);

			const float MorphTargetMaxBlendWeight = UE::SkeletalRender::Settings::GetMorphTargetMaxBlendWeight();

			// iterate over all active morph targets and accumulate their vertex deltas
			for(const TTuple<const UMorphTarget*, int32>& MorphItem: InActiveMorphTargets)
			{
				const UMorphTarget* MorphTarget = MorphItem.Key;
				const int32 WeightIndex = MorphItem.Value;
				checkSlow(MorphTarget != nullptr);
				checkSlow(MorphTarget->HasDataForLOD(LODIndex));
				const float MorphTargetWeight = MorphTargetWeights.IsValidIndex(WeightIndex) ? MorphTargetWeights[WeightIndex] : 0.0f;
				const float MorphAbsWeight = FMath::Abs(MorphTargetWeight);
				checkSlow(MorphAbsWeight >= MinMorphTargetBlendWeight && MorphAbsWeight <= MorphTargetMaxBlendWeight);

				// Get deltas
				int32 NumDeltas;
				const FMorphTargetDelta* Deltas = MorphTarget->GetMorphTargetDelta(LODIndex, NumDeltas);

				// iterate over the vertices that this lod model has changed
				for (int32 MorphVertIdx = 0; MorphVertIdx < NumDeltas; MorphVertIdx++)
				{
					const FMorphTargetDelta& MorphVertex = Deltas[MorphVertIdx];

					// @TODO FIXMELH : temp hack until we fix importing issue
					if ((MorphVertex.SourceIdx < LodData.GetNumVertices()))
					{
						FMorphGPUSkinVertex& DestVertex = Buffer[MorphVertex.SourceIdx];

						DestVertex.DeltaPosition += MorphVertex.PositionDelta * MorphTargetWeight;

						// todo: could be moved out of the inner loop to be more efficient
						if (bBlendTangentsOnCPU)
						{
							DestVertex.DeltaTangentZ += MorphVertex.TangentZDelta * MorphTargetWeight;
							// accumulate the weight so we can normalized it later
							MorphAccumulatedWeightArray[MorphVertex.SourceIdx] += MorphAbsWeight;
						}
					}
				} // for all vertices
			} // for all morph targets

			if (bBlendTangentsOnCPU)
			{
				// copy back all the tangent values (can't use Memcpy, since we have to pack the normals)
				for (uint32 iVertex = 0; iVertex < LodData.GetNumVertices(); ++iVertex)
				{
					FMorphGPUSkinVertex& DestVertex = Buffer[iVertex];
					float AccumulatedWeight = MorphAccumulatedWeightArray[iVertex];

					// if accumulated weight is >1.f
					// previous code was applying the weight again in GPU if less than 1, but it doesn't make sense to do so
					// so instead, we just divide by AccumulatedWeight if it's more than 1.
					// now DeltaTangentZ isn't FPackedNormal, so you can apply any value to it. 
					if (AccumulatedWeight > 1.f)
					{
						DestVertex.DeltaTangentZ /= AccumulatedWeight;
					}
				}
			}
		} // ApplyDelta

		// Lock the real buffer.
		{
			SCOPE_CYCLE_COUNTER(STAT_MorphVertexBuffer_RhiLockAndCopy);
			FMorphGPUSkinVertex* ActualBuffer = (FMorphGPUSkinVertex*)RHICmdList.LockBuffer(MorphVertexBuffer.VertexBufferRHI, 0, Size, RLM_WriteOnly);
			FMemory::Memcpy(ActualBuffer, Buffer, Size);
			FMemory::Free(Buffer);
		}

		{
			SCOPE_CYCLE_COUNTER(STAT_MorphVertexBuffer_RhiUnlock);
			// Unlock the buffer.
			RHICmdList.UnlockBuffer(MorphVertexBuffer.VertexBufferRHI);
			// Copy the section Ids use by all active morph targets
			MorphVertexBuffer.SectionIds = SectionIdsUseByActiveMorphTargets;
			// set update flag
			MorphVertexBuffer.bHasBeenUpdated = true;
		}
	}
}

const FVertexFactory* FSkeletalMeshObjectGPUSkin::GetSkinVertexFactory(const FSceneView* View, int32 LODIndex, int32 ChunkIdx, ESkinVertexFactoryMode VFMode) const
{
	checkSlow( LODs.IsValidIndex(LODIndex) );
	checkSlow( DynamicData );

	const FSkeletalMeshObjectLOD& LOD = LODs[LODIndex];

	// If a mesh deformer cache was used, return the passthrough vertex factory
	if (DynamicData->bHasMeshDeformer)
	{
		return LOD.GPUSkinVertexFactories.PassthroughVertexFactories[ChunkIdx].Get();
	}

#if RHI_RAYTRACING
	// Return the passthrough vertex factory if it is requested (by ray tracing)
	if (VFMode == ESkinVertexFactoryMode::RayTracing)
	{
		check(GetSkinCacheEntryForRayTracing());
		check(FGPUSkinCache::IsEntryValid(GetSkinCacheEntryForRayTracing(), ChunkIdx));

		return LOD.GPUSkinVertexFactories.PassthroughVertexFactories[ChunkIdx].Get();
	}
#endif

	// If the GPU skinning cache was used, return the passthrough vertex factory
	if (SkinCacheEntry && FGPUSkinCache::IsEntryValid(SkinCacheEntry, ChunkIdx) && DynamicData->bIsSkinCacheAllowed)
	{
		return LOD.GPUSkinVertexFactories.PassthroughVertexFactories[ChunkIdx].Get();
	}

	// If we have not compiled GPU Skin vertex factory variants
	static const auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.SkinCache.SkipCompilingGPUSkinVF"));
	if (FeatureLevel != ERHIFeatureLevel::ES3_1 && CVar && CVar->GetBool() == true)
	{
		UE_LOG(LogSkeletalMesh, Display, TEXT("We are attempting to render with a GPU Skin Vertex Factory, but r.SkinCache.SkipCompilingGPUSkinVF=1 so we don't have shaders.  Skeletal meshes will draw in ref pose.  Either disable r.SkinCache.SkipCompilingGPUSkinVF or increase the r.SkinCache.SceneMemoryLimitInMB size."));
		return LOD.GPUSkinVertexFactories.PassthroughVertexFactories[ChunkIdx].Get();
	}

	// No passthrough usage so return the base skin vertex factory.
	return GetBaseSkinVertexFactory(LODIndex, ChunkIdx);
}

const FSkinBatchVertexFactoryUserData* FSkeletalMeshObjectGPUSkin::GetVertexFactoryUserData(const int32 LODIndex, int32 ChunkIdx, ESkinVertexFactoryMode VFMode) const
{
#if RHI_RAYTRACING
	FGPUSkinCacheEntry* Entry = VFMode == ESkinVertexFactoryMode::RayTracing ? GetSkinCacheEntryForRayTracing() : SkinCacheEntry;
#else
	FGPUSkinCacheEntry* Entry = SkinCacheEntry;
#endif
	if (Entry != nullptr)
	{
		return FGPUSkinCache::GetVertexFactoryUserData(Entry, ChunkIdx);
	}

	return &LODs[LODIndex].DeformerGeometry.VertexFactoryUserData;
}

FGPUBaseSkinVertexFactory const* FSkeletalMeshObjectGPUSkin::GetBaseSkinVertexFactory(int32 LODIndex, int32 ChunkIdx) const
{
	const FSkeletalMeshObjectLOD& LOD = LODs[LODIndex];

	// cloth simulation is updated & if this ChunkIdx is for ClothVertexFactory
	if ( DynamicData->ClothingSimData.Num() > 0 
		&& LOD.GPUSkinVertexFactories.ClothVertexFactories.IsValidIndex(ChunkIdx)  
		&& LOD.GPUSkinVertexFactories.ClothVertexFactories[ChunkIdx].IsValid() )
	{
		return LOD.GPUSkinVertexFactories.ClothVertexFactories[ChunkIdx]->GetVertexFactory();
	}

	// use the morph enabled vertex factory if any active morphs are set
	if (DynamicData->NumWeightedActiveMorphTargets > 0)
	{
		for(const TTuple<const UMorphTarget*, int32>& MorphItem: DynamicData->ActiveMorphTargets)
		{
			const UMorphTarget* MorphTarget = MorphItem.Key;
			if (MorphTarget->HasDataForSection(LODIndex, ChunkIdx))
			{
				return LOD.GPUSkinVertexFactories.MorphVertexFactories[ChunkIdx].Get();
			}
		}
	}
	
	if (bAlwaysUpdateMorphVertexBuffer || !DynamicData->ExternalMorphWeightData.MorphSets.IsEmpty())
	{
		return LOD.GPUSkinVertexFactories.MorphVertexFactories[ChunkIdx].Get();
	}

	// use the default gpu skin vertex factory
	return LOD.GPUSkinVertexFactories.VertexFactories[ChunkIdx].Get();
}

FSkinWeightVertexBuffer* FSkeletalMeshObjectGPUSkin::GetSkinWeightVertexBuffer(int32 LODIndex) const
{
	checkSlow(LODs.IsValidIndex(LODIndex));
	return LODs[LODIndex].MeshObjectWeightBuffer;
}

FMatrix FSkeletalMeshObjectGPUSkin::GetTransform() const
{
	if (DynamicData)
	{
		return DynamicData->LocalToWorld;
	}
	return FMatrix();
}

void FSkeletalMeshObjectGPUSkin::SetTransform(const FMatrix& InNewLocalToWorld, uint32 FrameNumber)
{
	if (DynamicData)
	{
		DynamicData->LocalToWorld = InNewLocalToWorld;
	}
}

void FSkeletalMeshObjectGPUSkin::RefreshClothingTransforms(const FMatrix& InNewLocalToWorld, uint32 FrameNumber)
{
	if(DynamicData && DynamicData->ClothingSimData.Num() > 0)
	{
		FSkeletalMeshObjectLOD& LOD = LODs[DynamicData->LODIndex];
		const TArray<FSkelMeshRenderSection>& Sections = GetRenderSections(DynamicData->LODIndex);
		const int32 NumSections = Sections.Num();

		DynamicData->ClothObjectLocalToWorld = InNewLocalToWorld;

		for(int32 SectionIndex = 0; SectionIndex < NumSections; ++SectionIndex)
		{
			if(LOD.GPUSkinVertexFactories.ClothVertexFactories.IsValidIndex(SectionIndex))
			{
				FGPUBaseSkinAPEXClothVertexFactory* ClothFactory = LOD.GPUSkinVertexFactories.ClothVertexFactories[SectionIndex].Get();

				if(ClothFactory)
				{
					const FSkelMeshRenderSection& Section = Sections[SectionIndex];
					FGPUBaseSkinAPEXClothVertexFactory::ClothShaderType& ClothShaderData = LOD.GPUSkinVertexFactories.ClothVertexFactories[SectionIndex]->GetClothShaderData();
					const int16 ActorIdx = Section.CorrespondClothAssetIndex;

					if(FClothSimulData* SimData = DynamicData->ClothingSimData.Find(ActorIdx))
					{
						ClothShaderData.GetClothToLocalForWriting() = FMatrix44f(SimData->ComponentRelativeTransform.ToMatrixWithScale());
					}
				}
			}
		}
	}
}

/** 
 * Initialize the stream components common to all GPU skin vertex factory types 
 *
 * @param VertexFactoryData - context for setting the vertex factory stream components. commited later
 * @param VertexBuffers - vertex buffers which contains the data and also stride info
 * @param bUseInstancedVertexWeights - use instanced influence weights instead of default weights
 */
void InitGPUSkinVertexFactoryComponents(FGPUSkinDataType* VertexFactoryData, const FSkeletalMeshObjectGPUSkin::FVertexFactoryBuffers& VertexBuffers, FGPUBaseSkinVertexFactory* VertexFactory)
{
	//position
	VertexBuffers.StaticVertexBuffers->PositionVertexBuffer.BindPositionVertexBuffer(VertexFactory, *VertexFactoryData);

	// tangents
	VertexBuffers.StaticVertexBuffers->StaticMeshVertexBuffer.BindTangentVertexBuffer(VertexFactory, *VertexFactoryData);
	VertexBuffers.StaticVertexBuffers->StaticMeshVertexBuffer.BindPackedTexCoordVertexBuffer(VertexFactory, *VertexFactoryData, MAX_TEXCOORDS);

	const FSkinWeightVertexBuffer* WeightBuffer = VertexBuffers.SkinWeightVertexBuffer; 
	const bool bUse16BitBoneIndex = WeightBuffer->Use16BitBoneIndex();
	const bool bUse16BitBoneWeight = WeightBuffer->Use16BitBoneWeight();
	VertexFactoryData->bUse16BitBoneIndex = bUse16BitBoneIndex;
	VertexFactoryData->NumBoneInfluences = WeightBuffer->GetMaxBoneInfluences();

	GPUSkinBoneInfluenceType BoneInfluenceType = WeightBuffer->GetBoneInfluenceType();
	if (BoneInfluenceType == GPUSkinBoneInfluenceType::UnlimitedBoneInfluence)
	{
		if (VertexFactory != nullptr)
		{
			FGPUBaseSkinVertexFactory::FShaderDataType& ShaderData = VertexFactory->GetShaderData();
			ShaderData.InputWeightIndexSize = VertexBuffers.SkinWeightVertexBuffer->GetBoneIndexByteSize() | (VertexBuffers.SkinWeightVertexBuffer->GetBoneWeightByteSize() << 8);
			ShaderData.InputWeightStream = VertexBuffers.SkinWeightVertexBuffer->GetDataVertexBuffer()->GetSRV();
		}

		const FSkinWeightLookupVertexBuffer* LookupVertexBuffer = WeightBuffer->GetLookupVertexBuffer();
		VertexFactoryData->BlendOffsetCount = FVertexStreamComponent(LookupVertexBuffer, 0, LookupVertexBuffer->GetStride(), VET_UInt);
	}
	else
	{
		// bone indices & weights
		const FSkinWeightDataVertexBuffer* WeightDataVertexBuffer = WeightBuffer->GetDataVertexBuffer();
		const uint32 Stride = WeightBuffer->GetConstantInfluencesVertexStride();
		const uint32 WeightsOffset = WeightBuffer->GetConstantInfluencesBoneWeightsOffset();
		VertexFactoryData->BoneIndices = FVertexStreamComponent(WeightDataVertexBuffer, 0, Stride, bUse16BitBoneIndex ? VET_UShort4 : VET_UByte4);
		VertexFactoryData->BoneWeights = FVertexStreamComponent(WeightDataVertexBuffer, WeightsOffset, Stride, bUse16BitBoneWeight ? VET_UShort4N : VET_UByte4N);

		if (VertexFactoryData->NumBoneInfluences > MAX_INFLUENCES_PER_STREAM)
		{
			// Extra streams for bone indices & weights
			VertexFactoryData->ExtraBoneIndices = FVertexStreamComponent(
				WeightDataVertexBuffer, 4 * VertexBuffers.SkinWeightVertexBuffer->GetBoneIndexByteSize(), Stride, bUse16BitBoneIndex ? VET_UShort4 : VET_UByte4);
			VertexFactoryData->ExtraBoneWeights = FVertexStreamComponent(
				WeightDataVertexBuffer, WeightsOffset + 4 * VertexBuffers.SkinWeightVertexBuffer->GetBoneWeightByteSize(), Stride, bUse16BitBoneWeight ? VET_UShort4N : VET_UByte4N);
		}
	}

	// Color data may be NULL
	if( VertexBuffers.ColorVertexBuffer != NULL && 
		VertexBuffers.ColorVertexBuffer->IsInitialized() )
	{
		// Color
		VertexBuffers.ColorVertexBuffer->BindColorVertexBuffer(VertexFactory, *VertexFactoryData);
	}
	else
	{
		VertexFactoryData->ColorComponentsSRV = nullptr;
		VertexFactoryData->ColorIndexMask = 0;
	}
}

/** 
 * Initialize the stream components common to all GPU skin vertex factory types 
 *
 * @param VertexFactoryData - context for setting the vertex factory stream components. commited later
 * @param VertexBuffers - vertex buffers which contains the data and also stride info
 * @param bUseInstancedVertexWeights - use instanced influence weights instead of default weights
 */
void InitMorphVertexFactoryComponents(FGPUSkinDataType* VertexFactoryData, FMorphVertexBufferPool* MorphVertexBufferPool)
{
	VertexFactoryData->bMorphTarget = true;
	VertexFactoryData->MorphVertexBufferPool = MorphVertexBufferPool;

	const FMorphVertexBuffer* MorphVB = MorphVertexBufferPool ? &MorphVertexBufferPool->GetMorphVertexBufferForReading(false) : nullptr;

	// delta positions
	VertexFactoryData->DeltaPositionComponent = FVertexStreamComponent(
		MorphVB, STRUCT_OFFSET(FMorphGPUSkinVertex,DeltaPosition),sizeof(FMorphGPUSkinVertex),VET_Float3);
	// delta normals
	VertexFactoryData->DeltaTangentZComponent = FVertexStreamComponent(
		MorphVB, STRUCT_OFFSET(FMorphGPUSkinVertex, DeltaTangentZ), sizeof(FMorphGPUSkinVertex), VET_Float3);
}

/** 
 * Initialize the stream components common to all GPU skin vertex factory types 
 *
 * @param VertexFactoryData - context for setting the vertex factory stream components. commited later
 * @param VertexBuffers - vertex buffers which contains the data and also stride info
 * @param bUseInstancedVertexWeights - use instanced influence weights instead of default weights
 */
void InitAPEXClothVertexFactoryComponents(FGPUSkinAPEXClothDataType* VertexFactoryData,
										const FSkeletalMeshObjectGPUSkin::FVertexFactoryBuffers& VertexBuffers)
{
	VertexFactoryData->ClothBuffer = VertexBuffers.APEXClothVertexBuffer->GetSRV();
	VertexFactoryData->ClothIndexMapping = VertexBuffers.APEXClothVertexBuffer->GetClothIndexMapping();
}

/** 
 * Handles transferring data between game/render threads when initializing vertex factory components 
 */
class FDynamicUpdateVertexFactoryData
{
public:
	FDynamicUpdateVertexFactoryData(
		FGPUBaseSkinVertexFactory* InVertexFactory,
		const FSkeletalMeshObjectGPUSkin::FVertexFactoryBuffers& InVertexBuffers)
		:	VertexFactory(InVertexFactory)
		,	VertexBuffers(InVertexBuffers)
	{}

	FGPUBaseSkinVertexFactory* VertexFactory;
	const FSkeletalMeshObjectGPUSkin::FVertexFactoryBuffers VertexBuffers;
};

static FPSOPrecacheVertexFactoryData GetVertexFactoryData(FSkeletalMeshLODRenderData& LODRenderData, FGPUSkinDataType& GPUSkinDataType, ERHIFeatureLevel::Type FeatureLevel)
{
	const FVertexFactoryType* VertexFactoryType;
	FVertexDeclarationElementList VertexElements;
	if (LODRenderData.SkinWeightVertexBuffer.GetBoneInfluenceType() == GPUSkinBoneInfluenceType::DefaultBoneInfluence)
	{
		VertexFactoryType = &TGPUSkinVertexFactory<GPUSkinBoneInfluenceType::DefaultBoneInfluence>::StaticType;
		TGPUSkinVertexFactory<GPUSkinBoneInfluenceType::DefaultBoneInfluence>::GetVertexElements(FeatureLevel, EVertexInputStreamType::Default, GPUSkinDataType, VertexElements);
	}
	else
	{
		VertexFactoryType = &TGPUSkinVertexFactory<GPUSkinBoneInfluenceType::UnlimitedBoneInfluence>::StaticType;
		TGPUSkinVertexFactory<GPUSkinBoneInfluenceType::UnlimitedBoneInfluence>::GetVertexElements(FeatureLevel, EVertexInputStreamType::Default, GPUSkinDataType, VertexElements);
	}
	return FPSOPrecacheVertexFactoryData(VertexFactoryType, VertexElements);
}

/**
 * Creates a vertex factory entry for the given type and initialize it on the render thread
 */
static FGPUBaseSkinVertexFactory* CreateVertexFactory(TArray<TUniquePtr<FGPUBaseSkinVertexFactory>>& VertexFactories,
						 const FSkeletalMeshObjectGPUSkin::FVertexFactoryBuffers& InVertexBuffers,
						 ERHIFeatureLevel::Type FeatureLevel
						 )
{
	FGPUBaseSkinVertexFactory* VertexFactory = nullptr;
	GPUSkinBoneInfluenceType BoneInfluenceType = InVertexBuffers.SkinWeightVertexBuffer->GetBoneInfluenceType();
	if (BoneInfluenceType == GPUSkinBoneInfluenceType::DefaultBoneInfluence)
	{
		VertexFactory = new TGPUSkinVertexFactory<GPUSkinBoneInfluenceType::DefaultBoneInfluence>(FeatureLevel, InVertexBuffers.NumVertices);
	}
	else
	{
		VertexFactory = new TGPUSkinVertexFactory<GPUSkinBoneInfluenceType::UnlimitedBoneInfluence>(FeatureLevel, InVertexBuffers.NumVertices);
	}
	VertexFactories.Add(TUniquePtr<FGPUBaseSkinVertexFactory>(VertexFactory));

	// Setup the update data for enqueue
	FDynamicUpdateVertexFactoryData VertexUpdateData(VertexFactory, InVertexBuffers);

	// update vertex factory components and sync it
	ENQUEUE_RENDER_COMMAND(InitGPUSkinVertexFactory)(UE::RenderCommandPipe::SkeletalMesh,
		[VertexUpdateData](FRHICommandList& RHICmdList)
		{
			FGPUSkinDataType Data;
			InitGPUSkinVertexFactoryComponents(&Data, VertexUpdateData.VertexBuffers, VertexUpdateData.VertexFactory);
			VertexUpdateData.VertexFactory->SetData(RHICmdList, &Data);
			VertexUpdateData.VertexFactory->InitResource(RHICmdList);
		}
	);

	return VertexFactory;
}

void UpdateVertexFactory(TArray<TUniquePtr<FGPUBaseSkinVertexFactory>>& VertexFactories,
	const FSkeletalMeshObjectGPUSkin::FVertexFactoryBuffers& InVertexBuffers)
{
	for (TUniquePtr<FGPUBaseSkinVertexFactory>& FactoryPtr : VertexFactories)
	{
		FGPUBaseSkinVertexFactory* VertexFactory = FactoryPtr.Get();

		if (VertexFactory != nullptr)
		{
			// Setup the update data for enqueue
			FDynamicUpdateVertexFactoryData VertexUpdateData(VertexFactory, InVertexBuffers);

			// update vertex factory components and sync it
			ENQUEUE_RENDER_COMMAND(UpdateGPUSkinVertexFactory)(UE::RenderCommandPipe::SkeletalMesh,
				[VertexUpdateData](FRHICommandList& RHICmdList)
			{
				FGPUSkinDataType Data;
				InitGPUSkinVertexFactoryComponents(&Data, VertexUpdateData.VertexBuffers, VertexUpdateData.VertexFactory);
				VertexUpdateData.VertexFactory->SetData(RHICmdList, &Data);
			});
		}
	}
}

static void CreatePassthroughVertexFactory(ERHIFeatureLevel::Type InFeatureLevel, TArray<TUniquePtr<FGPUSkinPassthroughVertexFactory>>& PassthroughVertexFactories,
	FGPUBaseSkinVertexFactory* SourceVertexFactory)
{
	FGPUSkinPassthroughVertexFactory* NewPassthroughVertexFactory = new FGPUSkinPassthroughVertexFactory(InFeatureLevel);
	PassthroughVertexFactories.Add(TUniquePtr<FGPUSkinPassthroughVertexFactory>(NewPassthroughVertexFactory));

	// update vertex factory components and sync it
	ENQUEUE_RENDER_COMMAND(InitPassthroughGPUSkinVertexFactory)(UE::RenderCommandPipe::SkeletalMesh,
		[NewPassthroughVertexFactory, SourceVertexFactory](FRHICommandList& RHICmdList)
		{
			FLocalVertexFactory::FDataType Data;
			SourceVertexFactory->CopyDataTypeForLocalVertexFactory(Data);
			NewPassthroughVertexFactory->SetData(RHICmdList, Data);
			NewPassthroughVertexFactory->InitResource(RHICmdList);
		}
	);
}

/**
 * Creates a vertex factory entry for the given type and initialize it on the render thread
 */
static void CreateVertexFactoryMorph(TArray<TUniquePtr<FGPUBaseSkinVertexFactory>>& VertexFactories,
						 const FSkeletalMeshObjectGPUSkin::FVertexFactoryBuffers& InVertexBuffers,
						 ERHIFeatureLevel::Type FeatureLevel
						 )
{
	FGPUBaseSkinVertexFactory* VertexFactory = nullptr;
	GPUSkinBoneInfluenceType BoneInfluenceType = InVertexBuffers.SkinWeightVertexBuffer->GetBoneInfluenceType();
	if (BoneInfluenceType == GPUSkinBoneInfluenceType::DefaultBoneInfluence)
	{
		VertexFactory = new TGPUSkinVertexFactory<GPUSkinBoneInfluenceType::DefaultBoneInfluence>(FeatureLevel, InVertexBuffers.NumVertices);
	}
	else
	{
		VertexFactory = new TGPUSkinVertexFactory<GPUSkinBoneInfluenceType::UnlimitedBoneInfluence>(FeatureLevel, InVertexBuffers.NumVertices);
	}
	VertexFactories.Add(TUniquePtr<FGPUBaseSkinVertexFactory>(VertexFactory));

	// Setup the update data for enqueue
	FDynamicUpdateVertexFactoryData VertexUpdateData(VertexFactory, InVertexBuffers);

	// update vertex factory components and sync it
	ENQUEUE_RENDER_COMMAND(InitGPUSkinVertexFactoryMorph)(UE::RenderCommandPipe::SkeletalMesh,
		[VertexUpdateData](FRHICommandList& RHICmdList)
		{
			FGPUSkinDataType Data;
			InitGPUSkinVertexFactoryComponents(&Data, VertexUpdateData.VertexBuffers, VertexUpdateData.VertexFactory);
			InitMorphVertexFactoryComponents(&Data, VertexUpdateData.VertexBuffers.MorphVertexBufferPool);
			VertexUpdateData.VertexFactory->SetData(RHICmdList, &Data);
			VertexUpdateData.VertexFactory->InitResource(RHICmdList);
		}
	);
}

static void UpdateVertexFactoryMorph(TArray<TUniquePtr<FGPUBaseSkinVertexFactory>>& VertexFactories,
	const FSkeletalMeshObjectGPUSkin::FVertexFactoryBuffers& InVertexBuffers)
{
	for (TUniquePtr<FGPUBaseSkinVertexFactory>& FactoryPtr : VertexFactories)
	{
		FGPUBaseSkinVertexFactory* VertexFactory = FactoryPtr.Get();

		if (VertexFactory != nullptr)
		{
			// Setup the update data for enqueue
			FDynamicUpdateVertexFactoryData VertexUpdateData(VertexFactory, InVertexBuffers);

			// update vertex factory components and sync it
			ENQUEUE_RENDER_COMMAND(InitGPUSkinVertexFactoryMorph)(UE::RenderCommandPipe::SkeletalMesh,
				[VertexUpdateData](FRHICommandList& RHICmdList)
			{
				FGPUSkinDataType Data;
				InitGPUSkinVertexFactoryComponents(&Data, VertexUpdateData.VertexBuffers, VertexUpdateData.VertexFactory);
				InitMorphVertexFactoryComponents(&Data, VertexUpdateData.VertexBuffers.MorphVertexBufferPool);
				VertexUpdateData.VertexFactory->SetData(RHICmdList, &Data);
			});
		}
	}
}


// APEX cloth

static FPSOPrecacheVertexFactoryData GetVertexFactoryDataCloth(FSkeletalMeshLODRenderData& LODRenderData, FGPUSkinDataType& GPUSkinDataType, ERHIFeatureLevel::Type FeatureLevel)
{
	const FVertexFactoryType* VertexFactoryType;
	FVertexDeclarationElementList VertexElements;
	if (LODRenderData.SkinWeightVertexBuffer.GetBoneInfluenceType() == GPUSkinBoneInfluenceType::DefaultBoneInfluence)
	{
		VertexFactoryType = &TGPUSkinAPEXClothVertexFactory<GPUSkinBoneInfluenceType::DefaultBoneInfluence>::StaticType;
		TGPUSkinAPEXClothVertexFactory<GPUSkinBoneInfluenceType::DefaultBoneInfluence>::GetVertexElements(FeatureLevel, EVertexInputStreamType::Default, GPUSkinDataType, VertexElements);
	}
	else
	{
		VertexFactoryType = &TGPUSkinAPEXClothVertexFactory<GPUSkinBoneInfluenceType::UnlimitedBoneInfluence>::StaticType;
		TGPUSkinAPEXClothVertexFactory<GPUSkinBoneInfluenceType::UnlimitedBoneInfluence>::GetVertexElements(FeatureLevel, EVertexInputStreamType::Default, GPUSkinDataType, VertexElements);
	}
	return FPSOPrecacheVertexFactoryData(VertexFactoryType, VertexElements);
}

/**
 * Creates a vertex factory entry for the given type and initialize it on the render thread
 */
static void CreateVertexFactoryCloth(TArray<TUniquePtr<FGPUBaseSkinAPEXClothVertexFactory>>& VertexFactories,
						 const FSkeletalMeshObjectGPUSkin::FVertexFactoryBuffers& InVertexBuffers,
						 ERHIFeatureLevel::Type FeatureLevel,
						 uint32 NumInfluencesPerVertex
						 )
{
	FGPUBaseSkinAPEXClothVertexFactory* ClothVertexFactory = nullptr;
	GPUSkinBoneInfluenceType BoneInfluenceType = InVertexBuffers.SkinWeightVertexBuffer->GetBoneInfluenceType();
	if (BoneInfluenceType == GPUSkinBoneInfluenceType::DefaultBoneInfluence)
	{
		ClothVertexFactory = new TGPUSkinAPEXClothVertexFactory<GPUSkinBoneInfluenceType::DefaultBoneInfluence>(FeatureLevel, InVertexBuffers.NumVertices, NumInfluencesPerVertex);
	}
	else
	{
		ClothVertexFactory = new TGPUSkinAPEXClothVertexFactory<GPUSkinBoneInfluenceType::UnlimitedBoneInfluence>(FeatureLevel, InVertexBuffers.NumVertices, NumInfluencesPerVertex);
	}
	VertexFactories.Add(TUniquePtr<FGPUBaseSkinAPEXClothVertexFactory>(ClothVertexFactory));

	// Setup the update data for enqueue
	FGPUBaseSkinVertexFactory* VertexFactory = ClothVertexFactory->GetVertexFactory();
	FDynamicUpdateVertexFactoryData VertexUpdateData(VertexFactory, InVertexBuffers);

	// update vertex factory components and sync it
	ENQUEUE_RENDER_COMMAND(InitGPUSkinAPEXClothVertexFactory)(UE::RenderCommandPipe::SkeletalMesh,
		[VertexUpdateData](FRHICommandList& RHICmdList)
		{
			FGPUSkinAPEXClothDataType Data;
			InitGPUSkinVertexFactoryComponents(&Data, VertexUpdateData.VertexBuffers, VertexUpdateData.VertexFactory);
			InitAPEXClothVertexFactoryComponents(&Data, VertexUpdateData.VertexBuffers);
			VertexUpdateData.VertexFactory->SetData(RHICmdList, &Data);
			VertexUpdateData.VertexFactory->InitResource(RHICmdList);
		}
	);
}

static void UpdateVertexFactoryCloth(TArray<TUniquePtr<FGPUBaseSkinAPEXClothVertexFactory>>& VertexFactories,
	const FSkeletalMeshObjectGPUSkin::FVertexFactoryBuffers& InVertexBuffers)
{
	for (TUniquePtr<FGPUBaseSkinAPEXClothVertexFactory>& FactoryPtr : VertexFactories)
	{
		FGPUBaseSkinAPEXClothVertexFactory* BaseVertexFactory = FactoryPtr.Get();

		if (BaseVertexFactory != nullptr)
		{
			FGPUBaseSkinVertexFactory* VertexFactory = BaseVertexFactory->GetVertexFactory();

			if (VertexFactory != nullptr)
			{
				// Setup the update data for enqueue
				FDynamicUpdateVertexFactoryData VertexUpdateData(VertexFactory, InVertexBuffers);

				// update vertex factory components and sync it
				ENQUEUE_RENDER_COMMAND(InitGPUSkinAPEXClothVertexFactory)(UE::RenderCommandPipe::SkeletalMesh,
					[VertexUpdateData](FRHICommandList& RHICmdList)
				{
					FGPUSkinAPEXClothDataType Data;
					InitGPUSkinVertexFactoryComponents(&Data, VertexUpdateData.VertexBuffers, VertexUpdateData.VertexFactory);
					InitAPEXClothVertexFactoryComponents(&Data, VertexUpdateData.VertexBuffers);
					VertexUpdateData.VertexFactory->SetData(RHICmdList, &Data);
				});
			}
		}
	}
}


void FSkeletalMeshObjectGPUSkin::GetUsedVertexFactoryData(
	FSkeletalMeshRenderData* SkelMeshRenderData, 
	int32 LODIndex, 
	USkinnedMeshComponent* SkinnedMeshComponent, 
	FSkelMeshRenderSection& RenderSection, 
	ERHIFeatureLevel::Type InFeatureLevel, 
	bool bHasMorphTargets, 
	FPSOPrecacheVertexFactoryDataList& VertexFactoryDataList)
{
	FSkeletalMeshLODRenderData& LODRenderData = SkelMeshRenderData->LODRenderData[LODIndex];

	FSkelMeshComponentLODInfo* CompLODInfo = nullptr;
	if (SkinnedMeshComponent && SkinnedMeshComponent->LODInfo.IsValidIndex(LODIndex))
	{
		CompLODInfo = &SkinnedMeshComponent->LODInfo[LODIndex];
	}

	// Setup tmp MeshObjectLOD object to extract the vertex factory buffers
	FSkeletalMeshObjectGPUSkin::FSkeletalMeshObjectLOD SkeletalMeshObjectLOD(SkelMeshRenderData, LODIndex, InFeatureLevel, nullptr);
	SkeletalMeshObjectLOD.MeshObjectWeightBuffer = FSkeletalMeshObject::GetSkinWeightVertexBuffer(LODRenderData, CompLODInfo);
	SkeletalMeshObjectLOD.MeshObjectColorBuffer = FSkeletalMeshObject::GetColorVertexBuffer(LODRenderData, CompLODInfo);

	// Vertex buffers available for the LOD
	FVertexFactoryBuffers VertexBuffers;
	SkeletalMeshObjectLOD.GetVertexBuffers(VertexBuffers, LODRenderData);

	// Setup the skin data type so the correct vertex element data can be collected
	FGPUSkinDataType GPUSkinDataType;
	InitGPUSkinVertexFactoryComponents(&GPUSkinDataType, VertexBuffers, nullptr /*FGPUBaseSkinVertexFactory*/);

	bool bIsSkinCacheAllowed = SkinnedMeshComponent ? SkinnedMeshComponent->IsSkinCacheAllowed(LODIndex) : false;
	bool bHasMeshDeformer = SkinnedMeshComponent && SkinnedMeshComponent->GetMeshDeformerInstanceForLOD(LODIndex);
	if (bIsSkinCacheAllowed || bHasMeshDeformer)
	{
		VertexFactoryDataList.AddUnique(FPSOPrecacheVertexFactoryData(&FGPUSkinPassthroughVertexFactory::StaticType));
	}
	
	if (!bHasMeshDeformer)
	{
		// Add GPU skin cloth vertex factory type is needed
		const EShaderPlatform ShaderPlatform = GetFeatureLevelShaderPlatform(InFeatureLevel);
		const bool bClothEnabled = FGPUBaseSkinAPEXClothVertexFactory::IsClothEnabled(ShaderPlatform);
		if (bClothEnabled && RenderSection.HasClothingData())
		{
			VertexFactoryDataList.AddUnique(GetVertexFactoryDataCloth(LODRenderData, GPUSkinDataType, InFeatureLevel));
		}
		else
		{
			// Add GPU skin vertex factory type
			VertexFactoryDataList.AddUnique(GetVertexFactoryData(LODRenderData, GPUSkinDataType, InFeatureLevel));

			if (bHasMorphTargets)
			{
				InitMorphVertexFactoryComponents(&GPUSkinDataType, nullptr);
				VertexFactoryDataList.AddUnique(GetVertexFactoryData(LODRenderData, GPUSkinDataType, InFeatureLevel));
			}
		}
	}
}

/**
 * Determine the current vertex buffers valid for the current LOD
 *
 * @param OutVertexBuffers output vertex buffers
 */
void FSkeletalMeshObjectGPUSkin::FSkeletalMeshObjectLOD::GetVertexBuffers(FVertexFactoryBuffers& OutVertexBuffers, FSkeletalMeshLODRenderData& LODData)
{
	OutVertexBuffers.StaticVertexBuffers = &LODData.StaticVertexBuffers;
	OutVertexBuffers.ColorVertexBuffer = MeshObjectColorBuffer;
	OutVertexBuffers.SkinWeightVertexBuffer = MeshObjectWeightBuffer;
	OutVertexBuffers.MorphVertexBufferPool = MorphVertexBufferPool;
	OutVertexBuffers.APEXClothVertexBuffer = &LODData.ClothVertexBuffer;
	OutVertexBuffers.NumVertices = LODData.GetNumVertices();
}

/** 
 * Init vertex factory resources for this LOD 
 *
 * @param VertexBuffers - available vertex buffers to reference in vertex factory streams
 * @param Chunks - relevant chunk information (either original or from swapped influence)
 */
void FSkeletalMeshObjectGPUSkin::FVertexFactoryData::InitVertexFactories(
	const FVertexFactoryBuffers& VertexBuffers, 
	const TArray<FSkelMeshRenderSection>& Sections, 
	ERHIFeatureLevel::Type InFeatureLevel)
{
	// first clear existing factories (resources assumed to have been released already)
	// then [re]create the factories

	VertexFactories.Empty(Sections.Num());
	{
		for(int32 FactoryIdx = 0; FactoryIdx < Sections.Num(); ++FactoryIdx)
		{
			FGPUBaseSkinVertexFactory* VertexFactory = CreateVertexFactory(VertexFactories, VertexBuffers, InFeatureLevel);
			CreatePassthroughVertexFactory(InFeatureLevel, PassthroughVertexFactories, VertexFactory);
		}
	}
}

/** 
 * Release vertex factory resources for this LOD 
 */
void FSkeletalMeshObjectGPUSkin::FVertexFactoryData::ReleaseVertexFactories()
{
	// Default factories
	for( int32 FactoryIdx=0; FactoryIdx < VertexFactories.Num(); FactoryIdx++)
	{
		BeginReleaseResource(VertexFactories[FactoryIdx].Get(), &UE::RenderCommandPipe::SkeletalMesh);
	}

	for (int32 FactoryIdx = 0; FactoryIdx < PassthroughVertexFactories.Num(); FactoryIdx++)
	{
		BeginReleaseResource(PassthroughVertexFactories[FactoryIdx].Get(), &UE::RenderCommandPipe::SkeletalMesh);
	}
}

void FSkeletalMeshObjectGPUSkin::FVertexFactoryData::InitMorphVertexFactories(
	const FVertexFactoryBuffers& VertexBuffers, 
	const TArray<FSkelMeshRenderSection>& Sections,
	bool bInUsePerBoneMotionBlur,
	ERHIFeatureLevel::Type InFeatureLevel)
{
	// clear existing factories (resources assumed to have been released already)
	MorphVertexFactories.Empty(Sections.Num());
	for( int32 FactoryIdx=0; FactoryIdx < Sections.Num(); FactoryIdx++ )
	{
		CreateVertexFactoryMorph(MorphVertexFactories, VertexBuffers, InFeatureLevel);
	}
}

/** 
 * Release morph vertex factory resources for this LOD 
 */
void FSkeletalMeshObjectGPUSkin::FVertexFactoryData::ReleaseMorphVertexFactories()
{
	// Default morph factories
	for( int32 FactoryIdx=0; FactoryIdx < MorphVertexFactories.Num(); FactoryIdx++ )
	{
		BeginReleaseResource(MorphVertexFactories[FactoryIdx].Get(), &UE::RenderCommandPipe::SkeletalMesh);
	}
}

void FSkeletalMeshObjectGPUSkin::FVertexFactoryData::InitAPEXClothVertexFactories(
	const FVertexFactoryBuffers& VertexBuffers, 
	const TArray<FSkelMeshRenderSection>& Sections,
	ERHIFeatureLevel::Type InFeatureLevel)
{
	const EShaderPlatform ShaderPlatform = GetFeatureLevelShaderPlatform(InFeatureLevel);
	const bool bClothEnabled = FGPUBaseSkinAPEXClothVertexFactory::IsClothEnabled(ShaderPlatform);

	// clear existing factories (resources assumed to have been released already)
	ClothVertexFactories.Empty(Sections.Num());

	for (const FSkelMeshRenderSection& Section : Sections)
	{
		if (Section.HasClothingData() && bClothEnabled)
		{
			constexpr int32 ClothLODBias = 0;
			const uint32 NumClothWeights = Section.ClothMappingDataLODs.Num() ? Section.ClothMappingDataLODs[ClothLODBias].Num(): 0;
			const uint32 NumPositionVertices = Section.NumVertices;
			// NumInfluencesPerVertex should be a whole integer
			check(NumClothWeights % NumPositionVertices == 0);
			const uint32 NumInfluencesPerVertex = NumClothWeights / NumPositionVertices;
			CreateVertexFactoryCloth(ClothVertexFactories, VertexBuffers, InFeatureLevel, NumInfluencesPerVertex);
		}
		else
		{
			ClothVertexFactories.Add(TUniquePtr<FGPUBaseSkinAPEXClothVertexFactory>(nullptr));
		}
	}
}

/** 
 * Release APEX cloth vertex factory resources for this LOD 
 */
void FSkeletalMeshObjectGPUSkin::FVertexFactoryData::ReleaseAPEXClothVertexFactories()
{
	// Default APEX cloth factories
	for( int32 FactoryIdx=0; FactoryIdx < ClothVertexFactories.Num(); FactoryIdx++ )
	{
		TUniquePtr<FGPUBaseSkinAPEXClothVertexFactory>& ClothVertexFactory = ClothVertexFactories[FactoryIdx];
		if (ClothVertexFactory)
		{
			BeginReleaseResource(ClothVertexFactory->GetVertexFactory(), &UE::RenderCommandPipe::SkeletalMesh);
		}
	}
}

void FSkeletalMeshObjectGPUSkin::FVertexFactoryData::UpdateVertexFactoryData(const FVertexFactoryBuffers& VertexBuffers)
{
	UpdateVertexFactory(VertexFactories, VertexBuffers);
	UpdateVertexFactoryCloth(ClothVertexFactories, VertexBuffers);
	UpdateVertexFactoryMorph(MorphVertexFactories, VertexBuffers);
}

void FSkeletalMeshObjectGPUSkin::FSkeletalMeshObjectLOD::InitResources(const FSkelMeshObjectLODInfo& MeshLODInfo, FSkelMeshComponentLODInfo* CompLODInfo, ERHIFeatureLevel::Type InFeatureLevel)
{
	check(SkelMeshRenderData);
	check(SkelMeshRenderData->LODRenderData.IsValidIndex(LODIndex));

	// vertex buffer for each lod has already been created when skelmesh was loaded
	FSkeletalMeshLODRenderData& LODData = SkelMeshRenderData->LODRenderData[LODIndex];	
	MeshObjectWeightBuffer = FSkeletalMeshObject::GetSkinWeightVertexBuffer(LODData, CompLODInfo);
	MeshObjectColorBuffer = FSkeletalMeshObject::GetColorVertexBuffer(LODData, CompLODInfo);

	// Vertex buffers available for the LOD
	FVertexFactoryBuffers VertexBuffers;
	GetVertexBuffers(VertexBuffers, LODData);

	// init gpu skin factories
	GPUSkinVertexFactories.InitVertexFactories(VertexBuffers, LODData.RenderSections, InFeatureLevel);
	if (LODData.HasClothData() )
	{
		GPUSkinVertexFactories.InitAPEXClothVertexFactories(VertexBuffers, LODData.RenderSections, InFeatureLevel);
	}
}

/** 
 * Release rendering resources for this LOD 
 */
void FSkeletalMeshObjectGPUSkin::FSkeletalMeshObjectLOD::ReleaseResources()
{	
	// Release gpu skin vertex factories
	GPUSkinVertexFactories.ReleaseVertexFactories();

	// Release APEX cloth vertex factory
	GPUSkinVertexFactories.ReleaseAPEXClothVertexFactories();
}

void FSkeletalMeshObjectGPUSkin::FSkeletalMeshObjectLOD::InitMorphResources(const FSkelMeshObjectLODInfo& MeshLODInfo, bool bInUsePerBoneMotionBlur, ERHIFeatureLevel::Type InFeatureLevel)
{
	check(SkelMeshRenderData);
	check(SkelMeshRenderData->LODRenderData.IsValidIndex(LODIndex));

	// vertex buffer for each lod has already been created when skelmesh was loaded
	FSkeletalMeshLODRenderData& LODData = SkelMeshRenderData->LODRenderData[LODIndex];

	// init the delta vertex buffer for this LOD
	const FName OwnerName = LODData.MorphTargetVertexInfoBuffers.GetOwnerName();
	MorphVertexBufferPool->InitResources(OwnerName);

	// Vertex buffers available for the LOD
	FVertexFactoryBuffers VertexBuffers;
	GetVertexBuffers(VertexBuffers, LODData);
	// init morph skin factories
	GPUSkinVertexFactories.InitMorphVertexFactories(VertexBuffers, LODData.RenderSections, bInUsePerBoneMotionBlur, InFeatureLevel);
}

/** 
* Release rendering resources for the morph stream of this LOD
*/
void FSkeletalMeshObjectGPUSkin::FSkeletalMeshObjectLOD::ReleaseMorphResources()
{
	// Release morph vertex factories
	GPUSkinVertexFactories.ReleaseMorphVertexFactories();

	// By design, we do not release MorphVertexBufferPool, as it may persist when render state gets re-created.  Instead, it gets released
	// when its ref count goes to zero in the FSkeletalMeshObjectLOD destructor.
}


TArray<FTransform>* FSkeletalMeshObjectGPUSkin::GetComponentSpaceTransforms() const
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if(DynamicData)
	{
		return &(DynamicData->MeshComponentSpaceTransforms);
	}
	else
#endif
	{
		return NULL;
	}
}

const TArray<FMatrix44f>& FSkeletalMeshObjectGPUSkin::GetReferenceToLocalMatrices() const
{
	return DynamicData->ReferenceToLocal;
}

FMeshDeformerGeometry& FSkeletalMeshObjectGPUSkin::GetDeformerGeometry(int32 LODIndex)
{
	return LODs[LODIndex].DeformerGeometry;
}

bool FSkeletalMeshObjectGPUSkin::GetCachedGeometry(FCachedGeometry& OutCachedGeometry) const
{
	OutCachedGeometry = FCachedGeometry();

	// Cached geometry is only available if we are using skin cache or a mesh deformer.	
	if (DynamicData == nullptr || !(DynamicData->bIsSkinCacheAllowed || DynamicData->bHasMeshDeformer))
	{
		return false;
	}

	const int32 LodIndex = GetLOD();
	if (SkeletalMeshRenderData == nullptr || !SkeletalMeshRenderData->LODRenderData.IsValidIndex(LodIndex))
	{
		return false;
	}

	FSkeletalMeshLODRenderData const& LODRenderData = SkeletalMeshRenderData->LODRenderData[LodIndex];
	const uint32 SectionCount = LODRenderData.RenderSections.Num();

	FVertexFactoryData const& VertexFactories = LODs[LodIndex].GPUSkinVertexFactories;
	if (VertexFactories.PassthroughVertexFactories.Num() != SectionCount)
	{
		return false;
	}

	for (uint32 SectionIndex = 0; SectionIndex < SectionCount; ++SectionIndex)
	{
		FCachedGeometry::Section& CachedSection = OutCachedGeometry.Sections.AddDefaulted_GetRef();

		if (SkinCacheEntry != nullptr)
		{
			// Get the cached geometry SRVs from the skin cache.
			CachedSection.PositionBuffer = FGPUSkinCache::GetPositionBuffer(SkinCacheEntry, SectionIndex)->SRV;
			CachedSection.PreviousPositionBuffer = FGPUSkinCache::GetPreviousPositionBuffer(SkinCacheEntry, SectionIndex)->SRV;

			FRWBuffer* TangentBuffer = FGPUSkinCache::GetTangentBuffer(SkinCacheEntry, SectionIndex);
			CachedSection.TangentBuffer = TangentBuffer ? TangentBuffer->SRV : nullptr;
		}
		else
		{
			
			// Get the cached geometry SRVs from the deformer geometry.
			FMeshDeformerGeometry const& DeformerGeometry = LODs[LodIndex].DeformerGeometry;

			if (!DeformerGeometry.Position.IsValid())
			{
				// Reset all output if one section isn't available.
				OutCachedGeometry.Sections.Reset();
				return false;
			}

			CachedSection.PositionBuffer = DeformerGeometry.PositionSRV;
			CachedSection.PreviousPositionBuffer = DeformerGeometry.PrevPositionSRV;
			CachedSection.PreviousPositionBuffer = CachedSection.PreviousPositionBuffer != nullptr ? CachedSection.PreviousPositionBuffer : CachedSection.PositionBuffer;
		}
				
		CachedSection.IndexBuffer = LODRenderData.MultiSizeIndexContainer.GetIndexBuffer()->GetSRV();
		CachedSection.TotalIndexCount = LODRenderData.MultiSizeIndexContainer.GetIndexBuffer()->Num();
		CachedSection.TotalVertexCount = LODRenderData.StaticVertexBuffers.PositionVertexBuffer.GetNumVertices();
		CachedSection.UVsBuffer = LODRenderData.StaticVertexBuffers.StaticMeshVertexBuffer.GetTexCoordsSRV();
		CachedSection.UVsChannelOffset = 0; // Assume that we needs to pair meshes based on UVs 0
		CachedSection.UVsChannelCount = LODRenderData.StaticVertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords();

		FSkelMeshRenderSection const& Section = LODRenderData.RenderSections[SectionIndex];
		CachedSection.LODIndex = LodIndex;
		CachedSection.SectionIndex = SectionIndex;
		CachedSection.NumPrimitives = Section.NumTriangles;
		CachedSection.NumVertices = Section.NumVertices;
		CachedSection.IndexBaseIndex = Section.BaseIndex;
		CachedSection.VertexBaseIndex = Section.BaseVertexIndex;
	}
			
	OutCachedGeometry.LODIndex = LodIndex;
	OutCachedGeometry.LocalToWorld = FTransform(GetTransform());
	return true;
}

/*-----------------------------------------------------------------------------
FDynamicSkelMeshObjectDataGPUSkin
-----------------------------------------------------------------------------*/

void FDynamicSkelMeshObjectDataGPUSkin::Clear()
{
	ReferenceToLocal.Reset();
	ReferenceToLocalForRayTracing.Reset();
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST) 
	MeshComponentSpaceTransforms.Reset();
#endif
	LODIndex = 0;
	ActiveMorphTargets.Reset();
	MorphTargetWeights.Reset();
	ExternalMorphWeightData.Reset();
	ExternalMorphSets.Reset();
	NumWeightedActiveMorphTargets = 0;
	ClothingSimData.Reset();
	ClothBlendWeight = 0.0f;
	bIsSkinCacheAllowed = false;
	bHasMeshDeformer = false;
	bForceUpdateDynamicDataImmediately = false;
#if RHI_RAYTRACING
	bAnySegmentUsesWorldPositionOffset = false;
#endif
	LocalToWorld = FMatrix::Identity;
}

#define SKELETON_POOL_GPUSKINS 1
#if SKELETON_POOL_GPUSKINS
TArray<FDynamicSkelMeshObjectDataGPUSkin*> FreeGpuSkins;
FCriticalSection FreeGpuSkinsCriticalSection;

static int32 GPoolGpuSkins = 1;
static int32 GMinPoolCount = 0;
static int32 GAllocationCounter = 0;
static const int32 GAllocationsBeforeCleanup = 1000; // number of allocations we make before we clean up the pool, this number is increased when we have to allocate not from the pool
static FAutoConsoleVariableRef CVarPoolGpuSkins(
	TEXT("r.GpuSkin.Pool"),
	GPoolGpuSkins,
	TEXT("Should we pool gpu skins.\n")
	TEXT(" 0: Don't pool anything\n")
	TEXT(" 1: Pool gpu skins bro (default)\n"),
	ECVF_Default
);
#endif


FDynamicSkelMeshObjectDataGPUSkin* FDynamicSkelMeshObjectDataGPUSkin::AllocDynamicSkelMeshObjectDataGPUSkin()
{
#if SKELETON_POOL_GPUSKINS
	if (!GPoolGpuSkins)
	{
		return new FDynamicSkelMeshObjectDataGPUSkin;
	}

	FScopeLock S(&FreeGpuSkinsCriticalSection);
	++GAllocationCounter;
	GMinPoolCount = FMath::Min(FreeGpuSkins.Num(), GMinPoolCount);
	if ( FreeGpuSkins.Num() > 0 )
	{
		FDynamicSkelMeshObjectDataGPUSkin *Result = FreeGpuSkins[0];
		FreeGpuSkins.RemoveAtSwap(0);
		return Result;
	}
	else
	{
		return new FDynamicSkelMeshObjectDataGPUSkin;
	}
#else
	return new FDynamicSkelMeshObjectDataGPUSkin;
#endif
}

void FDynamicSkelMeshObjectDataGPUSkin::FreeDynamicSkelMeshObjectDataGPUSkin(FDynamicSkelMeshObjectDataGPUSkin* Who)
{
#if SKELETON_POOL_GPUSKINS
	if (!GPoolGpuSkins)
	{
		delete Who;

		if ( FreeGpuSkins.Num() > 0 )
		{
			FScopeLock S(&FreeGpuSkinsCriticalSection);
			for ( FDynamicSkelMeshObjectDataGPUSkin* I : FreeGpuSkins )
			{
				delete I;
			}
			FreeGpuSkins.Empty();
		}
		return;
	}

	Who->Clear();
	FScopeLock S(&FreeGpuSkinsCriticalSection);
	FreeGpuSkins.Add(Who);
	if ( GAllocationCounter > GAllocationsBeforeCleanup )
	{
		GAllocationCounter = 0;
		for ( int32 I = 0; I < GMinPoolCount; ++I )
		{
			delete FreeGpuSkins[0];
			FreeGpuSkins.RemoveAtSwap(0);
		}
		GMinPoolCount = FreeGpuSkins.Num();
	}
#else
	delete Who;
#endif
}

void FDynamicSkelMeshObjectDataGPUSkin::InitDynamicSkelMeshObjectDataGPUSkin(
	USkinnedMeshComponent* InMeshComponent,
	FSkeletalMeshRenderData* InSkeletalMeshRenderData,
	FSkeletalMeshObjectGPUSkin* InMeshObject,
	int32 InLODIndex,
	const FMorphTargetWeightMap& InActiveMorphTargets,
	const TArray<float>& InMorphTargetWeights, 
	EPreviousBoneTransformUpdateMode PreviousBoneTransformUpdateMode,
	const FExternalMorphWeightData& InExternalMorphWeightData)
{
	LODIndex = InLODIndex;
	check(!ActiveMorphTargets.Num() && !ReferenceToLocal.Num() && !ClothingSimData.Num() && !MorphTargetWeights.Num());

	// append instead of equals to avoid alloc
	MorphTargetWeights.Append(InMorphTargetWeights);
	NumWeightedActiveMorphTargets = 0;

	ExternalMorphWeightData = InExternalMorphWeightData;
	ExternalMorphWeightData.UpdateNumActiveMorphTargets();
	ExternalMorphSets = InMeshComponent->GetExternalMorphSets(InLODIndex);

	// Gather any bones referenced by shadow shapes
	FSkeletalMeshSceneProxy* SkeletalMeshProxy = (FSkeletalMeshSceneProxy*)InMeshComponent->SceneProxy;
	const TArray<FBoneIndexType>* ExtraRequiredBoneIndices = SkeletalMeshProxy ? &SkeletalMeshProxy->GetSortedShadowBoneIndices() : nullptr;

#if RHI_RAYTRACING
	RayTracingLODIndex = FMath::Clamp(FMath::Max(LODIndex + GetRayTracingSkeletalMeshGlobalLODBias(), InMeshObject->RayTracingMinLOD), LODIndex, InSkeletalMeshRenderData->LODRenderData.Num() - 1);
#endif

	// update ReferenceToLocal
	UpdateRefToLocalMatrices( ReferenceToLocal, InMeshComponent, InSkeletalMeshRenderData, LODIndex, ExtraRequiredBoneIndices );
#if RHI_RAYTRACING
	if (RayTracingLODIndex != LODIndex)
	{
		UpdateRefToLocalMatrices(ReferenceToLocalForRayTracing, InMeshComponent, InSkeletalMeshRenderData, RayTracingLODIndex, ExtraRequiredBoneIndices);
	}
#endif
	switch(PreviousBoneTransformUpdateMode)
	{
	case EPreviousBoneTransformUpdateMode::None:
		// otherwise, clear it, it will use previous buffer
		PreviousReferenceToLocal.Reset();
		PreviousReferenceToLocalForRayTracing.Reset();
		break;
	case EPreviousBoneTransformUpdateMode::UpdatePrevious:
		UpdatePreviousRefToLocalMatrices(PreviousReferenceToLocal, InMeshComponent, InSkeletalMeshRenderData, LODIndex, ExtraRequiredBoneIndices);
	#if RHI_RAYTRACING
		if (RayTracingLODIndex != LODIndex)
		{
			UpdatePreviousRefToLocalMatrices(PreviousReferenceToLocalForRayTracing, InMeshComponent, InSkeletalMeshRenderData, RayTracingLODIndex, ExtraRequiredBoneIndices);
		}
	#endif
		break;
	case EPreviousBoneTransformUpdateMode::DuplicateCurrentToPrevious:
		UpdateRefToLocalMatrices(PreviousReferenceToLocal, InMeshComponent, InSkeletalMeshRenderData, LODIndex, ExtraRequiredBoneIndices);
	#if RHI_RAYTRACING
		if (RayTracingLODIndex != LODIndex)
		{
			UpdateRefToLocalMatrices(PreviousReferenceToLocalForRayTracing, InMeshComponent, InSkeletalMeshRenderData, RayTracingLODIndex, ExtraRequiredBoneIndices);
		}
	#endif
		break;
	}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	check(!MeshComponentSpaceTransforms.Num());
	// append instead of equals to avoid alloc
	MeshComponentSpaceTransforms.Append(InMeshComponent->GetComponentSpaceTransforms());
#endif
	SectionIdsUseByActiveMorphTargets.Reset();

	// If we have external morph targets, just include all sections.
	if (ExternalMorphWeightData.HasActiveMorphs())
	{
		const FSkeletalMeshLODRenderData& LOD = InSkeletalMeshRenderData->LODRenderData[LODIndex];
		SectionIdsUseByActiveMorphTargets.SetNumUninitialized(LOD.RenderSections.Num(), EAllowShrinking::No);
		for (int32 Index = 0; Index < LOD.RenderSections.Num(); ++Index)
		{
			SectionIdsUseByActiveMorphTargets[Index] = Index;
		}
	}

	const float MorphTargetMaxBlendWeight = UE::SkeletalRender::Settings::GetMorphTargetMaxBlendWeight();

	// find number of morphs that are currently weighted and will affect the mesh
	ActiveMorphTargets.Reserve(InActiveMorphTargets.Num());
	for(const TTuple<const UMorphTarget*, int32>& MorphItem: InActiveMorphTargets)
	{
		const UMorphTarget* MorphTarget = MorphItem.Key;
		const int32 WeightIndex = MorphItem.Value;
		const float MorphTargetWeight = MorphTargetWeights[WeightIndex];
		const float MorphAbsWeight = FMath::Abs(MorphTargetWeight);

		if( MorphTarget != nullptr && 
			MorphAbsWeight >= MinMorphTargetBlendWeight &&
			MorphAbsWeight <= MorphTargetMaxBlendWeight &&
			MorphTarget->HasDataForLOD(LODIndex) ) 
		{
			NumWeightedActiveMorphTargets++;
			const TArray<int32>& MorphSectionIndices = MorphTarget->GetMorphLODModels()[LODIndex].SectionIndices;
			for (int32 SecId = 0; SecId < MorphSectionIndices.Num(); ++SecId)
			{
				SectionIdsUseByActiveMorphTargets.AddUnique(MorphSectionIndices[SecId]);
			}

			ActiveMorphTargets.Add(MorphTarget, WeightIndex);
		}
	}

	// Update local to world transform
	LocalToWorld = InMeshComponent->GetComponentTransform().ToMatrixWithScale();

	// Update the clothing simulation mesh positions and normals
	InMeshComponent->GetUpdateClothSimulationData_AnyThread(ClothingSimData, ClothObjectLocalToWorld, ClothBlendWeight);

	if (!IsSkeletalMeshClothBlendEnabled())
	{
		ClothBlendWeight = 0.f;
	}

	bIsSkinCacheAllowed = InMeshComponent->IsSkinCacheAllowed(InLODIndex);
	bHasMeshDeformer = InMeshComponent->GetMeshDeformerInstanceForLOD(LODIndex) != nullptr;
	bForceUpdateDynamicDataImmediately = InMeshComponent->GetForceUpdateDynamicDataImmediately();
	// Force immediate update when using mesh deformer.
	bForceUpdateDynamicDataImmediately |= bHasMeshDeformer;

	if (bIsSkinCacheAllowed && InMeshObject->FeatureLevel == ERHIFeatureLevel::ES3_1)
	{
		// Some mobile GPUs (MALI) has a 64K elements limitation on texel buffers
		// SkinCache fetches mesh position through R32F texel buffer, thus any mesh that has more than 64K/3 vertices will not work correctly on such GPUs
		// We force this limitation for all mobile, to have an uniform behaviour accross all mobile platforms
		const FSkeletalMeshLODRenderData& LOD = InSkeletalMeshRenderData->LODRenderData[LODIndex];
		bIsSkinCacheAllowed = (LOD.GetNumVertices()*3 < 0xffff);
	}

#if RHI_RAYTRACING
	if (SkeletalMeshProxy != nullptr)
	{
		bAnySegmentUsesWorldPositionOffset = SkeletalMeshProxy->bAnySegmentUsesWorldPositionOffset;
	}
#endif
}

bool FDynamicSkelMeshObjectDataGPUSkin::ActiveMorphTargetsEqual(
	const FMorphTargetWeightMap& InCompareActiveMorphTargets,
	const TArray<float>& CompareMorphTargetWeights
	)
{
	if (InCompareActiveMorphTargets.Num() != ActiveMorphTargets.Num())
	{
		return false;
	}

	for(const TTuple<const UMorphTarget*, int32>& MorphItem: ActiveMorphTargets)
	{
		const UMorphTarget* MorphTarget = MorphItem.Key;
		const int32 WeightIndex = MorphItem.Value;
		const int32* CompareWeightIndex = InCompareActiveMorphTargets.Find(MorphTarget);
		if (CompareWeightIndex == nullptr)
		{
			return false;
		}

		if( FMath::Abs(MorphTargetWeights[WeightIndex] - CompareMorphTargetWeights[*CompareWeightIndex]) >= GMorphTargetWeightThreshold)
		{
			return false;
		}
	}
	return true;
}

bool FDynamicSkelMeshObjectDataGPUSkin::UpdateClothSimulationData(USkinnedMeshComponent* InMeshComponent)
{
	USkeletalMeshComponent* SimMeshComponent = Cast<USkeletalMeshComponent>(InMeshComponent);

	if (InMeshComponent->LeaderPoseComponent.IsValid() && (SimMeshComponent && SimMeshComponent->IsClothBoundToLeaderComponent()))
	{
		USkeletalMeshComponent* SrcComponent = SimMeshComponent;
		// if I have leader, override sim component
		SimMeshComponent = Cast<USkeletalMeshComponent>(InMeshComponent->LeaderPoseComponent.Get());
		// IF we don't have sim component that is skeletalmeshcomponent, just ignore
		if (!SimMeshComponent)
		{
			return false;
		}

		ClothObjectLocalToWorld = SrcComponent->GetComponentToWorld().ToMatrixWithScale();
		ClothBlendWeight = IsSkeletalMeshClothBlendEnabled() ? SimMeshComponent->ClothBlendWeight : 0.0f;
		ClothingSimData = SimMeshComponent->GetCurrentClothingData_AnyThread();
		return true;
	}

	if (SimMeshComponent)
	{
		ClothObjectLocalToWorld = SimMeshComponent->GetComponentToWorld().ToMatrixWithScale();
		if(SimMeshComponent->bDisableClothSimulation)
		{
			ClothBlendWeight = 0.0f;
			ClothingSimData.Reset();
		}
		else
		{
			ClothBlendWeight = IsSkeletalMeshClothBlendEnabled() ? SimMeshComponent->ClothBlendWeight : 0.0f;
			ClothingSimData = SimMeshComponent->GetCurrentClothingData_AnyThread();
		}

		return true;
	}
	return false;
}
