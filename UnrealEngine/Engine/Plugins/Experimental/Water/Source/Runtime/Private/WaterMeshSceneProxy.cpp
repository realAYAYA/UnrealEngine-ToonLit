// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaterMeshSceneProxy.h"
#include "WaterMeshComponent.h"
#include "WaterVertexFactory.h"
#include "WaterInstanceDataBuffer.h"
#include "WaterSubsystem.h"
#include "WaterUtils.h"
#include "SceneManagement.h"
#include "Engine/Engine.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "DrawDebugHelpers.h"
#include "Math/ColorList.h"
#include "RayTracingInstance.h"

DECLARE_STATS_GROUP(TEXT("Water Mesh"), STATGROUP_WaterMesh, STATCAT_Advanced);

DECLARE_DWORD_COUNTER_STAT(TEXT("Tiles Drawn"), STAT_WaterTilesDrawn, STATGROUP_WaterMesh);
DECLARE_DWORD_COUNTER_STAT(TEXT("Draw Calls"), STAT_WaterDrawCalls, STATGROUP_WaterMesh);
DECLARE_DWORD_COUNTER_STAT(TEXT("Vertices Drawn"), STAT_WaterVerticesDrawn, STATGROUP_WaterMesh);
DECLARE_DWORD_COUNTER_STAT(TEXT("Number Drawn Materials"), STAT_WaterDrawnMats, STATGROUP_WaterMesh);

/** Scalability CVars */
static TAutoConsoleVariable<int32> CVarWaterMeshLODMorphEnabled(
	TEXT("r.Water.WaterMesh.LODMorphEnabled"), 1,
	TEXT("If the smooth LOD morph is enabled. Turning this off may cause slight popping between LOD levels but will skip the calculations in the vertex shader, making it cheaper"),
	ECVF_Scalability | ECVF_RenderThreadSafe);

