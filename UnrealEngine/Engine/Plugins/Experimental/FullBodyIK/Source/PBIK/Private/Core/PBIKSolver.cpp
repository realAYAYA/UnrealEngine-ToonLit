// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/PBIKSolver.h"
#include "Core/PBIKBody.h"
#include "Core/PBIKConstraint.h"
#include "Core/PBIKDebug.h"
#include "PBIK.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PBIKSolver)

namespace PBIK
{
	FEffector::FEffector(FBone* InBone)
	{
		check(InBone);
		Bone = InBone;
		SetGoal(Bone->Position, Bone->Rotation, Settings);
	}

	void FEffector::SetGoal(
		const FVector& InPositionGoal,
		const FQuat& InRotationGoal,
		const FEffectorSettings& InSettings)
	{
		PositionOrig = Bone->Position;
		RotationOrig = Bone->Rotation;

		Position = PositionGoal = InPositionGoal;
		Rotation = RotationGoal = InRotationGoal;

		Settings = InSettings;
	}

	void FEffector::UpdateChainStates()
	{
		// NOTE: this function must be called AFTER InitBodies() due to it's reliance on the FBody.bIsSubRoot flag
		
		ChainRootBody = nullptr;
		const int32 MaxDepth = Settings.ChainDepth;
		const FBone* Parent = Bone->Parent;
		int32 Depth = 1;
		while (Parent)
		{
			if (!Parent->bIsSolved)
			{
				break; // this only happens when effector is on solver root
			}

			// if user specified a custom chain depth, then use it.
			// otherwise halt when we hit a sub root (a branch) or root of the solver
			const bool bIsAtAnyRoot = Parent->bIsSubRoot || Parent->bIsSolverRoot;
			const bool bIsAtChainRoot = MaxDepth > 0 ? Depth > MaxDepth || bIsAtAnyRoot : bIsAtAnyRoot;
			if (bIsAtChainRoot)
			{
				ChainRootBody = Parent->Body;
				break;
			}
			
			Parent->Body->bIsPartOfSubChain = true;
			++Depth;
			Parent = Parent->Parent;
		}

		ChainDepthInitializedWith = Settings.ChainDepth;
	}

	void FEffector::UpdateFromInputs(const FBone& SolverRoot)
	{
		// blend effector transform by alpha and set pin goal transform
		Position = FMath::Lerp(PositionOrig, PositionGoal, Settings.PositionAlpha);
		Rotation = FMath::Lerp(RotationOrig, RotationGoal, Settings.RotationAlpha);
		Pin.Pin()->SetGoal(Position, Rotation, Settings.StrengthAlpha);
		
		// update length of chain this effector controls in the input pose
		DistToChainRootInInputPose = CalculateDistanceToChainRoot();

		// update distances to chain root (along bones)
		if (ChainRootBody)
		{
			DistancesFromEffector.Reset();
			DistancesFromEffector.Add(0.0f);
			DistToChainRootAlongBones = Bone->Length;
			const FBone* Parent = Bone->Parent;
			while (Parent && Parent->Body != ChainRootBody)
			{
				DistancesFromEffector.Add(DistToChainRootAlongBones);
				DistToChainRootAlongBones += Parent->Length;
				Parent = Parent->Parent;
			}
		}
	}

	float FEffector::CalculateDistanceToChainRoot() const
	{
		// calculates distance from the bone on this effector to the parent sub-root (chain root)
		if (ChainRootBody)
		{
			const FEffector* ParentEffector = ChainRootBody->Effector;
			const FVector ParentSubRootPosition = ParentEffector ? ParentEffector->PositionOrig : ChainRootBody->Position;
			return (ParentSubRootPosition - Bone->Position).Size();
		}

		return 0.f;
	}

