// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ControlRigDefines.h"
#include "ControlRigMathLibrary.generated.h"

UENUM()
enum class EControlRigAnimEasingType : uint8
{
	Linear,
	QuadraticEaseIn,
	QuadraticEaseOut,
	QuadraticEaseInOut,
	CubicEaseIn,
	CubicEaseOut,
	CubicEaseInOut,
	QuarticEaseIn,
	QuarticEaseOut,
	QuarticEaseInOut,
	QuinticEaseIn,
	QuinticEaseOut,
	QuinticEaseInOut,
	SineEaseIn,
	SineEaseOut,
	SineEaseInOut,
	CircularEaseIn,
	CircularEaseOut,
	CircularEaseInOut,
	ExponentialEaseIn,
	ExponentialEaseOut,
	ExponentialEaseInOut,
	ElasticEaseIn,
	ElasticEaseOut,
	ElasticEaseInOut,
	BackEaseIn,
	BackEaseOut,
	BackEaseInOut,
	BounceEaseIn,
	BounceEaseOut,
	BounceEaseInOut
};

USTRUCT(BlueprintType)
struct FCRFourPointBezier
{
	GENERATED_BODY()

	FCRFourPointBezier()
	{
		A = B = C = D = FVector::ZeroVector;
	}

	UPROPERTY(EditAnywhere, Category=Bezier)
	FVector A;

	UPROPERTY(EditAnywhere, Category=Bezier)
	FVector B;

	UPROPERTY(EditAnywhere, Category=Bezier)
	FVector C;

	UPROPERTY(EditAnywhere, Category=Bezier)
	FVector D;
};

class CONTROLRIG_API FControlRigMathLibrary
{
public:
	static float AngleBetween(const FVector& A, const FVector& B);
	static void FourPointBezier(const FVector& A, const FVector& B, const FVector& C, const FVector& D, float T, FVector& OutPosition, FVector& OutTangent);
	static void FourPointBezier(const FCRFourPointBezier& Bezier, float T, FVector& OutPosition, FVector& OutTangent);
	static float EaseFloat(float Value, EControlRigAnimEasingType Type);
	static FTransform LerpTransform(const FTransform& A, const FTransform& B, float T);
	static void SolveBasicTwoBoneIK(FTransform& BoneA, FTransform& BoneB, FTransform& Effector, const FVector& PoleVector, const FVector& PrimaryAxis, const FVector& SecondaryAxis, float SecondaryAxisWeight, float BoneALength, float BoneBLength, bool bEnableStretch, float StretchStartRatio, float StretchMaxRatio);
	static FVector ClampSpatially(const FVector& Value, EAxis::Type Axis, EControlRigClampSpatialMode::Type Type, float Minimum, float Maximum, FTransform Space);
	static FQuat FindQuatBetweenVectors(const FVector& A, const FVector& B);
	static FQuat FindQuatBetweenNormals(const FVector& A, const FVector& B);

	// See - "Computing Euler angles from a rotation matrix" by Gregory G. Slabaugh
	// Each spatial orientation can be mapped to two equivalent euler angles within range (-180, 180)
	static FVector GetEquivalentEulerAngle(const FVector& InEulerAngle, const EEulerRotationOrder& InOrder);
	
	static FVector& ChooseBetterEulerAngleForAxisFilter(const FVector& Base, FVector& A, FVector& B);
};