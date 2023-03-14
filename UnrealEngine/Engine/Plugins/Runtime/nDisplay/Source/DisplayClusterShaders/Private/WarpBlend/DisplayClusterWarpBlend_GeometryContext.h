// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "WarpBlend/DisplayClusterWarpBlend_GeometryProxy.h"

class FDisplayClusterWarpBlend_GeometryContext
{
public:
	FDisplayClusterWarpBlend_GeometryContext()
	{ }

	~FDisplayClusterWarpBlend_GeometryContext()
	{ }

public:
	bool Update(float WorldScale);

public:
	FDisplayClusterWarpBlend_GeometryProxy GeometryProxy;

	EDisplayClusterWarpProfileType ProfileType = EDisplayClusterWarpProfileType::Invalid;

	FMatrix RegionMatrix = FMatrix::Identity;

	FMatrix GeometryToOrigin = FMatrix::Identity;

	FBox     AABBox;
	FVector  AABBoxPts[8];

	// Static surface average normal for this region
	FVector SurfaceViewNormal;

	// Static surface average normal from 4 corner points
	FVector SurfaceViewPlane;

	// Force update caches
	bool bGeometryChanged = false;

};

