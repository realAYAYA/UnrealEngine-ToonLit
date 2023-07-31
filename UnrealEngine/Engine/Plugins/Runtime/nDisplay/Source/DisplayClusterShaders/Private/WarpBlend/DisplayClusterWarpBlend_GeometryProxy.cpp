// Copyright Epic Games, Inc. All Rights Reserved.

#include "WarpBlend/DisplayClusterWarpBlend_GeometryProxy.h"

#include "Render/Containers/IDisplayClusterRender_MeshComponent.h"

#include "WarpBlend/Math/DisplayClusterWarpBlendMath_WarpMesh.h"
#include "WarpBlend/Math/DisplayClusterWarpBlendMath_WarpProceduralMesh.h"
#include "WarpBlend/Math/DisplayClusterWarpBlendMath_WarpMap.h"

/////////////////////////////////////////////////////////////////////////////////
/// FDisplayClusterWarpBlend_GeometryProxy
/////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterWarpBlend_GeometryProxy::UpdateGeometry()
{
	switch (GeometryType)
	{
	case EDisplayClusterWarpGeometryType::WarpMesh:
		return ImplUpdateGeometry_WarpMesh();

	case EDisplayClusterWarpGeometryType::WarpProceduralMesh:
		return ImplUpdateGeometry_WarpProceduralMesh();

	case EDisplayClusterWarpGeometryType::WarpMap:
		return ImplUpdateGeometry_WarpMap();

	default:
		break;
	}

	bIsGeometryValid = false;
	return false;
}

const IDisplayClusterRender_MeshComponentProxy* FDisplayClusterWarpBlend_GeometryProxy::GetWarpMeshProxy_RenderThread() const
{
	check(IsInRenderingThread());

	switch (GeometryType)
	{
	case EDisplayClusterWarpGeometryType::WarpMesh:
	case EDisplayClusterWarpGeometryType::WarpProceduralMesh:
		return MeshComponent.IsValid() ? MeshComponent->GetMeshComponentProxy_RenderThread() : nullptr;

	default:
		break;
	}

	return nullptr;
}

bool FDisplayClusterWarpBlend_GeometryProxy::MarkWarpGeometryComponentDirty(const FName& InComponentName)
{
	switch (GeometryType)
	{
	case EDisplayClusterWarpGeometryType::WarpMesh:
	case EDisplayClusterWarpGeometryType::WarpProceduralMesh:
		if (MeshComponent.IsValid())
		{
			if (InComponentName == NAME_None || MeshComponent->EqualsMeshComponentName(InComponentName))
			{
				MeshComponent->MarkMeshComponentRefGeometryDirty();
				return true;
			}
		}
		break;
	default:
		break;
	}

	return false;
}

bool FDisplayClusterWarpBlend_GeometryProxy::ImplUpdateGeometry_WarpMap()
{
	bIsGeometryValid = false;

	if (WarpMapTexture.IsValid() && WarpMapTexture->IsEnabled())
	{
		// Update caches
		if ((bIsGeometryValid = bIsGeometryCacheValid) == false)
		{
			bIsGeometryValid = ImplUpdateGeometryCache_WarpMap();
		}

		GeometryCache.GeometryToOrigin = FTransform::Identity;
	}

	return bIsGeometryValid;
}

