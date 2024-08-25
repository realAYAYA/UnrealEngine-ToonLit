// Copyright Epic Games, Inc. All Rights Reserved. 

#include "Components/BaseDynamicMeshSceneProxy.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialRenderProxy.h"
#include "PhysicsEngine/BodySetup.h"
#include "RayTracingDefinitions.h"
#include "RayTracingInstance.h"
#include "SceneInterface.h"
#include "SceneManagement.h"
#include "Engine/Engine.h"		// for GEngine definition

FBaseDynamicMeshSceneProxy::FBaseDynamicMeshSceneProxy(UBaseDynamicMeshComponent* Component)
	: FPrimitiveSceneProxy(Component),
	ParentBaseComponent(Component),
	ColorSpaceTransformMode(Component->GetVertexColorSpaceTransformMode()),
	bEnableRaytracing(Component->GetEnableRaytracing()),
	bEnableViewModeOverrides(Component->GetViewModeOverridesEnabled())
{
	if (Component->GetColorOverrideMode() == EDynamicMeshComponentColorOverrideMode::Constant)
	{
		ConstantVertexColor = Component->GetConstantOverrideColor();
		bIgnoreVertexColors = true;
	}

	bUsePerTriangleNormals = Component->GetFlatShadingEnabled();
	
	SetCollisionData();
}

FBaseDynamicMeshSceneProxy::~FBaseDynamicMeshSceneProxy()
{
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

FMaterialRenderProxy* FBaseDynamicMeshSceneProxy::GetEngineVertexColorMaterialProxy(FMeshElementCollector& Collector, const FEngineShowFlags& EngineShowFlags, bool bProxyIsSelected, bool bIsHovered)
{
	FMaterialRenderProxy* ForceOverrideMaterialProxy = nullptr;
#if UE_ENABLE_DEBUG_DRAWING
	if (bProxyIsSelected && EngineShowFlags.VertexColors && AllowDebugViewmodes())
	{
		// Override the mesh's material with our material that draws the vertex colors
		UMaterial* VertexColorVisualizationMaterial = NULL;
		switch (GVertexColorViewMode)
		{
		case EVertexColorViewMode::Color:
			VertexColorVisualizationMaterial = GEngine->VertexColorViewModeMaterial_ColorOnly;
			break;

		case EVertexColorViewMode::Alpha:
			VertexColorVisualizationMaterial = GEngine->VertexColorViewModeMaterial_AlphaAsColor;
			break;

		case EVertexColorViewMode::Red:
			VertexColorVisualizationMaterial = GEngine->VertexColorViewModeMaterial_RedOnly;
			break;

		case EVertexColorViewMode::Green:
			VertexColorVisualizationMaterial = GEngine->VertexColorViewModeMaterial_GreenOnly;
			break;

		case EVertexColorViewMode::Blue:
			VertexColorVisualizationMaterial = GEngine->VertexColorViewModeMaterial_BlueOnly;
			break;
		}
		check(VertexColorVisualizationMaterial != NULL);

		// Note: static mesh renderer does something more complicated involving per-section selection,
		// but whole component selection seems ok for now
		bool bSectionIsSelected = bProxyIsSelected;

		FColoredMaterialRenderProxy* VertexColorVisualizationMaterialInstance = new FColoredMaterialRenderProxy(
			VertexColorVisualizationMaterial->GetRenderProxy(),
			GetSelectionColor(FLinearColor::White, bSectionIsSelected, bIsHovered)
		);

		Collector.RegisterOneFrameMaterialProxy(VertexColorVisualizationMaterialInstance);
		ForceOverrideMaterialProxy = VertexColorVisualizationMaterialInstance;
	}
#endif
	return ForceOverrideMaterialProxy;
}

bool FBaseDynamicMeshSceneProxy::IsCollisionView(const FEngineShowFlags& EngineShowFlags, bool& bDrawSimpleCollision, bool& bDrawComplexCollision) const
{
	bDrawSimpleCollision = bDrawComplexCollision = false;

	bool bDrawCollisionView = (EngineShowFlags.CollisionVisibility || EngineShowFlags.CollisionPawn);

#if UE_ENABLE_DEBUG_DRAWING
	// If in a 'collision view' and collision is enabled
	if (bHasCollisionData && bDrawCollisionView && IsCollisionEnabled())
	{
		// See if we have a response to the interested channel
		bool bHasResponse = EngineShowFlags.CollisionPawn && CollisionResponse.GetResponse(ECC_Pawn) != ECR_Ignore;
		bHasResponse |= EngineShowFlags.CollisionVisibility && CollisionResponse.GetResponse(ECC_Visibility) != ECR_Ignore;

		if(bHasResponse)
		{
			// Visibility uses complex and pawn uses simple. However, if UseSimpleAsComplex or UseComplexAsSimple is used we need to adjust accordingly
			bDrawComplexCollision = (EngineShowFlags.CollisionVisibility && CollisionTraceFlag != ECollisionTraceFlag::CTF_UseSimpleAsComplex) || (EngineShowFlags.CollisionPawn && CollisionTraceFlag == ECollisionTraceFlag::CTF_UseComplexAsSimple);
			bDrawSimpleCollision  = (EngineShowFlags.CollisionPawn && CollisionTraceFlag != ECollisionTraceFlag::CTF_UseComplexAsSimple) || (EngineShowFlags.CollisionVisibility && CollisionTraceFlag == ECollisionTraceFlag::CTF_UseSimpleAsComplex);
		}
	}
#endif
	return bDrawCollisionView;
}

void FBaseDynamicMeshSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_BaseDynamicMeshSceneProxy_GetDynamicMeshElements);

	const FEngineShowFlags& EngineShowFlags = ViewFamily.EngineShowFlags;
	bool bIsWireframeViewMode = (AllowDebugViewmodes() && EngineShowFlags.Wireframe);
	bool bWantWireframeOnShaded = ParentBaseComponent->GetEnableWireframeRenderPass();
	bool bWireframe = bIsWireframeViewMode || bWantWireframeOnShaded;
	const bool bProxyIsSelected = IsSelected();


	TArray<FMeshRenderBufferSet*> Buffers;
	GetActiveRenderBufferSets(Buffers);

