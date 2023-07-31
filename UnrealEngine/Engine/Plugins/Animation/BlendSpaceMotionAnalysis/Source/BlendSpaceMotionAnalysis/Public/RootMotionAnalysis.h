// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "BlendSpaceAnalysis.h"
#include "RootMotionAnalysis.generated.h"

//======================================================================================================================
UENUM()
enum class EAnalysisRootMotionAxis : uint8
{
	Speed,
	Direction,
	ForwardSpeed,
	RightwardSpeed,
	UpwardSpeed,
	ForwardSlope,
	RightwardSlope,
};

//======================================================================================================================
UCLASS()
class URootMotionAnalysisProperties : public UAnalysisProperties
{
	GENERATED_BODY()
public:
	void InitializeFromCache(TObjectPtr<UCachedAnalysisProperties> Cache) override;
	void MakeCache(TObjectPtr<UCachedAnalysisProperties>& Cache, UBlendSpace* BlendSpace) override;

	/** Axis for the analysis function */
	UPROPERTY(EditAnywhere, DisplayName = "Axis", Category = AnalysisProperties)
	EAnalysisRootMotionAxis FunctionAxis = EAnalysisRootMotionAxis::Speed;

	/** The bone or socket used for analysis */
	UPROPERTY(EditAnywhere, DisplayName = "Bone/Socket", Category = AnalysisProperties)
	FBoneSocketTarget BoneSocket;

	/**
	* The space in which to perform the analysis. Fixed will use the analysis bone/socket at the first frame
	* of the analysis time range. Changing will use the analysis bone/socket at the relevant frame during the
	* analysis, but calculate velocities assuming that frame isn't moving. Moving will do the same but velocities
	* as well as positions/rotations will be relative to this moving frame.
	*/
	UPROPERTY(EditAnywhere, Category = AnalysisProperties)
	EAnalysisSpace Space = EAnalysisSpace::World;

	/** Bone or socket that defines the analysis space (when it isn't World) */
	UPROPERTY(EditAnywhere, DisplayName = "Analysis Space Bone/Socket", Category = AnalysisProperties, meta = (EditCondition = "Space != EAnalysisSpace::World"))
	FBoneSocketTarget SpaceBoneSocket;

	/** World or bone/socket axis that specifies the character's facing direction */
	UPROPERTY(EditAnywhere, Category = AnalysisProperties)
	EAnalysisLinearAxis CharacterFacingAxis = EAnalysisLinearAxis::PlusY;

	/** World or bone/socket axis that specifies the character's up direction */
	UPROPERTY(EditAnywhere, Category = AnalysisProperties)
	EAnalysisLinearAxis CharacterUpAxis = EAnalysisLinearAxis::PlusZ;

	/** Fraction through each animation at which analysis starts */
	UPROPERTY(EditAnywhere, DisplayName = "Start time fraction", Category = AnalysisProperties, meta = (ClampMin = "0", ClampMax = "1"))
	float StartTimeFraction = 0.0f;

	/** Fraction through each animation at which analysis ends */
	UPROPERTY(EditAnywhere, DisplayName = "End time fraction", Category = AnalysisProperties, meta = (ClampMin = "0", ClampMax = "1"))
	float EndTimeFraction = 1.0f;
};

//======================================================================================================================
bool CalculateRootMotion(
	float&                               Result,
	const UBlendSpace&                   BlendSpace,
	const URootMotionAnalysisProperties* AnalysisProperties,
	const UAnimSequence&                 Animation,
	const float                          RateScale);
