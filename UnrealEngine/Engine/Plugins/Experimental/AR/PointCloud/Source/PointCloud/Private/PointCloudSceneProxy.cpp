// Copyright Epic Games, Inc. All Rights Reserved.

#include "PointCloudSceneProxy.h"
#include "PointCloud.h"
#include "PointCloudComponent.h"

#include "PrimitiveViewRelevance.h"
#include "RHI.h"
#include "RenderResource.h"
#include "RenderingThread.h"
#include "Containers/ResourceArray.h"
#include "EngineGlobals.h"
#include "VertexFactory.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "LocalVertexFactory.h"
#include "Engine/Engine.h"
#include "SceneManagement.h"
#include "DynamicMeshBuilder.h"

DECLARE_CYCLE_STAT(TEXT("Update Point Cloud GT"), STAT_PointCloud_UpdatePointCloud, STATGROUP_PointCloud);
DECLARE_CYCLE_STAT(TEXT("Get Mesh Elements"), STAT_PointCloud_GetMeshElements, STATGROUP_PointCloud);
DECLARE_CYCLE_STAT(TEXT("Create RT Resources"), STAT_PointCloud_CreateRenderThreadResources, STATGROUP_PointCloud);


FNoFetchPointCloudSceneProxy::FNoFetchPointCloudSceneProxy(UPointCloudComponent* Component)
	: FPrimitiveSceneProxy(Component)
	, Points(Component->PointCloud)
	, Color(Component->PointColor)
	, Size(Component->PointSize)
	, bIsVisible(Component->bIsVisible)
	, SpriteTexture(Component->SpriteTexture)
	, MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
{
	if (Size <= 0.f)
	{
		Size = 1.f;
	}
}

FNoFetchPointCloudSceneProxy::~FNoFetchPointCloudSceneProxy()
{
}

SIZE_T FNoFetchPointCloudSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

void FNoFetchPointCloudSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	SCOPE_CYCLE_COUNTER(STAT_PointCloud_GetMeshElements);
	
	if (!bIsVisible)
	{
		return;
	}

	const bool bIsWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			if (bIsWireframe)
			{
				// Draw bounds around the points
				RenderBounds(Collector.GetPDI(ViewIndex), ViewFamily.EngineShowFlags, GetBounds(), IsSelected());
			}
			FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
			// Loop through manually drawing the points since PT_PointList isn't really a supported primitive type
			if (SpriteTexture && SpriteTexture->GetResource())
			{
				const auto SpriteSizeX = Size / (float)SpriteTexture->GetResource()->GetSizeX();
				const auto SpriteSizeY = Size / (float)SpriteTexture->GetResource()->GetSizeY();
				for (const FVector& Point : Points)
				{
					PDI->DrawSprite(Point, SpriteSizeX, SpriteSizeY, SpriteTexture->GetResource(), Color, SDPG_World, 0.f, 0.f, 0.f, 0.f, SE_BLEND_Masked);
				}
			}
			else
			{
				for (const FVector& Point : Points)
				{
					PDI->DrawPoint(Point, Color, Size, SDPG_World);
				}
			}
		}
	}
}

FPrimitiveViewRelevance FNoFetchPointCloudSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View) && bIsVisible;
	Result.bShadowRelevance = false;
	Result.bDynamicRelevance = true;
	Result.bRenderInMainPass = true;
	Result.bUsesLightingChannels = false;
	Result.bRenderCustomDepth = false;
	MaterialRelevance.SetPrimitiveViewRelevance(Result);
	return Result;
}

uint32 FNoFetchPointCloudSceneProxy::GetMemoryFootprint(void) const
{
	return(sizeof(*this) + GetAllocatedSize());
}

uint32 FNoFetchPointCloudSceneProxy::GetAllocatedSize(void) const
{
	return FPrimitiveSceneProxy::GetAllocatedSize();
}


FPointCloudSceneProxy::FPointCloudSceneProxy(UPointCloudComponent* Component)
	: FPrimitiveSceneProxy(Component)
	, bIsVisible(Component->bIsVisible)
	, Points(Component->PointCloud)
	, Colors(Component->PointColors)
	, PointColor(Component->PointColor)
	, PointSize(Component->PointSize)
	, Material(Component->PointCloudMaterial)
	, MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
	, PointCloudVertexFactory(GetScene().GetFeatureLevel())
{
	if (PointSize <= 0.f)
	{
		PointSize = 1.f;
	}
}