#if UE_ENABLE_DEBUG_DRAWING
	bool bDrawSimpleCollision = false, bDrawComplexCollision = false;
	const bool bDrawCollisionView = IsCollisionView(EngineShowFlags, bDrawSimpleCollision, bDrawComplexCollision);

	// If we're in a collision view, run the only draw the collision and return without drawing mesh normally
	if (bDrawCollisionView)
	{
		GetCollisionDynamicMeshElements(Buffers, EngineShowFlags, bDrawCollisionView, bDrawSimpleCollision, bDrawComplexCollision, bProxyIsSelected, Views, VisibilityMap, Collector);
		return;
	}
#endif

	// Get wireframe material proxy if requested and available, otherwise disable wireframe
	FMaterialRenderProxy* WireframeMaterialProxy = nullptr;
	if (bWireframe)
	{
		UMaterialInterface* WireframeMaterial = UBaseDynamicMeshComponent::GetDefaultWireframeMaterial_RenderThread();
		if (WireframeMaterial != nullptr)
		{
			FLinearColor UseWireframeColor = (bProxyIsSelected && (bWantWireframeOnShaded == false || bIsWireframeViewMode))  ?
				GEngine->GetSelectedMaterialColor() : ParentBaseComponent->WireframeColor;
			FColoredMaterialRenderProxy* WireframeMaterialInstance = new FColoredMaterialRenderProxy(
				WireframeMaterial->GetRenderProxy(), UseWireframeColor);
			Collector.RegisterOneFrameMaterialProxy(WireframeMaterialInstance);
			WireframeMaterialProxy = WireframeMaterialInstance;
		}
		else
		{
			bWireframe = false;
		}
	}

	FMaterialRenderProxy* ForceOverrideMaterialProxy = GetEngineVertexColorMaterialProxy(Collector, EngineShowFlags, bProxyIsSelected, IsHovered());
	// If engine show flags aren't setting vertex color, also check if the component requested custom vertex color modes for the dynamic mesh
	if (!ForceOverrideMaterialProxy)
	{
		const bool bVertexColor = ParentBaseComponent->ColorMode == EDynamicMeshComponentColorOverrideMode::VertexColors ||
			ParentBaseComponent->ColorMode == EDynamicMeshComponentColorOverrideMode::Polygroups ||
			ParentBaseComponent->ColorMode == EDynamicMeshComponentColorOverrideMode::Constant;
		if (bVertexColor)
		{
			ForceOverrideMaterialProxy = UBaseDynamicMeshComponent::GetDefaultVertexColorMaterial_RenderThread()->GetRenderProxy();
		}
	}

	ESceneDepthPriorityGroup DepthPriority = SDPG_World;


	FMaterialRenderProxy* SecondaryMaterialProxy = ForceOverrideMaterialProxy;
	if (ParentBaseComponent->HasSecondaryRenderMaterial() && ForceOverrideMaterialProxy == nullptr)
	{
		SecondaryMaterialProxy = ParentBaseComponent->GetSecondaryRenderMaterial()->GetRenderProxy();
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
				FMaterialRenderProxy* MaterialProxy = ForceOverrideMaterialProxy;
				if (!MaterialProxy)
				{
					UMaterialInterface* UseMaterial = BufferSet->Material;
					if (ParentBaseComponent->HasOverrideRenderMaterial(0))
					{
						UseMaterial = ParentBaseComponent->GetOverrideRenderMaterial(0);
					}
					MaterialProxy = UseMaterial->GetRenderProxy();
				}

				if (BufferSet->TriangleCount == 0)
				{
					continue;
				}

				// lock buffers so that they aren't modified while we are submitting them
				FScopeLock BuffersLock(&BufferSet->BuffersLock);

				// do we need separate one of these for each MeshRenderBufferSet?
				FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
				DynamicPrimitiveUniformBuffer.Set(Collector.GetRHICommandList(), GetLocalToWorld(), PreviousLocalToWorld, GetBounds(), GetLocalBounds(), GetLocalBounds(), true, bHasPrecomputedVolumetricLightmap, bOutputVelocity, GetCustomPrimitiveData());

				// If we want Wireframe-on-Shaded, we have to draw the solid. If View Mode Overrides are enabled, the solid
				// will be replaced with it's wireframe, so we might as well not. 
				bool bDrawSolidWithWireframe = ( bWantWireframeOnShaded && (bIsWireframeViewMode == false || bEnableViewModeOverrides == false) );

				if (BufferSet->IndexBuffer.Indices.Num() > 0)
				{
					if (bWireframe)
					{
						if (bDrawSolidWithWireframe)
						{
							DrawBatch(Collector, *BufferSet, BufferSet->IndexBuffer, MaterialProxy, /*bWireframe*/false, DepthPriority, ViewIndex, DynamicPrimitiveUniformBuffer);
						}
						DrawBatch(Collector, *BufferSet, BufferSet->IndexBuffer, WireframeMaterialProxy, /*bWireframe*/true, DepthPriority, ViewIndex, DynamicPrimitiveUniformBuffer);
					}
					else
					{
						DrawBatch(Collector, *BufferSet, BufferSet->IndexBuffer, MaterialProxy, /*bWireframe*/false, DepthPriority, ViewIndex, DynamicPrimitiveUniformBuffer);
					}
				}

				// draw secondary buffer if we have it, falling back to base material if we don't have the Secondary material
				FMaterialRenderProxy* UseSecondaryMaterialProxy = (SecondaryMaterialProxy != nullptr) ? SecondaryMaterialProxy : MaterialProxy;
				if (bDrawSecondaryBuffers && BufferSet->SecondaryIndexBuffer.Indices.Num() > 0 && UseSecondaryMaterialProxy != nullptr)
				{
					if (bWireframe)
					{
						if (bDrawSolidWithWireframe)
						{
							DrawBatch(Collector, *BufferSet, BufferSet->SecondaryIndexBuffer, UseSecondaryMaterialProxy, /*bWireframe*/false, DepthPriority, ViewIndex, DynamicPrimitiveUniformBuffer);
						}
						DrawBatch(Collector, *BufferSet, BufferSet->SecondaryIndexBuffer, UseSecondaryMaterialProxy, /*bWireframe*/true, DepthPriority, ViewIndex, DynamicPrimitiveUniformBuffer);
					}
					else
					{
						DrawBatch(Collector, *BufferSet, BufferSet->SecondaryIndexBuffer, UseSecondaryMaterialProxy, /*bWireframe*/false, DepthPriority, ViewIndex, DynamicPrimitiveUniformBuffer);
					}
				}
			}
		}
	}

