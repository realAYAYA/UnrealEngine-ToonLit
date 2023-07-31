// Copyright Epic Games, Inc. All Rights Reserved.

#include "OculusMR_PlaneMeshComponent.h"
#include "RenderingThread.h"
#include "RenderResource.h"
#include "PrimitiveViewRelevance.h"
#include "PrimitiveSceneProxy.h"
#include "VertexFactory.h"
#include "MaterialShared.h"
#include "Engine/CollisionProfile.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/Material.h"
#include "LocalVertexFactory.h"
#include "SceneManagement.h"
#include "DynamicMeshBuilder.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"

/** Scene proxy */
class FOculusMR_PlaneMeshSceneProxy : public FPrimitiveSceneProxy
{
public:

	FOculusMR_PlaneMeshSceneProxy(UOculusMR_PlaneMeshComponent* Component, UTextureRenderTarget2D* RenderTarget)
		: FPrimitiveSceneProxy(Component)
		, MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetFeatureLevel()))
		, PlaneRenderTarget(RenderTarget)
	{
		const FColor VertexColor(255, 255, 255);

		const int32 NumTris = Component->CustomMeshTris.Num();
		Vertices.AddUninitialized(NumTris * 3);
		Indices.AddUninitialized(NumTris * 3);
		// Add each triangle to the vertex/index buffer
		for (int32 TriIdx = 0; TriIdx < NumTris; TriIdx++)
		{
			FOculusMR_PlaneMeshTriangle& Tri = Component->CustomMeshTris[TriIdx];

			const FVector Edge01 = (Tri.Vertex1 - Tri.Vertex0);
			const FVector Edge02 = (Tri.Vertex2 - Tri.Vertex0);

			const FVector TangentX = Edge01.GetSafeNormal();
			const FVector TangentZ = (Edge02 ^ Edge01).GetSafeNormal();
			const FVector TangentY = (TangentX ^ TangentZ).GetSafeNormal();

			FDynamicMeshVertex Vert;

			Vert.Color = VertexColor;
			Vert.SetTangents((FVector3f)TangentX, (FVector3f)TangentY, (FVector3f)TangentZ);

			Vert.Position = (FVector3f)Tri.Vertex0;
			Vert.TextureCoordinate[0] = FVector2f(Tri.UV0);	// LWC_TODO: Precision loss
			Vertices[TriIdx * 3 + 0] = Vert;
			Indices[TriIdx * 3 + 0] = TriIdx * 3 + 0;

			Vert.Position = (FVector3f)Tri.Vertex1;
			Vert.TextureCoordinate[0] = FVector2f(Tri.UV1);	// LWC_TODO: Precision loss
			Vertices[TriIdx * 3 + 1] = Vert;
			Indices[TriIdx * 3 + 1] = TriIdx * 3 + 1;

			Vert.Position = (FVector3f)Tri.Vertex2;
			Vert.TextureCoordinate[0] = FVector2f(Tri.UV2);	// LWC_TODO: Precision loss
			Vertices[TriIdx * 3 + 2] = Vert;
			Indices[TriIdx * 3 + 2] = TriIdx * 3 + 2;
		}

		// Grab material
		Material = Component->GetMaterial(0);
		if (Material == NULL)
		{
			Material = UMaterial::GetDefaultMaterial(MD_Surface);
		}
	}

	virtual ~FOculusMR_PlaneMeshSceneProxy()
	{
	}

	SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_OculusMR_PlaneMeshSceneProxy_GetDynamicMeshElements);

		// the mesh is only visible inside the CastingViewport, and the Full CastingLayer (the Composition mode)
		if (PlaneRenderTarget && ViewFamily.RenderTarget == PlaneRenderTarget->GetRenderTargetResource())
		{
			const bool bWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;

			FMaterialRenderProxy* MaterialProxy = NULL;
			if (bWireframe)
			{
				auto WireframeMaterialInstance = new FColoredMaterialRenderProxy(
					GEngine->WireframeMaterial->GetRenderProxy(),
					FLinearColor(0, 0.5f, 1.f)
				);

				Collector.RegisterOneFrameMaterialProxy(WireframeMaterialInstance);
				MaterialProxy = WireframeMaterialInstance;
			}
			else
			{
				MaterialProxy = Material->GetRenderProxy();
			}

			for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
			{
				if (VisibilityMap & (1 << ViewIndex))
				{
					const FSceneView* View = Views[ViewIndex];

					FDynamicMeshBuilder DynamicMeshBuilder(View->GetFeatureLevel());
					DynamicMeshBuilder.AddVertices(Vertices);
					DynamicMeshBuilder.AddTriangles(Indices);

					DynamicMeshBuilder.GetMesh(GetLocalToWorld(), MaterialProxy, SDPG_World, true, false, ViewIndex, Collector);

					// -- Original draw code for reference --
					//FMeshBatch& Mesh = Collector.AllocateMesh();
					//FMeshBatchElement& BatchElement = Mesh.Elements[0];
					//BatchElement.IndexBuffer = &IndexBuffer;
					//Mesh.bWireframe = bWireframe;
					//Mesh.VertexFactory = &VertexFactory;
					//Mesh.MaterialRenderProxy = MaterialProxy;
					//BatchElement.PrimitiveUniformBuffer = CreatePrimitiveUniformBufferImmediate(GetLocalToWorld(), GetBounds(), GetLocalBounds(), true, DrawsVelocity());
					//BatchElement.FirstIndex = 0;
					//BatchElement.NumPrimitives = IndexBuffer.Indices.Num() / 3;
					//BatchElement.MinVertexIndex = 0;
					//BatchElement.MaxVertexIndex = VertexBuffer.Vertices.Num() - 1;
					//Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
					//Mesh.Type = PT_TriangleList;
					//Mesh.DepthPriorityGroup = SDPG_World;
					//Mesh.bCanApplyViewModeOverrides = false;
					//Collector.AddMesh(ViewIndex, Mesh);
				}
			}
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View);
		Result.bShadowRelevance = IsShadowCast(View);
		Result.bDynamicRelevance = true;
		Result.bRenderInMainPass = ShouldRenderInMainPass();
		Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
		Result.bRenderCustomDepth = ShouldRenderCustomDepth();
		MaterialRelevance.SetPrimitiveViewRelevance(Result);
		return Result;
	}

	virtual bool CanBeOccluded() const override
	{
		return !MaterialRelevance.bDisableDepthTest;
	}

	virtual uint32 GetMemoryFootprint(void) const override { return(sizeof(*this) + GetAllocatedSize()); }

	uint32 GetAllocatedSize(void) const { return(FPrimitiveSceneProxy::GetAllocatedSize()); }

