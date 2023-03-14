// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Units/Highlevel/RigUnit_HighlevelBase.h"
#include "Drawing/ControlRigDrawInterface.h"

#include "Core/PBIKSolver.h"
#include "Core/PBIKDebug.h"

#include "PBIK_Shared.h"

#include "RigUnit_PBIK.generated.h"

using PBIK::FDebugLine;

USTRUCT(BlueprintType)
struct FPBIKDebug
{
	GENERATED_BODY()

	/** The scale of the debug drawing. */
	UPROPERTY(EditAnywhere, Category = Debug)
	float DrawScale = 1.0f;

	/** If true, turns on debug drawing for the node. */
	UPROPERTY(EditAnywhere, Category = Debug)
	bool bDrawDebug = false;

	void Draw(FControlRigDrawInterface* DrawInterface, FPBIKSolver* Solver) const
	{
		if (!(DrawInterface && Solver && bDrawDebug))
		{
			return;
		}

		const FLinearColor Bright = FLinearColor(0.f, 1.f, 1.f, 1.f);
		DrawInterface->DrawBox(FTransform::Identity, FTransform(FQuat::Identity, FVector(0, 0, 0), FVector(1.f, 1.f, 1.f) * DrawScale * 0.1f), Bright);

		TArray<FDebugLine> BodyLines;
		Solver->GetDebugDraw()->GetDebugLinesForBodies(BodyLines);
		const FLinearColor BodyColor = FLinearColor(0.1f, 0.1f, 1.f, 1.f);
		for (FDebugLine Line : BodyLines)
		{
			DrawInterface->DrawLine(FTransform::Identity, Line.A, Line.B, BodyColor);
		}
	}
};

USTRUCT(BlueprintType)
struct FPBIKEffector
{
	GENERATED_BODY()

	FPBIKEffector()	: Bone(NAME_None) {}

	/** The bone that this effector will pull on. */
	UPROPERTY(EditAnywhere, Category="Effector", meta = (CustomWidget = "BoneName"))
	FName Bone;

	/** The target location and rotation for this effector. The solver will try to get the specified bone to reach this location.*/
	UPROPERTY(EditAnywhere, Category="Effector")
	FTransform Transform;

	/** Range 0-1, default is 1. Blend between the input bone position (0.0) and the current effector position (1.0).*/
	UPROPERTY(EditAnywhere, Category = "Goal Settings", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float PositionAlpha = 1.0f;

	/** Range 0-1, default is 1. Blend between the input bone rotation (0.0) and the current effector rotation (1.0).*/
	UPROPERTY(EditAnywhere, Category = "Goal Settings", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float RotationAlpha = 1.0f;

	/** Range 0-1 (default is 1.0). The strength of the effector when pulling the bone towards it's target location.
	 * At 0.0, the effector does not pull at all, but the bones between the effector and the root will still slightly resist motion from other effectors.
	 * This can thus act as a "stabilizer" of sorts for parts of the body that you do not want to behave in a pure FK fashion.
	 */
	UPROPERTY(EditAnywhere, Category="Effector", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float StrengthAlpha = 1.0f;

	/** Range 0-1 (default is 1.0). When enabled (greater than 0.0), the solver internally partitions the skeleton into 'chains' which extend from the effector to the nearest fork in the skeleton.
	 *These chains are pre-rotated and translated, as a whole, towards the effector targets.
	 *This can improve the results for sparse bone chains, and significantly improve convergence on dense bone chains.
	 *But it may cause undesirable results in highly constrained bone chains (like robot arms).
	 */
	UPROPERTY(EditAnywhere, Category="Effector", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float PullChainAlpha = 0.0f;

	/** Range 0-1 (default is 1.0).
	 *Blends the effector bone rotation between the rotation of the effector transform (1.0) and the rotation of the input bone (0.0).*/
	UPROPERTY(EditAnywhere, Category="Effector", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float PinRotation = 1.0f;
};

/*
 * Based on a Position Based solver at core, this node can solve multi chains within a root using multi effectors
 */
USTRUCT(meta=(DisplayName="Full Body IK", Category="Hierarchy", Keywords="Position Based, PBIK, IK, Full Body, Multi, Effector, N-Chain, FB, HIK, HumanIK", NodeColor="0 1 1"))
struct FRigUnit_PBIK : public FRigUnit_HighlevelBaseMutable
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	FRigUnit_PBIK() :
		Root(NAME_None),
		bNeedsInit(true)
	{
	}

	/**This is usually the top-most skinned bone; often the "Pelvis" or "Hips", but can be set to any bone.
	 *Bones above the root will be ignored by the solver.
	 *Bones that are located *between* the Root and the effectors will be included in the solve.*/
	UPROPERTY(meta = (Input, CustomWidget = "BoneName"))
	FName Root;

	/** An array of effectors. These specify target transforms for different parts of the skeleton. */
	UPROPERTY(meta = (Input))
	TArray<FPBIKEffector> Effectors;
	
	UPROPERTY(transient)
	TArray<int32> EffectorSolverIndices;

	/** Per-bone settings to control the resulting pose. Includes limits and preferred angles. */
	UPROPERTY(meta = (Input))
	TArray<FPBIKBoneSetting> BoneSettings;

	/** These bones will be excluded from the solver. They will not bend and will not contribute to the constraint set.
	 * Use the ExcludedBones array instead of setting Rotation Stiffness to very high values or Rotation Limits with zero range. */
	UPROPERTY(meta = (Input, CustomWidget = "BoneName"))
	TArray<FName> ExcludedBones;

	/** Global solver settings. */
	UPROPERTY(meta = (Input))
	FPBIKSolverSettings Settings;

	/** Debug drawing options. */
	UPROPERTY(meta = (Input))
	FPBIKDebug Debug;

	UPROPERTY(transient)
	TArray<int32> BoneSettingToSolverBoneIndex;

	UPROPERTY(transient)
	TArray<int32> SolverBoneToElementIndex;

	UPROPERTY(transient)
	FPBIKSolver Solver;

	UPROPERTY(transient)
	bool bNeedsInit;
};
