// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaterBodySceneProxy.h"
#include "WaterSplineMetadata.h"
#include "WaterModule.h"
#include "WaterUtils.h"
#include "WaterBodyComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "SceneInterface.h"
#include "PrimitiveSceneInfo.h"
#include "Algo/Transform.h"

TAutoConsoleVariable<int32> CVarVisualizeWaterInfoSceneProxies(
		TEXT("r.Water.VisualizeWaterInfoSceneProxies"),
		0,
		TEXT("Enables a wireframe visualization mode for the water info scene proxy geometry. Modes: 0 to disable, 1 to show only the selected water body, 2 to show all water bodies."));

FWaterBodySceneProxy::FWaterBodySceneProxy(UWaterBodyComponent* Component)
	: FPrimitiveSceneProxy(Component)
	, WaterBodySectionedLODMesh(GetScene().GetFeatureLevel())
	, WaterBodyInfoMesh(GetScene().GetFeatureLevel())
	, WaterBodyInfoDilatedMesh(GetScene().GetFeatureLevel())
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FWaterBodySceneProxy::FWaterBodySceneProxy);
	WaterBodySectionedLODMesh.InitFromSections(Component->WaterBodyMeshSections);
	WaterBodyInfoMesh.Init(Component->WaterBodyMeshVertices, Component->WaterBodyMeshIndices);
	WaterBodyInfoDilatedMesh.Init(Component->DilatedWaterBodyMeshVertices, Component->DilatedWaterBodyMeshIndices);

	if (UMaterialInstance* WaterInfoMaterialInstance = Component->GetWaterInfoMaterialInstance())
	{
		WaterInfoMaterial = WaterInfoMaterialInstance->GetRenderProxy();
		WaterInfoMaterialRelevance |= WaterInfoMaterialInstance->GetRelevance_Concurrent(GetScene().GetFeatureLevel());
	}
	if (UMaterialInstance* WaterMaterialInstance = Component->GetWaterLODMaterialInstance())
	{
		WaterLODMaterial = WaterMaterialInstance->GetRenderProxy();
		WaterLODMaterialRelevance |= WaterMaterialInstance->GetRelevance_Concurrent(GetScene().GetFeatureLevel());
	}
}

void FWaterBodySceneProxy::FWaterBodySectionedLODMesh::RebuildIndexBuffer(const FBox2D& TessellatedWaterMeshBounds)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FWaterBodySceneProxy::RebuildIndexBuffer);

	TArray<uint32> Indices;
	for (const FWaterBodySectionedMeshProxy& MeshSectionProxy : Sections)
	{
		if (!(TessellatedWaterMeshBounds.IsInsideOrOn(MeshSectionProxy.Bounds.Min) && TessellatedWaterMeshBounds.IsInsideOrOn(MeshSectionProxy.Bounds.Max)))
		{
			Indices.Append(MeshSectionProxy.Indices);
		}
	}

	IndexBuffer.Indices = MoveTemp(Indices);

	if (IndexBuffer.Indices.Num() > 0)
	{
		if (IndexBuffer.IsInitialized())
		{
			BeginUpdateResourceRHI(&IndexBuffer);
		}
		else
		{
			BeginInitResource(&IndexBuffer);

			bInitialized = true;
		}
	}
}

void FWaterBodySceneProxy::FWaterBodySectionedLODMesh::InitFromSections(const TArray<FWaterBodyMeshSection>& MeshSections)
{
	TArray<FDynamicMeshVertex> Vertices;
	for (const FWaterBodyMeshSection& MeshSection : MeshSections)
	{
		if (MeshSection.Vertices.Num() > 0 && MeshSection.Indices.Num() > 0)
		{
			FWaterBodySectionedMeshProxy& NewSection = Sections.Emplace_GetRef(MeshSection.SectionBounds);
			
			const uint32 BaseIndex = Vertices.Num();
			TArray<uint32, TInlineAllocator<6>> Indices;
			Algo::Transform(MeshSection.Indices, Indices, [BaseIndex](const uint32 Index) { return Index + BaseIndex; });

			NewSection.Indices = MoveTemp(Indices);

			Vertices.Append(MeshSection.Vertices);
		}
	}

	if (Vertices.Num() > 0)
	{
		VertexBuffers.InitFromDynamicVertex(&VertexFactory, Vertices);
		BeginInitResource(&VertexBuffers.PositionVertexBuffer);
		BeginInitResource(&VertexBuffers.StaticMeshVertexBuffer);
		BeginInitResource(&VertexBuffers.ColorVertexBuffer);
		BeginInitResource(&VertexFactory);
	}
}

