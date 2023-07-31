// Copyright Epic Games, Inc. All Rights Reserved.

#include "Operations/MeshProjectionHull.h"
#include "Solvers/MeshLinearization.h"
#include "MeshSimplification.h"
#include "DynamicMesh/MeshNormals.h"
#include "CompGeom/ConvexHull2.h"
#include "Generators/SweepGenerator.h"

using namespace UE::Geometry;

bool FMeshProjectionHull::Compute()
{
	// compute set of projected 2D vertices, and find the 1D bounding interval along the projection axis
	FVector3d ProjAxis = ProjectionFrame.Z();
	FInterval1d ProjInterval = FInterval1d::Empty();
	TArray<FVector2d> Vertices;
	Vertices.Reserve(Mesh->VertexCount());
	for (int32 vid : Mesh->VertexIndicesItr())
	{
		FVector3d Position = Mesh->GetVertex(vid);
		FVector2d ProjPos = ProjectionFrame.ToPlaneUV(Position, 2);
		Vertices.Add(ProjPos);

		ProjInterval.Contain( (Position-ProjectionFrame.Origin).Dot(ProjAxis) );
	}

	// compute the 2D convex hull
	FConvexHull2d HullCompute;
	bool bOK = HullCompute.Solve(Vertices);
	if (!bOK)
	{
		return false;
	}

	// extract hull polygon
	ConvexHull2D = FPolygon2d(Vertices, HullCompute.GetPolygonIndices());
	if (ConvexHull2D.Area() < FMathf::ZeroTolerance)
	{
		return false;
	}

	// simplify if requested
	if (bSimplifyPolygon)
	{
		SimplifiedHull2D = ConvexHull2D;
		SimplifiedHull2D.Simplify(MinEdgeLength, DeviationTolerance);
	}

	// shift the projection frame to the min position along the axis
	FFrame3d CenterFrame = ProjectionFrame;
	CenterFrame.Origin += ProjInterval.Min * ProjAxis;

	// apply min-thickness if necessary
	double ExtrudeLength = ProjInterval.Length();
	if (ExtrudeLength < MinThickness)
	{
		ExtrudeLength = MinThickness;
		CenterFrame.Origin -= (ExtrudeLength * 0.5) * ProjAxis;
	}

	// generate the swept-polygon mesh
	FGeneralizedCylinderGenerator MeshGen;
	MeshGen.CrossSection = (bSimplifyPolygon) ? SimplifiedHull2D : ConvexHull2D;
	MeshGen.Path.Add(CenterFrame.Origin);
	CenterFrame.Origin += ExtrudeLength * ProjAxis;
	MeshGen.Path.Add(CenterFrame.Origin);
	MeshGen.bCapped = true;
	MeshGen.Generate();
	ConvexHull3D.Copy(&MeshGen);

	return true;
}