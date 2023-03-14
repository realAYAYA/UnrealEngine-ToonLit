// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "HAL/IConsoleManager.h"
#include "Async/ParallelFor.h"
#include "RHI.h"
#include "RenderResource.h"
#include "ShaderParameters.h"
#include "RendererInterface.h"
#include "Shader.h"
#include "SceneUtils.h"
#include "GlobalShader.h"
#include "DeferredShadingRenderer.h"
#include "ScenePrivate.h"
#include "DistanceFieldLightingShared.h"
#include "DistanceFieldAmbientOcclusion.h"
#include "ComponentRecreateRenderStateContext.h"
#include "GlobalDistanceField.h"
#include "HAL/LowLevelMemStats.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

DECLARE_GPU_STAT(DistanceFields);

extern int32 GDistanceFieldOffsetDataStructure;

float GMeshDistanceFieldsMaxObjectBoundingRadius = 100000;
FAutoConsoleVariableRef CVarMeshDistanceFieldsMaxObjectBoundingRadius(
	TEXT("r.DistanceFields.MaxObjectBoundingRadius"),
	GMeshDistanceFieldsMaxObjectBoundingRadius,
	TEXT("Objects larger than this will not be included in the Mesh Distance Field scene, to improve performance."),
	ECVF_RenderThreadSafe
	);

int32 GDFParallelUpdate = 0;
FAutoConsoleVariableRef CVarDFParallelUpdate(
	TEXT("r.DistanceFields.ParallelUpdate"),
	GDFParallelUpdate,
	TEXT(""),
	ECVF_RenderThreadSafe
);

int32 GDFReverseAtlasAllocationOrder = 0;
FAutoConsoleVariableRef CVarDFReverseAtlasAllocationOrder(
	TEXT("r.DistanceFields.ReverseAtlasAllocationOrder"),
	GDFReverseAtlasAllocationOrder,
	TEXT(""),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		FGlobalComponentRecreateRenderStateContext Context;
	}),
	ECVF_RenderThreadSafe
);

float GMeshSDFSurfaceBiasExpand = .25f;
FAutoConsoleVariableRef CVarMeshSDFSurfaceBiasExpand(
	TEXT("r.DistanceFields.SurfaceBiasExpand"),
	GMeshSDFSurfaceBiasExpand,
	TEXT("Fraction of a Mesh SDF voxel to expand the surface during intersection.  Expanding the surface improves representation quality, at the cost of over-occlusion."),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* InVariable)
	{
		FGlobalComponentRecreateRenderStateContext Context;
	}),
	ECVF_RenderThreadSafe
);

int32 GDFPreviousReverseAtlasAllocationOrder = 0;

#if ENABLE_LOW_LEVEL_MEM_TRACKER
DECLARE_LLM_MEMORY_STAT(TEXT("DistanceFields"), STAT_DistanceFieldsLLM, STATGROUP_LLMFULL);
DECLARE_LLM_MEMORY_STAT(TEXT("DistanceFields"), STAT_DistanceFieldsSummaryLLM, STATGROUP_LLM);
LLM_DEFINE_TAG(DistanceFields, NAME_None, NAME_None, GET_STATFNAME(STAT_DistanceFieldsLLM), GET_STATFNAME(STAT_DistanceFieldsSummaryLLM));
#endif // ENABLE_LOW_LEVEL_MEM_TRACKER

// Must match equivalent shader defines
template<> int32 TDistanceFieldObjectBuffers<DFPT_SignedDistanceField>::ObjectDataStride = GDistanceFieldObjectDataStride;
template<> int32 TDistanceFieldObjectBuffers<DFPT_SignedDistanceField>::ObjectBoundsStride = GDistanceFieldObjectBoundsStride;
template<> int32 TDistanceFieldObjectBuffers<DFPT_HeightField>::ObjectDataStride = GHeightFieldObjectDataStride;
template<> int32 TDistanceFieldObjectBuffers<DFPT_HeightField>::ObjectBoundsStride = GHeightFieldObjectBoundsStride;

template <EDistanceFieldPrimitiveType PrimitiveType>
void TDistanceFieldObjectBuffers<PrimitiveType>::Initialize()
{
}

FDistanceFieldObjectBufferParameters DistanceField::SetupObjectBufferParameters(FRDGBuilder& GraphBuilder, const FDistanceFieldSceneData& DistanceFieldSceneData)
{
	FDistanceFieldObjectBufferParameters ObjectBufferParameters;

	ObjectBufferParameters.NumSceneObjects = DistanceFieldSceneData.NumObjectsInBuffer;
	ObjectBufferParameters.NumSceneHeightfieldObjects = DistanceFieldSceneData.NumHeightFieldObjectsInBuffer;

	if (DistanceFieldSceneData.NumObjectsInBuffer > 0)
	{
		check(DistanceFieldSceneData.GetCurrentObjectBuffers());
		ObjectBufferParameters.SceneObjectBounds = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalBuffer(DistanceFieldSceneData.GetCurrentObjectBuffers()->Bounds));
		ObjectBufferParameters.SceneObjectData = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalBuffer(DistanceFieldSceneData.GetCurrentObjectBuffers()->Data));
	}
	else
	{
		ObjectBufferParameters.SceneObjectBounds = GraphBuilder.CreateSRV(GSystemTextures.GetDefaultStructuredBuffer(GraphBuilder, sizeof(FVector4f)));
		ObjectBufferParameters.SceneObjectData = GraphBuilder.CreateSRV(GSystemTextures.GetDefaultStructuredBuffer(GraphBuilder, sizeof(FVector4f)));
	}

	if (DistanceFieldSceneData.NumHeightFieldObjectsInBuffer > 0)
	{
		check(DistanceFieldSceneData.GetHeightFieldObjectBuffers());
		ObjectBufferParameters.SceneHeightfieldObjectBounds = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalBuffer(DistanceFieldSceneData.GetHeightFieldObjectBuffers()->Bounds));
		ObjectBufferParameters.SceneHeightfieldObjectData = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalBuffer(DistanceFieldSceneData.GetHeightFieldObjectBuffers()->Data));
	}
	else
	{
		ObjectBufferParameters.SceneHeightfieldObjectBounds = GraphBuilder.CreateSRV(GSystemTextures.GetDefaultStructuredBuffer(GraphBuilder, sizeof(FVector4f)));
		ObjectBufferParameters.SceneHeightfieldObjectData = GraphBuilder.CreateSRV(GSystemTextures.GetDefaultStructuredBuffer(GraphBuilder, sizeof(FVector4f)));
	}

	return ObjectBufferParameters;
}