void FWaterBodySceneProxy::FWaterBodyMesh::Init(TArray<FDynamicMeshVertex>& Vertices, TArray<uint32>& Indices)
{
	if (Indices.Num() > 0)
	{
		IndexBuffer.Indices = Indices;
		VertexBuffers.InitFromDynamicVertex(&VertexFactory, Vertices);
		BeginInitResource(&VertexBuffers.PositionVertexBuffer);
		BeginInitResource(&VertexBuffers.StaticMeshVertexBuffer);
		BeginInitResource(&VertexBuffers.ColorVertexBuffer);
		BeginInitResource(&VertexFactory);
		BeginInitResource(&IndexBuffer);
	}
}

void FWaterBodySceneProxy::FWaterBodySectionedLODMesh::ReleaseResources()
{
	Sections.Empty();
	VertexBuffers.PositionVertexBuffer.ReleaseResource();
	VertexBuffers.StaticMeshVertexBuffer.ReleaseResource();
	VertexBuffers.ColorVertexBuffer.ReleaseResource();
	VertexFactory.ReleaseResource();
	IndexBuffer.ReleaseResource();
}

void FWaterBodySceneProxy::FWaterBodyMesh::ReleaseResources()
{
	VertexBuffers.PositionVertexBuffer.ReleaseResource();
	VertexBuffers.StaticMeshVertexBuffer.ReleaseResource();
	VertexBuffers.ColorVertexBuffer.ReleaseResource();
	VertexFactory.ReleaseResource();
	IndexBuffer.ReleaseResource();
}

bool FWaterBodySceneProxy::FWaterBodyMesh::GetMeshElements(FMeshBatch& OutMeshBatch, uint8 DepthPriorityGroup, bool bUseReverseCulling) const
{
	uint32 FirstIndex = 0;
	uint32 NumPrimitives = IndexBuffer.Indices.Num() / 3;
	if (NumPrimitives == 0)
	{
		return false;
	}

	OutMeshBatch.VertexFactory = &VertexFactory;
	OutMeshBatch.ReverseCulling = bUseReverseCulling;
	OutMeshBatch.Type = PT_TriangleList;
	OutMeshBatch.DepthPriorityGroup = DepthPriorityGroup;
	OutMeshBatch.bCanApplyViewModeOverrides = false;

	FMeshBatchElement& BatchElement = OutMeshBatch.Elements[0];
	BatchElement.IndexBuffer = &IndexBuffer;
	BatchElement.FirstIndex = FirstIndex;
	BatchElement.NumPrimitives = NumPrimitives;
	check(BatchElement.NumPrimitives != 0);
	BatchElement.MinVertexIndex = 0;
	BatchElement.MaxVertexIndex = VertexBuffers.PositionVertexBuffer.GetNumVertices() - 1;

	return true;
}

uint32 FWaterBodySceneProxy::FWaterBodySectionedLODMesh::GetAllocatedSize() const
{
	return Sections.GetAllocatedSize();
}

bool FWaterBodySceneProxy::FWaterBodySectionedLODMesh::GetMeshElements(FMeshBatch& OutMeshBatch, uint8 DepthPriorityGroup, bool bUseReverseCulling) const
{
	if (!bInitialized)
	{
		return false;
	}
	const int32 NumPrimitives = IndexBuffer.Indices.Num() / 3;

	if (NumPrimitives == 0)
	{
		return false;
	}

	OutMeshBatch.VertexFactory = &VertexFactory;
	OutMeshBatch.ReverseCulling = bUseReverseCulling;
	OutMeshBatch.Type = PT_TriangleList;
	OutMeshBatch.DepthPriorityGroup = DepthPriorityGroup;
	OutMeshBatch.bCanApplyViewModeOverrides = false;
	OutMeshBatch.LODIndex = 0;
	OutMeshBatch.bUseForDepthPass = true;

	FMeshBatchElement& BatchElement = OutMeshBatch.Elements[0];
	BatchElement.IndexBuffer = &IndexBuffer;
	BatchElement.FirstIndex = 0;
	BatchElement.NumPrimitives = NumPrimitives;
	check(BatchElement.NumPrimitives != 0);
	BatchElement.MinVertexIndex = 0;
	BatchElement.MaxVertexIndex = VertexBuffers.PositionVertexBuffer.GetNumVertices() - 1;

	return true;
}

