// Copyright Epic Games, Inc. All Rights Reserved. 

#include "Components/BaseDynamicMeshSceneProxy.h"
#include "Materials/Material.h"
#include "RayTracingDefinitions.h"
#include "RayTracingInstance.h"

FBaseDynamicMeshSceneProxy::FBaseDynamicMeshSceneProxy(UBaseDynamicMeshComponent* Component)
	: FPrimitiveSceneProxy(Component),
	ParentBaseComponent(Component),
	bEnableRaytracing(Component->GetEnableRaytracing()),
	bEnableViewModeOverrides(Component->GetViewModeOverridesEnabled())
{
	if (Component->GetColorOverrideMode() == EDynamicMeshComponentColorOverrideMode::Constant)
	{
		ConstantVertexColor = Component->GetConstantOverrideColor();
		bIgnoreVertexColors = true;
	}

	bUsePerTriangleNormals = Component->GetFlatShadingEnabled();
}

FBaseDynamicMeshSceneProxy::~FBaseDynamicMeshSceneProxy()
{
	// we are assuming in code below that this is always called from the rendering thread
	check(IsInRenderingThread());

	// destroy all existing renderbuffers
	for (FMeshRenderBufferSet* BufferSet : AllocatedBufferSets)
	{
		FMeshRenderBufferSet::DestroyRenderBufferSet(BufferSet);
	}
}

FMeshRenderBufferSet* FBaseDynamicMeshSceneProxy::AllocateNewRenderBufferSet()
{
	// should we hang onto these and destroy them in constructor? leaving to subclass seems risky?
	FMeshRenderBufferSet* RenderBufferSet = new FMeshRenderBufferSet(GetScene().GetFeatureLevel());

	RenderBufferSet->Material = UMaterial::GetDefaultMaterial(MD_Surface);
	RenderBufferSet->bEnableRaytracing = this->bEnableRaytracing && this->IsVisibleInRayTracing();

	AllocatedSetsLock.Lock();
	AllocatedBufferSets.Add(RenderBufferSet);
	AllocatedSetsLock.Unlock();

	return RenderBufferSet;
}

void FBaseDynamicMeshSceneProxy::ReleaseRenderBufferSet(FMeshRenderBufferSet* BufferSet)
{
	FScopeLock Lock(&AllocatedSetsLock);
	if (ensure(AllocatedBufferSets.Contains(BufferSet)))
	{
		AllocatedBufferSets.Remove(BufferSet);
		Lock.Unlock();

		FMeshRenderBufferSet::DestroyRenderBufferSet(BufferSet);
	}
}

int32 FBaseDynamicMeshSceneProxy::GetNumMaterials() const
{
	return ParentBaseComponent->GetNumMaterials();
}

UMaterialInterface* FBaseDynamicMeshSceneProxy::GetMaterial(int32 k) const
{
	UMaterialInterface* Material = ParentBaseComponent->GetMaterial(k);
	return (Material != nullptr) ? Material : UMaterial::GetDefaultMaterial(MD_Surface);
}

void FBaseDynamicMeshSceneProxy::UpdatedReferencedMaterials()
{
#if WITH_EDITOR
	TArray<UMaterialInterface*> Materials;
	ParentBaseComponent->GetUsedMaterials(Materials, true);

	// Temporarily disable material verification while the enqueued render command is in flight.
	// The original value for bVerifyUsedMaterials gets restored when the command is executed.
	// If we do not do this, material verification might spuriously fail in cases where the render command for changing
	// the verfifcation material is still in flight but the render thread is already trying to render the mesh.
	const uint8 bRestoreVerifyUsedMaterials = bVerifyUsedMaterials;
	bVerifyUsedMaterials = false;

	ENQUEUE_RENDER_COMMAND(FMeshRenderBufferSetDestroy)(
		[this, Materials, bRestoreVerifyUsedMaterials](FRHICommandListImmediate& RHICmdList)
	{
		this->SetUsedMaterialForVerification(Materials);
		this->bVerifyUsedMaterials = bRestoreVerifyUsedMaterials;
	});
#endif
}

void FBaseDynamicMeshSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_BaseDynamicMeshSceneProxy_GetDynamicMeshElements);

	bool bWireframe = (AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe)
		|| ParentBaseComponent->GetEnableWireframeRenderPass();

	// Get wireframe material proxy if requested and available, otherwise disable wireframe
	FMaterialRenderProxy* WireframeMaterialProxy = nullptr;
	if (bWireframe)
	{
		UMaterialInterface* WireframeMaterial = UBaseDynamicMeshComponent::GetDefaultWireframeMaterial_RenderThread();
		if (WireframeMaterial != nullptr)
		{
			FColoredMaterialRenderProxy* WireframeMaterialInstance = new FColoredMaterialRenderProxy(
				WireframeMaterial->GetRenderProxy(), ParentBaseComponent->WireframeColor);
			Collector.RegisterOneFrameMaterialProxy(WireframeMaterialInstance);
			WireframeMaterialProxy = WireframeMaterialInstance;
		}
		else
		{
			bWireframe = false;
		}
	}

	// will use this material instead of any others below, if it becomes non-null
	UMaterialInterface* ForceOverrideMaterial = nullptr;

	// Note: there is an engine show flags for vertex colors. However if we enable that, then
	// we will fail the used-materials checks in the Editor, because UBaseDynamicMeshComponent::GetUsedMaterials()
	// cannot check the active show flags. One option would be to always add the vertex color material there...
	const bool bVertexColor = ParentBaseComponent->ColorMode == EDynamicMeshComponentColorOverrideMode::VertexColors ||
								ParentBaseComponent->ColorMode == EDynamicMeshComponentColorOverrideMode::Polygroups ||
								ParentBaseComponent->ColorMode == EDynamicMeshComponentColorOverrideMode::Constant;
	if (bVertexColor)
	{
		ForceOverrideMaterial = UBaseDynamicMeshComponent::GetDefaultVertexColorMaterial_RenderThread();
	}

	ESceneDepthPriorityGroup DepthPriority = SDPG_World;

	TArray<FMeshRenderBufferSet*> Buffers;
	GetActiveRenderBufferSets(Buffers);

	UMaterialInterface* UseSecondaryMaterial = ForceOverrideMaterial;
	if (ParentBaseComponent->HasSecondaryRenderMaterial() && ForceOverrideMaterial == nullptr)
	{
		UseSecondaryMaterial = ParentBaseComponent->GetSecondaryRenderMaterial();
	}
	bool bDrawSecondaryBuffers = ParentBaseComponent->GetSecondaryBuffersVisibility();

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			const FSceneView* View = Views[ViewIndex];

			bool bHasPrecomputedVolumetricLightmap;
			FMatrix PreviousLocalToWorld;
			int32 SingleCaptureIndex;
			bool bOutputVelocity;
			GetScene().GetPrimitiveUniformShaderParameters_RenderThread(GetPrimitiveSceneInfo(), bHasPrecomputedVolumetricLightmap, PreviousLocalToWorld, SingleCaptureIndex, bOutputVelocity);
			bOutputVelocity |= AlwaysHasVelocity();

			// Draw the mesh.
			for (FMeshRenderBufferSet* BufferSet : Buffers)
			{
				UMaterialInterface* UseMaterial = BufferSet->Material;
				if (ParentBaseComponent->HasOverrideRenderMaterial(0))
				{
					UseMaterial = ParentBaseComponent->GetOverrideRenderMaterial(0);
				}
				if (ForceOverrideMaterial)
				{
					UseMaterial = ForceOverrideMaterial;
				}
				FMaterialRenderProxy* MaterialProxy = UseMaterial->GetRenderProxy();

				if (BufferSet->TriangleCount == 0)
				{
					continue;
				}

				// lock buffers so that they aren't modified while we are submitting them
				FScopeLock BuffersLock(&BufferSet->BuffersLock);

				// do we need separate one of these for each MeshRenderBufferSet?
				FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
				DynamicPrimitiveUniformBuffer.Set(GetLocalToWorld(), PreviousLocalToWorld, GetBounds(), GetLocalBounds(), GetLocalBounds(), true, bHasPrecomputedVolumetricLightmap, bOutputVelocity, GetCustomPrimitiveData());

				if (BufferSet->IndexBuffer.Indices.Num() > 0)
				{
					// Unlike most meshes, which just use the wireframe material in wireframe mode, we draw the wireframe on top of the normal material if needed,
					// as this is easier to interpret. However, we do not do this in ortho viewports, where it frequently causes the our edit gizmo to be hidden 
					// beneath the material. So, only draw the base material if we are in perspective mode, or we're in ortho but not in wireframe.
					if (View->IsPerspectiveProjection() || !(AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe))
					{
						DrawBatch(Collector, *BufferSet, BufferSet->IndexBuffer, MaterialProxy, false, DepthPriority, ViewIndex, DynamicPrimitiveUniformBuffer);
					}
					if (bWireframe)
					{
						DrawBatch(Collector, *BufferSet, BufferSet->IndexBuffer, WireframeMaterialProxy, true, DepthPriority, ViewIndex, DynamicPrimitiveUniformBuffer);
					}
				}

				// draw secondary buffer if we have it, falling back to base material if we don't have the Secondary material
				FMaterialRenderProxy* UseSecondaryMaterialProxy = (UseSecondaryMaterial != nullptr) ? UseSecondaryMaterial->GetRenderProxy() : MaterialProxy;
				if (bDrawSecondaryBuffers && BufferSet->SecondaryIndexBuffer.Indices.Num() > 0 && UseSecondaryMaterialProxy != nullptr)
				{
					DrawBatch(Collector, *BufferSet, BufferSet->SecondaryIndexBuffer, UseSecondaryMaterialProxy, false, DepthPriority, ViewIndex, DynamicPrimitiveUniformBuffer);
					if (bWireframe)
					{
						DrawBatch(Collector, *BufferSet, BufferSet->SecondaryIndexBuffer, UseSecondaryMaterialProxy, true, DepthPriority, ViewIndex, DynamicPrimitiveUniformBuffer);
					}
				}
			}
		}
	}
}