	void FEffector::ApplyPreferredAngles() const
	{
		// optionally apply a preferred angle to give solver a hint which direction to favor
		// apply amount of preferred angle proportional to the amount this sub-limb is squashed

		// can't squash root chain
		if (!ChainRootBody)
		{
			return; 
		}

		// can't squash chain with zero length already
		if (DistToChainRootInInputPose <= SMALL_NUMBER)
		{
			return;
		}

		// we have to be careful here when calculating the distance to the parent sub-root.
		// if the parent sub-root is attached to an effector, use the effector's position
		// otherwise use the current position of the FRigidBody
		const FEffector* ParentEffector = ChainRootBody->Effector;
		const FVector ParentSubRootPosition = ParentEffector ? ParentEffector->Position : ChainRootBody->Position;
		const float DistToSubRootCurrent = (ParentSubRootPosition - Position).Size();
		if (DistToSubRootCurrent >= DistToChainRootInInputPose)
		{
			return; // limb is stretched
		}

		// amount squashed (clamped to scaled original length)
		const float DeltaSquash = DistToChainRootInInputPose - DistToSubRootCurrent;
		float SquashPercent = DeltaSquash / DistToChainRootInInputPose;
		SquashPercent = PBIK::CircularEaseOut(SquashPercent);
		if (FMath::IsNearlyZero(SquashPercent))
		{
			return; // limb not squashed enough
		}

		// iterate over all the bodies in the chain controlled by this effector...
		const FBone* Parent = Bone->Parent;
		while (Parent && Parent->bIsSolved)
		{
			FRigidBody* Body = Parent->Body;
			if (Body->J.bUsePreferredAngles)
			{
				const FRigidBody* CurrentParent = Body->GetParentBody();
				FQuat LocalRotation = CurrentParent ? CurrentParent->Rotation.Inverse() * Body->Rotation : Body->Rotation;
				const FRotator AnglesFromInput = LocalRotation.Rotator();
				const FRotator PreferredAngles = Body->J.PreferredAngles * SquashPercent;

				auto CalcDeltaAngle = [](double PreferredAngle, double InputAngle) -> double
				{
					if (FMath::Abs(PreferredAngle) <= SMALL_NUMBER)
					{
						return 0.f; // apply no delta, preferred angle was unset at 0
					}

					// if the target angle is greater than the input pose, return a delta
					if (FMath::Abs(PreferredAngle) > FMath::Abs(InputAngle))
					{
						return PreferredAngle - InputAngle;
					}

					// body is already bent more than the preferred angle
					return 0.f;
				};

				FRotator LocalDeltaAngles;
				LocalDeltaAngles.Pitch = CalcDeltaAngle(PreferredAngles.Pitch, AnglesFromInput.Pitch);
				LocalDeltaAngles.Yaw = CalcDeltaAngle(PreferredAngles.Yaw, AnglesFromInput.Yaw);
				LocalDeltaAngles.Roll = CalcDeltaAngle(PreferredAngles.Roll, AnglesFromInput.Roll);
				Body->Rotation = Body->Rotation * LocalDeltaAngles.Quaternion();
				Body->Rotation.Normalize();
			}

			if (Parent == ChainRootBody->Bone)
			{
				return;
			}

			Parent = Parent->Parent;
		}
	}
} // namespace

void FPBIKSolver::Solve(const FPBIKSolverSettings& Settings)
{
	SCOPE_CYCLE_COUNTER(STAT_PBIK_Solve);

	using PBIK::FEffector;
	using PBIK::FRigidBody;
	using PBIK::FBone;
	using PBIK::FBoneSettings;

	// don't run until properly initialized
	if (!Initialize())
	{
		return;
	}

	// initialize local bone transforms
	// this has to be done every tick because incoming animation can modify these
	// even the LocalPosition has to be updated in case translation is animated
	for (FBone& Bone : Bones)
	{
		Bone.UpdateFromInputs();
	}

	// update Bodies with new bone positions from incoming pose and solver settings
	for (FRigidBody& Body : Bodies)
	{
		Body.UpdateFromInputs(Settings);
	}

	// update constraints with new local pin locations relative to (possibly) translated joint positions
	for (const TSharedPtr<PBIK::FConstraint>& Constraint : Constraints)
	{
		Constraint->UpdateFromInputs();
	}

	// optionally pin root in-place (convenience, does not require an effector)
	if (Settings.RootBehavior == EPBIKRootBehavior::PinToInput)
	{
		// we can safely override the mass here to pin the root body
		// if the user switches to a different root mode, the call to Body.UpdateFromInputs() will restore the correct InvMass for the body
		SolverRoot->Body->InvMass = 0.0f;
	}

	// lazily updates effector chain depths (IFF settings change at runtime)
	UpdateEffectorDepths();

	// blend effectors by Alpha, update pin goals and update effector dist to root
	for (FEffector& Effector : Effectors)
	{
		Effector.UpdateFromInputs(*SolverRoot);
	}

	// do all constraint solving
	UpdateBodies(Settings);

	// now that bodies are posed, update bone transforms
	UpdateBonesFromBodies();
}

