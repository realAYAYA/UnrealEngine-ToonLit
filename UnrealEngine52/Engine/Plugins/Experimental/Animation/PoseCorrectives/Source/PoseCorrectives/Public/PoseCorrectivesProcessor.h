// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Runtime for applying pose correctives 
 *
 */

#include "CorrectivesRBFSolver.h"
#include "PoseCorrectivesAsset.h"
#include "PoseCorrectivesProcessor.generated.h"

UCLASS()
class POSECORRECTIVES_API UPoseCorrectivesProcessor : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	void ProcessPoseCorrective(TArray<FTransform>& OutBoneTransforms, 
		TArray<float>& OutCurveValues, 
		const TArray<FPoseCorrective>& Correctives,
		const FRBFParams& RBFParams,
		const TArray<FCorrectivesRBFTarget>& RBFTargets, 
		FCorrectivesRBFEntry RBFInput);

private:
	// RBF solver data
	TSharedPtr<const FCorrectivesRBFSolverData> SolverData = nullptr;

	// Last set of output weights from RBF solve
	TArray<FRBFOutputWeight> OutputWeights;
};


