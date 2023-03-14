// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigUnit_MathBase.h"
#include "Math/ControlRigMathLibrary.h"
#include "RigUnit_MathQuaternion.generated.h"

USTRUCT(meta=(Abstract, Category="Math|Quaternion", MenuDescSuffix="(Quaternion)"))
struct CONTROLRIG_API FRigUnit_MathQuaternionBase : public FRigUnit_MathBase
{
	GENERATED_BODY()
};

USTRUCT(meta=(Abstract))
struct CONTROLRIG_API FRigUnit_MathQuaternionUnaryOp : public FRigUnit_MathQuaternionBase
{
	GENERATED_BODY()

	FRigUnit_MathQuaternionUnaryOp()
	{
		Value = Result = FQuat::Identity;
	}

	UPROPERTY(meta=(Input))
	FQuat Value;

	UPROPERTY(meta=(Output))
	FQuat Result;
};

USTRUCT(meta=(Abstract))
struct CONTROLRIG_API FRigUnit_MathQuaternionBinaryOp : public FRigUnit_MathQuaternionBase
{
	GENERATED_BODY()

	FRigUnit_MathQuaternionBinaryOp()
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
struct CONTROLRIG_API FRigUnit_MathQuaternionBinaryAggregateOp : public FRigUnit_MathQuaternionBase
{
	GENERATED_BODY()

	FRigUnit_MathQuaternionBinaryAggregateOp()
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
 * Makes a quaternion from an axis and an angle in radians
 */
USTRUCT(meta=(DisplayName="From Axis And Angle", TemplateName="FromAxisAndAngle", Keywords="Make,Construct"))
struct CONTROLRIG_API FRigUnit_MathQuaternionFromAxisAndAngle : public FRigUnit_MathQuaternionBase
{
	GENERATED_BODY()