void FPBIKSolver::UpdateBodies(const FPBIKSolverSettings& Settings)
{
	using PBIK::FRigidBody;
	
	// apply large scale gross movement to entire skeleton based on effector deltas
	// (this creates a better pose from which to start the constraint solving)
	ApplyRootPrePull(Settings.RootBehavior, Settings.PrePullRootSettings);

	// pre-rotate bones towards preferred angles when effector limb is squashed
	ApplyPreferredAngles();

	// pull each effector's chain towards itself
	ApplyPullChainAlpha(Settings.GlobalPullChainAlpha);
		
	// run ALL constraint iterations
	SolveConstraints(Settings);
}

void FPBIKSolver::UpdateBonesFromBodies()
{
	using PBIK::FRigidBody;
	using PBIK::FBone;
	using PBIK::FEffector;
	
	// update Bone transforms controlled by Bodies
	for (FRigidBody& Body : Bodies)
	{
		Body.Bone->Position = Body.Position + Body.Rotation * Body.BoneLocalPosition;
		Body.Bone->Rotation = Body.Rotation;
	}

	// update Bone transforms controlled by effectors
	for (const FEffector& Effector : Effectors)
	{
		FBone* Bone = Effector.Bone;
		if (Bone->bIsSolverRoot)
		{
			continue; // if there's an effector on the root, leave it where the body ended up
		}

		// if effector is between other effectors, it will have an associated rigid body
		// so leave transform where body ended up after solve (applied above)
		// otherwise, snap position back to FK location
		if (!Bone->Body)
		{
			if (Bone->Parent)
			{
				Bone->Position = Bone->Parent->Position + Bone->Parent->Rotation * Bone->LocalPositionFromInput;
			}
			else
			{
				// effector was on the solver root
				Bone->Position = Effector.Position;
			}
		}

		// optionally pin rotation to that of effector
		const FQuat RotationWithoutEffector = Bone->Parent ? Bone->Parent->Rotation * Bone->LocalRotationFromInput : Bone->Rotation; 
		const float RotAmount = FMath::Clamp(Effector.Settings.PinRotation, 0.0f, 1.0f);
		Bone->Rotation = FQuat::FastLerp(RotationWithoutEffector, Effector.Rotation, RotAmount).GetNormalized();
	}

	// propagate to non-solved bones (requires storage in root to tip order)
	for (FBone& Bone : Bones)
	{
		if (Bone.bIsSolved || !Bone.Parent)
		{
			continue;
		}

		Bone.Position = Bone.Parent->Position + Bone.Parent->Rotation * Bone.LocalPositionFromInput;
		Bone.Rotation = Bone.Parent->Rotation * Bone.LocalRotationFromInput;
	}
}

void FPBIKSolver::ApplyRootPrePull(const EPBIKRootBehavior RootBehavior, const FRootPrePullSettings& PrePullSettings)
{
	using PBIK::FEffector;
	using PBIK::FRigidBody;
	
	if (RootBehavior != EPBIKRootBehavior::PrePull)
	{
		return;
	}

	// calculate a "best fit" transform from deformed goal locations
	TArray<FVector, TInlineAllocator<8>> InitialPoints;
	TArray<FVector, TInlineAllocator<8>> CurrentPoints;
	InitialPoints.Reserve(Effectors.Num());
	CurrentPoints.Reserve(Effectors.Num());
	for (FEffector& Effector : Effectors)
	{
		InitialPoints.Add(Effector.PositionOrig);
		CurrentPoints.Add(Effector.Position);
	}

	// calculate "best fit" rotational delta from deformed effector configuration
	FVector InitialCentroid = FVector::ZeroVector;
	FVector CurrentCentroid = FVector::ZeroVector;
	const FQuat RotationOffset = GetRotationFromDeformedPoints(
		InitialPoints,
		CurrentPoints,
		InitialCentroid,
		CurrentCentroid);

	// do per-axis alpha blend for position offset
	FVector PositionOffset = CurrentCentroid - InitialCentroid;
	PositionOffset *= FVector(PrePullSettings.PositionAlphaX, PrePullSettings.PositionAlphaY, PrePullSettings.PositionAlphaZ);
	// alpha blend entire offset
	PositionOffset *= PrePullSettings.PositionAlpha;
	
	// do per-axis alpha blend for rotation offset
	const FVector Euler = RotationOffset.Euler() * FVector(PrePullSettings.RotationAlphaX, PrePullSettings.RotationAlphaY, PrePullSettings.RotationAlphaZ);
	FQuat FinalRotationOffset = FQuat::MakeFromEuler(Euler);
	// alpha blend the entire rotation offset
	FinalRotationOffset = FQuat::FastLerp(FQuat::Identity, FinalRotationOffset, PrePullSettings.RotationAlpha).GetNormalized();

	// move and rotate all bodies rigidly with the root
	const FRigidBody* RootBody = SolverRoot->Body;
	const FVector RootPosition = RootBody->Position;
	const FQuat RootRotation = RootBody->Rotation;
	const FVector NewRootPosition = RootPosition + PositionOffset;
	const FQuat NewRootRotation = FinalRotationOffset * RootBody->Rotation;
	for (FRigidBody& Body : Bodies)
	{
		FVector LocalPos = RootRotation.Inverse() * (Body.Position - RootPosition);
		FQuat LocalRot = RootRotation.Inverse() * Body.Rotation;
		Body.Position = NewRootPosition + (NewRootRotation * LocalPos);
		Body.Rotation = NewRootRotation * LocalRot;
	}
}

