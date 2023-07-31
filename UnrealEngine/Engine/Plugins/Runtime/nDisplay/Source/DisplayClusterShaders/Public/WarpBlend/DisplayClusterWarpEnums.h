// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

enum class EDisplayClusterWarpProfileType : uint8
{
	// 2D mode
	warp_2D = 0,
	// 3D mode (2D + static Frustum)
	warp_3D,
	// Advanced 3D mode
	warp_A3D,
	// Shader lamps
	warp_SL,

	Invalid
};

enum class EDisplayClusterWarpGeometryType : uint8
{
	// Use texture warpmap as geometry source
	WarpMap = 0,
	
	// Use mesh component as geometry source
	WarpMesh,

	// Use procedural mesh component as geometry source
	WarpProceduralMesh,

	// Undefined source
	Invalid
};

enum class EDisplayClusterWarpBlendTextureType :uint8
{
	// Geometry data will be stored as a three-component (RGB) PFM file (Portable Float Map Format)
	WarpMap = 0,

	// Blend grayscale texture, color multiplier. Use in basic color correction: color = color * alpha;
	AlphaMap,

	// Bate grayscale texture, for advanced collor correction. Use with AlphaMap: color = (color*alpha*(1 - beta)) + beta;
	BetaMap
};

enum class EDisplayClusterWarpBlendFrustumType : uint8
{
	// Build frustum from 8 points of AABB
	AABB = 0,

	// Build frustum from all geometry points
	FULL,

	// Performance: Build frustum from part of geometry (LOD)
	LOD,
};

enum class EDisplayClusterWarpBlendProjectionType : uint8
{
	StaticSurfaceNormal = 0,
	StaticSurfacePlane,
	DynamicAABBCenter,

	RuntimeProjectionModes,
	RuntimeStaticSurfaceNormalInverted,
	RuntimeStaticSurfacePlaneInverted,
};


enum class EDisplayClusterWarpBlendStereoMode : uint8
{
	AsymmetricAABB = 0,
	SymmetricAABB,
};
