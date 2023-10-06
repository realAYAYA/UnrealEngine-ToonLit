// Copyright Epic Games, Inc. All Rights Reserved.

#include "Solvers/MeshParameterizationSolvers.h"
#include "Solvers/Internal/MeshUVSolver.h"


TUniquePtr<UE::Solvers::IConstrainedMeshUVSolver> UE::MeshDeformation::ConstructNaturalConformalParamSolver(const FDynamicMesh3& DynamicMesh)
{
	TUniquePtr<UE::Solvers::IConstrainedMeshUVSolver> Solver(new FLeastSquaresConformalMeshUVSolver(DynamicMesh));
	return Solver;
}

TUniquePtr<UE::Solvers::IConstrainedMeshUVSolver> UE::MeshDeformation::ConstructSpectralConformalParamSolver(const FDynamicMesh3& DynamicMesh, bool bPreserveIrregularity)
{
	TUniquePtr<UE::Solvers::IConstrainedMeshUVSolver> Solver(new FSpectralConformalMeshUVSolver(DynamicMesh, bPreserveIrregularity));
	return Solver;
}