FWaterBodySceneProxy::~FWaterBodySceneProxy()
{
	WaterBodyInfoMesh.ReleaseResources();
	WaterBodyInfoDilatedMesh.ReleaseResources();
	WaterBodySectionedLODMesh.ReleaseResources();
}

void FWaterBodySceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FWaterBodySceneProxy::GetDynamicMeshElements);

	const bool bWithinWaterInfoPasses = CurrentWaterInfoPass != EWaterInfoPass::None;

#if !UE_BUILD_SHIPPING
	const int VisualizeWaterInfoMode = CVarVisualizeWaterInfoSceneProxies.GetValueOnRenderThread();
#else
	const int VisualizeWaterInfoMode = 0;
#endif // !UE_BUILD_SHIPPING

	const bool bVisualizeWaterInfoMesh = (VisualizeWaterInfoMode != 0) && !bWithinWaterInfoPasses;

	FMaterialRenderProxy* MaterialToUse = bWithinWaterInfoPasses ? WaterInfoMaterial : WaterLODMaterial;

	if (!MaterialToUse)
	{
		FColoredMaterialRenderProxy* FallbackMaterial = new FColoredMaterialRenderProxy(
			GEngine->DebugMeshMaterial ? GEngine->DebugMeshMaterial->GetRenderProxy() : nullptr,
			FLinearColor(1.f, 1.f, 1.f));

		Collector.RegisterOneFrameMaterialProxy(FallbackMaterial);

		MaterialToUse = FallbackMaterial;
	}

	// If we are not in the waterinfo pass and the cvar is not set to show opaque bodies, we should be in wireframe
	const bool bWireframe = AllowDebugViewmodes() && (ViewFamily.EngineShowFlags.Wireframe || bVisualizeWaterInfoMesh);

	if (bWireframe)
	{
		FColoredMaterialRenderProxy* WireframeMaterialInstance= new FColoredMaterialRenderProxy(
			GEngine->WireframeMaterial ? GEngine->WireframeMaterial->GetRenderProxy() : NULL,
			FLinearColor(0, 0.5f, 1.f)
			);

		Collector.RegisterOneFrameMaterialProxy(WireframeMaterialInstance);
		MaterialToUse = WireframeMaterialInstance;
	}

	const bool bHasDilatedSections = WaterBodyInfoDilatedMesh.IndexBuffer.Indices.Num() > 0;
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FWaterBodySceneProxy::AddSectionsForView);

		if (CurrentWaterInfoPass == EWaterInfoPass::Color)
		{
			FMeshBatch& MeshBatch = Collector.AllocateMesh();
			MeshBatch.MaterialRenderProxy = MaterialToUse;
			MeshBatch.bWireframe = bWireframe;
			MeshBatch.bUseForDepthPass = true;
			if (WaterBodyInfoMesh.GetMeshElements(MeshBatch, SDPG_World, IsLocalToWorldDeterminantNegative()))
			{
				Collector.AddMesh(ViewIndex, MeshBatch);
			}
		}
		else if ((CurrentWaterInfoPass == EWaterInfoPass::Dilation) && bHasDilatedSections)
		{
			FMeshBatch& MeshBatch = Collector.AllocateMesh();
			MeshBatch.MaterialRenderProxy = MaterialToUse;
			MeshBatch.bWireframe = bWireframe;
			MeshBatch.bUseForDepthPass = true;
			if (WaterBodyInfoDilatedMesh.GetMeshElements(MeshBatch, SDPG_World, IsLocalToWorldDeterminantNegative()))
			{
				Collector.AddMesh(ViewIndex, MeshBatch);
			}
		}
		else if (CurrentWaterInfoPass == EWaterInfoPass::None)
		{
			FMeshBatch& MeshBatch = Collector.AllocateMesh();
			MeshBatch.MaterialRenderProxy = MaterialToUse;
			MeshBatch.bWireframe = bWireframe;
			if (WaterBodySectionedLODMesh.GetMeshElements(MeshBatch, SDPG_World, IsLocalToWorldDeterminantNegative()))
			{
				Collector.AddMesh(ViewIndex, MeshBatch);
			}
#if !UE_BUILD_SHIPPING
			if (bVisualizeWaterInfoMesh && (VisualizeWaterInfoMode > 1 || IsSelected()))
			{
				FMeshBatch& WireframeMeshBatch = Collector.AllocateMesh();
				WireframeMeshBatch.MaterialRenderProxy = MaterialToUse;
				WireframeMeshBatch.bWireframe = bWireframe;
				if (WaterBodyInfoMesh.GetMeshElements(WireframeMeshBatch, SDPG_World, IsLocalToWorldDeterminantNegative()))
				{
					Collector.AddMesh(ViewIndex, WireframeMeshBatch);
				}
			}
#endif // !UE_BUILD_SHIPPING
		}
	}
}

