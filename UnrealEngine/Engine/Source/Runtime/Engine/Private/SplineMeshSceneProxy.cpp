// Copyright Epic Games, Inc. All Rights Reserved.

#include "SplineMeshSceneProxy.h"
#include "Materials/Material.h"
#include "MeshMaterialShader.h"

IMPLEMENT_TYPE_LAYOUT(FSplineMeshVertexFactoryShaderParameters);

bool FSplineMeshVertexFactory::ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters)
{
	return (Parameters.MaterialParameters.bIsUsedWithSplineMeshes || Parameters.MaterialParameters.bIsSpecialEngineMaterial)
		&& FLocalVertexFactory::ShouldCompilePermutation(Parameters);
}

/** Modify compile environment to enable spline deformation */
void FSplineMeshVertexFactory::ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	const bool ContainsManualVertexFetch = OutEnvironment.GetDefinitions().Contains("MANUAL_VERTEX_FETCH");
	if (!ContainsManualVertexFetch)
	{
		OutEnvironment.SetDefine(TEXT("MANUAL_VERTEX_FETCH"), TEXT("0"));
	}

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
	bSupportsMeshCardRepresentation = false;

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