	FRigUnit_MathQuaternionFromAxisAndAngle()
	{
		Axis = FVector(1.f, 0.f, 0.f);
		Angle = 0.f;
		Result = FQuat::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

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
struct CONTROLRIG_API FRigUnit_MathQuaternionFromEuler : public FRigUnit_MathQuaternionBase
{
	GENERATED_BODY()

	FRigUnit_MathQuaternionFromEuler()
	{
		Euler = FVector::ZeroVector;
		RotationOrder = EEulerRotationOrder::ZYX;
		Result = FQuat::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

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
struct CONTROLRIG_API FRigUnit_MathQuaternionFromRotator : public FRigUnit_MathQuaternionBase
{
	GENERATED_BODY()
	
	FRigUnit_MathQuaternionFromRotator()
	{
		Rotator = FRotator::ZeroRotator;
		Result = FQuat::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

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
USTRUCT(meta=(DisplayName="From Rotator", TemplateName="Cast", Keywords="Make,Construct"))
struct CONTROLRIG_API FRigUnit_MathQuaternionFromRotatorV2 : public FRigUnit_MathQuaternionBase
{
	GENERATED_BODY()
	
	FRigUnit_MathQuaternionFromRotatorV2()
	{
		Value = FRotator::ZeroRotator;
		Result = FQuat::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	FRotator Value;

	UPROPERTY(meta=(Output))
	FQuat Result;
};

/**
 * Makes a quaternion from two vectors, representing the shortest rotation between the two vectors.
 */
USTRUCT(meta=(DisplayName="From Two Vectors", TemplateName="FromTwoVectors", Keywords="Make,Construct"))
struct CONTROLRIG_API FRigUnit_MathQuaternionFromTwoVectors : public FRigUnit_MathQuaternionBase
{
	GENERATED_BODY()
	
	FRigUnit_MathQuaternionFromTwoVectors()
	{
		A = B = FVector(1.f, 0.f, 0.f);
		Result = FQuat::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

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
struct CONTROLRIG_API FRigUnit_MathQuaternionToAxisAndAngle : public FRigUnit_MathQuaternionBase
{
	GENERATED_BODY()

	FRigUnit_MathQuaternionToAxisAndAngle()
	{
		Value = FQuat::Identity;
		Axis = FVector(1.f, 0.f, 0.f);
		Angle = 0.f;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	FQuat Value;

	UPROPERTY(meta=(Output))
	FVector Axis;

	UPROPERTY(meta=(Output))
	float Angle;
};

/**
 * Scales a quaternion's angle
 */
USTRUCT(meta=(DisplayName="Scale", Keywords="Multiply,Angle,Scale", Constant, Deprecated = "5.0.1"))
struct CONTROLRIG_API FRigUnit_MathQuaternionScale : public FRigUnit_MathQuaternionBase
{
	GENERATED_BODY()

	FRigUnit_MathQuaternionScale()
	{
		Value = FQuat::Identity;
		Scale = 1.f;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

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
struct CONTROLRIG_API FRigUnit_MathQuaternionScaleV2 : public FRigUnit_MathQuaternionBase
{
	GENERATED_BODY()

	FRigUnit_MathQuaternionScaleV2()
	{
		Value = Result = FQuat::Identity;
		Factor = 1.f;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

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
struct CONTROLRIG_API FRigUnit_MathQuaternionToEuler : public FRigUnit_MathQuaternionBase
{
	GENERATED_BODY()

	FRigUnit_MathQuaternionToEuler()
	{
		Value = FQuat::Identity;
		RotationOrder = EEulerRotationOrder::ZYX;
		Result = FVector::ZeroVector;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

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
USTRUCT(meta=(DisplayName="To Rotator", TemplateName="Cast", Keywords="Make,Construct"))
struct CONTROLRIG_API FRigUnit_MathQuaternionToRotator : public FRigUnit_MathQuaternionBase
{
	GENERATED_BODY()

	FRigUnit_MathQuaternionToRotator()
	{
		Value = FQuat::Identity;
		Result = FRotator::ZeroRotator;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	FQuat Value;

	UPROPERTY(meta=(Output))
	FRotator Result;
};

/**
 * Returns the product of the two values
 */
USTRUCT(meta=(DisplayName="Multiply", TemplateName="Multiply", Keywords="Product,*"))
struct CONTROLRIG_API FRigUnit_MathQuaternionMul : public FRigUnit_MathQuaternionBinaryAggregateOp
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the negative value
 */
USTRUCT(meta=(DisplayName="Inverse", TemplateName="Inverse"))
struct CONTROLRIG_API FRigUnit_MathQuaternionInverse : public FRigUnit_MathQuaternionUnaryOp
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Linearly interpolates between A and B using the ratio T
 */
USTRUCT(meta=(DisplayName="Interpolate", TemplateName="Interpolate", Keywords="Lerp,Mix,Blend,Slerp,SphericalInterpolate"))
struct CONTROLRIG_API FRigUnit_MathQuaternionSlerp : public FRigUnit_MathQuaternionBase
{
	GENERATED_BODY()

	FRigUnit_MathQuaternionSlerp()
	{
		A = B = Result = FQuat::Identity;
		T = 0.f;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

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
struct CONTROLRIG_API FRigUnit_MathQuaternionEquals : public FRigUnit_MathQuaternionBase
{
	GENERATED_BODY()

	FRigUnit_MathQuaternionEquals()
	{
		A = B = FQuat::Identity;
		Result = true;
	}	

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

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
struct CONTROLRIG_API FRigUnit_MathQuaternionNotEquals : public FRigUnit_MathQuaternionBase
{
	GENERATED_BODY()

	FRigUnit_MathQuaternionNotEquals()
	{
		A = B = FQuat::Identity;
		Result = false;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

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
USTRUCT(meta=(DisplayName="Select", TemplateName="Select", Keywords="Pick,If", Deprecated = "4.26.0"))
struct CONTROLRIG_API FRigUnit_MathQuaternionSelectBool : public FRigUnit_MathQuaternionBase
{
	GENERATED_BODY()

	FRigUnit_MathQuaternionSelectBool()
	{
		IfTrue = IfFalse = Result = FQuat::Identity;
		Condition = true;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

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
struct CONTROLRIG_API FRigUnit_MathQuaternionDot : public FRigUnit_MathQuaternionBase
{
	GENERATED_BODY()

	FRigUnit_MathQuaternionDot()
	{
		A = B = FQuat::Identity;
		Result = 0.f;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

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
struct CONTROLRIG_API FRigUnit_MathQuaternionUnit : public FRigUnit_MathQuaternionUnaryOp
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Rotates a given vector by the quaternion
 */
USTRUCT(meta=(DisplayName="Rotate Vector", TemplateName="Rotate Vector", Keywords="Transform,Multiply"))
struct CONTROLRIG_API FRigUnit_MathQuaternionRotateVector : public FRigUnit_MathQuaternionBase
{
	GENERATED_BODY()

	FRigUnit_MathQuaternionRotateVector()
	{
		Transform = FQuat::Identity;
		Vector = Result = FVector::ZeroVector;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

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
struct CONTROLRIG_API FRigUnit_MathQuaternionGetAxis: public FRigUnit_MathQuaternionBase
{
	GENERATED_BODY()

	FRigUnit_MathQuaternionGetAxis()
	{
		Quaternion = FQuat::Identity;
		Axis = EAxis::X;
		Result = FVector(1.f, 0.f, 0.f);
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

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
struct CONTROLRIG_API FRigUnit_MathQuaternionSwingTwist : public FRigUnit_MathQuaternionBase
{
	GENERATED_BODY()

	FRigUnit_MathQuaternionSwingTwist()
	{
		Input = Swing = Twist = FQuat::Identity;
		TwistAxis = FVector(1.f, 0.f, 0.f);
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

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
struct CONTROLRIG_API FRigUnit_MathQuaternionRotationOrder : public FRigUnit_MathBase
{
	GENERATED_BODY()

	FRigUnit_MathQuaternionRotationOrder()
	{
		RotationOrder = EEulerRotationOrder::ZYX;
	}

	UPROPERTY(meta = (Input, Output))
	EEulerRotationOrder RotationOrder;


	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the relative local transform within a parent's transform
 */
USTRUCT(meta=(DisplayName="Make Relative", TemplateName="Make Relative", Keywords="Local,Global,Absolute"))
struct CONTROLRIG_API FRigUnit_MathQuaternionMakeRelative : public FRigUnit_MathQuaternionBase
{
	GENERATED_BODY()

	FRigUnit_MathQuaternionMakeRelative()
	{
		Global = Parent = Local = FQuat::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

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
struct CONTROLRIG_API FRigUnit_MathQuaternionMakeAbsolute : public FRigUnit_MathQuaternionBase
{
	GENERATED_BODY()

	FRigUnit_MathQuaternionMakeAbsolute()
	{
		Global = Parent = Local = FQuat::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

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
struct CONTROLRIG_API FRigUnit_MathQuaternionMirrorTransform : public FRigUnit_MathQuaternionBase
{
	GENERATED_BODY()

	FRigUnit_MathQuaternionMirrorTransform()
	{
		Value = Result = FQuat::Identity;
		MirrorAxis = EAxis::X;
		AxisToFlip = EAxis::Z;
		CentralTransform = FTransform::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

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