/** Debug CVars */
static TAutoConsoleVariable<int32> CVarWaterMeshShowWireframe(
	TEXT("r.Water.WaterMesh.ShowWireframe"),
	0,
	TEXT("Forces wireframe rendering on for water"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarWaterMeshShowWireframeAtBaseHeight(
	TEXT("r.Water.WaterMesh.ShowWireframeAtBaseHeight"),
	0,
	TEXT("When rendering in wireframe, show the mesh with no displacement"),
	ECVF_RenderThreadSafe
);

TAutoConsoleVariable<int32> CVarWaterMeshEnableRendering(
	TEXT("r.Water.WaterMesh.EnableRendering"),
	1,
	TEXT("Turn off all water rendering from within the scene proxy"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarWaterMeshShowLODLevels(
	TEXT("r.Water.WaterMesh.ShowLODLevels"),
	0,
	TEXT("Shows the LOD levels as concentric squares around the observer position at height 0"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarWaterMeshShowTileBounds(
	TEXT("r.Water.WaterMesh.ShowTileBounds"),
	0,
	TEXT("Shows the tile bounds with optional color modes: 0 is disabled, 1 is by water body type, 2 is by LOD, 3 is by density index"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarWaterMeshPreAllocStagingInstanceMemory(
	TEXT("r.Water.WaterMesh.PreAllocStagingInstanceMemory"),
	0,
	TEXT("Pre-allocates staging instance data memory according to historical max. This reduces the overhead when the array needs to grow but may use more memory"),
	ECVF_RenderThreadSafe);

#if RHI_RAYTRACING
static TAutoConsoleVariable<int32> CVarRayTracingGeometryWater(
	TEXT("r.RayTracing.Geometry.Water"),
	0,
	TEXT("Include water in ray tracing effects (default = 0 (water disabled in ray tracing))"));
#endif

// ----------------------------------------------------------------------------------

FWaterMeshSceneProxy::FWaterMeshSceneProxy(UWaterMeshComponent* Component)
	: FPrimitiveSceneProxy(Component)
	, MaterialRelevance(Component->GetWaterMaterialRelevance(GetScene().GetFeatureLevel()))
{
	// Cache the tiles and settings
	WaterQuadTree = Component->GetWaterQuadTree();
	// Leaf size * 0.5 equals the tightest possible LOD Scale that doesn't break the morphing. Can be scaled larger
	LODScale = WaterQuadTree.GetLeafSize() * FMath::Max(Component->GetLODScale(), 0.5f);

	// Assign the force collapse level if there is one, otherwise leave it at the default
	if (Component->ForceCollapseDensityLevel > -1)
	{
		ForceCollapseDensityLevel = Component->ForceCollapseDensityLevel;
	}

	int32 NumQuads = (int32)FMath::Pow(2.0f, (float)Component->GetTessellationFactor());

	WaterVertexFactories.Reserve(WaterQuadTree.GetTreeDepth());
	for (uint8 i = 0; i < WaterQuadTree.GetTreeDepth(); i++)
	{
		WaterVertexFactories.Add(new WaterVertexFactoryType(GetScene().GetFeatureLevel(), NumQuads, LODScale));
		BeginInitResource(WaterVertexFactories.Last());

		NumQuads /= 2;

		// If LODs become too small, early out
		if (NumQuads <= 1)
		{
			break;
		}
	}

	WaterVertexFactories.Shrink();
	DensityCount = WaterVertexFactories.Num();

	const int32 TotalLeafNodes = WaterQuadTree.GetMaxLeafCount();
	WaterInstanceDataBuffers = new WaterInstanceDataBuffersType(TotalLeafNodes);

	WaterMeshUserDataBuffers = new WaterMeshUserDataBuffersType(WaterInstanceDataBuffers);

	WaterQuadTree.BuildMaterialIndices();

#if RHI_RAYTRACING
	RayTracingWaterData.SetNum(DensityCount);
#endif
}

FWaterMeshSceneProxy::~FWaterMeshSceneProxy()
{
	for (WaterVertexFactoryType* WaterFactory : WaterVertexFactories)
	{
		WaterFactory->ReleaseResource();
		delete WaterFactory;
	}

	delete WaterInstanceDataBuffers;

	delete WaterMeshUserDataBuffers;

#if RHI_RAYTRACING
	for (auto& WaterDataArray : RayTracingWaterData)
	{
		for (auto& WaterRayTracingItem : WaterDataArray)
		{
			WaterRayTracingItem.Geometry.ReleaseResource();
			WaterRayTracingItem.DynamicVertexBuffer.Release();
		}
	}	
#endif
}

FWaterMeshSceneProxy::FWaterLODParams FWaterMeshSceneProxy::GetWaterLODParams(const FVector& Position) const
{
	float WaterHeightForLOD = 0.0f;
	WaterQuadTree.QueryInterpolatedTileBaseHeightAtLocation(FVector2D(Position), WaterHeightForLOD);

	// Need to let the lowest LOD morph globally towards the next LOD. When the LOD is done morphing, simply clamp the LOD in the LOD selection to effectively promote the lowest LOD to the same LOD level as the one above
	float DistToWater = FMath::Abs(Position.Z - WaterHeightForLOD) / LODScale;
	DistToWater = FMath::Max(DistToWater - 2.0f, 0.0f);
	DistToWater *= 2.0f;

	// Clamp to WaterTileQuadTree.GetLODCount() - 1.0f prevents the last LOD to morph
	const float FloatLOD = FMath::Clamp(FMath::Log2(DistToWater), 0.0f, WaterQuadTree.GetTreeDepth() - 1.0f);

	FWaterLODParams WaterLODParams;
	WaterLODParams.HeightLODFactor = FMath::Frac(FloatLOD);
	WaterLODParams.LowestLOD = FMath::Clamp(FMath::FloorToInt(FloatLOD), 0, WaterQuadTree.GetTreeDepth() - 1);
	WaterLODParams.WaterHeightForLOD = WaterHeightForLOD;

	return WaterLODParams;
}

void FWaterMeshSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(Water);
	TRACE_CPUPROFILER_EVENT_SCOPE(FWaterMeshSceneProxy::GetDynamicMeshElements);

	// The water render groups we have to render for this batch : 
	TArray<EWaterMeshRenderGroupType, TInlineAllocator<WaterVertexFactoryType::NumRenderGroups>> BatchRenderGroups;
	// By default, render all water tiles : 
	BatchRenderGroups.Add(EWaterMeshRenderGroupType::RG_RenderWaterTiles);

#if WITH_WATER_SELECTION_SUPPORT
	bool bHasSelectedInstances = IsSelected();
	const bool bSelectionRenderEnabled = GIsEditor && ViewFamily.EngineShowFlags.Selection;

	if (bSelectionRenderEnabled && bHasSelectedInstances)
	{
		// Don't render all in one group: instead, render 2 groups : first, the selected only then, the non-selected only :
		BatchRenderGroups[0] = EWaterMeshRenderGroupType::RG_RenderSelectedWaterTilesOnly;
		BatchRenderGroups.Add(EWaterMeshRenderGroupType::RG_RenderUnselectedWaterTilesOnly);
	}
#endif // WITH_WATER_SELECTION_SUPPORT

	if (!HasWaterData() || !FWaterUtils::IsWaterMeshRenderingEnabled(/*bIsRenderThread = */true))
	{
		return;
	}

	// Set up wireframe material (if needed)
	const bool bWireframe = AllowDebugViewmodes() && (ViewFamily.EngineShowFlags.Wireframe || CVarWaterMeshShowWireframe.GetValueOnRenderThread() == 1);

	FColoredMaterialRenderProxy* WireframeMaterialInstance = nullptr;
	if (bWireframe && CVarWaterMeshShowWireframeAtBaseHeight.GetValueOnRenderThread() == 1)
	{
		WireframeMaterialInstance = new FColoredMaterialRenderProxy(
			GEngine->WireframeMaterial ? GEngine->WireframeMaterial->GetRenderProxy() : NULL,
			FColor::Cyan);

		Collector.RegisterOneFrameMaterialProxy(WireframeMaterialInstance);
	}

	const int32 NumBuckets = WaterQuadTree.GetWaterMaterials().Num() * DensityCount;

	TArray<FWaterQuadTree::FTraversalOutput, TInlineAllocator<4>> WaterInstanceDataPerView;

	bool bEncounteredISRView = false;
	int32 InstanceFactor = 1;

	// Gather visible tiles, their lod and materials for all renderable views (skip right view when stereo pair is rendered instanced)
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		const FSceneView* View = Views[ViewIndex];
		if (!bEncounteredISRView && View->IsInstancedStereoPass())
		{
			bEncounteredISRView = true;
			InstanceFactor = View->GetStereoPassInstanceFactor();
		}

		// skip gathering visible tiles from instanced right eye views
		if ((VisibilityMap & (1 << ViewIndex)) && (!bEncounteredISRView || View->IsPrimarySceneView()))
		{
			const FVector ObserverPosition = View->ViewMatrices.GetViewOrigin();
			
			FWaterLODParams WaterLODParams = GetWaterLODParams(ObserverPosition);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			if (CVarWaterMeshShowLODLevels.GetValueOnRenderThread())
			{
				for (int32 i = WaterLODParams.LowestLOD; i < WaterQuadTree.GetTreeDepth(); i++)
				{
					float LODDist = FWaterQuadTree::GetLODDistance(i, LODScale);
					FVector Orig = FVector(FVector2D(ObserverPosition), WaterLODParams.WaterHeightForLOD);

					DrawCircle(Collector.GetPDI(ViewIndex), Orig, FVector::ForwardVector, FVector::RightVector, GColorList.GetFColorByIndex(i + 1), LODDist, 64, 0);
				}
			}
#endif
			TRACE_CPUPROFILER_EVENT_SCOPE(QuadTreeTraversalPerView);

			FWaterQuadTree::FTraversalOutput& WaterInstanceData = WaterInstanceDataPerView.Emplace_GetRef();
			WaterInstanceData.BucketInstanceCounts.Empty(NumBuckets);
			WaterInstanceData.BucketInstanceCounts.AddZeroed(NumBuckets);
			if (!!CVarWaterMeshPreAllocStagingInstanceMemory.GetValueOnRenderThread())
			{
				WaterInstanceData.StagingInstanceData.Empty(HistoricalMaxViewInstanceCount);
			}

			FWaterQuadTree::FTraversalDesc TraversalDesc;
			TraversalDesc.LowestLOD = WaterLODParams.LowestLOD;
			TraversalDesc.HeightMorph = WaterLODParams.HeightLODFactor;
			TraversalDesc.LODCount = WaterQuadTree.GetTreeDepth();
			TraversalDesc.DensityCount = DensityCount;
			TraversalDesc.ForceCollapseDensityLevel = ForceCollapseDensityLevel;
			TraversalDesc.Frustum = View->ViewFrustum;
			TraversalDesc.ObserverPosition = ObserverPosition;
			TraversalDesc.PreViewTranslation = View->ViewMatrices.GetPreViewTranslation();
			TraversalDesc.LODScale = LODScale;
			TraversalDesc.bLODMorphingEnabled = !!CVarWaterMeshLODMorphEnabled.GetValueOnRenderThread();
			TraversalDesc.TessellatedWaterMeshBounds = TessellatedWaterMeshBounds;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			//Debug
			TraversalDesc.DebugPDI = Collector.GetPDI(ViewIndex);
			TraversalDesc.DebugShowTile = CVarWaterMeshShowTileBounds.GetValueOnRenderThread();
#endif
			WaterQuadTree.BuildWaterTileInstanceData(TraversalDesc, WaterInstanceData);

			HistoricalMaxViewInstanceCount = FMath::Max(HistoricalMaxViewInstanceCount, WaterInstanceData.InstanceCount);
		}
	}

	// Get number of total instances for all views
	int32 TotalInstanceCount = 0;
	for (const FWaterQuadTree::FTraversalOutput& WaterInstanceData : WaterInstanceDataPerView)
	{
		TotalInstanceCount += WaterInstanceData.InstanceCount;
	}

	if (TotalInstanceCount == 0)
	{
		// no instance visible, early exit
		return;
	}

	WaterInstanceDataBuffers->Lock(TotalInstanceCount * InstanceFactor);

	int32 InstanceDataOffset = 0;

	// Go through all buckets and issue one batched draw call per LOD level per material per view
	int32 TraversalIndex = 0;
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		// when rendering ISR, don't process the instanced view
		if ((VisibilityMap & (1 << ViewIndex)) && (!bEncounteredISRView || Views[ViewIndex]->IsPrimarySceneView()))
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(BucketsPerView);

			FWaterQuadTree::FTraversalOutput& WaterInstanceData = WaterInstanceDataPerView[TraversalIndex];
			const int32 NumWaterMaterials = WaterQuadTree.GetWaterMaterials().Num();
			TraversalIndex++;

			for (int32 MaterialIndex = 0; MaterialIndex < NumWaterMaterials; ++MaterialIndex)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(MaterialBucket);
				bool bMaterialDrawn = false;

				for (int32 DensityIndex = 0; DensityIndex < DensityCount; ++DensityIndex)
				{
					const int32 BucketIndex = MaterialIndex * DensityCount + DensityIndex;
					const int32 InstanceCount = WaterInstanceData.BucketInstanceCounts[BucketIndex];

					if (!InstanceCount)
					{
						continue;
					}

					TRACE_CPUPROFILER_EVENT_SCOPE(DensityBucket);

					const FMaterialRenderProxy* MaterialRenderProxy = (WireframeMaterialInstance != nullptr) ? WireframeMaterialInstance : WaterQuadTree.GetWaterMaterials()[MaterialIndex];
					check (MaterialRenderProxy != nullptr);

					bool bUseForDepthPass = false;

					// If there's a valid material, use that to figure out the depth pass status
					if (const FMaterial* BucketMaterial = MaterialRenderProxy->GetMaterialNoFallback(GetScene().GetFeatureLevel()))
					{
						// Preemptively turn off depth rendering for this mesh batch if the material doesn't need it
						bUseForDepthPass = !BucketMaterial->GetShadingModels().HasShadingModel(MSM_SingleLayerWater) && BucketMaterial->GetBlendMode() != EBlendMode::BLEND_Translucent;
					}

					bMaterialDrawn = true;
					for (EWaterMeshRenderGroupType RenderGroup : BatchRenderGroups)
					{
						// Set up mesh batch
						FMeshBatch& Mesh = Collector.AllocateMesh();
						Mesh.bWireframe = bWireframe;
						Mesh.VertexFactory = WaterVertexFactories[DensityIndex];
						Mesh.MaterialRenderProxy = MaterialRenderProxy;
						Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
						Mesh.Type = PT_TriangleList;
						Mesh.DepthPriorityGroup = SDPG_World;
						Mesh.bCanApplyViewModeOverrides = false;
						Mesh.bUseForMaterial = true;
						Mesh.CastShadow = false;
						// Preemptively turn off depth rendering for this mesh batch if the material doesn't need it
						Mesh.bUseForDepthPass = bUseForDepthPass;
						Mesh.bUseAsOccluder = false;

#if WITH_WATER_SELECTION_SUPPORT
						Mesh.bUseSelectionOutline = (RenderGroup == EWaterMeshRenderGroupType::RG_RenderSelectedWaterTilesOnly);
						Mesh.bUseWireframeSelectionColoring = (RenderGroup == EWaterMeshRenderGroupType::RG_RenderSelectedWaterTilesOnly);
#endif // WITH_WATER_SELECTION_SUPPORT

						Mesh.Elements.SetNumZeroed(1);

						{
							TRACE_CPUPROFILER_EVENT_SCOPE_STR("Setup batch element");

							// Set up one mesh batch element
							FMeshBatchElement& BatchElement = Mesh.Elements[0];

							// Set up for instancing
							//BatchElement.bIsInstancedMesh = true;
							BatchElement.NumInstances = InstanceCount;
							BatchElement.UserData = (void*)WaterMeshUserDataBuffers->GetUserData(RenderGroup);
							BatchElement.UserIndex = InstanceDataOffset * InstanceFactor;

							BatchElement.FirstIndex = 0;
							BatchElement.NumPrimitives = WaterVertexFactories[DensityIndex]->IndexBuffer->GetIndexCount() / 3;
							BatchElement.MinVertexIndex = 0;
							BatchElement.MaxVertexIndex = WaterVertexFactories[DensityIndex]->VertexBuffer->GetVertexCount() - 1;

							BatchElement.IndexBuffer = WaterVertexFactories[DensityIndex]->IndexBuffer;
							BatchElement.PrimitiveIdMode = PrimID_ForceZero;

							// We need the uniform buffer of this primitive because it stores the proper value for the bOutputVelocity flag.
							// The identity primitive uniform buffer simply stores false for this flag which leads to missing motion vectors.
							BatchElement.PrimitiveUniformBuffer = GetUniformBuffer(); 
						}

						{
							INC_DWORD_STAT_BY(STAT_WaterVerticesDrawn, WaterVertexFactories[DensityIndex]->VertexBuffer->GetVertexCount() * InstanceCount);
							INC_DWORD_STAT(STAT_WaterDrawCalls);
							INC_DWORD_STAT_BY(STAT_WaterTilesDrawn, InstanceCount);

							TRACE_CPUPROFILER_EVENT_SCOPE(Collector.AddMesh);

							Collector.AddMesh(ViewIndex, Mesh);
						}
					}

					// Note : we're repurposing the BucketInstanceCounts array here for storing the actual offset in the buffer. This means that effectively from this point on, BucketInstanceCounts doesn't actually 
					//  contain the number of instances anymore : 
					WaterInstanceData.BucketInstanceCounts[BucketIndex] = InstanceDataOffset;
					InstanceDataOffset += InstanceCount;
				}

				INC_DWORD_STAT_BY(STAT_WaterDrawnMats, (int32)bMaterialDrawn);
			}

			const int32 NumStagingInstances = WaterInstanceData.StagingInstanceData.Num();
			for (int32 Idx = 0; Idx < NumStagingInstances; ++Idx)
			{
				const FWaterQuadTree::FStagingInstanceData& Data = WaterInstanceData.StagingInstanceData[Idx];
				const int32 WriteIndex = WaterInstanceData.BucketInstanceCounts[Data.BucketIndex]++;

				for (int32 StreamIdx = 0; StreamIdx < WaterInstanceDataBuffersType::NumBuffers; ++StreamIdx)
				{
					TArrayView<FVector4f> BufferMemory = WaterInstanceDataBuffers->GetBufferMemory(StreamIdx);
					for (int32 IdxMultipliedInstance = 0; IdxMultipliedInstance < InstanceFactor; ++IdxMultipliedInstance)
					{
						BufferMemory[WriteIndex * InstanceFactor + IdxMultipliedInstance] = Data.Data[StreamIdx];
					}
				}
			}
		}
	}

	WaterInstanceDataBuffers->Unlock();
}

#if RHI_RAYTRACING
void FWaterMeshSceneProxy::SetupRayTracingInstances(int32 NumInstances, uint32 DensityIndex)
{
	TArray<FRayTracingWaterData>& WaterDataArray = RayTracingWaterData[DensityIndex];

	if (WaterDataArray.Num() > NumInstances)
	{
		for (int32 Item = NumInstances; Item < WaterDataArray.Num(); Item++)
		{
			auto& WaterItem = WaterDataArray[Item];
			WaterItem.Geometry.ReleaseResource();
			WaterItem.DynamicVertexBuffer.Release();
		}
		WaterDataArray.SetNum(NumInstances);
	}	

	if (WaterDataArray.Num() < NumInstances)
	{
		FRayTracingGeometryInitializer Initializer;
		static const FName DebugName("FWaterMeshSceneProxy");		
		Initializer.IndexBuffer = WaterVertexFactories[DensityIndex]->IndexBuffer->IndexBufferRHI;
		Initializer.GeometryType = RTGT_Triangles;
		Initializer.bFastBuild = true;
		Initializer.bAllowUpdate = true;
		Initializer.TotalPrimitiveCount = 0;

		WaterDataArray.Reserve(NumInstances);
		const int32 StartIndex = WaterDataArray.Num();

		for (int32 Item = StartIndex; Item < NumInstances; Item++)
		{
			FRayTracingWaterData& WaterData = WaterDataArray.AddDefaulted_GetRef();

			Initializer.DebugName = FName(DebugName, Item);

			WaterData.Geometry.SetInitializer(Initializer);
			WaterData.Geometry.InitResource();
		}
	}
}

template <bool bWithWaterSelectionSupport>
class TWaterVertexFactoryUserDataWrapper : public FOneFrameResource
{
public:
	TWaterMeshUserData<bWithWaterSelectionSupport> UserData;
};

void FWaterMeshSceneProxy::GetDynamicRayTracingInstances(FRayTracingMaterialGatheringContext& Context, TArray<FRayTracingInstance>& OutRayTracingInstances)
{
	if (!HasWaterData() || !FWaterUtils::IsWaterMeshRenderingEnabled(/*bIsRenderThread = */true) || !CVarRayTracingGeometryWater.GetValueOnRenderThread())
	{
		return;
	}

	const FSceneView& SceneView = *Context.ReferenceView;
	const FVector ObserverPosition = SceneView.ViewMatrices.GetViewOrigin();

	FWaterLODParams WaterLODParams = GetWaterLODParams(ObserverPosition);

	const int32 NumBuckets = WaterQuadTree.GetWaterMaterials().Num() * DensityCount;

	FWaterQuadTree::FTraversalOutput WaterInstanceData;
	WaterInstanceData.BucketInstanceCounts.Empty(NumBuckets);
	WaterInstanceData.BucketInstanceCounts.AddZeroed(NumBuckets);

	FWaterQuadTree::FTraversalDesc TraversalDesc;
	TraversalDesc.LowestLOD = WaterLODParams.LowestLOD;
	TraversalDesc.HeightMorph = WaterLODParams.HeightLODFactor;
	TraversalDesc.LODCount = WaterQuadTree.GetTreeDepth();
	TraversalDesc.DensityCount = DensityCount;
	TraversalDesc.ForceCollapseDensityLevel = ForceCollapseDensityLevel;
	TraversalDesc.PreViewTranslation = SceneView.ViewMatrices.GetPreViewTranslation();
	TraversalDesc.ObserverPosition = ObserverPosition;
	TraversalDesc.Frustum = FConvexVolume(); // Default volume to disable frustum culling
	TraversalDesc.LODScale = LODScale;
	TraversalDesc.bLODMorphingEnabled = !!CVarWaterMeshLODMorphEnabled.GetValueOnRenderThread();
	TraversalDesc.TessellatedWaterMeshBounds = TessellatedWaterMeshBounds;

	WaterQuadTree.BuildWaterTileInstanceData(TraversalDesc, WaterInstanceData);

	if (WaterInstanceData.InstanceCount == 0)
	{
		// no instance visible, early exit
		return;
	}

	const int32 NumWaterMaterials = WaterQuadTree.GetWaterMaterials().Num();	

	for (int32 DensityIndex = 0; DensityIndex < DensityCount; ++DensityIndex)
	{
		int32 DensityInstanceCount = 0;
		for (int32 MaterialIndex = 0; MaterialIndex < NumWaterMaterials; ++MaterialIndex)
		{
			const int32 BucketIndex = MaterialIndex * DensityCount + DensityIndex;
			const int32 InstanceCount = WaterInstanceData.BucketInstanceCounts[BucketIndex];
			DensityInstanceCount += InstanceCount;
		}

		SetupRayTracingInstances(DensityInstanceCount, DensityIndex);
	}

	// Create per-bucket prefix sum and sort instance data so we can easily access per-instance data for each density
	TArray<int32> BucketOffsets;
	BucketOffsets.SetNumZeroed(NumBuckets);

	for (int32 BucketIndex = 1; BucketIndex < NumBuckets; ++BucketIndex)
	{
		BucketOffsets[BucketIndex] = BucketOffsets[BucketIndex - 1] + WaterInstanceData.BucketInstanceCounts[BucketIndex - 1];
	}
	
	WaterInstanceData.StagingInstanceData.StableSort([](const FWaterQuadTree::FStagingInstanceData& Lhs, const FWaterQuadTree::FStagingInstanceData& Rhs)
		{
			return Lhs.BucketIndex < Rhs.BucketIndex;
		});

	FMeshBatch BaseMesh;
	BaseMesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
	BaseMesh.Type = PT_TriangleList;
	BaseMesh.bUseForMaterial = true;
	BaseMesh.CastShadow = false;
	BaseMesh.CastRayTracedShadow = false;
	BaseMesh.SegmentIndex = 0;
	BaseMesh.Elements.AddZeroed();

	for (int32 DensityIndex = 0; DensityIndex < DensityCount; ++DensityIndex)
	{
		int32 DensityInstanceIndex = 0;
		
		BaseMesh.VertexFactory = WaterVertexFactories[DensityIndex];

		FMeshBatchElement& BatchElement = BaseMesh.Elements[0];

		BatchElement.NumInstances = 1;

		BatchElement.FirstIndex = 0;
		BatchElement.NumPrimitives = WaterVertexFactories[DensityIndex]->IndexBuffer->GetIndexCount() / 3;
		BatchElement.MinVertexIndex = 0;
		BatchElement.MaxVertexIndex = WaterVertexFactories[DensityIndex]->VertexBuffer->GetVertexCount() - 1;

		// Don't use primitive buffer
		BatchElement.IndexBuffer = WaterVertexFactories[DensityIndex]->IndexBuffer;
		BatchElement.PrimitiveIdMode = PrimID_ForceZero;
		BatchElement.PrimitiveUniformBufferResource = &GIdentityPrimitiveUniformBuffer;

		for (int32 MaterialIndex = 0; MaterialIndex < NumWaterMaterials; ++MaterialIndex)
		{
			const int32 BucketIndex = MaterialIndex * DensityCount + DensityIndex;
			const int32 InstanceCount = WaterInstanceData.BucketInstanceCounts[BucketIndex];

			if (!InstanceCount)
			{
				continue;
			}

			const FMaterialRenderProxy* MaterialRenderProxy = WaterQuadTree.GetWaterMaterials()[MaterialIndex];
			check(MaterialRenderProxy != nullptr);

			BaseMesh.MaterialRenderProxy = MaterialRenderProxy;

			for (int32 InstanceIndex = 0; InstanceIndex < InstanceCount; ++InstanceIndex)
			{
				using FWaterVertexFactoryUserDataWrapperType = TWaterVertexFactoryUserDataWrapper<WITH_WATER_SELECTION_SUPPORT>;
				FWaterVertexFactoryUserDataWrapperType& UserDataWrapper = Context.RayTracingMeshResourceCollector.AllocateOneFrameResource<FWaterVertexFactoryUserDataWrapperType>();

				const int32 InstanceDataIndex = BucketOffsets[BucketIndex] + InstanceIndex;
				const FWaterQuadTree::FStagingInstanceData& InstanceData = WaterInstanceData.StagingInstanceData[InstanceDataIndex];

				FWaterVertexFactoryRaytracingParameters UniformBufferParams;
				UniformBufferParams.VertexBuffer = WaterVertexFactories[DensityIndex]->VertexBuffer->GetSRV();
				UniformBufferParams.InstanceData0 = InstanceData.Data[0];
				UniformBufferParams.InstanceData1 = InstanceData.Data[1];

				UserDataWrapper.UserData.InstanceDataBuffers = WaterMeshUserDataBuffers->GetUserData(EWaterMeshRenderGroupType::RG_RenderWaterTiles)->InstanceDataBuffers;
				UserDataWrapper.UserData.RenderGroupType = EWaterMeshRenderGroupType::RG_RenderWaterTiles;
				UserDataWrapper.UserData.WaterVertexFactoryRaytracingVFUniformBuffer = FWaterVertexFactoryRaytracingParametersRef::CreateUniformBufferImmediate(UniformBufferParams, UniformBuffer_SingleFrame);
							
				BatchElement.UserData = (void*)&UserDataWrapper.UserData;							

				FRayTracingWaterData& WaterInstanceRayTracingData = RayTracingWaterData[DensityIndex][DensityInstanceIndex++];

				FRayTracingInstance RayTracingInstance;
				RayTracingInstance.Geometry = &WaterInstanceRayTracingData.Geometry;
				RayTracingInstance.InstanceTransforms.Add(GetLocalToWorld());
				RayTracingInstance.Materials.Add(BaseMesh);
				RayTracingInstance.BuildInstanceMaskAndFlags(GetScene().GetFeatureLevel());
				OutRayTracingInstances.Add(RayTracingInstance);

				Context.DynamicRayTracingGeometriesToUpdate.Add(
					FRayTracingDynamicGeometryUpdateParams
					{
						RayTracingInstance.Materials,
						false,
						uint32(WaterVertexFactories[DensityIndex]->VertexBuffer->GetVertexCount()),
						uint32(WaterVertexFactories[DensityIndex]->VertexBuffer->GetVertexCount() * sizeof(FVector3f)),
						uint32(WaterVertexFactories[DensityIndex]->IndexBuffer->GetIndexCount() / 3),
						&WaterInstanceRayTracingData.Geometry,
						nullptr,
						true
					}
				);				
			}
		}
	}
}
#endif // RHI_RAYTRACING

FPrimitiveViewRelevance FWaterMeshSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View);
	Result.bShadowRelevance = false;
	Result.bDynamicRelevance = true;
	Result.bStaticRelevance = false;
	Result.bRenderInMainPass = ShouldRenderInMainPass();
	Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
	Result.bRenderCustomDepth = ShouldRenderCustomDepth();
	Result.bTranslucentSelfShadow = bCastVolumetricTranslucentShadow;
	MaterialRelevance.SetPrimitiveViewRelevance(Result);
	Result.bVelocityRelevance = DrawsVelocity() && Result.bOpaque && Result.bRenderInMainPass;
	return Result;
}

#if WITH_WATER_SELECTION_SUPPORT
HHitProxy* FWaterMeshSceneProxy::CreateHitProxies(UPrimitiveComponent* Component, TArray<TRefCountPtr<HHitProxy> >& OutHitProxies)
{
	WaterQuadTree.GatherHitProxies(OutHitProxies);

	// No default hit proxy.
	return nullptr;
}
#endif // WITH_WATER_SELECTION_SUPPORT


void FWaterMeshSceneProxy::OnTessellatedWaterMeshBoundsChanged_GameThread(const FBox2D& InTessellatedWaterMeshBounds)
{
	check(IsInParallelGameThread() || IsInGameThread());

	FWaterMeshSceneProxy* SceneProxy = this;
	ENQUEUE_RENDER_COMMAND(OnTessellatedWaterMeshBoundsChanged)(
		[SceneProxy, InTessellatedWaterMeshBounds](FRHICommandListImmediate& RHICmdList)
		{
			SceneProxy->OnTessellatedWaterMeshBoundsChanged_RenderThread(InTessellatedWaterMeshBounds);
		});
}

void FWaterMeshSceneProxy::OnTessellatedWaterMeshBoundsChanged_RenderThread(const FBox2D& InTessellatedWaterMeshBounds)
{
	check(IsInRenderingThread());

	TessellatedWaterMeshBounds = InTessellatedWaterMeshBounds;
}