bool FDisplayClusterWarpBlend_GeometryProxy::ImplUpdateGeometry_WarpMesh()
{
	bIsGeometryValid = false;

	if (!MeshComponent.IsValid())
	{
		return false;
	}

	UStaticMeshComponent* StaticMeshComponent = MeshComponent->GetStaticMeshComponent();
	USceneComponent*      OriginComponent     = MeshComponent->GetOriginComponent();

	const FStaticMeshLODResources* StaticMeshLODResources = (StaticMeshComponent!=nullptr) ? MeshComponent->GetStaticMeshComponentLODResources(StaticMeshComponentLODIndex) : nullptr;
	if (StaticMeshLODResources == nullptr)
	{
		// mesh deleted?
		MeshComponent->ReleaseProxyGeometry();
		bIsMeshComponentLost = true;
		return false;
	};

	// If StaticMesh geometry changed, update mpcdi math and RHI resources
	if (MeshComponent->IsMeshComponentRefGeometryDirty() || bIsMeshComponentLost)
	{
		MeshComponent->AssignStaticMeshComponentRefs(StaticMeshComponent, WarpMeshUVs, OriginComponent, StaticMeshComponentLODIndex);
		bIsMeshComponentLost = false;
	}
	
	// Update caches
	if ((bIsGeometryValid = bIsGeometryCacheValid) == false)
	{
		bIsGeometryValid = ImplUpdateGeometryCache_WarpMesh();
	}

	if (OriginComponent)
	{
		FMatrix MeshToWorldMatrix = StaticMeshComponent->GetComponentTransform().ToMatrixWithScale();
		FMatrix WorldToOriginMatrix = OriginComponent->GetComponentTransform().ToInverseMatrixWithScale();

		GeometryCache.GeometryToOrigin.SetFromMatrix(MeshToWorldMatrix * WorldToOriginMatrix);
	}
	else
	{
		GeometryCache.GeometryToOrigin = StaticMeshComponent->GetRelativeTransform();
	}

	return bIsGeometryValid;
}

bool FDisplayClusterWarpBlend_GeometryProxy::ImplUpdateGeometry_WarpProceduralMesh()
{
	bIsGeometryValid = false;

	if (!MeshComponent.IsValid())
	{
		return false;
	}

	UProceduralMeshComponent* ProceduralMeshComponent = MeshComponent->GetProceduralMeshComponent();
	USceneComponent*          OriginComponent         = MeshComponent->GetOriginComponent();

	const FProcMeshSection* ProcMeshSection = (ProceduralMeshComponent != nullptr) ? MeshComponent->GetProceduralMeshComponentSection(ProceduralMeshComponentSectionIndex) : nullptr;
	if (ProcMeshSection == nullptr)
	{
		// mesh deleted, lost or section not defined
		MeshComponent->ReleaseProxyGeometry();
		bIsMeshComponentLost = true;
		return false;
	};

	// If ProceduralMesh geometry changed, update mpcdi math and RHI resources
	if (MeshComponent->IsMeshComponentRefGeometryDirty() || bIsMeshComponentLost)
	{
		MeshComponent->AssignProceduralMeshComponentRefs(ProceduralMeshComponent, WarpMeshUVs, OriginComponent, ProceduralMeshComponentSectionIndex);
		bIsMeshComponentLost = false;
	}

	// Update caches
	if ((bIsGeometryValid = bIsGeometryCacheValid) == false)
	{
		bIsGeometryValid = ImplUpdateGeometryCache_WarpProceduralMesh();
	}

	// These matrices were copied from LocalPlayer.cpp.
	// They change the coordinate system from the Unreal "Game" coordinate system to the Unreal "Render" coordinate system
	static const FMatrix Game2Render(
		FPlane(0, 0, 1, 0),
		FPlane(1, 0, 0, 0),
		FPlane(0, 1, 0, 0),
		FPlane(0, 0, 0, 1));

	static const FMatrix Render2Game = Game2Render.Inverse();

	// Procedural mesh coord is render, so convert to game at first
	FMatrix GeometryToOriginMatrix;// = Render2Game;

	if (OriginComponent)
	{
		FMatrix MeshToWorldMatrix = ProceduralMeshComponent->GetComponentTransform().ToMatrixWithScale();
		FMatrix WorldToOriginMatrix = OriginComponent->GetComponentTransform().ToInverseMatrixWithScale();

		GeometryToOriginMatrix = MeshToWorldMatrix * WorldToOriginMatrix;
	}
	else
	{
		GeometryToOriginMatrix = ProceduralMeshComponent->GetRelativeTransform().ToMatrixWithScale();
	}

	GeometryCache.GeometryToOrigin.SetFromMatrix(GeometryToOriginMatrix);

	return bIsGeometryValid;
}

