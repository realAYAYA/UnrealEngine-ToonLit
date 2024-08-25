// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMFunction_MathBase.h"
#include "EulerTransform.h"
#include "RigVMFunction_MathQuaternion.generated.h"

USTRUCT(meta=(Abstract, Category="Math|Quaternion", MenuDescSuffix="(Quaternion)"))
struct RIGVM_API FRigVMFunction_MathQuaternionBase : public FRigVMFunction_MathBase
{
	GENERATED_BODY()
};

USTRUCT(meta=(Abstract))
struct RIGVM_API FRigVMFunction_MathQuaternionUnaryOp : public FRigVMFunction_MathQuaternionBase
{
	GENERATED_BODY()

	FRigVMFunction_MathQuaternionUnaryOp()
	{
		Value = Result = FQuat::Identity;
	}

	UPROPERTY(meta=(Input))
	FQuat Value;

	UPROPERTY(meta=(Output))
	FQuat Result;
};

USTRUCT(meta=(Abstract))
struct RIGVM_API FRigVMFunction_MathQuaternionBinaryOp : public FRigVMFunction_MathQuaternionBase
{
	GENERATED_BODY()

	FRigVMFunction_MathQuaternionBinaryOp()
	{
		A = B = Result = FQuat::Identity;
	}

	UPROPERTY(meta=(Input))
	FQuat A;

	UPROPERTY(meta=(Input))
	FQuat B;

	UPROPERTY(meta=(Output))
	FQuat Result;
};

USTRUCT(meta=(Abstract))
struct RIGVM_API FRigVMFunction_MathQuaternionBinaryAggregateOp : public FRigVMFunction_MathQuaternionBase
{
	GENERATED_BODY()

	FRigVMFunction_MathQuaternionBinaryAggregateOp()
	{
		A = B = Result = FQuat::Identity;
	}

	UPROPERTY(meta=(Input, Aggregate))
	FQuat A;

	UPROPERTY(meta=(Input, Aggregate))
	FQuat B;

	UPROPERTY(meta=(Output, Aggregate))
	FQuat Result;
};

/**
 * Makes a quaternion from its components
 */
USTRUCT(meta=(DisplayName="Make Quat", Keywords="Make,Construct,Constant"))
struct RIGVM_API FRigVMFunction_MathQuaternionMake : public FRigVMFunction_MathQuaternionBase
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute() override;

	FRigVMFunction_MathQuaternionMake()
	{
		X = Y = Z = W = 0.f;
		Result = FQuat::Identity;
	}

	UPROPERTY(meta=(Input))
	float X;

	UPROPERTY(meta=(Input))
	float Y;

	UPROPERTY(meta=(Input))
	float Z;

	UPROPERTY(meta=(Input))
	float W;

	UPROPERTY(meta=(Output))
	FQuat Result;
};

/**
 * Makes a quaternion from an axis and an angle in radians
 */
USTRUCT(meta=(DisplayName="From Axis And Angle", TemplateName="FromAxisAndAngle", Keywords="Make,Construct"))
struct RIGVM_API FRigVMFunction_MathQuaternionFromAxisAndAngle : public FRigVMFunction_MathQuaternionBase
{
	GENERATED_BODY()

	FRigVMFunction_MathQuaternionFromAxisAndAngle()
	{
		Axis = FVector(1.f, 0.f, 0.f);
		Angle = 0.f;
		Result = FQuat::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	FVector Axis;

	UPROPERTY(meta=(Input))
	float Angle;

	UPROPERTY(meta=(Output))
	FQuat Result;
};

/**
 * Makes a quaternion from euler values in degrees
 */
USTRUCT(meta=(DisplayName="From Euler", TemplateName="FromEuler", Keywords="Make,Construct"))
struct RIGVM_API FRigVMFunction_MathQuaternionFromEuler : public FRigVMFunction_MathQuaternionBase
{
	GENERATED_BODY()

