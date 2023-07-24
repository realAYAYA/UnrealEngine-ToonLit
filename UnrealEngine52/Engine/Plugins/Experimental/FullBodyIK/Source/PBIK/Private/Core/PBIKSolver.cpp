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

	void FEffector::UpdateFromInputs(const FBone& SolverRoot)
	{
		// blend effector transform by alpha and set pin goal transform
		Position = FMath::Lerp(PositionOrig, PositionGoal, Settings.PositionAlpha);
		Rotation = FMath::Lerp(RotationOrig, RotationGoal, Settings.RotationAlpha);
		Pin.Pin()->SetGoal(Position, Rotation, Settings.StrengthAlpha);

		// update length of chain this effector controls in the input pose
		if (ParentSubRoot)
		{
			FEffector* ParentEffector = ParentSubRoot->Effector;
			const FVector ParentSubRootPosition = ParentEffector ? ParentEffector->PositionOrig : ParentSubRoot->Position;
			DistanceToSubRootInInputPose = (ParentSubRootPosition - Bone->Position).Size();
		}

		DistToRootStraightLine = (PositionOrig - SolverRoot.Position).Size();

		// update distances to root (along bones)
		{
			if (Bone->bIsSolverRoot)
			{
				DistToRootAlongBones = 0.0f;
			}else
			{
				DistToRootAlongBones = Bone->Length;
				FBone* Parent = Bone->Parent;
				while (Parent && !Parent->bIsSolverRoot)
				{
					DistToRootAlongBones += Parent->Length;
					Parent = Parent->Parent;
				}
			}
		}

		// update distances to SUB root (along bones)
		if (ParentSubRoot)
		{
			DistancesFromEffector.Reset();
			DistancesFromEffector.Add(0.0f);
			DistToSubRootAlongBones = Bone->Length;
			FBone* Parent = Bone->Parent;
			while (Parent && Parent->Body != ParentSubRoot)
			{
				DistancesFromEffector.Add(DistToSubRootAlongBones);
				DistToSubRootAlongBones += Parent->Length;
				Parent = Parent->Parent;
			}
		}
	}

	void FEffector::ApplyPreferredAngles()
	{
		// optionally apply a preferred angle to give solver a hint which direction to favor
		// apply amount of preferred angle proportional to the amount this sub-limb is squashed

		// can't squash root chain
		if (!ParentSubRoot)
		{
			return; 
		}

		// can't squash chain with zero length already
		if (DistanceToSubRootInInputPose <= SMALL_NUMBER)
		{
			return;
		}

		// we have to be careful here when calculating the distance to the parent sub-root.
		// if the parent sub-root is attached to an effector, use the effector's position
		// otherwise use the current position of the FRigidBody
		FEffector* ParentEffector = ParentSubRoot->Effector;
		const FVector ParentSubRootPosition = ParentEffector ? ParentEffector->Position : ParentSubRoot->Position;
		const float DistToParentSubRoot = (ParentSubRootPosition - Position).Size();
		if (DistToParentSubRoot >= DistanceToSubRootInInputPose)
		{
			return; // limb is stretched
		}
		
		// amount squashed (clamped to scaled original length)
		float DeltaSquash = DistanceToSubRootInInputPose - DistToParentSubRoot;
		DeltaSquash = DeltaSquash > DistanceToSubRootInInputPose ? DistanceToSubRootInInputPose : DeltaSquash;
		float SquashPercent = DeltaSquash / DistanceToSubRootInInputPose;
		SquashPercent = PBIK::CircularEaseOut(SquashPercent);
		if (FMath::IsNearlyZero(SquashPercent))
		{
			return; // limb not squashed enough
		}

		FBone* Parent = Bone->Parent;
		while (Parent && Parent->bIsSolved)
		{
			if (Parent->Body->J.bUsePreferredAngles)
			{
				FQuat PartialRotation = FQuat::FastLerp(FQuat::Identity, FQuat(Parent->Body->J.PreferredAngles), SquashPercent);
				Parent->Body->Rotation = Parent->Body->Rotation * PartialRotation;
				Parent->Body->Rotation.Normalize();
			}

			if (Parent == ParentSubRoot->Bone)
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
	if (RootPin.IsValid())
	{
		RootPin.Pin()->bEnabled = Settings.RootBehavior == EPBIKRootBehavior::PinToInput;
		// pin to animated input root pose
		RootPin.Pin()->SetGoal(SolverRoot->Position, SolverRoot->Rotation, 1.0f);
	}

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

		if (Bone->Body)
		{
			continue; // effector is between other effectors, so leave transform where body ended up
		}
		
		Bone->Position = Bone->Parent->Position + Bone->Parent->Rotation * Bone->LocalPositionOrig;

		if (Effector.Settings.PinRotation > SMALL_NUMBER)
		{
			// optionally pin rotation to that of effector
			const float RotAmount = FMath::Clamp(0.0f, 1.0f, Effector.Settings.PinRotation);
			Bone->Rotation = FQuat::FastLerp(Bone->Rotation, Effector.Rotation, RotAmount).GetNormalized();
		}else
		{
			Bone->Rotation = Bone->Parent->Rotation * Bone->LocalRotationOrig;
		}
	}

	// propagate to non-solved bones (requires storage in root to tip order)
	for (FBone& Bone : Bones)
	{
		if (Bone.bIsSolved || !Bone.Parent)
		{
			continue;
		}

		Bone.Position = Bone.Parent->Position + Bone.Parent->Rotation * Bone.LocalPositionOrig;
		Bone.Rotation = Bone.Parent->Rotation * Bone.LocalRotationOrig;
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
		if (!Effector.ParentSubRoot)
		{
			continue;
		}

		if (Effector.DistToSubRootAlongBones < SMALL_NUMBER)
		{
			continue;
		}

		if (Effector.Settings.PullChainAlpha < SMALL_NUMBER)
		{
			continue;
		}

		// get original chain vector
		const FVector ChainStartOrig = Effector.ParentSubRoot->InputPosition;
		const FVector ChainEndOrig = Effector.PositionOrig;
		FVector ChainVecOrig;
		float ChainLenOrig;
		(ChainEndOrig - ChainStartOrig).ToDirectionAndLength(ChainVecOrig, ChainLenOrig);

		// get new chain vector
		FVector ChainStartNew;
		if (Effector.ParentSubRoot->Effector)
		{
			// add effector offset to chain root
			FEffector* RootEffector = Effector.ParentSubRoot->Effector;
			FVector RootEffectorDelta = RootEffector->Position - RootEffector->PositionOrig;
			ChainStartNew = Effector.ParentSubRoot->Position + RootEffectorDelta;
		}else
		{
			ChainStartNew = Effector.ParentSubRoot->Position;
		}
		const FVector ChainEndNew = Effector.Position;
		FVector ChainVecNew;
		float ChainLenNew;
		(ChainEndNew - ChainStartNew).ToDirectionAndLength(ChainVecNew, ChainLenNew);

		FQuat ChainDeltaRotation = FQuat::FindBetweenNormals(ChainVecOrig, ChainVecNew);
		const float ChainDeltaAlpha = Effector.Settings.PullChainAlpha * Effector.Settings.StrengthAlpha * GlobalPullChainAlpha;
		ChainDeltaRotation = FQuat::FastLerp(FQuat::Identity, ChainDeltaRotation, ChainDeltaAlpha);
		FVector ChainDeltaPosition = ChainVecNew * (ChainLenNew - ChainLenOrig) * ChainDeltaAlpha;

		FBone* Bone = Effector.Bone->Parent;
		int32 ChainIndex = 0;
		const float InvChainLength = 1.0f / Effector.DistToSubRootAlongBones;
		while (Bone && Bone->Body != Effector.ParentSubRoot)
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

	for (int32  I = 0; I < Settings.Iterations; ++I)
	{
		// solve all constraints
		for (const TSharedPtr<PBIK::FConstraint>& Constraint : Constraints)
		{
			Constraint->Solve(Settings);
		}
		
		// do post-pass to remove stretch
		if (!Settings.bAllowStretch)
		{
			const float StretchAmount = FMath::Min(1.0f, static_cast<float>(I+1) / (Settings.Iterations / 2.0f));
			for (int32 C = Constraints.Num() - 1; C >= 0; --C)
			{
				Constraints[C]->RemoveStretch(StretchAmount);
			}
		}
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
	for (FEffector& Effector : Effectors)
	{
		FBone* NextBone = Effector.Bone;
		while (NextBone)
		{
			NextBone->bIsSolved = true;
			NextBone = NextBone->Parent;
			if (NextBone && NextBone->bIsSolverRoot)
			{
				NextBone->bIsSolved = true;
				break;
			}
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

	// initialize Effector's nearest ParentSubRoot (FBody) pointer
	// must be done AFTER setting: Bone.IsSubRoot/IsSolverRoot/Parent
	for (FEffector& Effector : Effectors)
	{
		const FBone* Parent = Effector.Bone->Parent;
		while (Parent)
		{
			if (!Parent->bIsSolved)
			{
				break; // this only happens when effector is on solver root
			}

			if (Parent->bIsSubRoot || Parent->bIsSolverRoot)
			{
				Effector.ParentSubRoot = Parent->Body;
				break;
			}

			Parent = Parent->Parent;
		}
	}

	return true;
}

void FPBIKSolver::AddBodyForBone(PBIK::FBone* Bone)
{
	for (PBIK::FRigidBody& Body : Bodies)
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
		TSharedPtr<FPinConstraint> Constraint = MakeShared<FPinConstraint>(Body, Effector.Position, Effector.Rotation, false);
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

	// pin root body to animated location 
	// this constraint is by default off in solver settings
	// this constraint is added last to ensure it is fully solved at the end of the iteration loop
	if (!SolverRoot->Body->Effector) // only add if user hasn't added their own root effector
	{
		const TSharedPtr<FPinConstraint> RootConstraint = MakeShared<FPinConstraint>(SolverRoot->Body, SolverRoot->Position, SolverRoot->Rotation, true);
		Constraints.Add(RootConstraint);
		RootPin = RootConstraint;
		SolverRoot->Body->Pin = RootConstraint.Get();
	}
	
	return true;
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
	RootPin = nullptr;
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

int32 FPBIKSolver::AddEffector(FName BoneName)
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