void FPBIKSolver::ApplyPullChainAlpha(const float GlobalPullChainAlpha)
{
	using PBIK::FEffector;
	using PBIK::FBone;
	using PBIK::FRigidBody;

	if (FMath::IsNearlyZero(GlobalPullChainAlpha))
	{
		return;
	}
	
	for (FEffector& Effector : Effectors)
	{
		if (!Effector.ChainRootBody)
		{
			continue;
		}

		if (Effector.DistToChainRootAlongBones < SMALL_NUMBER)
		{
			continue;
		}

		if (Effector.Settings.PullChainAlpha < SMALL_NUMBER)
		{
			continue;
		}

		// get original chain vector
		const FVector ChainStartOrig = Effector.ChainRootBody->InputPosition;
		const FVector ChainEndOrig = Effector.PositionOrig;
		FVector ChainVecOrig;
		float ChainLenOrig;
		(ChainEndOrig - ChainStartOrig).ToDirectionAndLength(ChainVecOrig, ChainLenOrig);

		// get new chain vector
		FVector ChainStartNew;
		if (Effector.ChainRootBody->Effector)
		{
			// add effector offset to chain root
			const FEffector* RootEffector = Effector.ChainRootBody->Effector;
			FVector RootEffectorDelta = RootEffector->Position - RootEffector->PositionOrig;
			ChainStartNew = Effector.ChainRootBody->Position + RootEffectorDelta;
		}else
		{
			ChainStartNew = Effector.ChainRootBody->Position;
		}
		const FVector ChainEndNew = Effector.Position;
		FVector ChainVecNew;
		float ChainLenNew;
		(ChainEndNew - ChainStartNew).ToDirectionAndLength(ChainVecNew, ChainLenNew);

		FQuat ChainDeltaRotation = FQuat::FindBetweenNormals(ChainVecOrig, ChainVecNew);
		const float ChainDeltaAlpha = Effector.Settings.PullChainAlpha * Effector.Settings.StrengthAlpha * GlobalPullChainAlpha;
		ChainDeltaRotation = FQuat::FastLerp(FQuat::Identity, ChainDeltaRotation, ChainDeltaAlpha);
		FVector ChainDeltaPosition = ChainVecNew * (ChainLenNew - ChainLenOrig) * ChainDeltaAlpha;

		const FBone* Bone = Effector.Bone->Parent;
		int32 ChainIndex = 0;
		const float InvChainLength = 1.0f / Effector.DistToChainRootAlongBones;
		while (Bone && Bone->Body != Effector.ChainRootBody)
		{		
			// rotate body along with chain
			const FVector BodyRelativeToChain = Bone->Body->Position - ChainStartNew;
			const FVector RotatedBodyPos = ChainStartNew + ChainDeltaRotation.RotateVector(BodyRelativeToChain);
			Bone->Body->Position = RotatedBodyPos;
			Bone->Body->Rotation = ChainDeltaRotation * Bone->Body->Rotation;

			// move body proportional to chain stretch amount
			const float Strength = 1.0f - (Effector.DistancesFromEffector[ChainIndex] * InvChainLength);
			Bone->Body->Position += ChainDeltaPosition * Strength;
			
			Bone = Bone->Parent;
			++ChainIndex;
		}
	}
}