void FBaseDynamicMeshSceneProxy::DrawBatch(FMeshElementCollector& Collector, const FMeshRenderBufferSet& RenderBuffers, const FDynamicMeshIndexBuffer32& IndexBuffer, FMaterialRenderProxy* UseMaterial, bool bWireframe, ESceneDepthPriorityGroup DepthPriority, int ViewIndex, FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer) const
{
	FMeshBatch& Mesh = Collector.AllocateMesh();
	FMeshBatchElement& BatchElement = Mesh.Elements[0];
	BatchElement.IndexBuffer = &IndexBuffer;
	Mesh.bWireframe = bWireframe;
	Mesh.VertexFactory = &RenderBuffers.VertexFactory;
	Mesh.MaterialRenderProxy = UseMaterial;

	BatchElement.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer;

	BatchElement.FirstIndex = 0;
	BatchElement.NumPrimitives = IndexBuffer.Indices.Num() / 3;
	BatchElement.MinVertexIndex = 0;
	BatchElement.MaxVertexIndex = RenderBuffers.PositionVertexBuffer.GetNumVertices() - 1;
	Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
	Mesh.Type = PT_TriangleList;
	Mesh.DepthPriorityGroup = DepthPriority;
	Mesh.bCanApplyViewModeOverrides = this->bEnableViewModeOverrides;
	Collector.AddMesh(ViewIndex, Mesh);
}

#if RHI_RAYTRACING

bool FBaseDynamicMeshSceneProxy::IsRayTracingRelevant() const 
{
	return true;
}

bool FBaseDynamicMeshSceneProxy::HasRayTracingRepresentation() const
{
	return true;
}