FDistanceFieldAtlasParameters DistanceField::SetupAtlasParameters(FRDGBuilder& GraphBuilder, const FDistanceFieldSceneData& DistanceFieldSceneData)
{
	FDistanceFieldAtlasParameters SceneParameters;

	if (DistanceFieldSceneData.DistanceFieldBrickVolumeTexture)
	{
		SceneParameters.SceneDistanceFieldAssetData = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalBuffer(DistanceFieldSceneData.AssetDataBuffer));

		if (GDistanceFieldOffsetDataStructure == 0 || GDistanceFieldOffsetDataStructure == 1)
		{
			if (DistanceFieldSceneData.IndirectionTable)
			{
				FRDGBufferSRV* IndirectTableSRV = GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalBuffer(DistanceFieldSceneData.IndirectionTable));

				if (GDistanceFieldOffsetDataStructure == 0)
				{
					SceneParameters.DistanceFieldIndirectionTable = IndirectTableSRV;
				}
				else
				{
					SceneParameters.DistanceFieldIndirection2Table = IndirectTableSRV;
				}
			}
		}
		else if (DistanceFieldSceneData.IndirectionAtlas)
		{
			SceneParameters.DistanceFieldIndirectionAtlas = GraphBuilder.RegisterExternalTexture(DistanceFieldSceneData.IndirectionAtlas);
		}

		SceneParameters.DistanceFieldBrickTexture = GraphBuilder.RegisterExternalTexture(DistanceFieldSceneData.DistanceFieldBrickVolumeTexture);
		SceneParameters.DistanceFieldSampler = TStaticSamplerState<SF_Bilinear,AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();

		SceneParameters.DistanceFieldBrickSize = FVector3f(DistanceField::BrickSize);
		SceneParameters.DistanceFieldUniqueDataBrickSize = FVector3f(DistanceField::UniqueDataBrickSize);
		SceneParameters.DistanceFieldBrickAtlasSizeInBricks = DistanceFieldSceneData.BrickTextureDimensionsInBricks;
		SceneParameters.DistanceFieldBrickAtlasMask = DistanceFieldSceneData.BrickTextureDimensionsInBricks - FIntVector(1);
		SceneParameters.DistanceFieldBrickAtlasSizeLog2 = FIntVector(
			FMath::FloorLog2(DistanceFieldSceneData.BrickTextureDimensionsInBricks.X),
			FMath::FloorLog2(DistanceFieldSceneData.BrickTextureDimensionsInBricks.Y),
			FMath::FloorLog2(DistanceFieldSceneData.BrickTextureDimensionsInBricks.Z));
		SceneParameters.DistanceFieldBrickAtlasTexelSize = FVector3f(1.0f) / FVector3f(DistanceFieldSceneData.BrickTextureDimensionsInBricks * DistanceField::BrickSize);

		SceneParameters.DistanceFieldBrickAtlasHalfTexelSize = 0.5f * SceneParameters.DistanceFieldBrickAtlasTexelSize;
		SceneParameters.DistanceFieldUniqueDataBrickSizeInAtlasTexels = SceneParameters.DistanceFieldUniqueDataBrickSize * SceneParameters.DistanceFieldBrickAtlasTexelSize;

		SceneParameters.DistanceFieldBrickOffsetToAtlasUVScale = GDistanceFieldOffsetDataStructure == 0 ?
			SceneParameters.DistanceFieldBrickSize * SceneParameters.DistanceFieldBrickAtlasTexelSize :
			SceneParameters.DistanceFieldBrickSize * SceneParameters.DistanceFieldBrickAtlasTexelSize * (DistanceField::MaxIndirectionDimension - 1);
	}
	else
	{
		SceneParameters.SceneDistanceFieldAssetData = nullptr;
		SceneParameters.DistanceFieldIndirectionTable = nullptr;
		SceneParameters.DistanceFieldIndirection2Table = nullptr;
		SceneParameters.DistanceFieldIndirectionAtlas = nullptr;
		SceneParameters.DistanceFieldBrickTexture = nullptr;
		SceneParameters.DistanceFieldSampler = nullptr;
	}

	return SceneParameters;
}

const uint32 UpdateObjectsGroupSize = 64;

struct FParallelUpdateRangeDFO
{
	int32 ItemStart;
	int32 ItemCount;
};

struct FParallelUpdateRangesDFO
{
	FParallelUpdateRangeDFO Range[4];
};

// TODO: Improve and move to shared utility location.
static int32 PartitionUpdateRangesDFO(FParallelUpdateRangesDFO& Ranges, int32 ItemCount, bool bAllowParallel)
{
	if (ItemCount < 256 || !bAllowParallel)
	{
		Ranges.Range[0].ItemStart = 0;
		Ranges.Range[0].ItemCount = ItemCount;
		return 1;
	}

	const int32 RangeCount = Align(ItemCount, 4) >> 2;

	Ranges.Range[0].ItemCount = RangeCount;
	Ranges.Range[1].ItemCount = RangeCount;
	Ranges.Range[2].ItemCount = RangeCount;

	Ranges.Range[0].ItemStart = 0;
	Ranges.Range[1].ItemStart = RangeCount;
	Ranges.Range[2].ItemStart = RangeCount * 2;
	Ranges.Range[3].ItemStart = RangeCount * 3;
	Ranges.Range[3].ItemCount = ItemCount - Ranges.Range[3].ItemStart;

	return Ranges.Range[3].ItemCount > 0 ? 4 : 3;
}

void AddModifiedBounds(FScene* Scene, FGlobalDFCacheType CacheType, const FBox& Bounds)
{
	FDistanceFieldSceneData& DistanceFieldData = Scene->DistanceFieldSceneData;
	DistanceFieldData.PrimitiveModifiedBounds[CacheType].Add(Bounds);
}

void UpdateGlobalDistanceFieldObjectRemoves(FScene* Scene, TArray<FSetElementId>& DistanceFieldAssetRemoves)
{
	FDistanceFieldSceneData& DistanceFieldSceneData = Scene->DistanceFieldSceneData;

	if (DistanceFieldSceneData.PendingRemoveOperations.Num() > 0)
	{
		TArray<int32, SceneRenderingAllocator> PendingRemoveOperations;

		for (int32 RemoveIndex = 0; RemoveIndex < DistanceFieldSceneData.PendingRemoveOperations.Num(); RemoveIndex++)
		{
			const FPrimitiveRemoveInfo& PrimitiveRemoveInfo = DistanceFieldSceneData.PendingRemoveOperations[RemoveIndex];
			
			FSetElementId AssetSetId = DistanceFieldSceneData.AssetStateArray.FindId(PrimitiveRemoveInfo.DistanceFieldData);
			FDistanceFieldAssetState& AssetState = DistanceFieldSceneData.AssetStateArray[AssetSetId];
			AssetState.RefCount--;
			
			if (AssetState.RefCount == 0)
			{
				DistanceFieldAssetRemoves.Add(AssetSetId);
			}

			// Can't dereference the primitive here, it has already been deleted
			const FPrimitiveSceneInfo* Primitive = PrimitiveRemoveInfo.Primitive;
			const TArray<int32, TInlineAllocator<1>>& DistanceFieldInstanceIndices = PrimitiveRemoveInfo.DistanceFieldInstanceIndices;

			for (int32 RemoveInstanceIndex = 0; RemoveInstanceIndex < DistanceFieldInstanceIndices.Num(); RemoveInstanceIndex++)
			{
				const int32 InstanceIndex = DistanceFieldInstanceIndices[RemoveInstanceIndex];

				// InstanceIndex will be -1 with zero scale meshes
				if (InstanceIndex >= 0)
				{
					FGlobalDFCacheType CacheType = PrimitiveRemoveInfo.bOftenMoving ? GDF_Full : GDF_MostlyStatic;
					AddModifiedBounds(Scene, CacheType, DistanceFieldSceneData.PrimitiveInstanceMapping[InstanceIndex].WorldBounds);
					PendingRemoveOperations.Add(InstanceIndex);
				}
			}
		}

		DistanceFieldSceneData.PendingRemoveOperations.Reset();

		if (PendingRemoveOperations.Num() > 0)
		{
			check(DistanceFieldSceneData.NumObjectsInBuffer >= PendingRemoveOperations.Num());

			// Sort from largest to smallest so we can safely RemoveAtSwap without invalidating indices in this array
			PendingRemoveOperations.Sort(TGreater<int32>());

			for (int32 RemoveIndex : PendingRemoveOperations)
			{
				--DistanceFieldSceneData.NumObjectsInBuffer;
				const int32 MoveFromIndex = DistanceFieldSceneData.NumObjectsInBuffer;

				FPrimitiveAndInstance& PrimitiveAndInstanceBeingMoved = DistanceFieldSceneData.PrimitiveInstanceMapping[MoveFromIndex];
				if (RemoveIndex < DistanceFieldSceneData.NumObjectsInBuffer)
				{
					// Fixup indices of the primitive that is being moved
					check(PrimitiveAndInstanceBeingMoved.Primitive && PrimitiveAndInstanceBeingMoved.Primitive->DistanceFieldInstanceIndices.Num() > 0);
					PrimitiveAndInstanceBeingMoved.Primitive->DistanceFieldInstanceIndices[PrimitiveAndInstanceBeingMoved.InstanceIndex] = RemoveIndex;
				}

				DistanceFieldSceneData.PrimitiveInstanceMapping.RemoveAtSwap(RemoveIndex, 1, false);

				DistanceFieldSceneData.IndicesToUpdateInObjectBuffers.Add(RemoveIndex);
			}

			PendingRemoveOperations.Reset();
		}
	}
}

