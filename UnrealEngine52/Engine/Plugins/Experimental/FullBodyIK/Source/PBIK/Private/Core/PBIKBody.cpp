// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/PBIKBody.h"
#include "Core/PBIKConstraint.h"
#include "Core/PBIKSolver.h"

namespace PBIK
{

FBone::FBone(
	const FName InName,
	const int& InParentIndex,		// must pass -1 for root of whole skeleton
	const FVector& InOrigPosition,
	const FQuat& InOrigRotation,
	bool bInIsSolverRoot)
{
	Name = InName;
	ParentIndex = InParentIndex;
	Position = InOrigPosition;
	Rotation = InOrigRotation;
	bIsSolverRoot = bInIsSolverRoot;
}

bool FBone::HasChild(const FBone* Bone)
{
	for (const FBone* Child : Children)
	{
		if (Bone->Name == Child->Name)
		{
			return true;
		}
	}

	return false;
}

void FBone::UpdateFromInputs()
{
	if (!Parent)
	{
		return;
	}

	LocalPositionOrig = Parent->Rotation.Inverse() * (Position - Parent->Position);
	LocalRotationOrig = Parent->Rotation.Inverse() * Rotation;
	Length = LocalPositionOrig.Size();
}

FRigidBody::FRigidBody(FBone* InBone)
{
	Bone = InBone;
	J = FBoneSettings();
}

void FRigidBody::Initialize(const FBone* SolverRoot)
{
	// calculate transform and mass of body based on the skeleton
	UpdateTransformAndMassFromBones();

	// store initial rotation from ref pose
	InitialRotation = Rotation;
	
	// calculate num bones distance to root
	NumBonesToRoot = 0;
	const FBone* Parent = Bone;
	while (Parent && Parent != SolverRoot)
	{
		NumBonesToRoot += 1;
		Parent = Parent->Parent;
	}
}

void FRigidBody::UpdateFromInputs(const FPBIKSolverSettings& Settings)
{
	if (Settings.bStartSolveFromInputPose)
	{
		UpdateTransformAndMassFromBones();
	}

	// update InvMass based on global mass multiplier
	constexpr float MinMass = 0.5f; // prevent mass ever hitting zero
	InvMass = 1.0f / FMath::Max(MinMass,(Mass * Settings.MassMultiplier * GLOBAL_UNITS));

	SolverSettings = &Settings;
}

void FRigidBody::UpdateTransformAndMassFromBones()
{
	FVector Centroid = Bone->Position;
	Mass = 0.0f;
	for(const FBone* Child : Bone->Children)
	{
		Centroid += Child->Position;
		Mass += (Bone->Position - Child->Position).Size();
	}
	Centroid = Centroid * (1.0f / (Bone->Children.Num() + 1.0f));

	Position = InputPosition = Centroid;
	Rotation = InitialRotation = Bone->Rotation;
	BoneLocalPosition = Bone->Rotation.Inverse() * (Bone->Position - Centroid);

	ChildLocalPositions.Reserve(Bone->Children.Num());
	for (const FBone* Child : Bone->Children)
	{
		FVector ChildLocalPos = Rotation.Inverse() * (Child->Position - Centroid);
		ChildLocalPositions.Add(ChildLocalPos);
	}
}

int FRigidBody::GetNumBonesToRoot() const
{ 
	return NumBonesToRoot; 
}

FRigidBody* FRigidBody::GetParentBody() const
{
	if (Bone && Bone->Parent)
	{
		return Bone->Parent->Body;
	}

	return nullptr;
}

float FRigidBody::GetInverseMass()
{
	if (Pin && Pin->bEnabled)
	{
		return 1.0f - Pin->Alpha;
	}

	return InvMass;
}

void FRigidBody::ApplyPushToRotateBody(const FVector& Push, const FVector& Offset)
{
	if (Pin && Pin->bEnabled && Pin->bPinRotation)
	{
		return; // rotation of this body is pinned
	}
	
	// equation 8 in "Detailed Rigid Body Simulation with XPBD"
	const FVector Omega = InvMass * (1.0f - J.RotationStiffness) * FVector::CrossProduct(Offset, Push);
	const FQuat DeltaQ(Omega.X, Omega.Y, Omega.Z, 0.0f);
	ApplyRotationDelta(DeltaQ);
}

void FRigidBody::ApplyPushToPosition(const FVector& Push)
{
	Position += Push * (1.0f - J.PositionStiffness) * SolverSettings->OverRelaxation;
}

void FRigidBody::ApplyRotationDelta(const FQuat& DeltaQ)
{
	if (Pin && Pin->bEnabled && Pin->bPinRotation)
	{
		return; // rotation of this body is pinned
	}

	// limit rotation each iteration
	FQuat ClampedDQ = DeltaQ;
	const float MaxPhi = FMath::DegreesToRadians(SolverSettings->MaxAngle);
	const float Phi = DeltaQ.Size();
	if (Phi > MaxPhi)
	{
		ClampedDQ *= MaxPhi / Phi;
	}

	/** DeltaQ is assumed to be a "pure" quaternion representing an infintesimal rotation */
	FQuat Delta = ClampedDQ * Rotation;
	Delta.X *= 0.5f;
	Delta.Y *= 0.5f;
	Delta.Z *= 0.5f;
	Delta.W *= 0.5f;
	Rotation.X = Rotation.X + Delta.X;
	Rotation.Y = Rotation.Y + Delta.Y;
	Rotation.Z = Rotation.Z + Delta.Z;
	Rotation.W = Rotation.W + Delta.W;
	Rotation.Normalize();
}
} // namespace
