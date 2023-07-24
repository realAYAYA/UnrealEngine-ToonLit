// Copyright Epic Games, Inc. All Rights Reserved.

#include "SplineMeshSceneProxy.h"
#include "Materials/Material.h"
#include "MaterialDomain.h"
#include "MeshMaterialShader.h"
#include "Misc/DelayedAutoRegister.h"
#include "StaticMeshComponentLODInfo.h"
#include "StaticMeshResources.h"
#include "RayTracingInstance.h"

IMPLEMENT_TYPE_LAYOUT(FSplineMeshVertexFactoryShaderParameters);

static TAutoConsoleVariable<int32> CVarRayTracingSplineMeshes(
	TEXT("r.RayTracing.Geometry.SplineMeshes"),
	1,
	TEXT("Include splines meshes in ray tracing effects (default = 1 (spline meshes enabled in ray tracing))"));


bool FSplineMeshVertexFactory::ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters)
{
	return (Parameters.MaterialParameters.bIsUsedWithSplineMeshes || Parameters.MaterialParameters.bIsSpecialEngineMaterial)
		&& FLocalVertexFactory::ShouldCompilePermutation(Parameters);
}

/** Modify compile environment to enable spline deformation */
void FSplineMeshVertexFactory::ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	OutEnvironment.SetDefine(TEXT("VF_SUPPORTS_SPEEDTREE_WIND"), TEXT("0"));
	FLocalVertexFactory::ModifyCompilationEnvironment(Parameters, OutEnvironment);

	OutEnvironment.SetDefine(TEXT("USE_SPLINEDEFORM"), TEXT("1"));
}

/**
 * FSplineMeshVertexFactory does not support manual vertex fetch yet so worst case element set is returned to make sure the PSO can be compiled
 */
void FSplineMeshVertexFactory::GetPSOPrecacheVertexFetchElements(EVertexInputStreamType VertexInputStreamType, FVertexDeclarationElementList& Elements)
{
	if (VertexInputStreamType == EVertexInputStreamType::PositionOnly || VertexInputStreamType == EVertexInputStreamType::PositionAndNormalOnly)
	{
		FLocalVertexFactory::GetPSOPrecacheVertexFetchElements(VertexInputStreamType, Elements);
	}
	else
	{
		// Position
		Elements.Add(FVertexElement(0, 0, VET_Float3, 0, 12, false));

		// Normals
		Elements.Add(FVertexElement(1, 0, VET_PackedNormal, 1, 0, false));
		Elements.Add(FVertexElement(2, 0, VET_PackedNormal, 2, 0, false));

		// Color
		Elements.Add(FVertexElement(3, 0, VET_Color, 3, 0, false));

		// Texcoords
		Elements.Add(FVertexElement(4, 0, VET_Float4, 4, 0, false));
		Elements.Add(FVertexElement(5, 0, VET_Float4, 5, 0, false));
		Elements.Add(FVertexElement(6, 0, VET_Float4, 6, 0, false));
		Elements.Add(FVertexElement(7, 0, VET_Float4, 7, 0, false));

		// Lightmap coords
		Elements.Add(FVertexElement(8, 0, VET_Float2, 15, 0, false));
		
		// Primitive ID
		Elements.Add(FVertexElement(9, 0, VET_UInt, 13, 0, true));
	}
}

FSplineMeshSceneProxy::FSplineMeshSceneProxy(USplineMeshComponent* InComponent) :
	FStaticMeshSceneProxy(InComponent, false)
{
	bSupportsDistanceFieldRepresentation = false;

	// Dynamic draw path without Nanite isn't supported by Lumen
	bVisibleInLumenScene = false;

#if RHI_RAYTRACING
	bDynamicRayTracingGeometry = true;
#endif

	// make sure all the materials are okay to be rendered as a spline mesh
	for (FStaticMeshSceneProxy::FLODInfo& LODInfo : LODs)
	{
		for (FStaticMeshSceneProxy::FLODInfo::FSectionInfo& Section : LODInfo.Sections)
		{
			if (!Section.Material->CheckMaterialUsage_Concurrent(MATUSAGE_SplineMesh))
			{
				Section.Material = UMaterial::GetDefaultMaterial(MD_Surface);
			}
		}
	}

	// Copy spline params from component
	SplineParams = InComponent->SplineParams;
	SplineUpDir = InComponent->SplineUpDir;
	bSmoothInterpRollScale = InComponent->bSmoothInterpRollScale;
	ForwardAxis = InComponent->ForwardAxis;

	// Fill in info about the mesh
	InComponent->CalculateScaleZAndMinZ(SplineMeshScaleZ, SplineMeshMinZ);

	for (int32 LODIndex = 0; LODIndex < LODs.Num(); LODIndex++)
	{
		InitVertexFactory(InComponent, LODIndex, nullptr); // we always need this one for shadows etc
		if (InComponent->LODData.IsValidIndex(LODIndex) && InComponent->LODData[LODIndex].OverrideVertexColors)
		{
			InitVertexFactory(InComponent, LODIndex, InComponent->LODData[LODIndex].OverrideVertexColors);
		}
	}
}