void UpdateGlobalHeightFieldObjectRemoves(FScene* Scene)
{
	FDistanceFieldSceneData& SceneData = Scene->DistanceFieldSceneData;

	if (SceneData.PendingHeightFieldRemoveOps.Num())
	{
		TArray<int32, SceneRenderingAllocator> PendingRemoveObjectIndices;

		for (int32 Idx = 0; Idx < SceneData.PendingHeightFieldRemoveOps.Num(); ++Idx)
		{
			const FHeightFieldPrimitiveRemoveInfo& RemoveInfo = SceneData.PendingHeightFieldRemoveOps[Idx];
			check(RemoveInfo.DistanceFieldInstanceIndices.Num() == 1);
			const int32 ObjectIdx = RemoveInfo.DistanceFieldInstanceIndices[0];

			if (ObjectIdx >= 0)
			{
				const FGlobalDFCacheType CacheType = RemoveInfo.bOftenMoving ? GDF_Full : GDF_MostlyStatic;
				AddModifiedBounds(Scene, CacheType, RemoveInfo.WorldBounds);
				PendingRemoveObjectIndices.Add(ObjectIdx);
			}
		}

		SceneData.PendingHeightFieldRemoveOps.Reset();

		if (PendingRemoveObjectIndices.Num())
		{
			check(SceneData.NumHeightFieldObjectsInBuffer >= PendingRemoveObjectIndices.Num());
			check(SceneData.NumHeightFieldObjectsInBuffer == SceneData.HeightfieldPrimitives.Num());

			// Sort from largest to smallest so we can safely RemoveAtSwap without invalidating indices in this array
			PendingRemoveObjectIndices.Sort(TGreater<int32>());

			// Next RemoveAtSwap
			for (int32 RemoveIndex : PendingRemoveObjectIndices)
			{
				--SceneData.NumHeightFieldObjectsInBuffer;
				const int32 MoveFromIndex = SceneData.NumHeightFieldObjectsInBuffer;

				FPrimitiveSceneInfo* PrimitiveBeingMoved = SceneData.HeightfieldPrimitives[MoveFromIndex];
				if (RemoveIndex < SceneData.NumHeightFieldObjectsInBuffer)
				{
					// Fixup indices of the primitive that is being moved
					check(PrimitiveBeingMoved && PrimitiveBeingMoved->DistanceFieldInstanceIndices.Num() == 1);
					PrimitiveBeingMoved->DistanceFieldInstanceIndices[0] = RemoveIndex;
				}

				SceneData.HeightfieldPrimitives.RemoveAtSwap(RemoveIndex, 1, false);

				SceneData.IndicesToUpdateInHeightFieldObjectBuffers.Add(RemoveIndex);
			}

			PendingRemoveObjectIndices.Reset();
		}
	}
}

void LogDistanceFieldUpdate(FPrimitiveSceneInfo const* PrimitiveSceneInfo, float BoundingRadius, bool bIsAddOperation)
{
	extern int32 GAOLogGlobalDistanceFieldModifiedPrimitives;

	if (GAOLogGlobalDistanceFieldModifiedPrimitives == 1
		|| (GAOLogGlobalDistanceFieldModifiedPrimitives == 2 && !PrimitiveSceneInfo->Proxy->IsOftenMoving()))
	{
		UE_LOG(LogDistanceField, Log,
			TEXT("Global Distance Field %s primitive %s %s %s bounding radius %.1f"),
			PrimitiveSceneInfo->Proxy->IsOftenMoving() ? TEXT("Movable") : TEXT("CACHED"),
			(bIsAddOperation ? TEXT("add") : TEXT("update")),
			*PrimitiveSceneInfo->Proxy->GetOwnerName().ToString(),
			*PrimitiveSceneInfo->Proxy->GetResourceName().ToString(),
			BoundingRadius);
	}
}