void FPBIKSolver::ApplyPreferredAngles()
{
	// apply preferred angles to squashed effector chains
	using PBIK::FEffector;
	for (FEffector& Effector : Effectors)
	{
		Effector.ApplyPreferredAngles();
	}

	// preferred angles introduce stretch, so remove that to prevent biasing the first constraint iteration
	for (int32 C = Constraints.Num() - 1; C >= 0; --C)
	{
		constexpr float FullAmount = 1.0f;
		Constraints[C]->RemoveStretch(FullAmount);
	}
}

void FPBIKSolver::SolveConstraints(const FPBIKSolverSettings& Settings)
{
	using PBIK::FRigidBody;

	auto RunConstraintPass = [this, &Settings](int32 CurrentIteration, int32 MaxIterations)
	{
		// solve all constraints
		for (const TSharedPtr<PBIK::FConstraint>& Constraint : Constraints)
		{
			Constraint->Solve(Settings);
		}
		
		// do post-pass to remove stretch
		if (!Settings.bAllowStretch)
		{
			const float StretchAmount = FMath::Min(1.0f, static_cast<float>(CurrentIteration+1) / (MaxIterations / 2.0f));
			for (int32 C = Constraints.Num() - 1; C >= 0; --C)
			{
				Constraints[C]->RemoveStretch(StretchAmount);
			}
		}
		
	};

	// run pre-pass where we lock everything not in a sub-chain
	if (bHasSubChains && Settings.SubIterations > 0)
	{
		// first lock all the bodies not in a sub chain
		for (FRigidBody& Body : Bodies)
		{
			Body.bIsLockedBySubSolve = !Body.bIsPartOfSubChain;
		}
		
		// solve all constraints (locked constraints are skipped in this pass)
		for (int32  I = 0; I < Settings.SubIterations; ++I)
		{
			RunConstraintPass(I, Settings.SubIterations);
		}
		
		// unlock all bodies so we can proceed with normal constraint solve
		for (FRigidBody& Body : Bodies)
		{
			Body.bIsLockedBySubSolve = false;
		}
	}

	// run main constraint pass
	for (int32  I = 0; I < Settings.Iterations; ++I)
	{
		RunConstraintPass(I, Settings.Iterations);
	}
}

bool FPBIKSolver::Initialize()
{
	LLM_SCOPE_BYNAME(TEXT("Animation/FBIK"));
	
	if (bReadyToSimulate)
	{
		return true;
	}

	bReadyToSimulate = false;

	if (!InitBones())
	{
		return false;
	}

	if (!InitBodies())
	{
		return false;
	}

	if (!InitConstraints())
	{
		return false;
	}

	bReadyToSimulate = true;

	return true;
}

bool FPBIKSolver::InitBones()
{
	using PBIK::FEffector;
	using PBIK::FBone;

	if (Bones.IsEmpty())
	{
		UE_LOG(LogPBIKSolver, Warning, TEXT("PBIK: no bones added to solver. Cannot initialize."));
		return false;
	}

	if (Effectors.IsEmpty())
	{
		UE_LOG(LogPBIKSolver, Warning, TEXT("PBIK: no effectors added to solver. Cannot initialize."));
		return false;
	}

	// record solver root pointer
	int32  NumSolverRoots = 0;
	for (FBone& Bone : Bones)
	{
		if (Bone.bIsSolverRoot)
		{
			SolverRoot = &Bone;
			++NumSolverRoots;
		}
	}

	if (!SolverRoot)
	{
		UE_LOG(LogPBIKSolver, Warning, TEXT("PBIK: root bone not set or not found. Cannot initialize."));
		return false;
	}

	if (NumSolverRoots > 1)
	{
		UE_LOG(LogPBIKSolver, Warning, TEXT("PBIK: more than 1 bone was marked as solver root. Cannot initialize."));
		return false;
	}

	// initialize pointers to parents
	for (FBone& Bone : Bones)
	{
		// no parent on root, remains null
		if (Bone.ParentIndex == -1)
		{
			continue;
		}

		// validate parent
		const bool bIndexInRange = Bone.ParentIndex >= 0 && Bone.ParentIndex < Bones.Num();
		if (!bIndexInRange)
		{
			UE_LOG(LogPBIKSolver, Warning, TEXT("PBIK: bone found with invalid parent index. Cannot initialize."));
			return false;
		}

		// record parent
		Bone.Parent = &Bones[Bone.ParentIndex];
	}

	// walk upwards from each effector to the root to initialize "Bone.IsSolved"
	for (const FEffector& Effector : Effectors)
	{
		FBone* CurrentBone = Effector.Bone;
		CurrentBone->bIsSolved = true;
		while (CurrentBone)
		{
			if (CurrentBone->bIsSolverRoot)
			{
				CurrentBone->bIsSolved = true;
				break;
			}
			CurrentBone->bIsSolved = true;
			CurrentBone = CurrentBone->Parent;
		}
	}

	// initialize children lists
	for (FBone& Parent : Bones)
	{
		for (FBone& Child : Bones)
		{
			if (Child.bIsSolved && Child.Parent == &Parent)
			{
				Parent.Children.Add(&Child);
			}
		}
	}

	// initialize IsSubRoot flag
	for (FBone& Bone : Bones)
	{
		Bone.bIsSubRoot = Bone.Children.Num() > 1 || Bone.bIsSolverRoot;
	}

	// store initial local rotation (preferred angles are relative to this)
	for (FBone& Bone : Bones)
	{
		const FQuat LocalRotation = Bone.Parent ? Bone.Parent->Rotation.Inverse() * Bone.Rotation : Bone.Rotation;
		Bone.LocalRotationInitial = LocalRotation.Rotator();
	}

	return true;
}