#if UE_ENABLE_DEBUG_DRAWING
	GetCollisionDynamicMeshElements(Buffers, EngineShowFlags, bDrawCollisionView, bDrawSimpleCollision, bDrawComplexCollision, bProxyIsSelected, Views, VisibilityMap, Collector);
#endif
}

void FBaseDynamicMeshSceneProxy::GetCollisionDynamicMeshElements(TArray<FMeshRenderBufferSet*>& Buffers, 
	const FEngineShowFlags& EngineShowFlags, bool bDrawCollisionView, bool bDrawSimpleCollision, bool bDrawComplexCollision, bool bProxyIsSelected,
	const TArray<const FSceneView*>& Views, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
#if UE_ENABLE_DEBUG_DRAWING
	if (!bHasCollisionData)
	{
		return;
	}

	// Note: This is closely following StaticMeshRender.cpp's collision rendering code, from its GetDynamicMeshElements() implementation
	FColor SimpleCollisionColor = FColor(157, 149, 223, 255);
	FColor ComplexCollisionColor = FColor(0, 255, 255, 255);

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			const FSceneView* View = Views[ViewIndex];

			if(AllowDebugViewmodes())
			{
				// Should we draw the mesh wireframe to indicate we are using the mesh as collision
				bool bDrawComplexWireframeCollision = (EngineShowFlags.Collision && IsCollisionEnabled() && CollisionTraceFlag == ECollisionTraceFlag::CTF_UseComplexAsSimple);

				// If drawing complex collision as solid or wireframe
				if(bDrawComplexWireframeCollision || (bDrawCollisionView && bDrawComplexCollision))
				{
					bool bDrawWireframe = !bDrawCollisionView;

					UMaterial* MaterialToUse = UMaterial::GetDefaultMaterial(MD_Surface);
					FLinearColor DrawCollisionColor = GetWireframeColor();
					// Collision view modes draw collision mesh as solid
					if(bDrawCollisionView)
					{
						MaterialToUse = GEngine->ShadedLevelColorationUnlitMaterial;
					}
					// Wireframe, choose color based on complex or simple
					else
					{
						MaterialToUse = GEngine->WireframeMaterial;
						DrawCollisionColor = (CollisionTraceFlag == ECollisionTraceFlag::CTF_UseComplexAsSimple) ? SimpleCollisionColor : ComplexCollisionColor;
					}
					// Create colored proxy
					FColoredMaterialRenderProxy* CollisionMaterialInstance = new FColoredMaterialRenderProxy(MaterialToUse->GetRenderProxy(), DrawCollisionColor);
					Collector.RegisterOneFrameMaterialProxy(CollisionMaterialInstance);

					bool bHasPrecomputedVolumetricLightmap;
					FMatrix PreviousLocalToWorld;
					int32 SingleCaptureIndex;
					bool bOutputVelocity;
					GetScene().GetPrimitiveUniformShaderParameters_RenderThread(GetPrimitiveSceneInfo(), bHasPrecomputedVolumetricLightmap, PreviousLocalToWorld, SingleCaptureIndex, bOutputVelocity);
					bOutputVelocity |= AlwaysHasVelocity();

					// Draw the mesh with collision materials
					for (FMeshRenderBufferSet* BufferSet : Buffers)
					{

						if (BufferSet->TriangleCount == 0)
						{
							continue;
						}

						// lock buffers so that they aren't modified while we are submitting them
						FScopeLock BuffersLock(&BufferSet->BuffersLock);

						// do we need separate one of these for each MeshRenderBufferSet?
						FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
						DynamicPrimitiveUniformBuffer.Set(Collector.GetRHICommandList(), GetLocalToWorld(), PreviousLocalToWorld, GetBounds(), GetLocalBounds(), GetLocalBounds(), true, bHasPrecomputedVolumetricLightmap, bOutputVelocity, GetCustomPrimitiveData());

						if (BufferSet->IndexBuffer.Indices.Num() > 0)
						{
							DrawBatch(Collector, *BufferSet, BufferSet->IndexBuffer, CollisionMaterialInstance, bDrawWireframe, SDPG_World, ViewIndex, DynamicPrimitiveUniformBuffer);
						}
					}
				}
			}

			// Draw simple collision as wireframe if 'show collision', collision is enabled, and we are not using the complex as the simple
			const bool bDrawSimpleWireframeCollision = (EngineShowFlags.Collision && IsCollisionEnabled() && CollisionTraceFlag != ECollisionTraceFlag::CTF_UseComplexAsSimple);

			if((bDrawSimpleCollision || bDrawSimpleWireframeCollision))
			{
				if (ParentBaseComponent->GetBodySetup())
				{
					// Avoid zero scaling, otherwise GeomTransform below will assert
					if (FMath::Abs(GetLocalToWorld().Determinant()) > UE_SMALL_NUMBER)
					{
						const bool bDrawSolid = !bDrawSimpleWireframeCollision;

						if (AllowDebugViewmodes() && bDrawSolid)
						{
							// Make a material for drawing solid collision stuff
							FColoredMaterialRenderProxy* SolidMaterialInstance = new FColoredMaterialRenderProxy(
								GEngine->ShadedLevelColorationUnlitMaterial->GetRenderProxy(),
								GetWireframeColor()
							);

							Collector.RegisterOneFrameMaterialProxy(SolidMaterialInstance);

							FTransform GeomTransform(GetLocalToWorld());
							CachedAggGeom.GetAggGeom(GeomTransform, GetWireframeColor().ToFColor(true), SolidMaterialInstance, false, true, AlwaysHasVelocity(), ViewIndex, Collector);
						}
						// wireframe
						else
						{
							FTransform GeomTransform(GetLocalToWorld());
							CachedAggGeom.GetAggGeom(GeomTransform, GetSelectionColor(SimpleCollisionColor, bProxyIsSelected, IsHovered()).ToFColor(true), NULL, bOwnerIsNull, false, AlwaysHasVelocity(), ViewIndex, Collector);
						}

						// Note: if dynamic mesh component could have nav collision data, we'd also draw that here (see the similar code in StaticMeshRenderer.cpp)
					}
				}
			}
		}
	}