/** Gathers the information needed to represent a single object's distance field and appends it to the upload buffers. */
bool ProcessPrimitiveUpdate(
	bool bIsAddOperation,
	FScene* Scene,
	FPrimitiveSceneInfo* PrimitiveSceneInfo,
	TArray<FRenderTransform>& InstanceLocalToPrimitiveTransforms,
	TArray<int32>& IndicesToUpdateInObjectBuffers, 
	TArray<FDistanceFieldAssetMipId>& DistanceFieldAssetAdds,
	TArray<FSetElementId>& DistanceFieldAssetRemoves)
{
	const FPrimitiveSceneProxy* Proxy = PrimitiveSceneInfo->Proxy;

	FDistanceFieldSceneData& DistanceFieldSceneData = Scene->DistanceFieldSceneData;

	InstanceLocalToPrimitiveTransforms.Reset();

	const FDistanceFieldVolumeData* DistanceFieldData = nullptr;
	float SelfShadowBias;
	Proxy->GetDistanceFieldAtlasData(DistanceFieldData, SelfShadowBias);
	Proxy->GetDistanceFieldInstanceData(InstanceLocalToPrimitiveTransforms);

	if (DistanceFieldData && DistanceFieldData->Mips[0].IndirectionDimensions.GetMax() > 0 && InstanceLocalToPrimitiveTransforms.Num() > 0)
	{
		const float BoundingRadius = Proxy->GetBounds().SphereRadius;
		const FGlobalDFCacheType CacheType = Proxy->IsOftenMoving() ? GDF_Full : GDF_MostlyStatic;

		// Proxy bounds are only useful if single instance
		if (InstanceLocalToPrimitiveTransforms.Num() > 1 || BoundingRadius < GMeshDistanceFieldsMaxObjectBoundingRadius)
		{
			if (bIsAddOperation)
			{
				PrimitiveSceneInfo->DistanceFieldInstanceIndices.Empty(InstanceLocalToPrimitiveTransforms.Num());
				PrimitiveSceneInfo->DistanceFieldInstanceIndices.AddZeroed(InstanceLocalToPrimitiveTransforms.Num());

				FSetElementId AddSetId = DistanceFieldSceneData.AssetStateArray.FindId(DistanceFieldData);

				if (AddSetId.IsValidId())
				{
					FDistanceFieldAssetState& AssetState = DistanceFieldSceneData.AssetStateArray[AddSetId];
					AssetState.RefCount++;

					if (AssetState.RefCount == 1)
					{
						DistanceFieldAssetRemoves.Remove(AddSetId);
					}
				}
				else
				{
					FDistanceFieldAssetState NewAssetState;
					NewAssetState.RefCount = 1;
					NewAssetState.BuiltData = DistanceFieldData;
					FSetElementId AssetSetId = DistanceFieldSceneData.AssetStateArray.Add(NewAssetState);
					DistanceFieldAssetAdds.Add(FDistanceFieldAssetMipId(AssetSetId));
				}
			}

			for (int32 TransformIndex = 0; TransformIndex < InstanceLocalToPrimitiveTransforms.Num(); TransformIndex++)
			{
				const FMatrix LocalToWorld = InstanceLocalToPrimitiveTransforms[TransformIndex].ToMatrix() * Proxy->GetLocalToWorld();

				const FMatrix::FReal MaxScale = LocalToWorld.GetMaximumAxisScale();

				// Skip degenerate primitives
				if (MaxScale > 0)
				{
					uint32 UploadIndex;

					if (bIsAddOperation)
					{
						UploadIndex = DistanceFieldSceneData.NumObjectsInBuffer;
						++DistanceFieldSceneData.NumObjectsInBuffer;
					}
					else
					{
						UploadIndex = PrimitiveSceneInfo->DistanceFieldInstanceIndices[TransformIndex];
					}

					IndicesToUpdateInObjectBuffers.Add(UploadIndex);

					const FBox WorldBounds = DistanceFieldData->LocalSpaceMeshBounds.TransformBy(LocalToWorld);

					if (bIsAddOperation)
					{
						const int32 AddIndex = UploadIndex;
						const int32 MappingIndex = DistanceFieldSceneData.PrimitiveInstanceMapping.Add(FPrimitiveAndInstance(LocalToWorld, WorldBounds, PrimitiveSceneInfo, TransformIndex));
						PrimitiveSceneInfo->DistanceFieldInstanceIndices[TransformIndex] = AddIndex;

						AddModifiedBounds(Scene, CacheType, WorldBounds);
						LogDistanceFieldUpdate(PrimitiveSceneInfo, BoundingRadius, bIsAddOperation);
					}
					else 
					{
						// InstanceIndex will be -1 with zero scale meshes
						const int32 InstanceIndex = PrimitiveSceneInfo->DistanceFieldInstanceIndices[TransformIndex];
						if (InstanceIndex >= 0)
						{
							FPrimitiveAndInstance& Mapping = DistanceFieldSceneData.PrimitiveInstanceMapping[InstanceIndex];

							// Filter out global distance field updates which were too small
							if (!Mapping.WorldBounds.GetExtent().Equals(WorldBounds.GetExtent(), 0.01f)
								|| !Mapping.LocalToWorld.Equals(LocalToWorld, 0.01f))
							{
								// decide if we want to make a single global distance field update or two updates for large movement (teleport) case
								const FBox MergedBounds = Mapping.WorldBounds + WorldBounds;
								const FVector MergedExtentIncrease = MergedBounds.GetExtent() - Mapping.WorldBounds.GetExtent() - WorldBounds.GetExtent();
								if (MergedExtentIncrease.GetMax() < 100.0f)
								{
									AddModifiedBounds(Scene, CacheType, MergedBounds);
								}
								else
								{
									AddModifiedBounds(Scene, CacheType, Mapping.WorldBounds);
									AddModifiedBounds(Scene, CacheType, WorldBounds);
								}
								LogDistanceFieldUpdate(PrimitiveSceneInfo, BoundingRadius, bIsAddOperation);

								Mapping.LocalToWorld = LocalToWorld;
								Mapping.WorldBounds = WorldBounds;
							}
						}
					}
				}
				else if (bIsAddOperation)
				{
					// Set to -1 for zero scale meshes
					PrimitiveSceneInfo->DistanceFieldInstanceIndices[TransformIndex] = -1;
				}
			}
		}
		else
		{
			UE_LOG(LogDistanceField,Verbose,TEXT("Primitive %s %s excluded due to huge bounding radius %f"), *Proxy->GetOwnerName().ToString(), *Proxy->GetResourceName().ToString(), BoundingRadius);
		}
	}
	return true;
}

bool ProcessHeightFieldPrimitiveUpdate(
	bool bIsAddOperation,
	FScene* Scene,
	FPrimitiveSceneInfo* PrimitiveSceneInfo,
	TArray<int32>& IndicesToUpdateInObjectBuffers)
{
	FDistanceFieldSceneData& SceneData = Scene->DistanceFieldSceneData;

	UTexture2D* HeightNormalTexture;
	UTexture2D* DiffuseColorTexture;
	UTexture2D* VisibilityTexture;
	FHeightfieldComponentDescription HeightFieldCompDesc(PrimitiveSceneInfo->Proxy->GetLocalToWorld(), PrimitiveSceneInfo->GetInstanceSceneDataOffset());
	PrimitiveSceneInfo->Proxy->GetHeightfieldRepresentation(HeightNormalTexture, DiffuseColorTexture, VisibilityTexture, HeightFieldCompDesc);

	const uint32 Handle = GHeightFieldTextureAtlas.GetAllocationHandle(HeightNormalTexture);
	if (Handle == INDEX_NONE)
	{
		return false;
	}

	uint32 UploadIdx;
	if (bIsAddOperation)
	{
		UploadIdx = SceneData.NumHeightFieldObjectsInBuffer;
		++SceneData.NumHeightFieldObjectsInBuffer;
		SceneData.HeightfieldPrimitives.Add(PrimitiveSceneInfo);

		const FGlobalDFCacheType CacheType = PrimitiveSceneInfo->Proxy->IsOftenMoving() ? GDF_Full : GDF_MostlyStatic;
		const FBoxSphereBounds Bounds = PrimitiveSceneInfo->Proxy->GetBounds();
		AddModifiedBounds(Scene, CacheType, Bounds.GetBox());

		PrimitiveSceneInfo->DistanceFieldInstanceIndices.Empty(1);
		PrimitiveSceneInfo->DistanceFieldInstanceIndices.Add(UploadIdx);
	}
	else
	{
		UploadIdx = PrimitiveSceneInfo->DistanceFieldInstanceIndices[0];
	}

	IndicesToUpdateInObjectBuffers.Add(UploadIdx);

	return true;
}

bool bVerifySceneIntegrity = false;