FPrimitiveViewRelevance FWaterBodySceneProxy::GetViewRelevance(const FSceneView* View) const
{
	FPrimitiveViewRelevance Result;
	Result.bDrawRelevance = IsShown(View);
	Result.bShadowRelevance = IsShadowCast(View);
	Result.bDynamicRelevance = true;
	Result.bStaticRelevance = false;
	Result.bRenderInDepthPass = true;
	Result.bRenderInMainPass = ShouldRenderInMainPass();
	Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
	Result.bRenderCustomDepth = ShouldRenderCustomDepth();
	Result.bTranslucentSelfShadow = bCastVolumetricTranslucentShadow;

	const bool bWithinWaterInfoPasses = CurrentWaterInfoPass != EWaterInfoPass::None;
	if (bWithinWaterInfoPasses)
	{
		WaterInfoMaterialRelevance.SetPrimitiveViewRelevance(Result);
	}
	else
	{
		WaterLODMaterialRelevance.SetPrimitiveViewRelevance(Result);
	}

	Result.bVelocityRelevance = DrawsVelocity() && Result.bOpaque && Result.bRenderInMainPass;
	return Result;
}

SIZE_T FWaterBodySceneProxy::GetTypeHash() const
{
	static size_t UniquePtr;
	return reinterpret_cast<size_t>(&UniquePtr);
}

uint32 FWaterBodySceneProxy::GetMemoryFootprint() const 
{
	return (sizeof(*this) + GetAllocatedSize());
}

uint32 FWaterBodySceneProxy::GetAllocatedSize() const
{
	return FPrimitiveSceneProxy::GetAllocatedSize()
		+ WaterBodySectionedLODMesh.GetAllocatedSize();
}

bool FWaterBodySceneProxy::IsShown(const FSceneView* View) const
{
	return FPrimitiveSceneProxy::IsShown(View);
}

void FWaterBodySceneProxy::OnTessellatedWaterMeshBoundsChanged_GameThread(const FBox2D& TessellatedWaterMeshBounds)
{
	check(IsInParallelGameThread() || IsInGameThread());

	FWaterBodySceneProxy* SceneProxy = this;
	ENQUEUE_RENDER_COMMAND(SetNewActiveWaterInfoGridArea)(
		[SceneProxy, TessellatedWaterMeshBounds](FRHICommandListImmediate& RHICmdList)
		{
			SceneProxy->OnTessellatedWaterMeshBoundsChanged_RenderThread(TessellatedWaterMeshBounds);
		});
}

void FWaterBodySceneProxy::OnTessellatedWaterMeshBoundsChanged_RenderThread(const FBox2D& TessellatedWaterMeshBounds)
{
	check(IsInRenderingThread());

	WaterBodySectionedLODMesh.RebuildIndexBuffer(TessellatedWaterMeshBounds);
}