bool FPBIKSolver::InitBodies()
{
	using PBIK::FEffector;
	using PBIK::FRigidBody;
	using PBIK::FBone;

	Bodies.Empty();
	
	// create bodies
	for (const FEffector& Effector : Effectors)
	{
		FBone* NextBone = Effector.Bone;
		while (true)
		{
			FBone* BodyBone = NextBone->bIsSolverRoot ? NextBone : NextBone->Parent;
			if (!BodyBone)
			{
				UE_LOG(LogPBIKSolver, Warning, TEXT("PBIK: effector is on bone that is not on or below root bone."));
				return false;
			}

			AddBodyForBone(BodyBone);

			NextBone = BodyBone;
			if (NextBone == SolverRoot)
			{
				break;
			}
		}
	}

	// initialize bodies
	for (FRigidBody& Body : Bodies)
	{
		Body.Initialize(SolverRoot);
	}

	// sort bodies root to leaf
	Bodies.Sort();
	Algo::Reverse(Bodies);

	// store pointers to bodies on bones (after sort!)
	for (FRigidBody& Body : Bodies)
	{
		Body.Bone->Body = &Body;
	}

	return true;
}

void FPBIKSolver::AddBodyForBone(PBIK::FBone* Bone)
{
	using PBIK::FRigidBody;
	for (const FRigidBody& Body : Bodies)
	{
		if (Body.Bone == Bone)
		{
			return; // no duplicates
		}
	}
	Bodies.Emplace(Bone);
}

bool FPBIKSolver::InitConstraints()
{
	using PBIK::FEffector;
	using PBIK::FBone;
	using PBIK::FRigidBody;
	using PBIK::FPinConstraint;
	using PBIK::FJointConstraint;

	Constraints.Empty();
	
	// Add Effector Pin Constraints
	// (these are added first since they create the pulling effect which needs resolved by the joint constraints)
	for (FEffector& Effector : Effectors)
	{
		const FBone* BodyBone = Effector.Bone->bIsSolverRoot ? Effector.Bone : Effector.Bone->Parent;
		if (!BodyBone)
		{
			UE_LOG(LogPBIKSolver, Warning, TEXT("PBIK: effector is on bone that does not have a parent."));
			return false;
		}

		FRigidBody* Body = BodyBone->Body;
		TSharedPtr<FPinConstraint> Constraint = MakeShared<FPinConstraint>(Body, Effector.Position, Effector.Rotation);
		Effector.Pin = Constraint;
		Body->Effector = &Effector;
		Body->Pin = Constraint.Get();
		Constraints.Add(Constraint);
	}

	// Add joint constraints
	// (constrain all bodies together (child to parent))
	for (FRigidBody& Body : Bodies)
	{
		FRigidBody* ParentBody = Body.GetParentBody();
		if (!ParentBody)
		{
			continue; // root
		}

		TSharedPtr<FJointConstraint> Constraint = MakeShared<FJointConstraint>(ParentBody, &Body);
		Constraints.Add(Constraint);
	}

	// now we can set the initial effector depths
	UpdateEffectorDepths();
	
	return true;
}

