// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModelingOperators.h"
#include "SubdividePoly.generated.h"

PREDECLARE_USE_GEOMETRY_CLASS(FGroupTopology);
PREDECLARE_USE_GEOMETRY_CLASS(FDynamicMesh3);
class FProgressCancel;

/**
 * Subdivision scheme
 */
UENUM()
enum class ESubdivisionScheme : uint8
{
	Bilinear,
	CatmullClark,
	Loop
};


/**
 * Method for computing normals on the output mesh
 */
UENUM()
enum class ESubdivisionOutputNormals : uint8
{
	// Copy normals to poly mesh then interpolate to new triangles as we subdivide.
	Interpolated,

	// Recompute normals after subdivision
	Generated,

	// Leave normals uninitialized on output mesh
	None
};

/**
 * Method for computing UVs on the output mesh
 */
UENUM()
enum class ESubdivisionOutputUVs : uint8
{
	// Copy UVs to poly mesh vertices then interpolate as we subdivide.
	Interpolated,

	// Leave UVs uninitialized on output mesh
	None
};


/**
 * Interprets an FGroupTopology object as a poly mesh and refines it to a given level. Topological subdivision 
 * can be computed separately from the interpolation of vertex positions. A typical workflow might be to first 
 * precompute the refined topology, then compute the interpolated positions (and other vertex values) as the original 
 * polymesh is deformed.
 */
class MODELINGCOMPONENTSEDITORONLY_API FSubdividePoly
{
public:

	FSubdividePoly(const FGroupTopology& InTopology,
				   const FDynamicMesh3& InOriginalMesh,
				   int InLevel);

	~FSubdividePoly();

	enum class ETopologyCheckResult
	{
		Ok = 0,
		NoGroups = 1,
		InsufficientGroups = 2,
		UnboundedPolygroup = 3,
		MultiBoundaryPolygroup = 4,
		DegeneratePolygroup = 5
	};
	ETopologyCheckResult ValidateTopology();

	bool ComputeTopologySubdivision();

	bool ComputeSubdividedMesh(FDynamicMesh3& OutMesh);

	ESubdivisionScheme SubdivisionScheme = ESubdivisionScheme::CatmullClark;

	ESubdivisionOutputNormals NormalComputationMethod = ESubdivisionOutputNormals::Generated;

	ESubdivisionOutputUVs UVComputationMethod = ESubdivisionOutputUVs::Interpolated;

	bool bNewPolyGroups = false;

	// wrapper around Opensubdiv::TopologyRefiner to avoid including OSD headers here. See cpp file.
	class RefinerImpl;

private:

	const FGroupTopology& GroupTopology;
	const FDynamicMesh3& OriginalMesh;
	int Level;

	TUniquePtr<RefinerImpl> Refiner;
};

