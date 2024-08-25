// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "BonePose.h"

// Use the simulation space functions from the RBAN
#include "BoneControllers/AnimNode_RigidBody.h" 

struct FComponentSpacePoseContext;

namespace RigidBodyWithControl
{

//======================================================================================================================
struct FOutputBoneData
{
	FOutputBoneData()
		: CompactPoseBoneIndex(INDEX_NONE), CompactPoseParentBoneIndex(INDEX_NONE)
	{}

	TArray<FCompactPoseBoneIndex> BoneIndicesToParentBody;
	FCompactPoseBoneIndex CompactPoseBoneIndex;
	FCompactPoseBoneIndex CompactPoseParentBoneIndex;
	int32 BodyIndex; // Index into Bodies - will be the same index as into the joints
	int32 ParentBodyIndex;
};

//======================================================================================================================
// Simple minimal implementation of a "FTransform without scale"
struct FPosQuat
{
	FPosQuat(const FVector& Pos, const FQuat& Quat) : Translation(Pos), Rotation(Quat) {}
	FPosQuat(const FQuat& Quat, const FVector& Pos) : Translation(Pos), Rotation(Quat) {}
	FPosQuat(const FRotator& Rotator, const FVector& Pos) : Translation(Pos), Rotation(Rotator) {}
	FPosQuat() : Translation(EForceInit::ForceInitToZero), Rotation(EForceInit::ForceInit) {}
	FPosQuat(const FTransform& TM) : Translation(TM.GetTranslation()), Rotation(TM.GetRotation()) {}
	FPosQuat(ENoInit) {}

	FORCEINLINE FVector GetTranslation() const { return Translation; }
	FORCEINLINE FQuat GetRotation() const { return Rotation; }

	FORCEINLINE FTransform ToTransform() const
	{
		return FTransform(Rotation, Translation);
	}

	// Note that multiplication operates in the same sense as FTransform (i.e. "backwards")
	FORCEINLINE FPosQuat operator*(const FPosQuat& Other) const
	{
		FQuat OutRotation = Other.Rotation * Rotation;
		FVector OutTranslation = Other.Rotation * (Translation) + Other.Translation;
		return FPosQuat(OutTranslation, OutRotation);
	}

	FORCEINLINE FVector operator*(const FVector& Position) const
	{
		return Translation + Rotation * Position;
	}

	FORCEINLINE FPosQuat Inverse() const
	{
		const FQuat OutRotation = Rotation.Inverse();
		return FPosQuat(OutRotation * -Translation, OutRotation);
	}

	FVector Translation;
	FQuat Rotation;
};

//======================================================================================================================
struct FRigidBodyPoseData
{
	void Update(
		FComponentSpacePoseContext&    ComponentSpacePoseContext,
		const TArray<FOutputBoneData>& OutputBoneData,
		const ESimulationSpace         SimulationSpace,
		const FBoneReference&          BaseBoneRef,
		const FGraphTraversalCounter&  InUpdateCounter);

	FPosQuat GetTM(int32 Index) const { return BoneTMs[Index]; }
	bool IsValidIndex(const int32 Index) const { return BoneTMs.IsValidIndex(Index); }
	bool IsEmpty() const { return BoneTMs.IsEmpty(); }

	/**
	 * The cached skeletal data, updated at the start of each tick
	 */
	TArray<FPosQuat> BoneTMs;

	// Track when we were currently/last updated so the user can detect missing updates if calculating
	// velocity etc
	FGraphTraversalCounter UpdateCounter;
	// When the update is called we'll take the current counter, increment it, and store here so it
	// can be compared.
	FGraphTraversalCounter ExpectedUpdateCounter;
};

}