// Copyright Epic Games, Inc. All Rights Reserved.

// Port of geometry3Sharp PlanarHoleFiller

#pragma once

#include "HoleFiller.h"
#include "CoreMinimal.h"
#include "MathUtil.h"
#include "VectorTypes.h"
#include "IndexTypes.h"
#include "GeometryTypes.h"
#include "Curve/GeneralPolygon2.h"
#include "MeshBoundaryLoops.h"


namespace UE
{
namespace Geometry
{

class FDynamicMesh3;

/**
 * Fill a set of boundary loops with planar surfaces.  User must provide the triangulation function.
 */
class DYNAMICMESH_API FPlanarHoleFiller : public IHoleFiller
{
public:
	//
	// Inputs
	//
	FDynamicMesh3* Mesh;
	const TArray<TArray<int>> *VertexLoops;

	TFunction<TArray<FIndex3i>(const FGeneralPolygon2d&)> PlanarTriangulationFunc;

	FVector3d PlaneOrigin;
	FVector3d PlaneNormal;

public:
	/**
	 *  Construct planar hole filler (projects the hole edges to a plane and fills the hole w/ a general 2D polygon triangulation)
	 */
	FPlanarHoleFiller(FDynamicMesh3* Mesh, const TArray<TArray<int>> *VertexLoops, TFunction<TArray<FIndex3i>(const FGeneralPolygon2d&)> PlanarTriangulationFunc, FVector3d PlaneOrigin, FVector3d PlaneNormal) 
		: Mesh(Mesh), VertexLoops(VertexLoops), PlanarTriangulationFunc(PlanarTriangulationFunc), PlaneOrigin(PlaneOrigin), PlaneNormal(PlaneNormal)
	{
	}
	virtual ~FPlanarHoleFiller() {}
	
	
	/**
	 * @return EOperationValidationResult::Ok if we can apply operation, or error code if we cannot
	 */
	virtual EOperationValidationResult Validate()
	{
		// TODO: validation

		return EOperationValidationResult::Ok;
	}

	bool Fill(int GroupID = -1) override;
};


} // end namespace UE::Geometry
} // end namespace UE