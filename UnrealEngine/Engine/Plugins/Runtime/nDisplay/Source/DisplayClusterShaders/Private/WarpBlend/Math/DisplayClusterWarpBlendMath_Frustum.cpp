// Copyright Epic Games, Inc. All Rights Reserved.

#include "WarpBlend/Math/DisplayClusterWarpBlendMath_Frustum.h"

#include "Render/IDisplayClusterRenderTexture.h"
#include "Misc/DisplayClusterHelpers.h"

#include "Render/Viewport/IDisplayClusterViewport.h"

#include "StaticMeshResources.h"
#include "ProceduralMeshComponent.h"

FMatrix GetProjectionMatrixAssymetric(IDisplayClusterViewport* InViewport, const uint32 InContextNum, const FDisplayClusterWarpEye& InEye, const FDisplayClusterWarpContext& InContext)
{
	const float Left   = InContext.ProjectionAngles.Left;
	const float Right  = InContext.ProjectionAngles.Right;
	const float Top    = InContext.ProjectionAngles.Top;
	const float Bottom = InContext.ProjectionAngles.Bottom;

	InViewport->CalculateProjectionMatrix(InContextNum, Left, Right, Top, Bottom, InEye.ZNear, InEye.ZFar, false);

	return InViewport->GetContexts()[InContextNum].ProjectionMatrix;
}

void FDisplayClusterWarpBlendMath_Frustum::ImplBuildFrustum(IDisplayClusterViewport* InViewport, const uint32 InContextNum)
{
	// These matrices were copied from LocalPlayer.cpp.
	// They change the coordinate system from the Unreal "Game" coordinate system to the Unreal "Render" coordinate system
	static const FMatrix Game2Render(
		FPlane(0, 0, 1, 0),
		FPlane(1, 0, 0, 0),
		FPlane(0, 1, 0, 0),
		FPlane(0, 0, 0, 1));

	// Compute warp projection matrix
	Frustum.OutCameraRotation = Local2World.Rotator();
	Frustum.OutCameraOrigin = Local2World.GetOrigin();

	Frustum.TextureMatrix = ImplGetTextureMatrix();
	Frustum.RegionMatrix = GeometryContext.RegionMatrix;

	Frustum.ProjectionMatrix = GetProjectionMatrixAssymetric(InViewport, InContextNum, Eye, Frustum);

	Frustum.UVMatrix = World2Local * Game2Render * Frustum.ProjectionMatrix;

	Frustum.MeshToStageMatrix = GeometryContext.GeometryToOrigin;
}


bool FDisplayClusterWarpBlendMath_Frustum::ImplCalcFrustum_FULL_WarpMap()
{
	if(!GeometryContext.GeometryProxy.WarpMapTexture.IsValid())
	{
		return false;
	}

	bool bAllPointsInFrustum = true;

	const int32 PointsAmount = GeometryContext.GeometryProxy.WarpMapTexture->GetTotalPoints();
	const FVector4f* SourcePts = (FVector4f*)(GeometryContext.GeometryProxy.WarpMapTexture->GetData());

	// Search a camera space frustum
	for (int32 PointIndex = 0; PointIndex < PointsAmount; ++PointIndex)
	{
		const FVector4 Pts(SourcePts[PointIndex]);
		if (GetProjectionClip(Pts) == false)
		{
			bAllPointsInFrustum = false;
		}
	}

	return bAllPointsInFrustum;
}

bool FDisplayClusterWarpBlendMath_Frustum::ImplCalcFrustum_FULL_WarpMesh()
{
	if (!GeometryContext.GeometryProxy.MeshComponent.IsValid())
	{
		return false;
	}

	bool bAllPointsInFrustum = true;

	const FStaticMeshLODResources* StaticMeshLODResources = GeometryContext.GeometryProxy.GetStaticMeshComponentLODResources();
	if (StaticMeshLODResources == nullptr)
	{
		return false;
	}

	const FPositionVertexBuffer& VertexPosition = StaticMeshLODResources->VertexBuffers.PositionVertexBuffer;
	for (uint32 i = 0; i < VertexPosition.GetNumVertices(); i++)
	{
		if (GetProjectionClip(FVector4(FVector(VertexPosition.VertexPosition(i)), 1.f)) == false)
		{
			bAllPointsInFrustum = false;
		}
	}

	return bAllPointsInFrustum;
}

bool FDisplayClusterWarpBlendMath_Frustum::ImplCalcFrustum_FULL_WarpProceduralMesh()
{
	if (!GeometryContext.GeometryProxy.MeshComponent.IsValid())
	{
		return false;
	}

	bool bAllPointsInFrustum = true;

	const FProcMeshSection* ProcMeshSection = GeometryContext.GeometryProxy.MeshComponent->GetProceduralMeshComponentSection();
	if (ProcMeshSection == nullptr)
	{
		return false;
	}

	for (const FProcMeshVertex& VertexIt : ProcMeshSection->ProcVertexBuffer)
	{
		if (GetProjectionClip(FVector4(VertexIt.Position, 1)) == false)
		{
			bAllPointsInFrustum = false;
		}
	}

	return bAllPointsInFrustum;
}

// Calculate better performance for PFM
bool FDisplayClusterWarpBlendMath_Frustum::ImplCalcFrustum_LOD_WarpMap()
{
	if (!GeometryContext.GeometryProxy.WarpMapTexture.IsValid())
	{
		return false;
	}

	bool bAllPointsInFrustum = true;

	const TArray<int32>& IndexLOD = GeometryContext.GeometryProxy.GeometryCache.IndexLOD;

	if (IndexLOD.Num() == 0)
	{
		check(WarpMapLODRatio > 0);

		GeometryContext.GeometryProxy.UpdateGeometryLOD(FIntPoint(WarpMapLODRatio));
	}

	const int32 PointsAmount = GeometryContext.GeometryProxy.WarpMapTexture->GetTotalPoints();
	const FVector4f* SourcePts = (FVector4f*)(GeometryContext.GeometryProxy.WarpMapTexture->GetData());

	// Search a camera space frustum
	for (const int32& It: IndexLOD)
	{
		const FVector4 Pts(SourcePts[It]);
		if (GetProjectionClip(Pts) == false)
		{
			bAllPointsInFrustum = false;
		}
	}

	return bAllPointsInFrustum;
}

