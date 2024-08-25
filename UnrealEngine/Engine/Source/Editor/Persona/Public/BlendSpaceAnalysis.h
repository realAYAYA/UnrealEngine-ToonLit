// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/BoneSocketReference.h"
#include "Features/IModularFeature.h"
#include "Animation/BlendSpace.h"
#include "Animation/AnimSequence.h"

#include "BlendSpaceAnalysis.generated.h"

#define LOCTEXT_NAMESPACE "BlendSpaceAnalysis"
//#define ANALYSIS_VERBOSE_LOG

class UBlendSpace;
class UAnalysisProperties;

/**
* Users wishing to add their own analysis functions and structures should inherit from this, implement the virtual
* functions, and register an instance with IModularFeatures. It may help to look at the implementation of 
* FCoreBlendSpaceAnalysisFeature when doing this.
*/
class IBlendSpaceAnalysisFeature : public IModularFeature
{
public:
	static FName GetModuleFeatureName() { return "BlendSpaceAnalysis"; }

	// This should process the animation according to the analysis properties, or return false if that is not possible.
	virtual bool CalculateSampleValue(float&                     Result,
									  const UBlendSpace&         BlendSpace,
									  const UAnalysisProperties* AnalysisProperties,
									  const UAnimSequence&       Animation,
									  const float                RateScale) const = 0;

	// This should return an instance derived from UAnalysisProperties that is suitable for the Function. The caller
	// will pass in a suitable owning object, outer, that the implementation should assign as owner of the newly created
	// object. 
	virtual UAnalysisProperties* MakeAnalysisProperties(UObject* Outer, const FString& FunctionName) const = 0;

	// This should return the names of the functions handled
	virtual TArray<FString> GetAnalysisFunctions() const = 0;
};

UENUM()
enum class EAnalysisSpace : uint8
{
	World    UMETA(ToolTip = "Analysis is done in world space (relative to the root of the character)"),
	Fixed    UMETA(ToolTip = "Analysis is done in the space of the specified bone or socket based on the first frame of the animation used"),
	Changing UMETA(ToolTip = "Analysis is done in the space of the specified bone or socket based, but velocities are calculated as if this space is not moving"),
	Moving   UMETA(ToolTip = "Analysis is done in the space of the specified bone or socket"),
};

UENUM()
enum class EAnalysisLinearAxis : uint8
{
	PlusX UMETA(DisplayName = "+X", ToolTip = "The axis points in the positive X direction"),
	PlusY UMETA(DisplayName = "+Y", ToolTip = "The axis points in the positive Y direction"),
	PlusZ UMETA(DisplayName = "+Z", ToolTip = "The axis points in the positive Z direction"),
	MinusX UMETA(DisplayName = "-X", ToolTip = "The axis points in the negative X direction"),
	MinusY UMETA(DisplayName = "-Y", ToolTip = "The axis points in the negative Y direction"),
	MinusZ UMETA(DisplayName = "-Z", ToolTip = "The axis points in the negative Z direction"),
};

UENUM()
enum class EEulerCalculationMethod : uint8
{
	AimDirection    UMETA(ToolTip = "Calculates the yaw by looking at the BoneRightAxis. This can provide better yaw values, especially when aiming (e.g. with a weapon that has minimal rotation around its pointing axis) and covering extreme angles up and down, but only if this rightwards facing axis is reliable. It won't work well if the bone is also rolling around its axis."),
	PointDirection  UMETA(ToolTip = "Calculates the yaw based only on the BoneFacingAxis. This will work when you're most interested in the yaw and pitch from a pointing direction, but can produce undesirable results when pointing almost directly up or down."),
};

UENUM()
enum class EAnalysisEulerAxis : uint8
{
	Roll,
	Pitch,
	Yaw,
};