SIZE_T FSplineMeshSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

void FSplineMeshSceneProxy::SetupMeshBatchForSpline(int32 InLODIndex, FMeshBatch& OutMeshBatch) const
{
	const FStaticMeshVertexFactories& VFs = RenderData->LODVertexFactories[InLODIndex];
	check(OutMeshBatch.Elements.Num() == 1);
	OutMeshBatch.VertexFactory = OutMeshBatch.Elements[0].bUserDataIsColorVertexBuffer ? VFs.SplineVertexFactoryOverrideColorVertexBuffer : VFs.SplineVertexFactory;
	check(OutMeshBatch.VertexFactory);
	OutMeshBatch.Elements[0].SplineMeshSceneProxy = const_cast<FSplineMeshSceneProxy*>(this);
	OutMeshBatch.Elements[0].bIsSplineProxy = true;
	OutMeshBatch.Elements[0].PrimitiveUniformBuffer = GetUniformBuffer();
	OutMeshBatch.ReverseCulling ^= (SplineParams.StartScale.X < 0) ^ (SplineParams.StartScale.Y < 0);
}

bool FSplineMeshSceneProxy::GetShadowMeshElement(int32 LODIndex, int32 BatchIndex, uint8 InDepthPriorityGroup, FMeshBatch& OutMeshBatch, bool bDitheredLODTransition) const
{
	//checkf(LODIndex == 0, TEXT("Getting spline static mesh element with invalid LOD [%d]"), LODIndex);

	if (FStaticMeshSceneProxy::GetShadowMeshElement(LODIndex, BatchIndex, InDepthPriorityGroup, OutMeshBatch, bDitheredLODTransition))
	{
		SetupMeshBatchForSpline(LODIndex, OutMeshBatch);
		return true;
	}
	return false;
}

bool FSplineMeshSceneProxy::GetMeshElement(int32 LODIndex, int32 BatchIndex, int32 SectionIndex, uint8 InDepthPriorityGroup, bool bUseSelectionOutline, bool bAllowPreCulledIndices, FMeshBatch& OutMeshBatch) const
{
	//checkf(LODIndex == 0 /*&& SectionIndex == 0*/, TEXT("Getting spline static mesh element with invalid params [%d, %d]"), LODIndex, SectionIndex);

	if (FStaticMeshSceneProxy::GetMeshElement(LODIndex, BatchIndex, SectionIndex, InDepthPriorityGroup, bUseSelectionOutline, bAllowPreCulledIndices, OutMeshBatch))
	{
		SetupMeshBatchForSpline(LODIndex, OutMeshBatch);
		return true;
	}
	return false;
}

bool FSplineMeshSceneProxy::GetWireframeMeshElement(int32 LODIndex, int32 BatchIndex, const FMaterialRenderProxy* WireframeRenderProxy, uint8 InDepthPriorityGroup, bool bAllowPreCulledIndices, FMeshBatch& OutMeshBatch) const
{
	//checkf(LODIndex == 0, TEXT("Getting spline static mesh element with invalid LOD [%d]"), LODIndex);

	if (FStaticMeshSceneProxy::GetWireframeMeshElement(LODIndex, BatchIndex, WireframeRenderProxy, InDepthPriorityGroup, bAllowPreCulledIndices, OutMeshBatch))
	{
		SetupMeshBatchForSpline(LODIndex, OutMeshBatch);
		return true;
	}
	return false;
}