#endif // UE_ENABLE_DEBUG_DRAWING

}

void FBaseDynamicMeshSceneProxy::DrawBatch(FMeshElementCollector& Collector, const FMeshRenderBufferSet& RenderBuffers, const FDynamicMeshIndexBuffer32& IndexBuffer, FMaterialRenderProxy* UseMaterial, bool bWireframe, ESceneDepthPriorityGroup DepthPriority, int ViewIndex, FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer) const
{
	FMeshBatch& Mesh = Collector.AllocateMesh();
	FMeshBatchElement& BatchElement = Mesh.Elements[0];
	BatchElement.IndexBuffer = &IndexBuffer;
	Mesh.bWireframe = bWireframe;
	//Mesh.bDisableBackfaceCulling = bWireframe;		// todo: doing this would be more consistent w/ other meshes in wireframe mode, but it is problematic for modeling tools - perhaps should be configurable
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
	// if this is a wireframe draw pass then we do not want to apply View Mode Overrides
	Mesh.bCanApplyViewModeOverrides = (bWireframe) ? false : this->bEnableViewModeOverrides;
	Collector.AddMesh(ViewIndex, Mesh);
}

void FBaseDynamicMeshSceneProxy::SetCollisionData()
{
#if UE_ENABLE_DEBUG_DRAWING
	bHasCollisionData = true;
	bOwnerIsNull = ParentBaseComponent->GetOwner() == nullptr;
	if (UBodySetup* BodySetup = ParentBaseComponent->GetBodySetup())
	{
		CollisionTraceFlag = BodySetup->GetCollisionTraceFlag();
		CachedAggGeom = BodySetup->AggGeom;
	}
	else
	{
		CachedAggGeom = FKAggregateGeom();
	}
	CollisionResponse = ParentBaseComponent->GetCollisionResponseToChannels();
#endif
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
	DynamicPrimitiveUniformBuffer.Set(Context.RHICmdList, GetLocalToWorld(), PreviousLocalToWorld, GetBounds(), GetLocalBounds(), true, bHasPrecomputedVolumetricLightmap, bOutputVelocity);

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

	OutRayTracingInstances.Add(RayTracingInstance);
}

#endif // RHI_RAYTRACING
