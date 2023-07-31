// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "WarpBlend/DisplayClusterWarpEnums.h"
#include "Render/Containers/IDisplayClusterRender_MeshComponent.h"
#include "Render/IDisplayClusterRenderTexture.h"

class IDisplayClusterRender_MeshComponentProxy;

struct FStaticMeshLODResources;
struct FProcMeshSection;

class FDisplayClusterWarpBlend_GeometryProxy
{
public:
	FDisplayClusterWarpBlend_GeometryProxy()
	{ }

	~FDisplayClusterWarpBlend_GeometryProxy()
	{ ImplReleaseResources(); }

public:
	const IDisplayClusterRenderTexture* ImplGetTexture(EDisplayClusterWarpBlendTextureType TextureType) const
	{
		switch (TextureType)
		{
		case EDisplayClusterWarpBlendTextureType::WarpMap:
			return WarpMapTexture.Get();

		case EDisplayClusterWarpBlendTextureType::AlphaMap:
			return AlphaMapTexture.Get();

		case EDisplayClusterWarpBlendTextureType::BetaMap:
			return BetaMapTexture.Get();

		default:
			break;
		}
		return nullptr;
	}

	bool MarkWarpGeometryComponentDirty(const FName& InComponentName);

	bool UpdateGeometry();
	bool UpdateGeometryLOD(const FIntPoint& InSizeLOD);

	const IDisplayClusterRender_MeshComponentProxy* GetWarpMeshProxy_RenderThread() const;

	const FStaticMeshLODResources* GetStaticMeshComponentLODResources() const;
	const FProcMeshSection* GetProceduralMeshComponentSection() const;

private:
	bool ImplUpdateGeometry_WarpMesh();
	bool ImplUpdateGeometry_WarpProceduralMesh();
	bool ImplUpdateGeometry_WarpMap();

public:
	bool bIsGeometryCacheValid = false;
	bool bIsGeometryValid = false;
	EDisplayClusterWarpGeometryType GeometryType = EDisplayClusterWarpGeometryType::Invalid;
	
private:
	void ImplReleaseResources();

public:
	// Render resources:
	TUniquePtr<IDisplayClusterRenderTexture> WarpMapTexture;
	TUniquePtr<IDisplayClusterRenderTexture> AlphaMapTexture;
	TUniquePtr<IDisplayClusterRenderTexture> BetaMapTexture;

	TSharedPtr<IDisplayClusterRender_MeshComponent, ESPMode::ThreadSafe> MeshComponent;

	FDisplayClusterMeshUVs WarpMeshUVs;

	// for huge warp mesh geometry, change this value, and use LOD geometry in frustum math
	int32 StaticMeshComponentLODIndex = 0;
	int32 ProceduralMeshComponentSectionIndex = 0;

	float AlphaMapEmbeddedGamma = 1.f;

private:
	bool bIsMeshComponentLost = false;

private:
	bool ImplUpdateGeometryCache_WarpMesh();
	bool ImplUpdateGeometryCache_WarpProceduralMesh();
	bool ImplUpdateGeometryCache_WarpMap();

public:
	// Cached values for geometry (updated only if geometry changed)
	class FDisplayClusterWarpBlend_GeometryCache
	{
	public:
		FTransform GeometryToOrigin;

		FBox       AABBox;
		FVector    SurfaceViewNormal;
		FVector    SurfaceViewPlane;

		TArray<int32> IndexLOD;
	};

	FDisplayClusterWarpBlend_GeometryCache GeometryCache;
};