/**
 * This will be used to preserve values as far as possible when switching between analysis functions, so it contains all
 * the parameters used by the engine functions. User defined can inherit from this and add their own - then the
 * user-defined MakeCache function should replace any base class cache that is passed in with their own.
*/
UCLASS()
class PERSONA_API UCachedAnalysisProperties : public UObject
{
	GENERATED_BODY()
public:
	void CopyFrom(const UCachedAnalysisProperties& Other);
	EAnalysisLinearAxis     LinearFunctionAxis = EAnalysisLinearAxis::PlusX;
	EAnalysisEulerAxis      EulerFunctionAxis = EAnalysisEulerAxis::Pitch;
	FBoneSocketTarget       BoneSocket1;
	FBoneSocketTarget       BoneSocket2;
	EAnalysisLinearAxis     BoneFacingAxis = EAnalysisLinearAxis::PlusX;
	EAnalysisLinearAxis     BoneRightAxis = EAnalysisLinearAxis::PlusY;
	EAnalysisSpace          Space = EAnalysisSpace::World;
	FBoneSocketTarget       SpaceBoneSocket;
	EAnalysisLinearAxis     CharacterFacingAxis = EAnalysisLinearAxis::PlusY;
	EAnalysisLinearAxis     CharacterUpAxis = EAnalysisLinearAxis::PlusZ;
	float                   StartTimeFraction = 0.0f;
	float                   EndTimeFraction = 1.0f;
};

UCLASS()
class PERSONA_API ULinearAnalysisProperties : public UAnalysisProperties
{
	GENERATED_BODY()
public:
	void InitializeFromCache(TObjectPtr<UCachedAnalysisProperties> Cache) override;
	void MakeCache(TObjectPtr<UCachedAnalysisProperties>& Cache, UBlendSpace* BlendSpace) override;

	/** Axis for the analysis function */
	UPROPERTY(EditAnywhere, DisplayName = "Axis", Category = AnalysisProperties)
	EAnalysisLinearAxis FunctionAxis = EAnalysisLinearAxis::PlusX;

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

	/** Fraction through each animation at which analysis starts */
	UPROPERTY(EditAnywhere, DisplayName = "Start time fraction", Category = AnalysisProperties, meta = (ClampMin = "0", ClampMax = "1"))
	float StartTimeFraction = 0.0f;

	/** Fraction through each animation at which analysis ends */
	UPROPERTY(EditAnywhere, DisplayName = "End time fraction", Category = AnalysisProperties, meta = (ClampMin = "0", ClampMax = "1"))
	float EndTimeFraction = 1.0f;
};

UCLASS()
class PERSONA_API UEulerAnalysisProperties : public UAnalysisProperties
{
	GENERATED_BODY()
public:
	void InitializeFromCache(TObjectPtr<UCachedAnalysisProperties> Cache) override;
	void MakeCache(TObjectPtr<UCachedAnalysisProperties>& Cache, UBlendSpace* BlendSpace) override;

	/** Axis for the analysis function */
	UPROPERTY(EditAnywhere, DisplayName = "Axis", Category = AnalysisProperties)
	EAnalysisEulerAxis FunctionAxis = EAnalysisEulerAxis::Pitch;

	/** The bone or socket used for analysis */
	UPROPERTY(EditAnywhere, DisplayName = "Bone/Socket", Category = AnalysisProperties)
	FBoneSocketTarget BoneSocket;

	/** Used for some analysis functions - specifies the bone/socket axis that points in the facing/forwards direction */
	UPROPERTY(EditAnywhere, Category = AnalysisProperties)
	EAnalysisLinearAxis BoneFacingAxis = EAnalysisLinearAxis::PlusX;

	/** Used for some analysis functions - specifies the bone/socket axis that points to the "right" */
	UPROPERTY(EditAnywhere, Category = AnalysisProperties)
	EAnalysisLinearAxis BoneRightAxis = EAnalysisLinearAxis::PlusY;