bool FDisplayClusterWarpBlend_GeometryProxy::ImplUpdateGeometryCache_WarpMesh()
{
	bIsGeometryValid = false;

	if (MeshComponent.IsValid())
	{
		const FStaticMeshLODResources* StaticMeshLODResources = MeshComponent->GetStaticMeshComponentLODResources(StaticMeshComponentLODIndex);
		if (StaticMeshLODResources != nullptr)
		{
			FDisplayClusterWarpBlendMath_WarpMesh MeshHelper(*StaticMeshLODResources);

			GeometryCache.AABBox = MeshHelper.CalcAABBox();
			MeshHelper.CalcSurfaceVectors(GeometryCache.SurfaceViewNormal, GeometryCache.SurfaceViewPlane);

			bIsGeometryCacheValid = true;
			bIsGeometryValid = true;
		}
	}

	return bIsGeometryValid;
}

bool FDisplayClusterWarpBlend_GeometryProxy::ImplUpdateGeometryCache_WarpProceduralMesh()
{
	bIsGeometryValid = false;

	if (MeshComponent.IsValid())
	{
		const FProcMeshSection* ProcMeshSection = MeshComponent->GetProceduralMeshComponentSection(ProceduralMeshComponentSectionIndex);
		if (ProcMeshSection != nullptr)
		{
			FDisplayClusterWarpBlendMath_WarpProceduralMesh ProceduralMeshHelper(*ProcMeshSection);

			GeometryCache.AABBox = ProceduralMeshHelper.CalcAABBox();
			ProceduralMeshHelper.CalcSurfaceVectors(GeometryCache.SurfaceViewNormal, GeometryCache.SurfaceViewPlane);

			bIsGeometryCacheValid = true;
			bIsGeometryValid = true;

			return true;
		}
	}

	return false;
}

bool FDisplayClusterWarpBlend_GeometryProxy::ImplUpdateGeometryCache_WarpMap()
{
	// WarpMap must be initialized before, outside
	bIsGeometryValid = false;
	
	if (WarpMapTexture.IsValid() && WarpMapTexture->IsEnabled())
	{
		FDisplayClusterWarpBlendMath_WarpMap DataHelper(*(WarpMapTexture.Get()));

		GeometryCache.AABBox = DataHelper.GetAABBox();
		GeometryCache.SurfaceViewNormal = DataHelper.GetSurfaceViewNormal();
		GeometryCache.SurfaceViewPlane  = DataHelper.GetSurfaceViewPlane();

		bIsGeometryCacheValid = true;
		bIsGeometryValid = true;

		return true;
	}

	return false;
}

bool FDisplayClusterWarpBlend_GeometryProxy::UpdateGeometryLOD(const FIntPoint& InSizeLOD)
{
	check(InSizeLOD.X > 0 && InSizeLOD.Y > 0);

	GeometryCache.IndexLOD.Empty();

	if (WarpMapTexture.IsValid() && WarpMapTexture->IsEnabled())
	{
		switch (GeometryType)
		{
		case EDisplayClusterWarpGeometryType::WarpMap:
		{
			// Generate valid points for texturebox method:
			FDisplayClusterWarpBlendMath_WarpMap DataHelper(*(WarpMapTexture.Get()));
			DataHelper.BuildIndexLOD(InSizeLOD.X, InSizeLOD.Y, GeometryCache.IndexLOD);

			return true;
		}
		default:
			break;
		}
	}

	return false;
}

const FStaticMeshLODResources* FDisplayClusterWarpBlend_GeometryProxy::GetStaticMeshComponentLODResources() const
{
	return MeshComponent.IsValid() ? MeshComponent->GetStaticMeshComponentLODResources(StaticMeshComponentLODIndex) : nullptr;
}

const FProcMeshSection* FDisplayClusterWarpBlend_GeometryProxy::GetProceduralMeshComponentSection() const
{
	return MeshComponent.IsValid() ? MeshComponent->GetProceduralMeshComponentSection(ProceduralMeshComponentSectionIndex) : nullptr;
}

void FDisplayClusterWarpBlend_GeometryProxy::ImplReleaseResources()
{
	WarpMapTexture.Reset();
	AlphaMapTexture.Reset();
	BetaMapTexture.Reset();
	MeshComponent.Reset();
}
