// Copyright Epic Games, Inc. All Rights Reserved.

#include "WarpBlend/DisplayClusterWarpBlend_GeometryProxy.h"

#include "Render/Containers/IDisplayClusterRender_MeshComponent.h"

#include "WarpBlend/Math/DisplayClusterWarpBlendMath_WarpMesh.h"
#include "WarpBlend/Math/DisplayClusterWarpBlendMath_WarpProceduralMesh.h"
#include "WarpBlend/Math/DisplayClusterWarpBlendMath_WarpMap.h"

#include "WarpBlend/Exporter/DisplayClusterWarpBlendExporter_WarpMap.h"

/////////////////////////////////////////////////////////////////////////////////
/// FDisplayClusterWarpBlend_GeometryProxy
/////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterWarpBlend_GeometryProxy::UpdateFrustumGeometry()
{
	switch (FrustumGeometryType)
	{
	case EDisplayClusterWarpFrustumGeometryType::WarpMesh:
		return ImplUpdateFrustumGeometry_WarpMesh();

	case EDisplayClusterWarpFrustumGeometryType::WarpProceduralMesh:
		return ImplUpdateFrustumGeometry_WarpProceduralMesh();

	case EDisplayClusterWarpFrustumGeometryType::WarpMap:
		return ImplUpdateFrustumGeometry_WarpMap();

	case EDisplayClusterWarpFrustumGeometryType::MPCDIAttributes:
		return ImplUpdateFrustumGeometry_MPCDIAttributes();

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
		return WarpMeshComponent.IsValid() ? WarpMeshComponent->GetMeshComponentProxy_RenderThread() : nullptr;

	default:
		break;
	}

	return nullptr;
}

bool FDisplayClusterWarpBlend_GeometryProxy::MarkWarpFrustumGeometryComponentDirty(const FName& InComponentName)
{
	switch (FrustumGeometryType)
	{
	case EDisplayClusterWarpFrustumGeometryType::WarpMesh:
	case EDisplayClusterWarpFrustumGeometryType::WarpProceduralMesh:
		if (WarpMeshComponent.IsValid())
		{
			if (InComponentName == NAME_None || WarpMeshComponent->EqualsMeshComponentName(InComponentName))
			{
				WarpMeshComponent->MarkMeshComponentRefGeometryDirty();
				return true;
			}
		}
		break;
	default:
		break;
	}

	return false;
}

bool FDisplayClusterWarpBlend_GeometryProxy::ImplUpdateFrustumGeometry_MPCDIAttributes()
{
	bIsGeometryValid = false;

	// Update caches
	if ((bIsGeometryValid = bIsGeometryCacheValid) == false)
	{
		bIsGeometryValid = ImplUpdateFrustumGeometryCache_MPCDIAttributes();
	}

	GeometryCache.GeometryToOrigin = FTransform::Identity;

	return bIsGeometryValid;
}

bool FDisplayClusterWarpBlend_GeometryProxy::ImplUpdateFrustumGeometry_WarpMap()
{
	bIsGeometryValid = false;

	if (WarpMapTexture.IsValid() && WarpMapTexture->IsEnabled())
	{
		// Update caches
		if ((bIsGeometryValid = bIsGeometryCacheValid) == false)
		{
			bIsGeometryValid = ImplUpdateFrustumGeometryCache_WarpMap();
		}

		GeometryCache.GeometryToOrigin = FTransform::Identity;
	}

	return bIsGeometryValid;
}