void FDistanceFieldSceneData::UpdateDistanceFieldObjectBuffers(
	FRDGBuilder& GraphBuilder,
	FRDGExternalAccessQueue& ExternalAccessQueue,
	FScene* Scene,
	TArray<FDistanceFieldAssetMipId>& DistanceFieldAssetAdds,
	TArray<FSetElementId>& DistanceFieldAssetRemoves)
{
	// Mask should be set in FSceneRenderer::PrepareDistanceFieldScene before calling this
	check(GraphBuilder.RHICmdList.GetGPUMask() == FRHIGPUMask::All());

	const bool bExecuteInParallel = GDFParallelUpdate != 0 && FApp::ShouldUseThreadingForPerformance();

	if (HasPendingOperations() || PendingThrottledOperations.Num() > 0)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_UpdateDistanceFieldObjectBuffers);
		RDG_EVENT_SCOPE(GraphBuilder, "UpdateDistanceFieldObjectBuffers");

		if (ObjectBuffers == nullptr)
		{
			ObjectBuffers = new FDistanceFieldObjectBuffers();
		}

		if (PendingAddOperations.Num() > 0)
		{
			PendingThrottledOperations.Reserve(PendingThrottledOperations.Num() + PendingAddOperations.Num());
		}

		PendingAddOperations.Append(PendingThrottledOperations);
		PendingThrottledOperations.Reset();

		// Process removes before adds, as the adds will overwrite primitive allocation info
		// This also prevents re-uploading distance fields on render state recreation
		UpdateGlobalDistanceFieldObjectRemoves(Scene, DistanceFieldAssetRemoves);

		if ((PendingAddOperations.Num() > 0 || PendingUpdateOperations.Num() > 0) && GDFReverseAtlasAllocationOrder == GDFPreviousReverseAtlasAllocationOrder)
		{
			TArray<FRenderTransform> InstanceLocalToPrimitiveTransforms;

			int32 OriginalNumObjects = NumObjectsInBuffer;
			for (FPrimitiveSceneInfo* PrimitiveSceneInfo : PendingAddOperations)
			{
				if (!ProcessPrimitiveUpdate(
					true,
					Scene,
					PrimitiveSceneInfo,
					InstanceLocalToPrimitiveTransforms,
					IndicesToUpdateInObjectBuffers,
					DistanceFieldAssetAdds,
					DistanceFieldAssetRemoves))
				{
					PendingThrottledOperations.Add(PrimitiveSceneInfo);
				}
			}

			for (FPrimitiveSceneInfo* PrimitiveSceneInfo : PendingUpdateOperations)
			{
				ProcessPrimitiveUpdate(
					false,
					Scene,
					PrimitiveSceneInfo,
					InstanceLocalToPrimitiveTransforms,
					IndicesToUpdateInObjectBuffers,
					DistanceFieldAssetAdds,
					DistanceFieldAssetRemoves);
			}

			PendingAddOperations.Reset();
			PendingUpdateOperations.Reset();
			if (PendingThrottledOperations.Num() == 0)
			{
				PendingThrottledOperations.Reset();
			}
		}

		GDFPreviousReverseAtlasAllocationOrder = GDFReverseAtlasAllocationOrder;

		// Upload buffer changes
		if (IndicesToUpdateInObjectBuffers.Num() > 0)
		{
			QUICK_SCOPE_CYCLE_COUNTER(UpdateDFObjectBuffers);

			// Upload DF object data and bounds
			{
				const uint32 NumDFObjects = NumObjectsInBuffer;

				const uint32 DFObjectDataNumFloat4s = FMath::RoundUpToPowerOfTwo(NumDFObjects * FDistanceFieldObjectBuffers::ObjectDataStride);
				const uint32 DFObjectDataNumBytes = DFObjectDataNumFloat4s * sizeof(FVector4f);
				FRDGBuffer* DFObjectDataBuffer = ResizeStructuredBufferIfNeeded(GraphBuilder, ObjectBuffers->Data, DFObjectDataNumBytes, TEXT("DistanceFields.DFObjectData"));

				const uint32 DFObjectBoundsNumFloat4s = FMath::RoundUpToPowerOfTwo(NumDFObjects * FDistanceFieldObjectBuffers::ObjectBoundsStride);
				const uint32 DFObjectBoundsNumBytes = DFObjectBoundsNumFloat4s * sizeof(FVector4f);
				FRDGBuffer* DFObjectBoundsBuffer = ResizeStructuredBufferIfNeeded(GraphBuilder, ObjectBuffers->Bounds, DFObjectBoundsNumBytes, TEXT("DistanceFields.DFObjectBounds"));

				const int32 NumDFObjectUploads = IndicesToUpdateInObjectBuffers.Num();

				static FCriticalSection DFUpdateCS;

				if (NumDFObjectUploads > 0)
				{
					UploadDistanceFieldDataBuffer.Init(GraphBuilder, NumDFObjectUploads, FDistanceFieldObjectBuffers::ObjectDataStride * sizeof(FVector4f), true, TEXT("DistanceFields.DFObjectDataUploadBuffer"));
					UploadDistanceFieldBoundsBuffer.Init(GraphBuilder, NumDFObjectUploads, FDistanceFieldObjectBuffers::ObjectBoundsStride * sizeof(FVector4f), true, TEXT("DistanceFields.DFObjectBoundsUploadBuffer"));

					const TArray<FPrimitiveBounds>& PrimitiveBounds = Scene->PrimitiveBounds;

					FParallelUpdateRangesDFO ParallelRanges;

					int32 RangeCount = PartitionUpdateRangesDFO(ParallelRanges, IndicesToUpdateInObjectBuffers.Num(), bExecuteInParallel);

					ParallelFor(RangeCount,
						[this, &ParallelRanges, &PrimitiveBounds, RangeCount](int32 RangeIndex)
						{
							for (int32 ItemIndex = ParallelRanges.Range[RangeIndex].ItemStart; ItemIndex < ParallelRanges.Range[RangeIndex].ItemStart + ParallelRanges.Range[RangeIndex].ItemCount; ++ItemIndex)
							{
								const int32 Index = IndicesToUpdateInObjectBuffers[ItemIndex];
								if (Index >= 0 && Index < PrimitiveInstanceMapping.Num())
								{
									const FPrimitiveAndInstance& PrimAndInst = PrimitiveInstanceMapping[Index];
									const FPrimitiveSceneProxy* PrimitiveSceneProxy = PrimAndInst.Primitive->Proxy;

									if (RangeCount > 1)
									{
										DFUpdateCS.Lock();
									}

									FVector4f* UploadObjectData = (FVector4f*)UploadDistanceFieldDataBuffer.Add_GetRef(Index);
									FVector4f* UploadObjectBounds = (FVector4f*)UploadDistanceFieldBoundsBuffer.Add_GetRef(Index);

									if (RangeCount > 1)
									{
										DFUpdateCS.Unlock();
									}

									const FDistanceFieldVolumeData* DistanceFieldData = nullptr;
									float SelfShadowBias;
									PrimitiveSceneProxy->GetDistanceFieldAtlasData(DistanceFieldData, SelfShadowBias);

									const FBox LocalSpaceMeshBounds = DistanceFieldData->LocalSpaceMeshBounds;
			
									const FMatrix LocalToWorld = PrimAndInst.LocalToWorld;

									{
										const FBox WorldSpaceMeshBounds = LocalSpaceMeshBounds.TransformBy(LocalToWorld);

										const FLargeWorldRenderPosition AbsoluteWorldPosition(WorldSpaceMeshBounds.GetCenter());

										const FVector4f ObjectBoundingSphere(AbsoluteWorldPosition.GetOffset(), WorldSpaceMeshBounds.GetExtent().Size());

										UploadObjectBounds[0] = AbsoluteWorldPosition.GetTile();
										UploadObjectBounds[1] = ObjectBoundingSphere;

										const FGlobalDFCacheType CacheType = PrimitiveSceneProxy->IsOftenMoving() ? GDF_Full : GDF_MostlyStatic;
										const bool bOftenMoving = CacheType == GDF_Full;
										const bool bCastShadow = PrimitiveSceneProxy->CastsDynamicShadow();
										const bool bIsNaniteMesh = PrimitiveSceneProxy->IsNaniteMesh();
										const bool bEmissiveLightSource = PrimitiveSceneProxy->IsEmissiveLightSource();
										const bool bVisible = PrimitiveSceneProxy->IsDrawnInGame(); // Distance field object can be invisible in main view, but cast shadows
										const bool bAffectIndirectLightingWhileHidden = PrimitiveSceneProxy->AffectsIndirectLightingWhileHidden();

										uint32 Flags = 0;
										Flags |= bOftenMoving ? 1u : 0;
										Flags |= bCastShadow ? 2u : 0;
										Flags |= bIsNaniteMesh ? 4u : 0;
										Flags |= bEmissiveLightSource ? 8u : 0;
										Flags |= bVisible ? 16u : 0;
										Flags |= bAffectIndirectLightingWhileHidden ? 32u : 0;

										FVector4f ObjectWorldExtentAndFlags((FVector3f)WorldSpaceMeshBounds.GetExtent(), 0.0f);
										ObjectWorldExtentAndFlags.W = *(const float*)&Flags;
										UploadObjectBounds[2] = ObjectWorldExtentAndFlags;
									}

									// Uniformly scale our Volume space to lie within [-1, 1] at the max extent
									// This is mirrored in the SDF encoding
									const FBox::FReal LocalToVolumeScale = 1.0f / LocalSpaceMeshBounds.GetExtent().GetMax();

									const FMatrix VolumeToWorld = FScaleMatrix(1.0f / LocalToVolumeScale) * FTranslationMatrix(LocalSpaceMeshBounds.GetCenter()) * LocalToWorld;

									const FLargeWorldRenderPosition WorldPosition(VolumeToWorld.GetOrigin());
									const FVector TilePositionOffset = WorldPosition.GetTileOffset();

									// Inverse on FMatrix44f can generate NaNs if the source matrix contains large scaling, so do it in double precision.
									FMatrix LocalToRelativeWorld = FLargeWorldRenderScalar::MakeToRelativeWorldMatrixDouble(TilePositionOffset, VolumeToWorld);

									// TilePosition
									UploadObjectData[0] = WorldPosition.GetTile();

									const FMatrix44f WorldToVolumeT = FMatrix44f(LocalToRelativeWorld.Inverse().GetTransposed());
									// WorldToVolumeT
									UploadObjectData[1] = (*(FVector4f*)&WorldToVolumeT.M[0]);
									UploadObjectData[2] = (*(FVector4f*)&WorldToVolumeT.M[1]);
									UploadObjectData[3] = (*(FVector4f*)&WorldToVolumeT.M[2]);

									const FVector VolumePositionExtent = LocalSpaceMeshBounds.GetExtent() * LocalToVolumeScale;

									// Minimal surface bias which increases chance that ray hit will a surface located between two texels
									float ExpandSurfaceDistance = (GMeshSDFSurfaceBiasExpand * VolumePositionExtent / FVector(DistanceFieldData->Mips[0].IndirectionDimensions * DistanceField::UniqueDataBrickSize)).Size();

									const float WSign = DistanceFieldData->bMostlyTwoSided ? -1 : 1;
									UploadObjectData[4] = FVector4f((FVector3f)VolumePositionExtent, WSign * ExpandSurfaceDistance);

									const int32 PrimIdx = PrimAndInst.Primitive->GetIndex();
									const FPrimitiveBounds& PrimBounds = PrimitiveBounds[PrimIdx];
									float MinDrawDist2 = FMath::Square(PrimBounds.MinDrawDistance);
									// For IEEE compatible machines, float operations goes to inf if overflow
									// In this case, it will effectively disable max draw distance culling
									float MaxDrawDist = FMath::Max(PrimBounds.MaxCullDistance, 0.f) * GetCachedScalabilityCVars().ViewDistanceScale;

									const uint32 GPUSceneInstanceIndex = PrimitiveSceneProxy->SupportsInstanceDataBuffer() ? 
										PrimAndInst.Primitive->GetInstanceSceneDataOffset() + PrimAndInst.InstanceIndex :
										PrimAndInst.Primitive->GetInstanceSceneDataOffset();

									// Bypass NaN checks in FVector4f ctor
									FVector4f Vector4;
									Vector4.X = MinDrawDist2;
									Vector4.Y = MaxDrawDist * MaxDrawDist;
									Vector4.Z = SelfShadowBias;
									Vector4.W = *(const float*)&GPUSceneInstanceIndex;
									UploadObjectData[5] = Vector4;

									const FMatrix44f VolumeToWorldT = FMatrix44f(LocalToRelativeWorld.GetTransposed());
									UploadObjectData[6] = *(FVector4f*)&VolumeToWorldT.M[0];
									UploadObjectData[7] = *(FVector4f*)&VolumeToWorldT.M[1];
									UploadObjectData[8] = *(FVector4f*)&VolumeToWorldT.M[2];

									FVector4f FloatVector8(FVector3f(VolumeToWorld.GetScaleVector()), 0.0f);

									// Bypass NaN checks in FVector4f ctor
									FSetElementId AssetStateSetId = AssetStateArray.FindId(DistanceFieldData);
									check(AssetStateSetId.IsValidId());
									const int32 AssetStateInt = AssetStateSetId.AsInteger();
									FloatVector8.W = *(const float*)&AssetStateInt;
									
									UploadObjectData[9] = FloatVector8;
								}
							}
						},
						RangeCount == 1
					);

					UploadDistanceFieldDataBuffer.ResourceUploadTo(GraphBuilder, DFObjectDataBuffer);
					UploadDistanceFieldBoundsBuffer.ResourceUploadTo(GraphBuilder, DFObjectBoundsBuffer);

					ExternalAccessQueue.Add(DFObjectDataBuffer, ERHIAccess::SRVMask, ERHIPipeline::All);
					ExternalAccessQueue.Add(DFObjectBoundsBuffer, ERHIAccess::SRVMask, ERHIPipeline::All);
				}
			}
		}

		check(NumObjectsInBuffer == PrimitiveInstanceMapping.Num());

		if (bVerifySceneIntegrity)
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_UpdateObjectData_VerifyIntegrity);
			VerifyIntegrity();
		}
	}

	IndicesToUpdateInObjectBuffers.Reset();
}

