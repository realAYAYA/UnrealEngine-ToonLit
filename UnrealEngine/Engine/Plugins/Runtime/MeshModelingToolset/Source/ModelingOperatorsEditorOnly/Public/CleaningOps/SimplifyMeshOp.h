// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MeshConstraints.h"
#include "ModelingOperators.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "Util/ProgressCancel.h"

#include "SimplifyMeshOp.generated.h"

struct FMeshDescription;
class IMeshReduction;

UENUM()
enum class ESimplifyTargetType : uint8
{
	/** Percentage of input triangles */
	Percentage = 0 UMETA(DisplayName = "Percentage"),

	/** Target triangle count */
	TriangleCount = 1 UMETA(DisplayName = "Triangle Count"),

	/** Target vertex count */
	VertexCount = 2 UMETA(DisplayName = "Vertex Count"),

	/** Target edge length */
	EdgeLength = 3 UMETA(DisplayName = "Edge Length"),

	/** Apply all allowable edge collapses that do not change the shape */
	MinimalPlanar = 4 UMETA(Hidden)
};

UENUM()
enum class ESimplifyType : uint8
{
	/** Fastest. Standard quadric error metric.*/
	QEM = 0 UMETA(DisplayName = "QEM"),

	/** Potentially higher quality. Takes the normal into account. */
	Attribute = 1 UMETA(DisplayName = "Normal Aware"),

	/** Highest quality reduction. */
	UEStandard = 2 UMETA(DisplayName = "UE Standard"),

	/** Edge collapse to existing vertices only.  Quality may suffer.*/
	MinimalExistingVertex = 3 UMETA(DisplayName = "Existing Positions"),

	/** Collapse any spurious edges but do not change the 3D shape. */
	MinimalPlanar = 4 UMETA(DisplayName = "Minimal Shape-Preserving"),

	/** Only preserve polygroup boundaries; ignore all other shape features */
	MinimalPolygroup = 5 UMETA(DisplayName = "Minimal Polygroup-Preserving"),

};

namespace UE
{
namespace Geometry
{


class MODELINGOPERATORSEDITORONLY_API FSimplifyMeshOp : public FDynamicMeshOperator
{
public:
	virtual ~FSimplifyMeshOp() {}

	//
	// Inputs
	// 
	ESimplifyTargetType TargetMode;
	ESimplifyType SimplifierType;
	int TargetPercentage, TargetCount;
	float TargetEdgeLength;
	bool bDiscardAttributes, bReproject, bPreventNormalFlips, bPreserveSharpEdges, bAllowSeamCollapse;
	// When true, result will have attributes object regardless of whether attributes 
	// were discarded or present initially.
	bool bResultMustHaveAttributesEnabled = false;
	UE::Geometry::EEdgeRefineFlags MeshBoundaryConstraint, GroupBoundaryConstraint, MaterialBoundaryConstraint;
	/** Angle threshold in degrees used for testing if two triangles should be considered coplanar, or two lines collinear */
	float MinimalPlanarAngleThresh = 0.01f;

	// For minimal polygroup-preserving simplification: Threshold angle change (in degrees) along a polygroup edge, above which a vertex must be added
	float PolyEdgeAngleTolerance = 0.1f;

	bool bGeometricDeviationConstraint = false;
	float GeometricTolerance = 0.0f;

	// stored for the UEStandard path
	TSharedPtr<FMeshDescription, ESPMode::ThreadSafe> OriginalMeshDescription;
	// stored for the GeometryProcessing custom simplifier paths (currently precomputed once in tool setup)
	TSharedPtr<FDynamicMesh3, ESPMode::ThreadSafe> OriginalMesh;
	TSharedPtr<FDynamicMeshAABBTree3, ESPMode::ThreadSafe> OriginalMeshSpatial;

	IMeshReduction* MeshReduction;


	// set ability on protected transform.
	void SetTransform(const FTransform& Transform)
	{
		ResultTransform = (FTransform3d)Transform;
	}

	//
	// FDynamicMeshOperator implementation
	// 

	virtual void CalculateResult(FProgressCancel* Progress) override;
};

} // end namespace UE::Geometry
} // end namespace UE