bool FDisplayClusterWarpBlend_GeometryProxy::ImplUpdateFrustumGeometry_WarpMesh()
{
	bIsGeometryValid = false;

	if (!WarpMeshComponent.IsValid())
	{
		return false;
	}

	UStaticMeshComponent* StaticMeshComponent = WarpMeshComponent->GetStaticMeshComponent();
	USceneComponent*      OriginComponent     = WarpMeshComponent->GetOriginComponent();

	const FStaticMeshLODResources* StaticMeshLODResources = (StaticMeshComponent!=nullptr) ? WarpMeshComponent->GetStaticMeshComponentLODResources(StaticMeshComponentLODIndex) : nullptr;
	if (StaticMeshLODResources == nullptr)
	{
		// mesh deleted?
		WarpMeshComponent->ReleaseProxyGeometry();
		bIsMeshComponentLost = true;
		return false;
	};

	// If StaticMesh geometry changed, update mpcdi math and RHI resources
	if (WarpMeshComponent->IsMeshComponentRefGeometryDirty() || bIsMeshComponentLost)
	{
		WarpMeshComponent->AssignStaticMeshComponentRefs(StaticMeshComponent, WarpMeshUVs, OriginComponent, StaticMeshComponentLODIndex);
		bIsMeshComponentLost = false;
	}
	
	// Update caches
	if ((bIsGeometryValid = bIsGeometryCacheValid) == false)
	{
		bIsGeometryValid = ImplUpdateFrustumGeometryCache_WarpMesh();
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

bool FDisplayClusterWarpBlend_GeometryProxy::ImplUpdateFrustumGeometry_WarpProceduralMesh()
{
	bIsGeometryValid = false;

	if (!WarpMeshComponent.IsValid())
	{
		return false;
	}

	UProceduralMeshComponent* ProceduralMeshComponent = WarpMeshComponent->GetProceduralMeshComponent();
	USceneComponent*          OriginComponent         = WarpMeshComponent->GetOriginComponent();

	const FProcMeshSection* ProcMeshSection = (ProceduralMeshComponent != nullptr) ? WarpMeshComponent->GetProceduralMeshComponentSection(ProceduralMeshComponentSectionIndex) : nullptr;
	if (ProcMeshSection == nullptr)
	{
		// mesh deleted, lost or section not defined
		WarpMeshComponent->ReleaseProxyGeometry();
		bIsMeshComponentLost = true;
		return false;
	};

	// If ProceduralMesh geometry changed, update mpcdi math and RHI resources
	if (WarpMeshComponent->IsMeshComponentRefGeometryDirty() || bIsMeshComponentLost)
	{
		WarpMeshComponent->AssignProceduralMeshComponentRefs(ProceduralMeshComponent, WarpMeshUVs, OriginComponent, ProceduralMeshComponentSectionIndex);
		bIsMeshComponentLost = false;
	}

	// Update caches
	if ((bIsGeometryValid = bIsGeometryCacheValid) == false)
	{
		bIsGeometryValid = ImplUpdateFrustumGeometryCache_WarpProceduralMesh();
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

bool FDisplayClusterWarpBlend_GeometryProxy::ImplUpdateFrustumGeometryCache_WarpMesh()
{
	bIsGeometryValid = false;

	if (WarpMeshComponent.IsValid())
	{
		const FStaticMeshLODResources* StaticMeshLODResources = WarpMeshComponent->GetStaticMeshComponentLODResources(StaticMeshComponentLODIndex);
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

bool FDisplayClusterWarpBlend_GeometryProxy::ImplUpdateFrustumGeometryCache_WarpProceduralMesh()
{
	bIsGeometryValid = false;

	if (WarpMeshComponent.IsValid())
	{
		const FProcMeshSection* ProcMeshSection = WarpMeshComponent->GetProceduralMeshComponentSection(ProceduralMeshComponentSectionIndex);
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

bool FDisplayClusterWarpBlend_GeometryProxy::ImplUpdateFrustumGeometryCache_WarpMap()
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

bool FDisplayClusterWarpBlend_GeometryProxy::ImplUpdateFrustumGeometryCache_MPCDIAttributes()
{
	// WarpMap must be initialized before, outside
	bIsGeometryValid = false;

	switch (MPCDIAttributes.ProfileType)
	{
	case EDisplayClusterWarpProfileType::warp_2D:
		GeometryCache.SurfaceViewNormal = FVector(1, 0, 0);
		GeometryCache.SurfaceViewPlane = FVector(1, 0, 0);

		// Calc AABB for 2D profile geometry:
		{
			TArray<FVector> ScreenPoints;
			FDisplayClusterWarpBlendExporter_WarpMap::Get2DProfileGeometry(MPCDIAttributes, ScreenPoints);

			FDisplayClusterWarpAABB WarpAABB;
			WarpAABB.UpdateAABB(ScreenPoints);

			GeometryCache.AABBox = WarpAABB;
		}
		break;

	default:
		return false;
	}

	bIsGeometryCacheValid = true;
	bIsGeometryValid = true;

	return true;
}

bool FDisplayClusterWarpBlend_GeometryProxy::UpdateFrustumGeometryLOD(const FIntPoint& InSizeLOD)
{
	check(InSizeLOD.X > 0 && InSizeLOD.Y > 0);

	GeometryCache.IndexLOD.Empty();

	if (WarpMapTexture.IsValid() && WarpMapTexture->IsEnabled())
	{
		switch (FrustumGeometryType)
		{
		case EDisplayClusterWarpFrustumGeometryType::WarpMap:
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
	return WarpMeshComponent.IsValid() ? WarpMeshComponent->GetStaticMeshComponentLODResources(StaticMeshComponentLODIndex) : nullptr;
}

const FProcMeshSection* FDisplayClusterWarpBlend_GeometryProxy::GetProceduralMeshComponentSection() const
{
	return WarpMeshComponent.IsValid() ? WarpMeshComponent->GetProceduralMeshComponentSection(ProceduralMeshComponentSectionIndex) : nullptr;
}

void FDisplayClusterWarpBlend_GeometryProxy::ReleaseResources()
{
	WarpMapTexture.Reset();
	AlphaMapTexture.Reset();
	BetaMapTexture.Reset();
	WarpMeshComponent.Reset();
	PreviewMeshComponentRef.ResetSceneComponent();
}
