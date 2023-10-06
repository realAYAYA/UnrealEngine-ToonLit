// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace UE
{
namespace Geometry
{

class FDynamicMesh3;

/**
 * If both triangles on an edge are coplanar, we can arbitrarily flip the interior edge to
 * improve triangle quality. Similarly if one triangle on an edge is degenerate, we can flip
 * the edge without affecting the shape to try to remove it. This code does one or more passes of
 * such an optimization.
 */
class DYNAMICMESH_API FPlanarFlipsOptimization
{
public:

	explicit FPlanarFlipsOptimization(FDynamicMesh3* MeshIn, int32 NumPassesIn = 1, double PlanarDotThreshIn = 0.99)
		: Mesh(MeshIn)
		, NumPasses(NumPassesIn)
		, PlanarDotThresh(PlanarDotThreshIn)
	{
	}

	FDynamicMesh3* Mesh;
	int32 NumPasses;
	double PlanarDotThresh;
	bool bRespectGroupBoundaries = true;

	/**
	 * Apply the operation to the mesh.
	 */
	void Apply();

protected:
	void ApplySinglePass();
};

} // end namespace UE::Geometry
} // end namespace UE
