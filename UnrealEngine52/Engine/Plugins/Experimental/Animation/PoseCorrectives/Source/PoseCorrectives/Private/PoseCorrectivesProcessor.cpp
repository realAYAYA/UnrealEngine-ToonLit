// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseCorrectivesProcessor.h"

/////////////////////////////////////////////////////
// UPoseCorrectivesProcessor
/////////////////////////////////////////////////////
UPoseCorrectivesProcessor::UPoseCorrectivesProcessor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// Array of transforms and curve values should match num corrective bones/curves in asset
void UPoseCorrectivesProcessor::ProcessPoseCorrective(TArray<FTransform>& OutBoneTransforms, 
	TArray<float>& OutCurveValues,
	const TArray<FPoseCorrective>& Correctives,
	const FRBFParams& RBFParams,
	const TArray<FCorrectivesRBFTarget>& RBFTargets, 
	FCorrectivesRBFEntry RBFInput)
{
	//Init solver
	if(!SolverData.IsValid() || !FCorrectivesRBFSolver::IsSolverDataValid(*SolverData, RBFParams, RBFTargets))
	{
		SolverData = FCorrectivesRBFSolver::InitSolver(RBFParams, RBFTargets);
	}

	// Run RBF solver
	OutputWeights.Reset();
	FCorrectivesRBFSolver::Solve(*SolverData, RBFParams, RBFTargets, RBFInput, OutputWeights);

	// Blend
	for(const auto& OutputWeight : OutputWeights)
	{		
		const FPoseCorrective& Corrective = Correctives[OutputWeight.TargetIndex];

		for (int32 BoneIndex : Corrective.CorrectiveBoneIndices)
		{
			FTransform& OutBoneTransform = OutBoneTransforms[BoneIndex];	
			const FTransform& CorrectiveTransform = Corrective.CorrectivePoseLocal[BoneIndex];
			
			OutBoneTransform = OutBoneTransform * ScalarRegister(1.f - OutputWeight.TargetWeight);
			OutBoneTransform.AccumulateWithShortestRotation(CorrectiveTransform, ScalarRegister(OutputWeight.TargetWeight));
		}

		for (int32 CurveIndex : Corrective.CorrectiveCurveIndices)
		{	
			float& OutCurveValue = OutCurveValues[CurveIndex];	
			
			const float& CorrectiveDelta = Corrective.CorrectiveCurvesDelta[CurveIndex];
			OutCurveValue += CorrectiveDelta * OutputWeight.TargetWeight;
		}
	}
}