	FRigVMFunction_MathQuaternionFromEuler()
	{
		Euler = FVector::ZeroVector;
		RotationOrder = EEulerRotationOrder::ZYX;
		Result = FQuat::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	FVector Euler;

	UPROPERTY(meta = (Input))
	EEulerRotationOrder RotationOrder;

	UPROPERTY(meta=(Output))
	FQuat Result;
};

/**
 * Makes a quaternion from a rotator
 */
USTRUCT(meta=(DisplayName="From Rotator", Keywords="Make,Construct", Deprecated="5.0.1"))
struct RIGVM_API FRigVMFunction_MathQuaternionFromRotator : public FRigVMFunction_MathQuaternionBase
{
	GENERATED_BODY()
	
	FRigVMFunction_MathQuaternionFromRotator()
	{
		Rotator = FRotator::ZeroRotator;
		Result = FQuat::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	FRotator Rotator;

	UPROPERTY(meta=(Output))
	FQuat Result;
	
	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
 * Makes a quaternion from a rotator
 */
USTRUCT(meta=(DisplayName="From Rotator", TemplateName="Cast", ExecuteContext="FRigVMExecuteContext", Keywords="Make,Construct"))
struct RIGVM_API FRigVMFunction_MathQuaternionFromRotatorV2 : public FRigVMFunction_MathQuaternionBase
{
	GENERATED_BODY()
	
	FRigVMFunction_MathQuaternionFromRotatorV2()
	{
		Value = FRotator::ZeroRotator;
		Result = FQuat::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	FRotator Value;

	UPROPERTY(meta=(Output))
	FQuat Result;
};

/**
 * Makes a quaternion from two vectors, representing the shortest rotation between the two vectors.
 */
USTRUCT(meta=(DisplayName="From Two Vectors", TemplateName="FromTwoVectors", Keywords="Make,Construct"))
struct RIGVM_API FRigVMFunction_MathQuaternionFromTwoVectors : public FRigVMFunction_MathQuaternionBase
{
	GENERATED_BODY()
	
	FRigVMFunction_MathQuaternionFromTwoVectors()
	{
		A = B = FVector(1.f, 0.f, 0.f);
		Result = FQuat::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	FVector A;

	UPROPERTY(meta=(Input))
	FVector B;

	UPROPERTY(meta=(Output))
	FQuat Result;
};

/**
 * Retrieves the axis and angle of a quaternion in radians
 */
USTRUCT(meta=(DisplayName="To Axis And Angle", TemplateName="ToAxisAndAngle", Keywords="Make,Construct,GetAxis,GetAngle"))
struct RIGVM_API FRigVMFunction_MathQuaternionToAxisAndAngle : public FRigVMFunction_MathQuaternionBase
{
	GENERATED_BODY()

	FRigVMFunction_MathQuaternionToAxisAndAngle()
	{
		Value = FQuat::Identity;
		Axis = FVector(1.f, 0.f, 0.f);
		Angle = 0.f;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	FQuat Value;

	UPROPERTY(meta=(Output))
	FVector Axis;

	UPROPERTY(meta=(Output))
	float Angle;
};

/**
 * Retrieves the forward, right and up vectors of a quaternion
 */
USTRUCT(meta=(DisplayName="To Vectors", TemplateName="ToVectors", Keywords="Forward,Right,Up"))
struct RIGVM_API FRigVMFunction_MathQuaternionToVectors : public FRigVMFunction_MathQuaternionBase
{
	GENERATED_BODY()

	FRigVMFunction_MathQuaternionToVectors()
	{
		Value = FQuat::Identity;
		Forward = Right = Up = FVector(0.f, 0.f, 0.f);
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	FQuat Value;

	UPROPERTY(meta=(Output))
	FVector Forward;

	UPROPERTY(meta=(Output))
	FVector Right;

	UPROPERTY(meta=(Output))
	FVector Up;
};

/**
 * Scales a quaternion's angle
 */
USTRUCT(meta=(DisplayName="Scale", Keywords="Multiply,Angle,Scale", Constant, Deprecated = "5.0.1"))
struct RIGVM_API FRigVMFunction_MathQuaternionScale : public FRigVMFunction_MathQuaternionBase
{
	GENERATED_BODY()

	FRigVMFunction_MathQuaternionScale()
	{
		Value = FQuat::Identity;
		Scale = 1.f;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input, Output))
	FQuat Value;

	UPROPERTY(meta=(Input))
	float Scale;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
 * Scales a quaternion's angle
 */
USTRUCT(meta=(DisplayName="Scale", TemplateName="Scale", Keywords="Multiply,Angle,Scale", Constant))
struct RIGVM_API FRigVMFunction_MathQuaternionScaleV2 : public FRigVMFunction_MathQuaternionBase
{
	GENERATED_BODY()

	FRigVMFunction_MathQuaternionScaleV2()
	{
		Value = Result = FQuat::Identity;
		Factor = 1.f;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	FQuat Value;

	UPROPERTY(meta=(Input))
	float Factor;

	UPROPERTY(meta=(Output))
	FQuat Result;
};

/**
 * Retrieves the euler angles in degrees
 */
USTRUCT(meta=(DisplayName="To Euler", TemplateName="ToEuler", Keywords="Make,Construct"))
struct RIGVM_API FRigVMFunction_MathQuaternionToEuler : public FRigVMFunction_MathQuaternionBase
{
	GENERATED_BODY()

	FRigVMFunction_MathQuaternionToEuler()
	{
		Value = FQuat::Identity;
		RotationOrder = EEulerRotationOrder::ZYX;
		Result = FVector::ZeroVector;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	FQuat Value;

	UPROPERTY(meta = (Input))
	EEulerRotationOrder RotationOrder;

	UPROPERTY(meta=(Output))
	FVector Result;
};

/**
 * Retrieves the rotator
 */
USTRUCT(meta=(DisplayName="To Rotator", TemplateName="Cast", ExecuteContext="FRigVMExecuteContext", Keywords="Make,Construct"))
struct RIGVM_API FRigVMFunction_MathQuaternionToRotator : public FRigVMFunction_MathQuaternionBase
{
	GENERATED_BODY()

	FRigVMFunction_MathQuaternionToRotator()
	{
		Value = FQuat::Identity;
		Result = FRotator::ZeroRotator;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	FQuat Value;

	UPROPERTY(meta=(Output))
	FRotator Result;
};

/**
 * Returns the product of the two values
 */
USTRUCT(meta=(DisplayName="Multiply", TemplateName="Multiply", Keywords="Product,*"))
struct RIGVM_API FRigVMFunction_MathQuaternionMul : public FRigVMFunction_MathQuaternionBinaryAggregateOp
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the negative value
 */
USTRUCT(meta=(DisplayName="Inverse", TemplateName="Inverse"))
struct RIGVM_API FRigVMFunction_MathQuaternionInverse : public FRigVMFunction_MathQuaternionUnaryOp
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Linearly interpolates between A and B using the ratio T
 */
USTRUCT(meta=(DisplayName="Interpolate", TemplateName="Interpolate", Keywords="Lerp,Mix,Blend,Slerp,SphericalInterpolate"))
struct RIGVM_API FRigVMFunction_MathQuaternionSlerp : public FRigVMFunction_MathQuaternionBase
{
	GENERATED_BODY()

	FRigVMFunction_MathQuaternionSlerp()
	{
		A = B = Result = FQuat::Identity;
		T = 0.f;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	FQuat A;

	UPROPERTY(meta=(Input))
	FQuat B;

	UPROPERTY(meta=(Input))
	float T;

	UPROPERTY(meta=(Output))
	FQuat Result;
};

/**
 * Returns true if the value A equals B
 */
USTRUCT(meta=(DisplayName="Equals", TemplateName="Equals", Keywords="Same,==", Deprecated="5.1"))
struct RIGVM_API FRigVMFunction_MathQuaternionEquals : public FRigVMFunction_MathQuaternionBase
{
	GENERATED_BODY()

	FRigVMFunction_MathQuaternionEquals()
	{
		A = B = FQuat::Identity;
		Result = true;
	}	

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	FQuat A;

	UPROPERTY(meta=(Input))
	FQuat B;

	UPROPERTY(meta=(Output))
	bool Result;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
 * Returns true if the value A does not equal B
 */
USTRUCT(meta=(DisplayName="Not Equals", TemplateName="NotEquals", Keywords="Different,!=", Deprecated="5.1"))
struct RIGVM_API FRigVMFunction_MathQuaternionNotEquals : public FRigVMFunction_MathQuaternionBase
{
	GENERATED_BODY()

	FRigVMFunction_MathQuaternionNotEquals()
	{
		A = B = FQuat::Identity;
		Result = false;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	FQuat A;

	UPROPERTY(meta=(Input))
	FQuat B;

	UPROPERTY(meta=(Output))
	bool Result;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
 * Return one of the two values based on the condition
 */
USTRUCT(meta=(DisplayName="Select", Keywords="Pick,If", Deprecated = "4.26.0"))
struct RIGVM_API FRigVMFunction_MathQuaternionSelectBool : public FRigVMFunction_MathQuaternionBase
{
	GENERATED_BODY()

	FRigVMFunction_MathQuaternionSelectBool()
	{
		IfTrue = IfFalse = Result = FQuat::Identity;
		Condition = true;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	bool Condition;

	UPROPERTY(meta=(Input))
	FQuat IfTrue;

	UPROPERTY(meta=(Input))
	FQuat IfFalse;

	UPROPERTY(meta=(Output))
	FQuat Result;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
 * Returns the dot product between two quaternions
 */
USTRUCT(meta=(DisplayName="Dot", TemplateName="Dot,|"))
struct RIGVM_API FRigVMFunction_MathQuaternionDot : public FRigVMFunction_MathQuaternionBase
{
	GENERATED_BODY()

	FRigVMFunction_MathQuaternionDot()
	{
		A = B = FQuat::Identity;
		Result = 0.f;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	FQuat A;

	UPROPERTY(meta=(Input))
	FQuat B;

	UPROPERTY(meta=(Output))
	float Result;
};

/**
 * Returns the normalized quaternion
 */
USTRUCT(meta=(DisplayName="Unit", TemplateName="Unit", Keywords="Normalize"))
struct RIGVM_API FRigVMFunction_MathQuaternionUnit : public FRigVMFunction_MathQuaternionUnaryOp
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Rotates a given vector by the quaternion
 */
USTRUCT(meta=(DisplayName="Rotate Vector", TemplateName="Rotate Vector", Keywords="Transform,Multiply"))
struct RIGVM_API FRigVMFunction_MathQuaternionRotateVector : public FRigVMFunction_MathQuaternionBase
{
	GENERATED_BODY()

	FRigVMFunction_MathQuaternionRotateVector()
	{
		Transform = FQuat::Identity;
		Vector = Result = FVector::ZeroVector;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	FQuat Transform;

	UPROPERTY(meta=(Input))
	FVector Vector;

	UPROPERTY(meta=(Output))
	FVector Result;
};

/**
 * Rotates a given vector by the quaternion
 */
USTRUCT(meta = (DisplayName = "Axis", TemplateName="Axis", Keywords = "GetAxis,xAxis,yAxis,zAxis"))
struct RIGVM_API FRigVMFunction_MathQuaternionGetAxis: public FRigVMFunction_MathQuaternionBase
{
	GENERATED_BODY()

	FRigVMFunction_MathQuaternionGetAxis()
	{
		Quaternion = FQuat::Identity;
		Axis = EAxis::X;
		Result = FVector(1.f, 0.f, 0.f);
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	FQuat Quaternion;

	UPROPERTY(meta = (Input))
	TEnumAsByte<EAxis::Type> Axis;

	UPROPERTY(meta = (Output))
	FVector Result;
};


/**
 * Computes the swing and twist components of a quaternion
 */
USTRUCT(meta = (DisplayName = "To Swing & Twist"))
struct RIGVM_API FRigVMFunction_MathQuaternionSwingTwist : public FRigVMFunction_MathQuaternionBase
{
	GENERATED_BODY()

	FRigVMFunction_MathQuaternionSwingTwist()
	{
		Input = Swing = Twist = FQuat::Identity;
		TwistAxis = FVector(1.f, 0.f, 0.f);
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	FQuat	Input;

	UPROPERTY(meta = (Input))
	FVector TwistAxis;

	UPROPERTY(meta = (Output))
	FQuat Swing;

	UPROPERTY(meta = (Output))
	FQuat Twist;
};

/**
 * Enum of possible rotation orders
 */
USTRUCT(meta = (DisplayName = "Rotation Order", Category = "Math|Quaternion", Constant))
struct RIGVM_API FRigVMFunction_MathQuaternionRotationOrder : public FRigVMFunction_MathBase
{
	GENERATED_BODY()

	FRigVMFunction_MathQuaternionRotationOrder()
	{
		RotationOrder = EEulerRotationOrder::ZYX;
	}

	UPROPERTY(meta = (Input, Output))
	EEulerRotationOrder RotationOrder;


	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the relative local transform within a parent's transform
 */
USTRUCT(meta=(DisplayName="Make Relative", TemplateName="Make Relative", Keywords="Local,Global,Absolute"))
struct RIGVM_API FRigVMFunction_MathQuaternionMakeRelative : public FRigVMFunction_MathQuaternionBase
{
	GENERATED_BODY()

	FRigVMFunction_MathQuaternionMakeRelative()
	{
		Global = Parent = Local = FQuat::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	FQuat Global;

	UPROPERTY(meta=(Input))
	FQuat Parent;

	UPROPERTY(meta=(Output))
	FQuat Local;
};

/**
 * Returns the absolute global transform within a parent's transform
 */
USTRUCT(meta = (DisplayName = "Make Absolute", TemplateName="Make Absolute", Keywords = "Local,Global,Relative"))
struct RIGVM_API FRigVMFunction_MathQuaternionMakeAbsolute : public FRigVMFunction_MathQuaternionBase
{
	GENERATED_BODY()

	FRigVMFunction_MathQuaternionMakeAbsolute()
	{
		Global = Parent = Local = FQuat::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	FQuat Local;

	UPROPERTY(meta = (Input))
	FQuat Parent;

	UPROPERTY(meta = (Output))
	FQuat Global;
};

/**
 * Mirror a rotation about a central transform.
 */
USTRUCT(meta=(DisplayName="Mirror", TemplateName="Mirror"))
struct RIGVM_API FRigVMFunction_MathQuaternionMirrorTransform : public FRigVMFunction_MathQuaternionBase
{
	GENERATED_BODY()

	FRigVMFunction_MathQuaternionMirrorTransform()
	{
		Value = Result = FQuat::Identity;
		MirrorAxis = EAxis::X;
		AxisToFlip = EAxis::Z;
		CentralTransform = FTransform::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	FQuat Value;

	// the axis to mirror against
	UPROPERTY(meta=(Input))
	TEnumAsByte<EAxis::Type> MirrorAxis;

	// the axis to flip for rotations
	UPROPERTY(meta=(Input))
	TEnumAsByte<EAxis::Type> AxisToFlip;

	// The transform about which to mirror
	UPROPERTY(meta=(Input))
	FTransform CentralTransform;

	UPROPERTY(meta=(Output))
	FQuat Result;
};