void FSceneRenderer::UpdateGlobalHeightFieldObjectBuffers(FRDGBuilder& GraphBuilder)
{
	// Mask should be set in FSceneRenderer::PrepareDistanceFieldScene before calling this
	check(GraphBuilder.RHICmdList.GetGPUMask() == FRHIGPUMask::All());

	FDistanceFieldSceneData& DistanceFieldSceneData = Scene->DistanceFieldSceneData;

	if (GHeightFieldTextureAtlas.HasAtlasTexture()
		&& (DistanceFieldSceneData.HasPendingHeightFieldOperations()
			|| DistanceFieldSceneData.HeightFieldAtlasGeneration != GHeightFieldTextureAtlas.GetGeneration()
			|| DistanceFieldSceneData.HFVisibilityAtlasGenerattion != GHFVisibilityTextureAtlas.GetGeneration()))
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_UpdateHeightFieldSceneObjectData);
		RDG_EVENT_SCOPE(GraphBuilder, "UpdateHeightFieldSceneObjectData");

		if (!DistanceFieldSceneData.HeightFieldObjectBuffers)
		{
			AddOrRemoveSceneHeightFieldPrimitives(true);

			for (int32 Idx = 0; Idx < DistanceFieldSceneData.HeightfieldPrimitives.Num(); ++Idx)
			{
				FPrimitiveSceneInfo* Primitive = DistanceFieldSceneData.HeightfieldPrimitives[Idx];
				check(!DistanceFieldSceneData.PendingHeightFieldAddOps.Contains(Primitive));
				DistanceFieldSceneData.PendingHeightFieldAddOps.Add(Primitive);
			}
			DistanceFieldSceneData.HeightfieldPrimitives.Reset();
			DistanceFieldSceneData.HeightFieldObjectBuffers = new FHeightFieldObjectBuffers;
		}

		if (DistanceFieldSceneData.HeightFieldAtlasGeneration != GHeightFieldTextureAtlas.GetGeneration()
			|| DistanceFieldSceneData.HFVisibilityAtlasGenerattion != GHFVisibilityTextureAtlas.GetGeneration())
		{
			DistanceFieldSceneData.HeightFieldAtlasGeneration = GHeightFieldTextureAtlas.GetGeneration();
			DistanceFieldSceneData.HFVisibilityAtlasGenerattion = GHFVisibilityTextureAtlas.GetGeneration();

			for (int32 Idx = 0; Idx < DistanceFieldSceneData.HeightfieldPrimitives.Num(); ++Idx)
			{
				FPrimitiveSceneInfo* Primitive = DistanceFieldSceneData.HeightfieldPrimitives[Idx];

				if (!DistanceFieldSceneData.HasPendingRemoveHeightFieldPrimitive(Primitive)
					&& !DistanceFieldSceneData.PendingHeightFieldAddOps.Contains(Primitive)
					&& !DistanceFieldSceneData.PendingHeightFieldUpdateOps.Contains(Primitive))
				{
					DistanceFieldSceneData.PendingHeightFieldUpdateOps.Add(Primitive);
				}
			}
		}

		UpdateGlobalHeightFieldObjectRemoves(Scene);

		if (DistanceFieldSceneData.PendingHeightFieldAddOps.Num() || DistanceFieldSceneData.PendingHeightFieldUpdateOps.Num())
		{
			for (FPrimitiveSceneInfo* PrimitiveSceneInfo : DistanceFieldSceneData.PendingHeightFieldAddOps)
			{
				ProcessHeightFieldPrimitiveUpdate(true, Scene, PrimitiveSceneInfo, DistanceFieldSceneData.IndicesToUpdateInHeightFieldObjectBuffers);
			}

			for (FPrimitiveSceneInfo* PrimitiveSceneInfo : DistanceFieldSceneData.PendingHeightFieldUpdateOps)
			{
				ProcessHeightFieldPrimitiveUpdate(false, Scene, PrimitiveSceneInfo, DistanceFieldSceneData.IndicesToUpdateInHeightFieldObjectBuffers);
			}

			DistanceFieldSceneData.PendingHeightFieldAddOps.Reset();
			DistanceFieldSceneData.PendingHeightFieldUpdateOps.Empty();

			FHeightFieldObjectBuffers*& ObjectBuffers = DistanceFieldSceneData.HeightFieldObjectBuffers;

			// Upload height field object data and bounds
			{
				const uint32 NumHeightFieldObjects = DistanceFieldSceneData.NumHeightFieldObjectsInBuffer;

				const uint32 HeighFieldObjectDataNumFloat4s = FMath::RoundUpToPowerOfTwo(NumHeightFieldObjects * FHeightFieldObjectBuffers::ObjectDataStride);
				const uint32 HeighFieldObjectDataNumBytes = HeighFieldObjectDataNumFloat4s * sizeof(FVector4f);
				FRDGBuffer* HeightfieldObjectDataBuffer = ResizeStructuredBufferIfNeeded(GraphBuilder, ObjectBuffers->Data, HeighFieldObjectDataNumBytes, TEXT("HeighFieldObjectData"));

				const uint32 HeighFieldObjectBoundsNumFloat4s = FMath::RoundUpToPowerOfTwo(NumHeightFieldObjects * FHeightFieldObjectBuffers::ObjectBoundsStride);
				const uint32 HeighFieldObjectBoundsNumBytes = HeighFieldObjectBoundsNumFloat4s * sizeof(FVector4f);
				FRDGBuffer* HeightfieldObjectBoundsBuffer = ResizeStructuredBufferIfNeeded(GraphBuilder, ObjectBuffers->Bounds, HeighFieldObjectBoundsNumBytes, TEXT("HeighFieldObjectBounds"));

				const int32 NumHeighFieldObjectUploads = DistanceFieldSceneData.IndicesToUpdateInHeightFieldObjectBuffers.Num();

				if (NumHeighFieldObjectUploads > 0)
				{
					DistanceFieldSceneData.UploadHeightFieldDataBuffer.Init(GraphBuilder, NumHeighFieldObjectUploads, FHeightFieldObjectBuffers::ObjectDataStride * sizeof(FVector4f), true, TEXT("HeighFieldObjectDataUploadBuffer"));
					DistanceFieldSceneData.UploadHeightFieldBoundsBuffer.Init(GraphBuilder, NumHeighFieldObjectUploads, FHeightFieldObjectBuffers::ObjectBoundsStride * sizeof(FVector4f), true, TEXT("HeighFieldObjectBoundsUploadBuffer"));

					for (int32 Index : DistanceFieldSceneData.IndicesToUpdateInHeightFieldObjectBuffers)
					{
						if (Index >= 0 && Index < DistanceFieldSceneData.HeightfieldPrimitives.Num())
						{
							FPrimitiveSceneInfo* Primitive = DistanceFieldSceneData.HeightfieldPrimitives[Index];

							FVector4f* UploadObjectData = (FVector4f*)DistanceFieldSceneData.UploadHeightFieldDataBuffer.Add_GetRef(Index);
							FVector4f* UploadObjectBounds = (FVector4f*)DistanceFieldSceneData.UploadHeightFieldBoundsBuffer.Add_GetRef(Index);

							UTexture2D* HeightNormalTexture;
							UTexture2D* DiffuseColorTexture;
							UTexture2D* VisibilityTexture;
							FHeightfieldComponentDescription HeightFieldCompDesc(Primitive->Proxy->GetLocalToWorld(), Primitive->GetInstanceSceneDataOffset());
							Primitive->Proxy->GetHeightfieldRepresentation(HeightNormalTexture, DiffuseColorTexture, VisibilityTexture, HeightFieldCompDesc);

							{
								const FBoxSphereBounds& Bounds = Primitive->Proxy->GetBounds();
								const FBox BoxBound = Bounds.GetBox();

								const FLargeWorldRenderPosition AbsoluteWorldPosition(BoxBound.GetCenter());

								const FVector4f ObjectBoundingSphere(AbsoluteWorldPosition.GetOffset(), Bounds.SphereRadius);

								UploadObjectBounds[0] = AbsoluteWorldPosition.GetTile();
								UploadObjectBounds[1] = ObjectBoundingSphere;
								UploadObjectBounds[2] = FVector4f((FVector3f)BoxBound.GetExtent(), 0.f);
							}

							const FMatrix& LocalToWorld = HeightFieldCompDesc.LocalToWorld;
							check(LocalToWorld.GetMaximumAxisScale() > 0.f);

							const FLargeWorldRenderPosition WorldPosition(LocalToWorld.GetOrigin());
							const FVector TilePositionOffset = WorldPosition.GetTileOffset();

							// Inverse on FMatrix44f can generate NaNs if the source matrix contains large scaling, so do it in double precision.
							FMatrix LocalToRelativeWorld = FLargeWorldRenderScalar::MakeToRelativeWorldMatrixDouble(TilePositionOffset, LocalToWorld);

							UploadObjectData[0] = WorldPosition.GetTile();

							const FMatrix44f WorldToLocalT = FMatrix44f(LocalToRelativeWorld.Inverse().GetTransposed());
							UploadObjectData[1] = *(const FVector4f*)&WorldToLocalT.M[0];
							UploadObjectData[2] = *(const FVector4f*)&WorldToLocalT.M[1];
							UploadObjectData[3] = *(const FVector4f*)&WorldToLocalT.M[2];

							const FIntRect& HeightFieldRect = HeightFieldCompDesc.HeightfieldRect;
							const float WorldToLocalScale = FMath::Min3(
								WorldToLocalT.GetColumn(0).Size(),
								WorldToLocalT.GetColumn(1).Size(),
								WorldToLocalT.GetColumn(2).Size());
							UploadObjectData[4] = FVector4f(HeightFieldRect.Width(), HeightFieldRect.Height(), WorldToLocalScale, 0.f);

							FVector4f HeightUVScaleBias(ForceInitToZero);
							if (HeightNormalTexture)
							{
								const uint32 HeightNormalTextureHandle = GHeightFieldTextureAtlas.GetAllocationHandle(HeightNormalTexture);
								if (HeightNormalTextureHandle != INDEX_NONE)
								{
									const FVector4f HeightFieldScaleBias = HeightFieldCompDesc.HeightfieldScaleBias;
									check(HeightFieldScaleBias.Y >= 0.f && HeightFieldScaleBias.Z >= 0.f && HeightFieldScaleBias.W >= 0.f);

									const FVector4f ScaleBias = GHeightFieldTextureAtlas.GetAllocationScaleBias(HeightNormalTextureHandle);
									HeightUVScaleBias.Set(FMath::Abs(HeightFieldScaleBias.X) * ScaleBias.X,
										HeightFieldScaleBias.Y * ScaleBias.Y,
										HeightFieldScaleBias.Z * ScaleBias.X + ScaleBias.Z,
										HeightFieldScaleBias.W * ScaleBias.Y + ScaleBias.W);
								}
							}
							UploadObjectData[5] = HeightUVScaleBias;

							FVector4f VisUVScaleBias(ForceInitToZero);
							if (VisibilityTexture)
							{
								const uint32 VisHandle = GHFVisibilityTextureAtlas.GetAllocationHandle(VisibilityTexture);
								if (VisHandle != INDEX_NONE)
								{
									const FVector4f ScaleBias = GHFVisibilityTextureAtlas.GetAllocationScaleBias(VisHandle);
									VisUVScaleBias = FVector4f(1.f / HeightFieldRect.Width() * ScaleBias.X, 1.f / HeightFieldRect.Height() * ScaleBias.Y, ScaleBias.Z, ScaleBias.W);
								}
							}
							UploadObjectData[6] = VisUVScaleBias;
						}
					}

					DistanceFieldSceneData.UploadHeightFieldDataBuffer.ResourceUploadTo(GraphBuilder, HeightfieldObjectDataBuffer);
					DistanceFieldSceneData.UploadHeightFieldBoundsBuffer.ResourceUploadTo(GraphBuilder, HeightfieldObjectBoundsBuffer);

					DistanceFieldSceneData.IndicesToUpdateInHeightFieldObjectBuffers.Reset();
				}
			}
		}
	}
}

