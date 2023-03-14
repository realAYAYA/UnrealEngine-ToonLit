// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterWarpBlend_GeometryContext.h"

#include "WarpBlend/DisplayClusterWarpBlend_GeometryProxy.h"

bool FDisplayClusterWarpBlend_GeometryContext::Update(float WorldScale)
{
	if (!GeometryProxy.UpdateGeometry())
	{
		return false;
	}

	// Update runtime data:
	GeometryToOrigin = GeometryProxy.GeometryCache.GeometryToOrigin.ToMatrixWithScale();

	SurfaceViewNormal = GeometryToOrigin.TransformVector(GeometryProxy.GeometryCache.SurfaceViewNormal);
	SurfaceViewPlane = GeometryToOrigin.TransformVector(GeometryProxy.GeometryCache.SurfaceViewPlane);

	AABBox.Max = GeometryToOrigin.TransformPosition(GeometryProxy.GeometryCache.AABBox.Max) * WorldScale;
	AABBox.Min = GeometryToOrigin.TransformPosition(GeometryProxy.GeometryCache.AABBox.Min) * WorldScale;

	// Build AABB 3d-cube points:
	AABBoxPts[0] = FVector(AABBox.Max.X, AABBox.Max.Y, AABBox.Max.Z);
	AABBoxPts[1] = FVector(AABBox.Max.X, AABBox.Max.Y, AABBox.Min.Z);

	AABBoxPts[2] = FVector(AABBox.Min.X, AABBox.Max.Y, AABBox.Min.Z);
	AABBoxPts[3] = FVector(AABBox.Min.X, AABBox.Max.Y, AABBox.Max.Z);

	AABBoxPts[4] = FVector(AABBox.Max.X, AABBox.Min.Y, AABBox.Max.Z);
	AABBoxPts[5] = FVector(AABBox.Max.X, AABBox.Min.Y, AABBox.Min.Z);

	AABBoxPts[6] = FVector(AABBox.Min.X, AABBox.Min.Y, AABBox.Min.Z);
	AABBoxPts[7] = FVector(AABBox.Min.X, AABBox.Min.Y, AABBox.Max.Z);

	return true;
}