	/** Used for some analysis functions - specifies how yaw should be calculated from the bone axes */
	UPROPERTY(EditAnywhere, Category = AnalysisProperties)
	EEulerCalculationMethod EulerCalculationMethod = EEulerCalculationMethod::AimDirection;

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
// The following are helper functions which may be useful when implementing analysis functions
//======================================================================================================================

namespace BlendSpaceAnalysis
{

//======================================================================================================================
// Retrieves the bone index and transform offset given the BoneSocketTarget. Returns true if found
PERSONA_API bool GetBoneInfo(const UAnimSequence&     Animation, 
							 const FBoneSocketTarget& BoneSocket, 
							 FTransform&              BoneOffset, 
							 FName&                   BoneName);

//======================================================================================================================
PERSONA_API FTransform GetBoneTransform(const UAnimSequence& Animation, int32 Key, const FName& BoneName);

//======================================================================================================================
template<typename T>
void CalculateFrameTM(
	bool& bNeedToUpdateFrameTM, FTransform& FrameTM, 
	const int32 SampleKey, const T& AnalysisProperties, const UAnimSequence& Animation)
{
	if (bNeedToUpdateFrameTM)
	{
		FrameTM.SetIdentity();
		if (AnalysisProperties->Space != EAnalysisSpace::World)
		{
			FTransform SpaceBoneOffset;
			FName SpaceBoneName;
			if (GetBoneInfo(Animation, AnalysisProperties->SpaceBoneSocket, SpaceBoneOffset, SpaceBoneName))
			{
				FTransform SpaceBoneTM = GetBoneTransform(Animation, SampleKey, SpaceBoneName);
				FrameTM = SpaceBoneOffset * SpaceBoneTM;
			}
		}

		bNeedToUpdateFrameTM = (
			AnalysisProperties->Space == EAnalysisSpace::Changing || 
			AnalysisProperties->Space == EAnalysisSpace::Moving);
	}
}

//======================================================================================================================
PERSONA_API FVector GetAxisFromTM(const FTransform& TM, EAnalysisLinearAxis Axis);

//======================================================================================================================
template<typename T>
void GetFrameDirs(
	FVector& FrameFacingDir, FVector& FrameUpDir, FVector& FrameRightDir, 
	const FTransform& FrameTM, const T& AnalysisProperties)
{
	FrameFacingDir = GetAxisFromTM(FrameTM, AnalysisProperties->CharacterFacingAxis);
	FrameUpDir = GetAxisFromTM(FrameTM, AnalysisProperties->CharacterUpAxis);
	FrameRightDir = FVector::CrossProduct(FrameUpDir, FrameFacingDir);
}

//======================================================================================================================
/**
* Helper to extract the component from the FVector functions
*/
template<typename FunctionType, typename T>
static bool CalculateComponentSampleValue(
	double&                    Result,
	const FunctionType&        Fn,
	const UBlendSpace&         BlendSpace, 
	const T*                   AnalysisProperties, 
	const UAnimSequence&       Animation,
	const float                RateScale)
{
	FVector Value;
	int32 ComponentIndex = (int32) AnalysisProperties->FunctionAxis;
	if (Fn(Value, BlendSpace, AnalysisProperties, Animation, RateScale))
	{
		Result = Value[ComponentIndex];
		return true;
	}
	return false;
}

//======================================================================================================================
/**
* Helper to extract the component from the FVector functions
*/
template<typename FunctionType, typename T>
static bool CalculateComponentSampleValue(
	float&               Result,
	const FunctionType&  Fn,
	const UBlendSpace&   BlendSpace,
	const T*             AnalysisProperties,
	const UAnimSequence& Animation,
	const float          RateScale)
{
	double DoubleResult = Result;
	bool bResult = CalculateComponentSampleValue(DoubleResult, Fn, BlendSpace, AnalysisProperties, Animation, RateScale);
	Result = float(DoubleResult);
	return bResult;
}

//======================================================================================================================
template <typename T>
static bool CalculatePosition(
	FVector&                         Result,
	const UBlendSpace&               BlendSpace,
	const T*                         AnalysisProperties,
	const UAnimSequence&             Animation,
	const float                      RateScale)
{
	const int32 NumSampledKeys = Animation.GetNumberOfSampledKeys() - 1;
	if (!AnalysisProperties || NumSampledKeys <= 0)
	{
		return false;
	}

	FTransform BoneOffset;
	FName BoneName;
	if (!GetBoneInfo(Animation, AnalysisProperties->BoneSocket, BoneOffset, BoneName))
	{
		return false;
	}

	int32 FirstKey = FMath::Clamp(
		(int32) (float(NumSampledKeys) * AnalysisProperties->StartTimeFraction), 0, NumSampledKeys);
	int32 LastKey = FMath::Clamp(
		(int32) (float(NumSampledKeys) * AnalysisProperties->EndTimeFraction), FirstKey, NumSampledKeys);

	FTransform FrameTM;
	bool bNeedToUpdateFrameTM = true;
	CalculateFrameTM(bNeedToUpdateFrameTM, FrameTM, FirstKey, AnalysisProperties, Animation);

	Result.Set(0, 0, 0);
	for (int32 Key = FirstKey; Key != LastKey + 1; ++Key)
	{
		CalculateFrameTM(bNeedToUpdateFrameTM, FrameTM, Key, AnalysisProperties, Animation);
		FTransform BoneTM = GetBoneTransform(Animation, Key, BoneName);
		FTransform TM = BoneOffset * BoneTM;
		FVector RelativePos = FrameTM.InverseTransformPosition(TM.GetTranslation());
		Result += RelativePos;
	}
	Result /= (1 + LastKey - FirstKey);
	return true;
}

//======================================================================================================================
template <typename T>
static bool CalculateDeltaPosition(
	FVector&                         Result,
	const UBlendSpace&               BlendSpace,
	const T*                         AnalysisProperties,
	const UAnimSequence&             Animation,
	const float                      RateScale)
{
	const int32 NumSampledKeys = Animation.GetNumberOfSampledKeys() - 1;
	if (!AnalysisProperties || NumSampledKeys <= 0)
	{
		return false;
	}

	FTransform BoneOffset;
	FName BoneName;
	if (!GetBoneInfo(Animation, AnalysisProperties->BoneSocket, BoneOffset, BoneName))
	{
		return false;
	}

	int32 FirstKey = FMath::Clamp(
		(int32) (float(NumSampledKeys) * AnalysisProperties->StartTimeFraction), 0, NumSampledKeys);
	int32 LastKey = FMath::Clamp(
		(int32) (float(NumSampledKeys) * AnalysisProperties->EndTimeFraction), FirstKey, NumSampledKeys);

	FTransform FrameTM;
	bool bNeedToUpdateFrameTM = true;
	CalculateFrameTM(bNeedToUpdateFrameTM, FrameTM, FirstKey, AnalysisProperties, Animation);

	FTransform BoneTM1 = GetBoneTransform(Animation, FirstKey, BoneName);
	FTransform TM1 = BoneOffset * BoneTM1;
	FVector RelativePos1 = FrameTM.InverseTransformPosition(TM1.GetTranslation());

	CalculateFrameTM(bNeedToUpdateFrameTM, FrameTM, FirstKey, AnalysisProperties, Animation);
	FTransform BoneTM2 = GetBoneTransform(Animation, LastKey, BoneName);
	FTransform TM2 = BoneOffset * BoneTM2;
	FVector RelativePos2 = FrameTM.InverseTransformPosition(TM2.GetTranslation());

	Result = RelativePos2 - RelativePos1;
	return true;
}

//======================================================================================================================
template <typename T>
static bool CalculateVelocity(
	FVector&             Result,
	const UBlendSpace&   BlendSpace,
	const T*             AnalysisProperties,
	const UAnimSequence& Animation,
	const float          RateScale)
{
	const int32 NumSampledKeys = Animation.GetNumberOfSampledKeys() - 1;
	if (!AnalysisProperties || NumSampledKeys <= 0)
	{
		return false;
	}

	if (NumSampledKeys == 1)
	{
		Result.Set(0, 0, 0);
		return true;
	}

	FTransform BoneOffset;
	FName BoneName;
	if (!GetBoneInfo(Animation, AnalysisProperties->BoneSocket, BoneOffset, BoneName))
	{
		return false;
	}

	double DeltaTime = Animation.GetPlayLength() / double(NumSampledKeys);

	int32 FirstKey = FMath::Clamp(
		(int32) (float(NumSampledKeys) * AnalysisProperties->StartTimeFraction), 0, NumSampledKeys);
	int32 LastKey = FMath::Clamp(
		(int32) (float(NumSampledKeys) * AnalysisProperties->EndTimeFraction), FirstKey, NumSampledKeys-1);

	// First and Last key are for averaging. However, the finite differencing always goes from one frame to the next
	int32 NumKeys = FMath::Max(1 + LastKey - FirstKey, 1);

	FTransform FrameTM;
	bool bNeedToUpdateFrameTM = true;
	CalculateFrameTM(bNeedToUpdateFrameTM, FrameTM, FirstKey, AnalysisProperties, Animation);

	Result.Set(0, 0, 0);
	for (int32 iKey = 0; iKey != NumKeys; ++iKey)
	{
		int32 Key = (FirstKey + iKey) % (NumSampledKeys + 1);
		CalculateFrameTM(bNeedToUpdateFrameTM, FrameTM, Key, AnalysisProperties, Animation);

		FTransform BoneTM1 = GetBoneTransform(Animation, Key, BoneName);
		FTransform TM1 = BoneOffset * BoneTM1;
		FVector RelativePos1 = FrameTM.InverseTransformPosition(TM1.GetTranslation());

		int32 NextKey = (Key + 1) % (NumSampledKeys + 1);
		if (AnalysisProperties->Space == EAnalysisSpace::Moving)
		{
			CalculateFrameTM(bNeedToUpdateFrameTM, FrameTM, NextKey, AnalysisProperties, Animation);
		}

		FTransform BoneTM2 = GetBoneTransform(Animation, NextKey, BoneName);
		FTransform TM2 = BoneOffset * BoneTM2;
		FVector RelativePos2 = FrameTM.InverseTransformPosition(TM2.GetTranslation());
		FVector Velocity = (RelativePos2 - RelativePos1) / DeltaTime;

#ifdef ANALYSIS_VERBOSE_LOG
		UE_LOG(LogAnimation, Log, TEXT("%d Velocity = %f %f %f Height = %f"), 
			   Key, Velocity.X, Velocity.Y, Velocity.Z, 0.5f * (RelativePos1 + RelativePos2).Z);
#endif
		Result += Velocity;
	}
	Result /= (1 + LastKey - FirstKey);
	Result *= Animation.RateScale * RateScale;
	UE_LOG(LogAnimation, Log, TEXT("%s vel = %f %f %f"), *BoneName.ToString(), Result.X, Result.Y, Result.Z);
	return true;
}

//======================================================================================================================
template <typename T>
void CalculateBoneOrientation(
	FVector&                   RollPitchYaw,
	const UAnimSequence&       Animation, 
	const int32                Key, 
	const FName                BoneName, 
	const FTransform&          BoneOffset, 
	const T&                   AnalysisProperties, 
	const FVector&             FrameFacingDir, 
	const FVector&             FrameRightDir, 
	const FVector&             FrameUpDir)
{
	const FTransform BoneTM = GetBoneTransform(Animation, Key, BoneName);

	const FTransform TM = BoneOffset * BoneTM;
	const FVector AimForwardDir = BlendSpaceAnalysis::GetAxisFromTM(TM, AnalysisProperties->BoneFacingAxis);
	const FVector AimRightDir = BlendSpaceAnalysis::GetAxisFromTM(TM, AnalysisProperties->BoneRightAxis);

	double Yaw;
	if (AnalysisProperties->EulerCalculationMethod == EEulerCalculationMethod::AimDirection)
	{
		// Yaw is taken from the AimRightDir to avoid problems when the gun is pointing up or down - especially if it 
		// goes beyond 90 degrees in pitch. However, if there is roll around the gun axis, then this can produce
		// incorrect/undesirable results.
		Yaw = FMath::RadiansToDegrees(FMath::Atan2(
			FVector::DotProduct(AimRightDir, -FrameFacingDir), FVector::DotProduct(AimRightDir, FrameRightDir)));
	}
	else
	{
		// This takes yaw directly from the forwards direction. Note that if the pose is really one with small yaw 
		// and pitch more than 90 degrees, then this will calculate a yaw that is nearer to 180 degrees.
		Yaw = FMath::RadiansToDegrees(FMath::Atan2(
			FVector::DotProduct(AimForwardDir, FrameRightDir), FVector::DotProduct(AimForwardDir, FrameFacingDir)));
	}

	// Undo the yaw to get pitch
	const FQuat YawQuat(FrameUpDir, FMath::DegreesToRadians(Yaw));
	const FVector UnYawedAimForwardDir = YawQuat.UnrotateVector(AimForwardDir);
	const double Up = UnYawedAimForwardDir | FrameUpDir;
	const double Forward = UnYawedAimForwardDir | FrameFacingDir;
	const double Pitch = FMath::RadiansToDegrees(FMath::Atan2(Up, Forward));

	// Undo the pitch to get roll
	const FVector UnYawedAimRightDir = YawQuat.UnrotateVector(AimRightDir);
	const FQuat PitchQuat(FrameRightDir, -FMath::DegreesToRadians(Pitch));

	const FVector UnYawedUnPitchedAimRightDir = PitchQuat.UnrotateVector(UnYawedAimRightDir);

	const double Roll = FMath::RadiansToDegrees(FMath::Atan2(
		FVector::DotProduct(UnYawedUnPitchedAimRightDir, -FrameUpDir), 
		FVector::DotProduct(UnYawedUnPitchedAimRightDir, FrameRightDir)));

	RollPitchYaw.Set(Roll, Pitch, Yaw);
}

//======================================================================================================================
// Note that if a looping animation has 56 keys, then its first key is 0 and last is 55, but these will be identical poses.
// Thus it has one fewer intervals/unique keys
template <typename T>
static bool CalculateOrientation(
	FVector&                        Result,
	const UBlendSpace&              BlendSpace,
	const T*                        AnalysisProperties,
	const UAnimSequence&            Animation,
	const float                     RateScale)
{
	const int32 NumSampledKeys = Animation.GetNumberOfSampledKeys() - 1;
	if (!AnalysisProperties || NumSampledKeys <= 0)
	{
		return false;
	}

	FTransform BoneOffset;
	FName BoneName;
	if (!GetBoneInfo(Animation, AnalysisProperties->BoneSocket, BoneOffset, BoneName))
	{
		return false;
	}

	int32 FirstKey = FMath::Clamp(
		(int32) (float(NumSampledKeys) * AnalysisProperties->StartTimeFraction), 0, NumSampledKeys);
	int32 LastKey = FMath::Clamp(
		(int32) (float(NumSampledKeys) * AnalysisProperties->EndTimeFraction), FirstKey, NumSampledKeys);

	FTransform FrameTM;
	bool bNeedToUpdateFrameTM = true;
	FVector FrameFacingDir, FrameUpDir, FrameRightDir;
	CalculateFrameTM(bNeedToUpdateFrameTM, FrameTM, FirstKey, AnalysisProperties, Animation);
	GetFrameDirs(FrameFacingDir, FrameUpDir, FrameRightDir, FrameTM, AnalysisProperties);

	Result.Set(0, 0, 0);
	for (int32 Key = FirstKey; Key != LastKey + 1; ++Key)
	{
		CalculateFrameTM(bNeedToUpdateFrameTM, FrameTM, Key, AnalysisProperties, Animation);
		GetFrameDirs(FrameFacingDir, FrameUpDir, FrameRightDir, FrameTM, AnalysisProperties);

		FVector RollPitchYaw;
		CalculateBoneOrientation(
			RollPitchYaw, Animation, Key, BoneName, BoneOffset, 
			AnalysisProperties, FrameFacingDir, FrameRightDir, FrameUpDir);
#ifdef ANALYSIS_VERBOSE_LOG
		UE_LOG(LogAnimation, Log, TEXT("Roll/pitch/yaw = %f %f %f"), RollPitchYaw.X, RollPitchYaw.Y, RollPitchYaw.Z);
#endif
		Result += RollPitchYaw;
	}
	Result /= (1 + LastKey - FirstKey);
	UE_LOG(LogAnimation, Log, TEXT("%s Orientation = %f %f %f"), *BoneName.ToString(), Result.X, Result.Y, Result.Z);
	return true;
}

//======================================================================================================================
// Note that if a looping animation has 56 keys, then its first key is 0 and last is 55, but these will be identical poses.
// Thus it has one fewer intervals/unique keys
template <typename T>
static bool CalculateDeltaOrientation(
	FVector&                        Result,
	const UBlendSpace&              BlendSpace,
	const T*                        AnalysisProperties,
	const UAnimSequence&            Animation,
	const float                     RateScale)
{
	const int32 NumSampledKeys = Animation.GetNumberOfSampledKeys() - 1;
	if (!AnalysisProperties || NumSampledKeys <= 0)
	{
		return false;
	}

	FTransform BoneOffset;
	FName BoneName;
	if (!GetBoneInfo(Animation, AnalysisProperties->BoneSocket, BoneOffset, BoneName))
	{
		return false;
	}

	int32 FirstKey = FMath::Clamp(
		(int32) (float(NumSampledKeys) * AnalysisProperties->StartTimeFraction), 0, NumSampledKeys);
	int32 LastKey = FMath::Clamp(
		(int32) (float(NumSampledKeys) * AnalysisProperties->EndTimeFraction), FirstKey, NumSampledKeys);

	FTransform FrameTM;
	bool bNeedToUpdateFrameTM = true;
	FVector FrameFacingDir, FrameUpDir, FrameRightDir;
	CalculateFrameTM(bNeedToUpdateFrameTM, FrameTM, FirstKey, AnalysisProperties, Animation);
	GetFrameDirs(FrameFacingDir, FrameUpDir, FrameRightDir, FrameTM, AnalysisProperties);

	FVector RollPitchYaw1;
	CalculateBoneOrientation(
		RollPitchYaw1, Animation, FirstKey, BoneName, BoneOffset,
		AnalysisProperties, FrameFacingDir, FrameRightDir, FrameUpDir);

	CalculateFrameTM(bNeedToUpdateFrameTM, FrameTM, LastKey, AnalysisProperties, Animation);
	GetFrameDirs(FrameFacingDir, FrameUpDir, FrameRightDir, FrameTM, AnalysisProperties);

	FVector RollPitchYaw2;
	CalculateBoneOrientation(
		RollPitchYaw2, Animation, LastKey, BoneName, BoneOffset,
		AnalysisProperties, FrameFacingDir, FrameRightDir, FrameUpDir);

	Result = RollPitchYaw2 - RollPitchYaw1;
	return true;
}

//======================================================================================================================
template <typename T>
static bool CalculateAngularVelocity(
	FVector&                         Result,
	const UBlendSpace&               BlendSpace,
	const T*                         AnalysisProperties,
	const UAnimSequence&             Animation,
	const float                      RateScale)
{
	const int32 NumSampledKeys = Animation.GetNumberOfSampledKeys() - 1;
	if (!AnalysisProperties || NumSampledKeys <= 0)
	{
		return false;
	}

	if (NumSampledKeys == 1)
	{
		Result.Set(0, 0, 0);
		return true;
	}

	FTransform BoneOffset;
	FName BoneName;
	if (!GetBoneInfo(Animation, AnalysisProperties->BoneSocket, BoneOffset, BoneName))
	{
		return false;
	}

	double DeltaTime = Animation.GetPlayLength() / double(NumSampledKeys);

	int32 FirstKey = FMath::Clamp(
		(int32) (float(NumSampledKeys) * AnalysisProperties->StartTimeFraction), 0, NumSampledKeys);
	int32 LastKey = FMath::Clamp(
		(int32) (float(NumSampledKeys) * AnalysisProperties->EndTimeFraction), FirstKey, NumSampledKeys-1);

	// First and Last key are for averaging. However, the finite differencing always goes from one frame to the next
	int32 NumKeys = FMath::Max(1 + LastKey - FirstKey, 1);

	FTransform FrameTM;
	bool bNeedToUpdateFrameTM = true;
	CalculateFrameTM(bNeedToUpdateFrameTM, FrameTM, FirstKey, AnalysisProperties, Animation);

	Result.Set(0, 0, 0);
	for (int32 iKey = 0; iKey != NumKeys; ++iKey)
	{
		int32 Key = (FirstKey + iKey) % (NumSampledKeys + 1);
		CalculateFrameTM(bNeedToUpdateFrameTM, FrameTM, Key, AnalysisProperties, Animation);

		FTransform BoneTM1 = GetBoneTransform(Animation, Key, BoneName);
		FTransform TM1 = BoneOffset * BoneTM1;
		FQuat RelativeQuat1 = FrameTM.InverseTransformRotation(TM1.GetRotation());

		int32 NextKey = (Key + 1) % (NumSampledKeys + 1);
		if (AnalysisProperties->Space == EAnalysisSpace::Moving)
		{
			CalculateFrameTM(bNeedToUpdateFrameTM, FrameTM, NextKey, AnalysisProperties, Animation);
		}

		FTransform BoneTM2 = GetBoneTransform(Animation, NextKey, BoneName);
		FTransform TM2 = BoneOffset * BoneTM2;
		FQuat RelativeQuat2 = FrameTM.InverseTransformRotation(TM2.GetRotation());

		FQuat Rotation = RelativeQuat2 * RelativeQuat1.Inverse();
		FVector Axis;
		double Angle;
		Rotation.ToAxisAndAngle(Axis, Angle);
		FVector AngularVelocity = FMath::RadiansToDegrees(Axis * (Angle / DeltaTime));
#ifdef ANALYSIS_VERBOSE_LOG
		UE_LOG(LogAnimation, Log, TEXT("Angular Velocity = %f %f %f"), AngularVelocity.X, AngularVelocity.Y, AngularVelocity.Z);
#endif
		Result += AngularVelocity;
	}
	Result /= (1 + LastKey - FirstKey);
	Result *= Animation.RateScale * RateScale;
	UE_LOG(LogAnimation, Log, TEXT("%s angular velocity = %f %f %f"), *BoneName.ToString(), Result.X, Result.Y, Result.Z);
	return true;
}

//======================================================================================================================
template <typename T>
static bool CalculateOrientationRate(
	FVector&                        Result,
	const UBlendSpace&              BlendSpace,
	const T*                        AnalysisProperties,
	const UAnimSequence&            Animation,
	const float                     RateScale)
{
	const int32 NumSampledKeys = Animation.GetNumberOfSampledKeys() - 1;
	if (!AnalysisProperties || NumSampledKeys <= 0)
	{
		return false;
	}

	if (NumSampledKeys == 1)
	{
		Result.Set(0, 0, 0);
		return true;
	}

	FTransform BoneOffset;
	FName BoneName;
	if (!GetBoneInfo(Animation, AnalysisProperties->BoneSocket, BoneOffset, BoneName))
	{
		return false;
	}

	double DeltaTime = Animation.GetPlayLength() / double(NumSampledKeys);

	int32 FirstKey = FMath::Clamp(
		(int32) (float(NumSampledKeys) * AnalysisProperties->StartTimeFraction), 0, NumSampledKeys);
	int32 LastKey = FMath::Clamp(
		(int32) (float(NumSampledKeys) * AnalysisProperties->EndTimeFraction), FirstKey, NumSampledKeys-1);

	// First and Last key are for averaging. However, the finite differencing always goes from one frame to the next
	int32 NumKeys = FMath::Max(1 + LastKey - FirstKey, 1);

	FTransform FrameTM;
	bool bNeedToUpdateFrameTM = true;
	FVector FrameFacingDir, FrameUpDir, FrameRightDir;
	CalculateFrameTM(bNeedToUpdateFrameTM, FrameTM, FirstKey, AnalysisProperties, Animation);
	GetFrameDirs(FrameFacingDir, FrameUpDir, FrameRightDir, FrameTM, AnalysisProperties);

	Result.Set(0, 0, 0);
	for (int32 iKey = 0; iKey != NumKeys; ++iKey)
	{
		int32 Key = (FirstKey + iKey) % (NumSampledKeys + 1);
		CalculateFrameTM(bNeedToUpdateFrameTM, FrameTM, Key, AnalysisProperties, Animation);
		GetFrameDirs(FrameFacingDir, FrameUpDir, FrameRightDir, FrameTM, AnalysisProperties);

		FVector RollPitchYaw1;
		CalculateBoneOrientation(
			RollPitchYaw1, Animation, Key, BoneName, BoneOffset, 
			AnalysisProperties, FrameFacingDir, FrameRightDir, FrameUpDir);

		int32 NextKey = (Key + 1) % (NumSampledKeys + 1);
		if (AnalysisProperties->Space == EAnalysisSpace::Moving)
		{
			CalculateFrameTM(bNeedToUpdateFrameTM, FrameTM, NextKey, AnalysisProperties, Animation);
			GetFrameDirs(FrameFacingDir, FrameUpDir, FrameRightDir, FrameTM, AnalysisProperties);
		}

		FVector RollPitchYaw2;
		CalculateBoneOrientation(
			RollPitchYaw2, Animation, NextKey, BoneName, BoneOffset, 
			AnalysisProperties, FrameFacingDir, FrameRightDir, FrameUpDir);

		const FVector OrientationRate = (RollPitchYaw2 - RollPitchYaw1) / DeltaTime;
#ifdef ANALYSIS_VERBOSE_LOG
		UE_LOG(LogAnimation, Log, TEXT("Orientation rate = %f %f %f"), OrientationRate.X, OrientationRate.Y, OrientationRate.Z);
#endif
		Result += OrientationRate;
	}
	Result /= (1 + LastKey - FirstKey);
	Result *= Animation.RateScale * RateScale;
	UE_LOG(LogAnimation, Log, TEXT("%s Orientation rate = %f %f %f"), *BoneName.ToString(), Result.X, Result.Y, Result.Z);
	return true;
}


}

#undef LOCTEXT_NAMESPACE