void FBaseDynamicMeshSceneProxy::GetDynamicRayTracingInstances(FRayTracingMaterialGatheringContext& Context, TArray<FRayTracingInstance>& OutRayTracingInstances)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_BaseDynamicMeshSceneProxy_GetDynamicRayTracingInstances);

	ESceneDepthPriorityGroup DepthPriority = SDPG_World;

	TArray<FMeshRenderBufferSet*> Buffers;
	GetActiveRenderBufferSets(Buffers);

	// will use this material instead of any others below, if it becomes non-null
	UMaterialInterface* ForceOverrideMaterial = nullptr;
	const bool bVertexColor = ParentBaseComponent->ColorMode == EDynamicMeshComponentColorOverrideMode::VertexColors ||
		ParentBaseComponent->ColorMode == EDynamicMeshComponentColorOverrideMode::Polygroups ||
		ParentBaseComponent->ColorMode == EDynamicMeshComponentColorOverrideMode::Constant;
	if (bVertexColor)
	{
		ForceOverrideMaterial = UBaseDynamicMeshComponent::GetDefaultVertexColorMaterial_RenderThread();
	}

	UMaterialInterface* UseSecondaryMaterial = ForceOverrideMaterial;
	if (ParentBaseComponent->HasSecondaryRenderMaterial() && ForceOverrideMaterial == nullptr)
	{
		UseSecondaryMaterial = ParentBaseComponent->GetSecondaryRenderMaterial();
	}
	bool bDrawSecondaryBuffers = ParentBaseComponent->GetSecondaryBuffersVisibility();

	bool bHasPrecomputedVolumetricLightmap;
	FMatrix PreviousLocalToWorld;
	int32 SingleCaptureIndex;
	bool bOutputVelocity;
	GetScene().GetPrimitiveUniformShaderParameters_RenderThread(GetPrimitiveSceneInfo(), bHasPrecomputedVolumetricLightmap, PreviousLocalToWorld, SingleCaptureIndex, bOutputVelocity);
	bOutputVelocity |= AlwaysHasVelocity();

	// is it safe to share this between primary and secondary raytracing batches?
	FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Context.RayTracingMeshResourceCollector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
	DynamicPrimitiveUniformBuffer.Set(GetLocalToWorld(), PreviousLocalToWorld, GetBounds(), GetLocalBounds(), true, bHasPrecomputedVolumetricLightmap, bOutputVelocity);

	// Draw the active buffer sets
	for (FMeshRenderBufferSet* BufferSet : Buffers)
	{
		UMaterialInterface* UseMaterial = BufferSet->Material;
		if (ParentBaseComponent->HasOverrideRenderMaterial(0))
		{
			UseMaterial = ParentBaseComponent->GetOverrideRenderMaterial(0);
		}
		if (ForceOverrideMaterial)
		{
			UseMaterial = ForceOverrideMaterial;
		}
		FMaterialRenderProxy* MaterialProxy = UseMaterial->GetRenderProxy();

		if (BufferSet->TriangleCount == 0)
		{
			continue;
		}
		if (BufferSet->bIsRayTracingDataValid == false)
		{
			continue;
		}

		// Lock buffers so that they aren't modified while we are submitting them.
		FScopeLock BuffersLock(&BufferSet->BuffersLock);

		// draw primary index buffer
		if (BufferSet->IndexBuffer.Indices.Num() > 0
			&& BufferSet->PrimaryRayTracingGeometry.RayTracingGeometryRHI.IsValid())
		{
			ensure(BufferSet->PrimaryRayTracingGeometry.Initializer.IndexBuffer.IsValid());
			DrawRayTracingBatch(Context, *BufferSet, BufferSet->IndexBuffer, BufferSet->PrimaryRayTracingGeometry, MaterialProxy, DepthPriority, DynamicPrimitiveUniformBuffer, OutRayTracingInstances);
		}

		// draw secondary index buffer if we have it, falling back to base material if we don't have the Secondary material
		FMaterialRenderProxy* UseSecondaryMaterialProxy = (UseSecondaryMaterial != nullptr) ? UseSecondaryMaterial->GetRenderProxy() : MaterialProxy;
		if (bDrawSecondaryBuffers
			&& BufferSet->SecondaryIndexBuffer.Indices.Num() > 0
			&& UseSecondaryMaterialProxy != nullptr
			&& BufferSet->SecondaryRayTracingGeometry.RayTracingGeometryRHI.IsValid())
		{
			ensure(BufferSet->SecondaryRayTracingGeometry.Initializer.IndexBuffer.IsValid());
			DrawRayTracingBatch(Context, *BufferSet, BufferSet->SecondaryIndexBuffer, BufferSet->SecondaryRayTracingGeometry, UseSecondaryMaterialProxy, DepthPriority, DynamicPrimitiveUniformBuffer, OutRayTracingInstances);
		}
	}
}

void FBaseDynamicMeshSceneProxy::DrawRayTracingBatch(FRayTracingMaterialGatheringContext& Context, const FMeshRenderBufferSet& RenderBuffers, const FDynamicMeshIndexBuffer32& IndexBuffer, FRayTracingGeometry& RayTracingGeometry, FMaterialRenderProxy* UseMaterialProxy, ESceneDepthPriorityGroup DepthPriority, FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer, TArray<FRayTracingInstance>& OutRayTracingInstances) const
{
	ensure(RayTracingGeometry.Initializer.IndexBuffer.IsValid());

	FRayTracingInstance RayTracingInstance;
	RayTracingInstance.Geometry = &RayTracingGeometry;
	RayTracingInstance.InstanceTransforms.Add(GetLocalToWorld());

	uint32 SectionIdx = 0;
	FMeshBatch MeshBatch;

	MeshBatch.VertexFactory = &RenderBuffers.VertexFactory;
	MeshBatch.SegmentIndex = 0;
	MeshBatch.MaterialRenderProxy = UseMaterialProxy;
	MeshBatch.ReverseCulling = IsLocalToWorldDeterminantNegative();
	MeshBatch.Type = PT_TriangleList;
	MeshBatch.DepthPriorityGroup = DepthPriority;
	MeshBatch.bCanApplyViewModeOverrides = this->bEnableViewModeOverrides;
	MeshBatch.CastRayTracedShadow = IsShadowCast(Context.ReferenceView);

	FMeshBatchElement& BatchElement = MeshBatch.Elements[0];
	BatchElement.IndexBuffer = &IndexBuffer;
	BatchElement.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer;
	BatchElement.FirstIndex = 0;
	BatchElement.NumPrimitives = IndexBuffer.Indices.Num() / 3;
	BatchElement.MinVertexIndex = 0;
	BatchElement.MaxVertexIndex = RenderBuffers.PositionVertexBuffer.GetNumVertices() - 1;

	RayTracingInstance.Materials.Add(MeshBatch);

	RayTracingInstance.BuildInstanceMaskAndFlags(GetScene().GetFeatureLevel());
	OutRayTracingInstances.Add(RayTracingInstance);
}

#endif // RHI_RAYTRACING