void FPBIKSolver::UpdateEffectorDepths()
{
	using PBIK::FEffector;
	using PBIK::FBone;
	using PBIK::FRigidBody;
	
	// initialize Effector's nearest ParentSubRoot (FBody) pointer
	// must be done AFTER setting: Bone.IsSubRoot/IsSolverRoot/Parent
	bool bIsEffectorDepthDirty = false;
	bHasSubChains = false;
	for (const FEffector& Effector : Effectors)
	{
		if (Effector.ChainDepthInitializedWith != Effector.Settings.ChainDepth)
		{
			bIsEffectorDepthDirty = true;
		}
		if (Effector.Settings.ChainDepth > 0)
		{
			bHasSubChains = true;
		}
	}

	if (bIsEffectorDepthDirty)
	{
		// reset body states related to sub chains
		for (FRigidBody& Body : Bodies)
		{
			Body.bIsPartOfSubChain = false;
			Body.bIsLockedBySubSolve = false;
		}
		// walk up each effector and set chain states
		for (FEffector& EffectorToUpdate : Effectors)
		{
			EffectorToUpdate.UpdateChainStates();
		}
	}
}

PBIK::FDebugDraw* FPBIKSolver::GetDebugDraw()
{
	DebugDraw.Solver = this;
	return &DebugDraw;
}

void FPBIKSolver::Reset()
{
	LLM_SCOPE_BYNAME(TEXT("Animation/FBIK"));
	
	bReadyToSimulate = false;
	SolverRoot = nullptr;
	Bodies.Empty();
	Bones.Empty();
	Constraints.Empty();
	Effectors.Empty();
}

bool FPBIKSolver::IsReadyToSimulate() const
{
	return bReadyToSimulate;
}

int32 FPBIKSolver::AddBone(
	const FName Name,
	const int32 ParentIndex,
	const FVector& InOrigPosition,
	const FQuat& InOrigRotation,
	bool bIsSolverRoot)
{
	return Bones.Emplace(Name, ParentIndex, InOrigPosition, InOrigRotation, bIsSolverRoot);
}

int32 FPBIKSolver::AddEffector(const FName BoneName)
{
	for (PBIK::FBone& Bone : Bones)
	{
		if (Bone.Name == BoneName)
		{
			Effectors.Emplace(&Bone);
			return Effectors.Num() - 1;
		}
	}

	return -1;
}

void FPBIKSolver::SetBoneTransform(
	const int32 Index,
	const FTransform& InTransform)
{
	check(Index >= 0 && Index < Bones.Num());
	Bones[Index].Position = InTransform.GetLocation();
	Bones[Index].Rotation = InTransform.GetRotation();
	Bones[Index].Scale = InTransform.GetScale3D();
}

PBIK::FBoneSettings* FPBIKSolver::GetBoneSettings(const int32 Index)
{
	// make sure to call Initialize() before applying bone settings
	if (!ensureMsgf(bReadyToSimulate, TEXT("PBIK: trying to access Bone Settings before Solver is initialized.")))
	{
		return nullptr;
	}

	if (!ensureMsgf(Bones.IsValidIndex(Index), TEXT("PBIK: trying to access Bone Settings with invalid bone index.")))
	{
		return nullptr;
	}

	if (!Bones[Index].Body)
	{
		// Bone is not part of the simulation. This happens if the bone is not located between an effector and the
		// root of the solver. Not necessarily an error, as some systems dynamically disable effectors which can leave
		// orphaned Bone Settings, so we simply ignore them.
		return nullptr;
	}

	return &Bones[Index].Body->J;
}

void FPBIKSolver::GetBoneGlobalTransform(const int32 Index, FTransform& OutTransform)
{
	check(Index >= 0 && Index < Bones.Num());
	const PBIK::FBone& Bone = Bones[Index];
	OutTransform.SetLocation(Bone.Position);
	OutTransform.SetRotation(Bone.Rotation);
	OutTransform.SetScale3D(Bone.Scale);
}

int32 FPBIKSolver::GetBoneIndex(FName BoneName) const
{
	return Bones.IndexOfByPredicate([&BoneName](const PBIK::FBone& Bone) { return Bone.Name == BoneName; });
}