FPointCloudSceneProxy::~FPointCloudSceneProxy()
{
	PointCloudVertexFactory.ReleaseResource();
	PointCloudIndexBuffer.ReleaseResource();
	PointCloudColorVertexBuffer.ReleaseResource();
	PointCloudLocationVertexBuffer.ReleaseResource();
}

void FPointCloudSceneProxy::CreateRenderThreadResources()
{
	SCOPE_CYCLE_COUNTER(STAT_PointCloud_CreateRenderThreadResources);

	PointCloudVertexFactory.InitResource();
	PointCloudIndexBuffer.InitRHIWithSize(Points.Num());

	// We either use a single color or the color array
	if (Colors.Num())
	{
		PointCloudColorVertexBuffer.InitRHIWith(Colors);
		Colors.Empty();
	}
	else
	{
		PointCloudColorVertexBuffer.InitRHIWith(PointColor.ToFColor(false));
	}

	PointCloudLocationVertexBuffer.InitRHIWith(Points);
	Points.Empty();

	// Setup the vertex factory shader parameters
	FPointCloudVertexFactoryParameters UniformParameters;
	UniformParameters.VertexFetch_PointLocationBuffer = PointCloudLocationVertexBuffer.GetBufferSRV();
	UniformParameters.VertexFetch_PointColorBuffer = PointCloudColorVertexBuffer.GetBufferSRV();
	PointCloudVertexFactory.SetParameters(UniformParameters, PointCloudColorVertexBuffer.GetColorMask(), PointSize);
}

SIZE_T FPointCloudSceneProxy::GetTypeHash() const
{
	static size_t UniquePointer;
	return reinterpret_cast<size_t>(&UniquePointer);
}

void FPointCloudSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	SCOPE_CYCLE_COUNTER(STAT_PointCloud_GetMeshElements);

	if (!bIsVisible)
	{
		return;
	}

	const bool bIsWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;
	FMaterialRenderProxy* MaterialProxy = Material->GetRenderProxy();
	if (bIsWireframe)
	{
		FMaterialRenderProxy* WireframeMaterialInstance = new FColoredMaterialRenderProxy(
				GEngine->WireframeMaterial->GetRenderProxy(),
				FLinearColor(0, 0.5f, 1.f)
		);
		Collector.RegisterOneFrameMaterialProxy(WireframeMaterialInstance);
		MaterialProxy = WireframeMaterialInstance;
	}
	// Nothing to render with
	if (MaterialProxy == nullptr)
	{
		return;
	}

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			if (bIsWireframe)
			{
				// Draw bounds around the points
				RenderBounds(Collector.GetPDI(ViewIndex), ViewFamily.EngineShowFlags, GetBounds(), IsSelected());
			}

			// Create a mesh batch for this chunk of point cloud
			FMeshBatch& MeshBatch = Collector.AllocateMesh();
			MeshBatch.CastShadow = false;
			MeshBatch.bUseAsOccluder = false;
			MeshBatch.VertexFactory = &PointCloudVertexFactory;
			MeshBatch.MaterialRenderProxy = MaterialProxy;
			MeshBatch.ReverseCulling = IsLocalToWorldDeterminantNegative();
			MeshBatch.DepthPriorityGroup = SDPG_World;

			// Set up index buffer
			MeshBatch.Type = PointCloudIndexBuffer.IsTriList() ? PT_TriangleList : PT_QuadList;
			FMeshBatchElement& BatchElement = MeshBatch.Elements[0];
			BatchElement.FirstIndex = 0;
			BatchElement.NumPrimitives = PointCloudIndexBuffer.GetNumPrimitives();
			BatchElement.MinVertexIndex = 0;
			BatchElement.MaxVertexIndex = PointCloudIndexBuffer.GetMaxIndex();
			BatchElement.IndexBuffer = &PointCloudIndexBuffer;
			BatchElement.PrimitiveUniformBufferResource = &GIdentityPrimitiveUniformBuffer;

			MeshBatch.bCanApplyViewModeOverrides = false;
			Collector.AddMesh(ViewIndex, MeshBatch);
		}
	}
}

FPrimitiveViewRelevance FPointCloudSceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View) && bIsVisible;
	Result.bShadowRelevance = false;
	Result.bDynamicRelevance = true;
	Result.bRenderInMainPass = true;
	Result.bUsesLightingChannels = false;
	Result.bRenderCustomDepth = false;
	MaterialRelevance.SetPrimitiveViewRelevance(Result);
	return Result;
}
