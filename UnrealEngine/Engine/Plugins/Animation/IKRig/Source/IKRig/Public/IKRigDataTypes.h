// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BoneContainer.h"
#include "IKRigDefinition.h"

#include "IKRigDataTypes.generated.h"

class UIKRigProcessor;

UENUM(BlueprintType)
enum class EIKRigGoalSpace : uint8
{
	Component		UMETA(DisplayName = "Component Space"),
	Additive		UMETA(DisplayName = "Additive"),
    World			UMETA(DisplayName = "World Space"),
};

UENUM(BlueprintType)
enum class EIKRigGoalTransformSource : uint8
{
	Manual			UMETA(DisplayName = "Manual Input"),
    Bone			UMETA(DisplayName = "Bone"),
    ActorComponent	UMETA(DisplayName = "Actor Component"),
};

USTRUCT(Blueprintable)
struct IKRIG_API FIKRigGoal
{
	GENERATED_BODY()
	
	/** Name of the IK Goal. Must correspond to the name of a Goal in the target IKRig asset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Goal)
	FName Name;

	/** Set the source of the transform data for the Goal.
	 * "Manual Input" uses the values provided by the blueprint node pin.
	 * "Bone" uses the transform of the bone provided by OptionalSourceBone.
	 * "Actor Component" uses the transform supplied by any Actor Components that implements the IIKGoalCreatorInterface*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Goal)
	EIKRigGoalTransformSource TransformSource = EIKRigGoalTransformSource::Manual;

	/** When TransformSource is set to "Bone" mode, the Position and Rotation will be driven by this Bone's input transform.
	 * When using a Bone as the transform source, the Position and Rotation Alpha values can still be set as desired.
	 * But the PositionSpace and RotationSpace are no longer relevant and will not be used.*/
	UPROPERTY(EditAnywhere, Category = Goal)
	FBoneReference SourceBone;
	
	// UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Goal, meta = (CustomWidget = "BoneName"))
	// FName OptionalSourceBone;
	
	/** Position of the IK goal in Component Space of target actor component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Position, meta=(EditCondition="Source == EIKRigGoalSource::Manual"))
	FVector Position;

	/** Rotation of the IK goal in Component Space of target actor component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Rotation, meta=(EditCondition="Source == EIKRigGoalSource::Manual"))
	FRotator Rotation;

	/** Range 0-1, default is 1.0. Smoothly blends the Goal position from the input pose (0.0) to the Goal position (1.0). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Position, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float PositionAlpha;

	/** Range 0-1, default is 1.0. Smoothly blends the Goal rotation from the input pose (0.0) to the Goal rotation (1.0). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Rotation, meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float RotationAlpha;

	/** The space that the goal position is in.
	 *"Additive" treats the goal transform as an additive offset relative to the Bone at the effector.
	 *"Component" treats the goal transform as being in the space of the Skeletal Mesh Actor Component.
	 *"World" treats the goal transform as being in the space of the World. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Position)
	EIKRigGoalSpace PositionSpace;
	
	/** The space that the goal rotation is in.
	*"Additive" treats the goal transform as an additive offset relative to the Bone at the effector.
	*"Component" treats the goal transform as being in the space of the Skeletal Mesh Actor Component.
	*"World" treats the goal transform as being in the space of the World. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Rotation)
	EIKRigGoalSpace RotationSpace;

	UPROPERTY(Transient)
	FVector FinalBlendedPosition;

	UPROPERTY(Transient)
	FQuat FinalBlendedRotation;

	FIKRigGoal()
    : Name(NAME_None),
	Position(ForceInitToZero),
	Rotation(FRotator::ZeroRotator),
	PositionAlpha(1.0f),
    RotationAlpha(1.0f),
	PositionSpace(EIKRigGoalSpace::Additive),
	RotationSpace(EIKRigGoalSpace::Additive),
	FinalBlendedPosition(ForceInitToZero),
	FinalBlendedRotation(FQuat::Identity){}
	
	FIKRigGoal(const FName& GoalName)
    : Name(GoalName),
	Position(ForceInitToZero),
	Rotation(FRotator::ZeroRotator),
    PositionAlpha(1.0f),
    RotationAlpha(1.0f),
	PositionSpace(EIKRigGoalSpace::Additive),
    RotationSpace(EIKRigGoalSpace::Additive),
	FinalBlendedPosition(ForceInitToZero),
    FinalBlendedRotation(FQuat::Identity){}

	FIKRigGoal(
        const FName& InName,
        const FVector& InPosition,
        const FQuat& InRotation,
        const float InPositionAlpha,
        const float InRotationAlpha,
        EIKRigGoalSpace InPositionSpace,
        EIKRigGoalSpace InRotationSpace)
        : Name(InName),
        Position(InPosition),
        Rotation(InRotation.Rotator()),
        PositionAlpha(InPositionAlpha),
        RotationAlpha(InRotationAlpha),
		PositionSpace(InPositionSpace),
		RotationSpace(InRotationSpace),
		FinalBlendedPosition(InPosition),
		FinalBlendedRotation(InRotation){}

	FIKRigGoal(const UIKRigEffectorGoal* InGoal)
		: Name(InGoal->GoalName),
		Position(InGoal->CurrentTransform.GetTranslation()),
		Rotation(InGoal->CurrentTransform.Rotator()),
		PositionAlpha(InGoal->PositionAlpha),
        RotationAlpha(InGoal->RotationAlpha),
		PositionSpace(EIKRigGoalSpace::Component),
        RotationSpace(EIKRigGoalSpace::Component),
        FinalBlendedPosition(ForceInitToZero),
        FinalBlendedRotation(FQuat::Identity){}

	FString ToString() const
	{
		return FString::Printf(TEXT("Name=%s, Pos=(%s, Alpha=%3.3f), Rot=(%s, Alpha=%3.3f)"),
			*Name.ToString(), *FinalBlendedPosition.ToString(), PositionAlpha, *FinalBlendedRotation.ToString(), RotationAlpha);
	}
};

inline uint32 GetTypeHash(FIKRigGoal ObjectRef) { return GetTypeHash(ObjectRef.Name); }

USTRUCT()
struct IKRIG_API FIKRigGoalContainer
{
	GENERATED_BODY()

public:

	/** Set an IK goal to go to a specific location and rotation (in the specified space) blended by alpha.
	 * Will ADD the goal if none exist with the input name. */
	void SetIKGoal(const FIKRigGoal& InGoal);

	/** Set an IK goal to go to a specific location and rotation (in the specified space) blended by alpha.
	* Will ADD the goal if none exist with the input name. */
	void SetIKGoal(const UIKRigEffectorGoal* InEffectorGoal);

	/** Get an IK goal with the given name. Returns nullptr if no Goal is found in the container with the name. */
	const FIKRigGoal* FindGoalByName(const FName& GoalName) const;

	/** Clear out all Goals in container. */
	void Empty() { Goals.Empty(); };

	/** Get read-only access to array of Goals. */
	const TArray<FIKRigGoal>& GetGoalArray() const { return Goals; };
	
private:

	FIKRigGoal* FindGoalWriteable(const FName& GoalName) const;
	
	/** Array of Goals. Cannot contain duplicates (name is key). */
	TArray<FIKRigGoal> Goals;

	friend UIKRigProcessor;
};