void FSceneRenderer::PrepareDistanceFieldScene(FRDGBuilder& GraphBuilder, FRDGExternalAccessQueue& ExternalAccessQueue, bool bSplitDispatch)
{
	RDG_CSV_STAT_EXCLUSIVE_SCOPE(GraphBuilder, PrepareDistanceFieldScene);
	TRACE_CPUPROFILER_EVENT_SCOPE(FSceneRenderer::PrepareDistanceFieldScene);
	QUICK_SCOPE_CYCLE_COUNTER(STAT_PrepareDistanceFieldScene);
	LLM_SCOPE_BYTAG(DistanceFields);
	RDG_GPU_MASK_SCOPE(GraphBuilder, FRHIGPUMask::All());
	RDG_RHI_GPU_STAT_SCOPE(GraphBuilder, DistanceFields);

	const bool bShouldPrepareHeightFieldScene = ShouldPrepareHeightFieldScene();
	const bool bShouldPrepareDistanceFieldScene = ShouldPrepareDistanceFieldScene();

	if (bShouldPrepareHeightFieldScene)
	{
		extern int32 GHFShadowQuality;
		if (GHFShadowQuality > 2)
		{
			GHFVisibilityTextureAtlas.UpdateAllocations(GraphBuilder, FeatureLevel);
		}
		GHeightFieldTextureAtlas.UpdateAllocations(GraphBuilder, FeatureLevel);
		UpdateGlobalHeightFieldObjectBuffers(GraphBuilder);
	}
	else if (bShouldPrepareDistanceFieldScene)
	{
		AddOrRemoveSceneHeightFieldPrimitives();
	}

	if (bShouldPrepareDistanceFieldScene)
	{
		TArray<FDistanceFieldAssetMipId> DistanceFieldAssetAdds;
		TArray<FSetElementId> DistanceFieldAssetRemoves;
		Scene->DistanceFieldSceneData.UpdateDistanceFieldObjectBuffers(GraphBuilder, ExternalAccessQueue, Scene, DistanceFieldAssetAdds, DistanceFieldAssetRemoves);

		Scene->DistanceFieldSceneData.UpdateDistanceFieldAtlas(GraphBuilder, ExternalAccessQueue, Views[0], Scene, IsLumenEnabled(Views[0]), Views[0].ShaderMap, DistanceFieldAssetAdds, DistanceFieldAssetRemoves);

		if (bSplitDispatch)
		{
			GraphBuilder.AddDispatchHint();
		}

		if (ShouldPrepareGlobalDistanceField())
		{
			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				FViewInfo& View = Views[ViewIndex];

				RDG_GPU_MASK_SCOPE(GraphBuilder, GNumAlternateFrameRenderingGroups > 1 ? FRHIGPUMask::All() : View.GPUMask);

				float OcclusionMaxDistance = Scene->DefaultMaxDistanceFieldOcclusionDistance;

				// Use the skylight's max distance if there is one
				if (Scene->SkyLight && Scene->SkyLight->bCastShadows && !Scene->SkyLight->bWantsStaticShadowing)
				{
					OcclusionMaxDistance = Scene->SkyLight->OcclusionMaxDistance;
				}

				UpdateGlobalDistanceFieldVolume(GraphBuilder, ExternalAccessQueue, View, Scene, OcclusionMaxDistance, IsLumenEnabled(View), View.GlobalDistanceFieldInfo);
			}
		}
		if (!bSplitDispatch)
		{
			GraphBuilder.AddDispatchHint();
		}
	}
}