void FPBIKSolver::SetEffectorGoal(
	const int32 Index,
	const FVector& InPosition,
	const FQuat& InRotation,
	const PBIK::FEffectorSettings& InSettings)
{
	check(Index >= 0 && Index < Effectors.Num());
	Effectors[Index].SetGoal(InPosition, InRotation, InSettings);
}

// given a set of points in an initial configuration and the same set of points in a deformed configuration,
// this function outputs a quaternion that represents the "best fit" rotation that rotates the initial points to the
// current points.
FQuat FPBIKSolver::GetRotationFromDeformedPoints(
	const TArrayView<FVector>& InInitialPoints,
	const TArrayView<FVector>& InCurrentPoints,
	FVector& OutInitialCentroid,
	FVector& OutCurrentCentroid)
{
	check(InInitialPoints.Num() == InCurrentPoints.Num());

	// must have more than 1 point to generate a gradient
	if (InInitialPoints.Num() <= 1)
	{
		OutInitialCentroid = (InInitialPoints.Num() == 1) ? InInitialPoints[0] : FVector::ZeroVector;
		OutCurrentCentroid = (InCurrentPoints.Num() == 1) ? InCurrentPoints[0] : FVector::ZeroVector;
		return FQuat::Identity;
	}
	
	// calculate initial and current centroids
	OutInitialCentroid = FVector::ZeroVector;
	OutCurrentCentroid = FVector::ZeroVector;
	for (int32 PointIndex=0; PointIndex<InInitialPoints.Num(); ++PointIndex)
	{
		OutInitialCentroid += InInitialPoints[PointIndex];
		OutCurrentCentroid += InCurrentPoints[PointIndex];
	}

	// average centroids
	const float InvNumEffectors = 1.0f / static_cast<float>(InInitialPoints.Num());
	OutInitialCentroid *= InvNumEffectors;
	OutCurrentCentroid *= InvNumEffectors;

	// accumulate deformation gradient to extract a rotation from
	FVector DX = FVector::ZeroVector; // DX, DY, DZ are rows of 3x3 deformation gradient tensor
	FVector DY = FVector::ZeroVector;
	FVector DZ = FVector::ZeroVector;
	for (int32 PointIndex=0; PointIndex<InInitialPoints.Num(); ++PointIndex)
	{
		// accumulate the deformation gradient tensor for all points
		// "Meshless Deformations Based on Shape Matching"
		// Equation 7 describes accumulation of deformation gradient from points
		//
		// P is normalized vector from INITIAL centroid to INITIAL point
		// Q is normalized vector from CURRENT centroid to CURRENT point
		FVector P = (InInitialPoints[PointIndex] - OutInitialCentroid).GetSafeNormal();
		FVector Q = (InCurrentPoints[PointIndex] - OutCurrentCentroid).GetSafeNormal();
		// PQ^T is the outer product of P and Q which is a 3x3 matrix
		// https://en.m.wikipedia.org/wiki/Outer_product
		DX += FVector(P[0]*Q[0], P[0]*Q[1], P[0]*Q[2]);
		DY += FVector(P[1]*Q[0], P[1]*Q[1], P[1]*Q[2]);
		DZ += FVector(P[2]*Q[0], P[2]*Q[1], P[2]*Q[2]);
	}

	// extract "best fit" rotation from deformation gradient
	FQuat Q = FQuat::Identity;
	constexpr int32 MaxIter = 50;
	// "A Robust Method to Extract the Rotational Part of Deformations" equation 7
	// https://matthias-research.github.io/pages/publications/stablePolarDecomp.pdf
	for (unsigned int Iter = 0; Iter < MaxIter; Iter++)
	{
		FMatrix R = FRotationMatrix::Make(Q);
		FVector RCol0(R.M[0][0], R.M[0][1], R.M[0][2]);
		FVector RCol1(R.M[1][0], R.M[1][1], R.M[1][2]);
		FVector RCol2(R.M[2][0], R.M[2][1], R.M[2][2]);
		FVector Omega = RCol0.Cross(DX) + RCol1.Cross(DY) + RCol2.Cross(DZ);
		Omega *= 1.0f / (fabs(RCol0.Dot(DX) + RCol1.Dot(DY) + RCol2.Dot(DZ)) + SMALL_NUMBER);
		const float W = Omega.Size();
		if (W < SMALL_NUMBER)
		{
			break;
		}
		Q = FQuat(FQuat((1.0 / W) * Omega, W)) * Q;
		Q.Normalize();
	}

	return Q;
}

