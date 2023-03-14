// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/PBIKBody.h"
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

void FRigidBody::Initialize(FBone* SolverRoot)
{
	FVector Centroid = Bone->Position;
	Mass = 0.0f;
	for(const FBone* Child : Bone->Children)
	{
		Centroid += Child->Position;
		Mass += (Bone->Position - Child->Position).Size();
	}
	Centroid = Centroid * (1.0f / (Bone->Children.Num() + 1.0f));

	Position = Centroid;
	Rotation = InitialRotation = Bone->Rotation;
	BoneLocalPosition = Rotation.Inverse() * (Bone->Position - Centroid);

	for (FBone* Child : Bone->Children)
	{
		FVector ChildLocalPos = Rotation.Inverse() * (Child->Position - Centroid);
		ChildLocalPositions.Add(ChildLocalPos);
	}

	// calculate num bones distance to root
	NumBonesToRoot = 0;
	FBone* Parent = Bone;
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
		// set to input pose
		Position = Bone->Position - Bone->Rotation * BoneLocalPosition;
		Rotation = Bone->Rotation;
		InputPosition = Position;
	}

	// for fork joints (multiple solved children) we sum lengths to all children (see Initialize)
	const float MinMass = 0.5f; // prevent mass ever hitting zero
	MaxInvMass = 1.0f / (Mass * ((Settings.MassMultiplier * GLOBAL_UNITS) + MinMass));
	MinInvMass = 1.0f / (Mass * ((Settings.MinMassMultiplier * GLOBAL_UNITS) + MinMass));
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
	Position += Push * (1.0f - J.PositionStiffness);
}

void FRigidBody::ApplyRotationDelta(const FQuat& DeltaQ)
{
	if (Pin && Pin->bEnabled && Pin->bPinRotation)
	{
		return; // rotation of this body is pinned
	}

	/** DeltaQ is assumed to be a "pure" quaternion representing an infintesimal rotation */
	FQuat Delta = DeltaQ * Rotation;
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