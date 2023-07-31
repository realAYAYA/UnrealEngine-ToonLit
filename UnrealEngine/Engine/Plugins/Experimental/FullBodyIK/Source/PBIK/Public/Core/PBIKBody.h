// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/NameTypes.h"
#include "Math/Vector.h"
#include "Math/Quat.h"

struct FPBIKSolverSettings;

namespace PBIK
{
	struct FPinConstraint;
	struct FJointConstraint;
	struct FRigidBody;
	struct FEffector;

struct FBone
{
	FName Name;
	int32 ParentIndex = -2; // -2 is unset, -1 for root, or 0...n otherwise
	FVector Position;
	FQuat Rotation;
	FVector Scale; // just passed through, not modified
	FVector LocalPositionOrig;
	FQuat LocalRotationOrig;

	// initialized - these fields are null/empty until after Solver::Initialize()
	FRigidBody* Body = nullptr;
	FBone* Parent = nullptr;
	TArray<FBone*> Children;
	bool bIsSolverRoot = false;
	bool bIsSolved = false;
	bool bIsSubRoot = false;
	float Length = 0.f;
	// initialized

	FBone(
		const FName InName,
		const int32& InParentIndex,		// must pass -1 for root of whole skeleton
		const FVector& InOrigPosition,
		const FQuat& InOrigRotation,
		bool bInIsSolverRoot);

	bool HasChild(const FBone* Bone);
	void UpdateFromInputs();
};

enum class ELimitType : uint8
{
	Free,
	Limited,
	Locked,
};

struct FBoneSettings
{
	float RotationStiffness = 0.0f; // range (0, 1)
	float PositionStiffness = 0.0f; // range (0, 1)

	ELimitType X;
	float MinX = 0.0f; // range (-180, 180)
	float MaxX = 0.0f;

	ELimitType Y;
	float MinY = 0.0f;
	float MaxY = 0.0f;

	ELimitType Z;
	float MinZ = 0.0f;
	float MaxZ = 0.0f;

	bool bUsePreferredAngles = false;
	FRotator PreferredAngles = FRotator::ZeroRotator;
};

struct FRigidBody
{
	FBone* Bone = nullptr;
	FBoneSettings J;
	FPinConstraint* Pin = nullptr;
	FEffector* Effector = nullptr;

	FVector Position;
	FQuat Rotation;
	FVector InputPosition;
	FQuat InitialRotation;
	FVector BoneLocalPosition;
	TArray<FVector> ChildLocalPositions;

	float InvMass = 0.f;
	float MaxInvMass = 0.f;
	float MinInvMass = 0.f;
	float Mass = 0.f;
	
private:

	int32 NumBonesToRoot = 0;

public:

	FRigidBody(FBone* InBone);

	void Initialize(FBone* SolverRoot);

	void UpdateFromInputs(const FPBIKSolverSettings& Settings);

	int GetNumBonesToRoot() const;

	FRigidBody* GetParentBody() const;

	void ApplyPushToRotateBody(const FVector& Push, const FVector& Offset);
	
	void ApplyPushToPosition(const FVector& Push);

	void ApplyRotationDelta(const FQuat& InDelta);
};

// for sorting Bodies hierarchically (root to leaf order)
inline bool operator<(const FRigidBody& Lhs, const FRigidBody& Rhs)
{ 
	return Lhs.GetNumBonesToRoot() < Rhs.GetNumBonesToRoot(); 
}

} // namespace