void FSceneRenderer::AddOrRemoveSceneHeightFieldPrimitives(bool bSkipAdd)
{
	FDistanceFieldSceneData& SceneData = Scene->DistanceFieldSceneData;

	if (SceneData.HeightFieldObjectBuffers)
	{
		delete SceneData.HeightFieldObjectBuffers;
		SceneData.HeightFieldObjectBuffers = nullptr;
		SceneData.NumHeightFieldObjectsInBuffer = 0;
		SceneData.HeightFieldAtlasGeneration = 0;
		SceneData.HFVisibilityAtlasGenerattion = 0;
	}

	TArray<int32, SceneRenderingAllocator> PendingRemoveIndices;
	for (int32 Idx = 0; Idx < SceneData.PendingHeightFieldRemoveOps.Num(); ++Idx)
	{
		const FHeightFieldPrimitiveRemoveInfo& RemoveInfo = SceneData.PendingHeightFieldRemoveOps[Idx];
		check(RemoveInfo.DistanceFieldInstanceIndices.Num() == 1);
		PendingRemoveIndices.Add(RemoveInfo.DistanceFieldInstanceIndices[0]);
		const FGlobalDFCacheType CacheType = RemoveInfo.bOftenMoving ? GDF_Full : GDF_MostlyStatic;
		AddModifiedBounds(Scene, CacheType, RemoveInfo.WorldBounds);
	}
	SceneData.PendingHeightFieldRemoveOps.Reset();
	Algo::Sort(PendingRemoveIndices);
	for (int32 Idx = PendingRemoveIndices.Num() - 1; Idx >= 0; --Idx)
	{
		const int32 RemoveIdx = PendingRemoveIndices[Idx];
		const int32 LastObjectIdx = SceneData.HeightfieldPrimitives.Num() - 1;
		if (RemoveIdx != LastObjectIdx)
		{
			SceneData.HeightfieldPrimitives[LastObjectIdx]->DistanceFieldInstanceIndices[0] = RemoveIdx;
		}
		SceneData.HeightfieldPrimitives.RemoveAtSwap(RemoveIdx);
	}

	if (!bSkipAdd)
	{
		for (FPrimitiveSceneInfo* Primitive : SceneData.PendingHeightFieldAddOps)
		{
			const int32 HFIdx = SceneData.HeightfieldPrimitives.Add(Primitive);
			Primitive->DistanceFieldInstanceIndices.Empty(1);
			Primitive->DistanceFieldInstanceIndices.Add(HFIdx);
			const FGlobalDFCacheType CacheType = Primitive->Proxy->IsOftenMoving() ? GDF_Full : GDF_MostlyStatic;
			const FBoxSphereBounds& Bounds = Primitive->Proxy->GetBounds();
			AddModifiedBounds(Scene, CacheType, Bounds.GetBox());
		}
		SceneData.PendingHeightFieldAddOps.Reset();
	}

	SceneData.PendingHeightFieldUpdateOps.Empty();
}
