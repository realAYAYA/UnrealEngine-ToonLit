// Copyright Epic Games, Inc. All Rights Reserved.
// Port of geometry3Sharp MinimalHoleFiller

#pragma once

#include "HoleFiller.h"
#include "MathUtil.h"
#include "VectorTypes.h"
#include "GeometryTypes.h"
#include "MeshBoundaryLoops.h"
#include "SimpleHoleFiller.h"
#include "Containers/Set.h"
#include "MeshRegionOperator.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "QueueRemesher.h"
#include "MeshConstraintsUtil.h"

namespace UE
{
namespace Geometry
{


/// Construct a "minimal" fill surface for the hole. This surface is often quasi-developable, reconstructs sharp edges, 
/// etc. There are various options.

class DYNAMICMESH_API FMinimalHoleFiller : public IHoleFiller
{
public:

	FMinimalHoleFiller(FDynamicMesh3* InMesh, FEdgeLoop InFillLoop) :
		Mesh(InMesh),
		FillLoop(InFillLoop),
		FillMesh(nullptr)
	{}

	bool Fill(int32 GroupID = -1) override;

	// Settings
	bool bIgnoreBoundaryTriangles = false;
	bool bOptimizeDevelopability = true;
	bool bOptimizeTriangles = true;
	double DevelopabilityTolerance = 0.0001;

private:

	// Inputs
	FDynamicMesh3* Mesh;
	FEdgeLoop FillLoop;

	TUniquePtr<FMeshRegionOperator> RegionOp;
	FDynamicMesh3* FillMesh;
	TSet<int> BoundaryVertices;
	TMap<int, double> ExteriorAngleSums;
	TArray<double> Curvatures;

	void AddAllEdges(int EdgeID, TSet<int>& EdgeSet);

	double AspectMetric(int eid);
	static double GetTriAspect(const FDynamicMesh3& mesh, FIndex3i& tri);

	void UpdateCurvature(int vid);
	double CurvatureMetricCached(int a, int b, int c, int d);
	double CurvatureMetricEval(int a, int b, int c, int d);
	double ComputeGaussCurvature(int vid);

	// Steps in Fill:
	void CollapseToMinimal();
	void RemoveRemainingInteriorVerts();
	void FlipToFlatter();
	void FlipToMinimizeCurvature();
	void FlipToImproveAspectRatios();
};


} // end namespace UE::Geometry
} // end namespace UE