private:
	UMaterialInterface* Material;
	TArray<FDynamicMeshVertex> Vertices;
	TArray<uint32> Indices;
	FMaterialRelevance MaterialRelevance;
	UTextureRenderTarget2D* PlaneRenderTarget;
};

//////////////////////////////////////////////////////////////////////////

UOculusMR_PlaneMeshComponent::UOculusMR_PlaneMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;

	SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);

	bRenderCustomDepth = true;
}

bool UOculusMR_PlaneMeshComponent::SetCustomMeshTriangles(const TArray<FOculusMR_PlaneMeshTriangle>& Triangles)
{
	CustomMeshTris = Triangles;

	// Need to recreate scene proxy to send it over
	MarkRenderStateDirty();

	return true;
}

void UOculusMR_PlaneMeshComponent::AddCustomMeshTriangles(const TArray<FOculusMR_PlaneMeshTriangle>& Triangles)
{
	CustomMeshTris.Append(Triangles);

	// Need to recreate scene proxy to send it over
	MarkRenderStateDirty();
}

void  UOculusMR_PlaneMeshComponent::ClearCustomMeshTriangles()
{
	CustomMeshTris.Reset();

	// Need to recreate scene proxy to send it over
	MarkRenderStateDirty();
}

void UOculusMR_PlaneMeshComponent::Place(const FVector& Center, const FVector& Up, const FVector& Normal, const FVector2D& Size)
{
	FVector Right = FVector::CrossProduct(Up, Normal);

	FVector Up_N = Up.GetUnsafeNormal();
	FVector Right_N = Right.GetUnsafeNormal();

	FVector V0 = Center - Right_N * Size.X * 0.5f - Up_N * Size.Y * 0.5f;
	FVector2D UV0(1, 1);
	FVector V1 = Center + Right_N * Size.X * 0.5f - Up_N * Size.Y * 0.5f;
	FVector2D UV1(0, 1);
	FVector V2 = Center - Right_N * Size.X * 0.5f + Up_N * Size.Y * 0.5f;
	FVector2D UV2(1, 0);
	FVector V3 = Center + Right_N * Size.X * 0.5f + Up_N * Size.Y * 0.5f;
	FVector2D UV3(0, 0);

	FOculusMR_PlaneMeshTriangle Tri0, Tri1;
	Tri0.Vertex0 = V1;
	Tri0.UV0 = UV1;
	Tri0.Vertex1 = V0;
	Tri0.UV1 = UV0;
	Tri0.Vertex2 = V2;
	Tri0.UV2 = UV2;
	Tri1.Vertex0 = V1;
	Tri1.UV0 = UV1;
	Tri1.Vertex1 = V2;
	Tri1.UV1 = UV2;
	Tri1.Vertex2 = V3;
	Tri1.UV2 = UV3;

	SetCustomMeshTriangles({ Tri0, Tri1 });
}


FPrimitiveSceneProxy* UOculusMR_PlaneMeshComponent::CreateSceneProxy()
{
	FPrimitiveSceneProxy* Proxy = NULL;
	if (CustomMeshTris.Num() > 0)
	{
		Proxy = new FOculusMR_PlaneMeshSceneProxy(this, PlaneRenderTarget);
	}
	return Proxy;
}

int32 UOculusMR_PlaneMeshComponent::GetNumMaterials() const
{
	return 1;
}


FBoxSphereBounds UOculusMR_PlaneMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBoxSphereBounds NewBounds;
	NewBounds.Origin = FVector::ZeroVector;
	NewBounds.BoxExtent = FVector(HALF_WORLD_MAX, HALF_WORLD_MAX, HALF_WORLD_MAX);
	NewBounds.SphereRadius = FMath::Sqrt(3.0f * FMath::Square(HALF_WORLD_MAX));
	return NewBounds;
}