bool FSplineMeshSceneProxy::GetCollisionMeshElement(int32 LODIndex, int32 BatchIndex, int32 SectionIndex, uint8 InDepthPriorityGroup, const FMaterialRenderProxy* RenderProxy, FMeshBatch& OutMeshBatch) const
{
	//checkf(LODIndex == 0, TEXT("Getting spline static mesh element with invalid LOD [%d]"), LODIndex);

	if (FStaticMeshSceneProxy::GetCollisionMeshElement(LODIndex, BatchIndex, SectionIndex, InDepthPriorityGroup, RenderProxy, OutMeshBatch))
	{
		SetupMeshBatchForSpline(LODIndex, OutMeshBatch);
		return true;
	}
	return false;
}

#if RHI_RAYTRACING
void FSplineMeshSceneProxy::GetDynamicRayTracingInstances(struct FRayTracingMaterialGatheringContext& Context, TArray<FRayTracingInstance>& OutRayTracingInstances)
{
	if (CVarRayTracingSplineMeshes.GetValueOnRenderThread() == 0)
	{
		return;
	}

	ESceneDepthPriorityGroup PrimitiveDPG = GetStaticDepthPriorityGroup();
	const int32 LODIndex = FMath::Max(GetLOD(Context.ReferenceView), (int32)GetCurrentFirstLODIdx_RenderThread());
	const FStaticMeshLODResources& LODModel = RenderData->LODResources[LODIndex];

	FRayTracingGeometry& Geometry = DynamicRayTracingGeometries[LODIndex];

	FRayTracingInstance& RayTracingInstance = OutRayTracingInstances.AddDefaulted_GetRef();

	const int32 NumBatches = GetNumMeshBatches();
	const int32 NumRayTracingMaterialEntries = LODModel.Sections.Num() * NumBatches;

	if (NumRayTracingMaterialEntries != CachedRayTracingMaterials.Num() || CachedRayTracingMaterialsLODIndex != LODIndex)
	{
		CachedRayTracingMaterials.Reset();
		CachedRayTracingMaterials.Reserve(NumRayTracingMaterialEntries);

		for (int32 BatchIndex = 0; BatchIndex < NumBatches; BatchIndex++)
		{
			for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++)
			{
				FMeshBatch& MeshBatch = CachedRayTracingMaterials.AddDefaulted_GetRef();

				bool bResult = GetMeshElement(LODIndex, BatchIndex, SectionIndex, PrimitiveDPG, false, false, MeshBatch);
				if (!bResult)
				{
					// Hidden material
					MeshBatch.MaterialRenderProxy = UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();
					MeshBatch.VertexFactory = &RenderData->LODVertexFactories[LODIndex].VertexFactory;
				}

				MeshBatch.SegmentIndex = SectionIndex;
				MeshBatch.MeshIdInPrimitive = SectionIndex;
			}
		}

		CachedRayTracingMaterialsLODIndex = LODIndex;
	}

	RayTracingInstance.Geometry = &Geometry;
	// scene proxies live for the duration of Render(), making array views below safe
	const FMatrix& ThisLocalToWorld = GetLocalToWorld();
	RayTracingInstance.InstanceTransformsView = MakeArrayView(&ThisLocalToWorld, 1);
	RayTracingInstance.MaterialsView = MakeArrayView(CachedRayTracingMaterials);
	CachedRayTracingInstanceMaskAndFlags = Context.BuildInstanceMaskAndFlags(RayTracingInstance, *this);

	if (RenderData->LODVertexFactories[LODIndex].VertexFactory.GetType()->SupportsRayTracingDynamicGeometry())
	{
		Context.DynamicRayTracingGeometriesToUpdate.Add(
			FRayTracingDynamicGeometryUpdateParams
			{
				CachedRayTracingMaterials, // TODO: this copy can be avoided if FRayTracingDynamicGeometryUpdateParams supported array views
				false,
				(uint32)LODModel.GetNumVertices(),
				uint32((SIZE_T)LODModel.GetNumVertices() * sizeof(FVector3f)),
				Geometry.Initializer.TotalPrimitiveCount,
				&Geometry,
				nullptr /* VertexBuffer */,
				true
			}
		);
	}

	check(CachedRayTracingMaterials.Num() == RayTracingInstance.GetMaterials().Num());
	checkf(RayTracingInstance.Geometry->Initializer.Segments.Num() == CachedRayTracingMaterials.Num(), TEXT("Segments/Materials mismatch. Number of segments: %d. Number of Materials: %d. LOD Index: %d"),
		RayTracingInstance.Geometry->Initializer.Segments.Num(),
		CachedRayTracingMaterials.Num(),
		LODIndex);
}
#endif // RHI_RAYTRACING
