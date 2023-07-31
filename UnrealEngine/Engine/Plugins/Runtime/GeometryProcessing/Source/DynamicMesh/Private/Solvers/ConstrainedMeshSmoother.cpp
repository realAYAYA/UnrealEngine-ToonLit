// Copyright Epic Games, Inc. All Rights Reserved.

#include "Solvers/ConstrainedMeshSmoother.h"
#include "Solvers/Internal/ConstrainedMeshSmoothers.h"


TUniquePtr<UE::Solvers::IConstrainedMeshSolver> UE::MeshDeformation::ConstructConstrainedMeshSmoother(const ELaplacianWeightScheme WeightScheme, const FDynamicMesh3& DynamicMesh)
{
	TUniquePtr<UE::Solvers::IConstrainedMeshSolver> Deformer(new FBiHarmonicMeshSmoother(DynamicMesh, WeightScheme));
	return Deformer;
}

