// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMFunction_MathBase.h"
#include "RigVMMathLibrary.h"
#include "RigVMFunction_MathVector.generated.h"

USTRUCT(meta=(Abstract, Category="Math|Vector", MenuDescSuffix="(Vector)"))
struct RIGVM_API FRigVMFunction_MathVectorBase : public FRigVMFunction_MathBase
{
	GENERATED_BODY()
};

USTRUCT(meta=(Abstract))
struct RIGVM_API FRigVMFunction_MathVectorUnaryOp : public FRigVMFunction_MathVectorBase
{
	GENERATED_BODY()
	
	FRigVMFunction_MathVectorUnaryOp()
	{
		Value = Result = FVector::ZeroVector;
	}

	UPROPERTY(meta=(Input))
	FVector Value;

	UPROPERTY(meta=(Output))
	FVector Result;
};

USTRUCT(meta=(Abstract))
struct RIGVM_API FRigVMFunction_MathVectorBinaryOp : public FRigVMFunction_MathVectorBase
{
	GENERATED_BODY()

	FRigVMFunction_MathVectorBinaryOp()
	{
		A = B = Result = FVector::ZeroVector;
	}

	UPROPERTY(meta=(Input))
	FVector A;

	UPROPERTY(meta=(Input))
	FVector B;

	UPROPERTY(meta=(Output))
	FVector Result;
};

USTRUCT(meta=(Abstract))
struct RIGVM_API FRigVMFunction_MathVectorBinaryAggregateOp : public FRigVMFunction_MathVectorBase
{
	GENERATED_BODY()

	FRigVMFunction_MathVectorBinaryAggregateOp()
	{
		A = B = Result = FVector::ZeroVector;
	}

	UPROPERTY(meta=(Input, Aggregate))
	FVector A;

	UPROPERTY(meta=(Input, Aggregate))
	FVector B;

	UPROPERTY(meta=(Output, Aggregate))
	FVector Result;
};

/**
 * Makes a vector from its components
 */
USTRUCT(meta=(DisplayName="Make Vector", Keywords="Make,Construct,Constant"))
struct RIGVM_API FRigVMFunction_MathVectorMake : public FRigVMFunction_MathVectorBase
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute() override;

	FRigVMFunction_MathVectorMake()
	{
		X = Y = Z = 0.f;
		Result = FVector::ZeroVector;
	}

	UPROPERTY(meta=(Input))
	float X;

	UPROPERTY(meta=(Input))
	float Y;

	UPROPERTY(meta=(Input))
	float Z;

	UPROPERTY(meta=(Output))
	FVector Result;
};

/**
 * Makes a vector from a single float
 */
USTRUCT(meta=(DisplayName="From Float", TemplateName="Cast", ExecuteContext="FRigVMExecuteContext", Keywords="Make,Construct"))
struct RIGVM_API FRigVMFunction_MathVectorFromFloat : public FRigVMFunction_MathVectorBase
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute() override;

	FRigVMFunction_MathVectorFromFloat()
	{
		Value = 0.f;
		Result = FVector::ZeroVector;
	}

	UPROPERTY(meta=(Input))
	float Value;

	UPROPERTY(meta=(Output))
	FVector Result;
};

/**
 * Makes a vector from a single double
 */
USTRUCT(meta=(DisplayName="From Float", TemplateName="Cast", ExecuteContext="FRigVMExecuteContext", Keywords="Make,Construct"))
struct RIGVM_API FRigVMFunction_MathVectorFromDouble : public FRigVMFunction_MathVectorBase
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute() override;

	FRigVMFunction_MathVectorFromDouble()
	{
		Value = 0.0;
		Result = FVector::ZeroVector;
	}

	UPROPERTY(meta=(Input))
	double Value;

	UPROPERTY(meta=(Output))
	FVector Result;
};

/**
 * Returns the sum of the two values
 */
USTRUCT(meta=(DisplayName="Add", TemplateName="Add", Keywords="Sum,+"))
struct RIGVM_API FRigVMFunction_MathVectorAdd : public FRigVMFunction_MathVectorBinaryAggregateOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the difference of the two values
 */
USTRUCT(meta=(DisplayName="Subtract", TemplateName="Subtract", Keywords="-"))
struct RIGVM_API FRigVMFunction_MathVectorSub : public FRigVMFunction_MathVectorBinaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the product of the two values
 */
USTRUCT(meta=(DisplayName="Multiply", TemplateName="Multiply", Keywords="Product,*"))
struct RIGVM_API FRigVMFunction_MathVectorMul : public FRigVMFunction_MathVectorBinaryAggregateOp
{
	GENERATED_BODY()

	FRigVMFunction_MathVectorMul()
	{
		A = B = FVector::OneVector;
	}

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the product of the the vector and the float value
 */
USTRUCT(meta = (DisplayName = "Scale", TemplateName="Scale", Keywords = "Multiply,Product,*,ByScalar,ByFloat"))
struct RIGVM_API FRigVMFunction_MathVectorScale : public FRigVMFunction_MathVectorBase
{
	GENERATED_BODY()

		FRigVMFunction_MathVectorScale()
	{
		Value = Result = FVector::ZeroVector;
		Factor = 1.f;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	FVector Value;

	UPROPERTY(meta = (Input))
	float Factor;

	UPROPERTY(meta = (Output))
	FVector Result;
};

/**
 * Returns the division of the two values
 */
USTRUCT(meta=(DisplayName="Divide", TemplateName="Divide", Keywords="Division,Divisor,/"))
struct RIGVM_API FRigVMFunction_MathVectorDiv : public FRigVMFunction_MathVectorBinaryOp
{
	GENERATED_BODY()

	FRigVMFunction_MathVectorDiv()
	{
		B = FVector::OneVector;
	}

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the modulo of the two values
 */
USTRUCT(meta=(DisplayName="Modulo", TemplateName="Modulo", Keywords="%,fmod"))
struct RIGVM_API FRigVMFunction_MathVectorMod : public FRigVMFunction_MathVectorBinaryOp
{
	GENERATED_BODY()

	FRigVMFunction_MathVectorMod()
	{
		A = FVector::ZeroVector;
		B = FVector::OneVector;
	}

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the smaller of the two values for each component
 */
USTRUCT(meta=(DisplayName="Minimum", TemplateName="Minimum"))
struct RIGVM_API FRigVMFunction_MathVectorMin : public FRigVMFunction_MathVectorBinaryAggregateOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the larger of the two values each component
 */
USTRUCT(meta=(DisplayName="Maximum", TemplateName="Maximum"))
struct RIGVM_API FRigVMFunction_MathVectorMax : public FRigVMFunction_MathVectorBinaryAggregateOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the negative value
 */
USTRUCT(meta=(DisplayName="Negate", TemplateName="Negate", Keywords="-,Abs"))
struct RIGVM_API FRigVMFunction_MathVectorNegate : public FRigVMFunction_MathVectorUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the absolute (positive) value
 */
USTRUCT(meta=(DisplayName="Absolute", TemplateName="Absolute", Keywords="Abs,Neg"))
struct RIGVM_API FRigVMFunction_MathVectorAbs : public FRigVMFunction_MathVectorUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the closest lower full number (integer) of the value for each component
 */
USTRUCT(meta=(DisplayName="Floor", Keywords="Round"))
struct RIGVM_API FRigVMFunction_MathVectorFloor : public FRigVMFunction_MathVectorUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the closest higher full number (integer) of the value for each component
 */
USTRUCT(meta=(DisplayName="Ceiling", Keywords="Round"))
struct RIGVM_API FRigVMFunction_MathVectorCeil : public FRigVMFunction_MathVectorUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the closest higher full number (integer) of the value for each component
 */
USTRUCT(meta=(DisplayName="Round"))
struct RIGVM_API FRigVMFunction_MathVectorRound : public FRigVMFunction_MathVectorUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the sign of the value (+1 for >= FVector(0.f, 0.f, 0.f), -1 for < 0.f) for each component
 */
USTRUCT(meta=(DisplayName="Sign", TemplateName="Sign"))
struct RIGVM_API FRigVMFunction_MathVectorSign : public FRigVMFunction_MathVectorUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Clamps the given value within the range provided by minimum and maximum for each component
 */
USTRUCT(meta=(DisplayName="Clamp", TemplateName="Clamp", Keywords="Range,Remap"))
struct RIGVM_API FRigVMFunction_MathVectorClamp : public FRigVMFunction_MathVectorBase
{
	GENERATED_BODY()

	FRigVMFunction_MathVectorClamp()
	{
		Value = Minimum = Result = FVector::ZeroVector;
		Maximum = FVector::OneVector;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	FVector Value;

	UPROPERTY(meta=(Input))
	FVector Minimum;

	UPROPERTY(meta=(Input))
	FVector Maximum;

	UPROPERTY(meta=(Output))
	FVector Result;
};

/**
 * Linearly interpolates between A and B using the ratio T
 */
USTRUCT(meta=(DisplayName="Interpolate", TemplateName="Interpolate", Keywords="Lerp,Mix,Blend"))
struct RIGVM_API FRigVMFunction_MathVectorLerp : public FRigVMFunction_MathVectorBase
{
	GENERATED_BODY()

	FRigVMFunction_MathVectorLerp()
	{
		A = Result = FVector::ZeroVector;
		B = FVector::OneVector;
		T = 0.f;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	FVector A;

	UPROPERTY(meta=(Input))
	FVector B;

	UPROPERTY(meta=(Input))
	float T;

	UPROPERTY(meta=(Output))
	FVector Result;
};

/**
 * Remaps the given value from a source range to a target range for each component
 */
USTRUCT(meta=(DisplayName="Remap", TemplateName="Remap", Keywords="Rescale,Scale"))
struct RIGVM_API FRigVMFunction_MathVectorRemap : public FRigVMFunction_MathVectorBase
{
	GENERATED_BODY()

	FRigVMFunction_MathVectorRemap()
	{
		Value = SourceMinimum = TargetMinimum = Result = FVector::ZeroVector;
		SourceMaximum = TargetMaximum = FVector::OneVector;
		bClamp = false;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	FVector Value;

	UPROPERTY(meta=(Input))
	FVector SourceMinimum;

	UPROPERTY(meta=(Input))
	FVector SourceMaximum;

	UPROPERTY(meta=(Input))
	FVector TargetMinimum;

	UPROPERTY(meta=(Input))
	FVector TargetMaximum;

	/** If set to true the result is clamped to the target range */
	UPROPERTY(meta=(Input))
	bool bClamp;

	UPROPERTY(meta=(Output))
	FVector Result;
};

/**
 * Returns true if the value A equals B
 */
USTRUCT(meta=(DisplayName="Equals", TemplateName="Equals", Keywords="Same,==", Deprecated="5.1"))
struct RIGVM_API FRigVMFunction_MathVectorEquals : public FRigVMFunction_MathVectorBase
{
	GENERATED_BODY()

	FRigVMFunction_MathVectorEquals()
	{
		A = B = FVector::ZeroVector;
		Result = true;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	FVector A;

	UPROPERTY(meta=(Input))
	FVector B;

	UPROPERTY(meta=(Output))
	bool Result;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
 * Returns true if the value A does not equal B
 */
USTRUCT(meta=(DisplayName="Not Equals", TemplateName="NotEquals", Keywords="Different,!=", Deprecated="5.1"))
struct RIGVM_API FRigVMFunction_MathVectorNotEquals : public FRigVMFunction_MathVectorBase
{
	GENERATED_BODY()

	FRigVMFunction_MathVectorNotEquals()
	{
		A = B = FVector::ZeroVector;
		Result = false;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	FVector A;

	UPROPERTY(meta=(Input))
	FVector B;

	UPROPERTY(meta=(Output))
	bool Result;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
 * Returns true if the value is nearly zero
 */
USTRUCT(meta=(DisplayName="Is Nearly Zero", TemplateName="IsNearlyZero", Keywords="AlmostZero,0"))
struct RIGVM_API FRigVMFunction_MathVectorIsNearlyZero : public FRigVMFunction_MathVectorBase
{
	GENERATED_BODY()

	FRigVMFunction_MathVectorIsNearlyZero()
	{
		Value = FVector::ZeroVector;
		Tolerance = 0.f;
		Result = true;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	FVector Value;

	UPROPERTY(meta=(Input))
	float Tolerance;

	UPROPERTY(meta=(Output))
	bool Result;
};

/**
 * Returns true if the value A is almost equal to B
 */
USTRUCT(meta=(DisplayName="Is Nearly Equal", TemplateName="IsNearlyEqual", Keywords="AlmostEqual,=="))
struct RIGVM_API FRigVMFunction_MathVectorIsNearlyEqual : public FRigVMFunction_MathVectorBase
{
	GENERATED_BODY()

	FRigVMFunction_MathVectorIsNearlyEqual()
	{
		A = B = FVector::ZeroVector;
		Tolerance = 0.f;
		Result = true;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	FVector A;

	UPROPERTY(meta=(Input))
	FVector B;

	UPROPERTY(meta=(Input))
	float Tolerance;

	UPROPERTY(meta=(Output))
	bool Result;
};

/**
 * Return one of the two values based on the condition
 */
USTRUCT(meta=(DisplayName="Select", Keywords="Pick,If", Deprecated = "4.26.0"))
struct RIGVM_API FRigVMFunction_MathVectorSelectBool : public FRigVMFunction_MathVectorBase
{
	GENERATED_BODY()

	FRigVMFunction_MathVectorSelectBool()
	{
		Condition = false;
		IfTrue = IfFalse = Result = FVector::ZeroVector;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	bool Condition;

	UPROPERTY(meta=(Input))
	FVector IfTrue;

	UPROPERTY(meta=(Input))
	FVector IfFalse;

	UPROPERTY(meta=(Output))
	FVector Result;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
 * Returns the degrees of a given value in radians
 */
USTRUCT(meta=(DisplayName="Degrees", TemplateName="Degrees"))
struct RIGVM_API FRigVMFunction_MathVectorDeg : public FRigVMFunction_MathVectorUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the radians of a given value in degrees
 */
USTRUCT(meta=(DisplayName="Radians", TemplateName="Radians"))
struct RIGVM_API FRigVMFunction_MathVectorRad : public FRigVMFunction_MathVectorUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the squared length of the vector
 */
USTRUCT(meta=(DisplayName="Length Squared", TemplateName="LengthSquared", Keywords="Length,Size,Magnitude"))
struct RIGVM_API FRigVMFunction_MathVectorLengthSquared : public FRigVMFunction_MathVectorBase
{
	GENERATED_BODY()

	FRigVMFunction_MathVectorLengthSquared()
	{
		Value = FVector::ZeroVector;
		Result = 0.f;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	FVector Value;

	UPROPERTY(meta=(Output))
	float Result;
};

/**
 * Returns the length of the vector
 */
USTRUCT(meta=(DisplayName="Length", TemplateName="Length", Keywords="Size,Magnitude"))
struct RIGVM_API FRigVMFunction_MathVectorLength : public FRigVMFunction_MathVectorBase
{
	GENERATED_BODY()

	FRigVMFunction_MathVectorLength()
	{
		Value = FVector::ZeroVector;
		Result = 0.f;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	FVector Value;

	UPROPERTY(meta=(Output))
	float Result;
};

/**
 * Returns the distance from A to B
 */
USTRUCT(meta=(DisplayName="Distance Between", TemplateName="Distance"))
struct RIGVM_API FRigVMFunction_MathVectorDistance : public FRigVMFunction_MathVectorBase
{
	GENERATED_BODY()

	FRigVMFunction_MathVectorDistance()
	{
		A = B = FVector::ZeroVector;
		Result = 0.f;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	FVector A;

	UPROPERTY(meta=(Input))
	FVector B;

	UPROPERTY(meta=(Output))
	float Result;
};

/**
 * Returns the cross product between two vectors
 */
USTRUCT(meta=(DisplayName="Cross", TemplateName="Cross", Keywords="^"))
struct RIGVM_API FRigVMFunction_MathVectorCross : public FRigVMFunction_MathVectorBinaryOp
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the dot product between two vectors
 */
USTRUCT(meta=(DisplayName="Dot", TemplateName="Dot,|"))
struct RIGVM_API FRigVMFunction_MathVectorDot : public FRigVMFunction_MathVectorBase
{
	GENERATED_BODY()

	FRigVMFunction_MathVectorDot()
	{
		A = B = FVector::ZeroVector;
		Result = 0.f;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	FVector A;

	UPROPERTY(meta=(Input))
	FVector B;

	UPROPERTY(meta=(Output))
	float Result;
};

/**
 * Returns the normalized value
 */
USTRUCT(meta=(DisplayName="Unit", TemplateName="Unit", Keywords="Normalize"))
struct RIGVM_API FRigVMFunction_MathVectorUnit : public FRigVMFunction_MathVectorUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Sets the length of a given vector
 */
USTRUCT(meta = (DisplayName = "SetLength", TemplateName="SetLength", Keywords = "Unit,Normalize,Scale"))
struct RIGVM_API FRigVMFunction_MathVectorSetLength: public FRigVMFunction_MathVectorBase
{
	GENERATED_BODY()

	FRigVMFunction_MathVectorSetLength()
	{
		Value = Result = FVector(1.f, 0.f, 0.f);
		Length = 1.f;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	FVector Value;

	UPROPERTY(meta = (Input))
	float Length;

	UPROPERTY(meta = (Output))
	FVector Result;
};

/**
 * Clamps the length of a given vector between a minimum and maximum
 */
USTRUCT(meta = (DisplayName = "ClampLength", TemplateName="ClampLength", Keywords = "Unit,Normalize,Scale"))
struct RIGVM_API FRigVMFunction_MathVectorClampLength: public FRigVMFunction_MathVectorBase
{
	GENERATED_BODY()

	FRigVMFunction_MathVectorClampLength()
	{
		Value = Result = FVector(1.f, 0.f, 0.f);
		MinimumLength = 0.f;
		MaximumLength = 1.f;
	}


	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	FVector Value;

	UPROPERTY(meta = (Input))
	float MinimumLength;

	UPROPERTY(meta = (Input))
	float MaximumLength;

	UPROPERTY(meta = (Output))
	FVector Result;
};

/**
 * Mirror a vector about a normal vector.
 */
USTRUCT(meta=(DisplayName="Mirror on Normal"))
struct RIGVM_API FRigVMFunction_MathVectorMirror : public FRigVMFunction_MathVectorBase
{
	GENERATED_BODY()

	FRigVMFunction_MathVectorMirror()
	{
		Value = Result = FVector::ZeroVector;
		Normal = FVector(1.f, 0.f, 0.f);
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	FVector Value;

	UPROPERTY(meta=(Input))
	FVector Normal;

	UPROPERTY(meta=(Output))
	FVector Result;
};

/**
 * Returns the angle between two vectors in radians
 */
USTRUCT(meta=(DisplayName="Angle Between", TemplateName="AngleBetween"))
struct RIGVM_API FRigVMFunction_MathVectorAngle : public FRigVMFunction_MathVectorBase
{
	GENERATED_BODY()

		FRigVMFunction_MathVectorAngle()
	{
		A = B = FVector::ZeroVector;
		Result = 0.f;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	FVector A;

	UPROPERTY(meta=(Input))
	FVector B;

	UPROPERTY(meta=(Output))
	float Result;
};

/**
 * Returns true if the two vectors are parallel
 */
USTRUCT(meta=(DisplayName="Parallel", TemplateName="Parallel"))
struct RIGVM_API FRigVMFunction_MathVectorParallel : public FRigVMFunction_MathVectorBase
{
	GENERATED_BODY()

	FRigVMFunction_MathVectorParallel()
	{
		A = B = FVector(1.f, 0.f, 0.f);
		Result = true;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	FVector A;

	UPROPERTY(meta=(Input))
	FVector B;

	UPROPERTY(meta=(Output))
	bool Result;
};

/**
 * Returns true if the two vectors are orthogonal
 */
USTRUCT(meta=(DisplayName="Orthogonal", TemplateName="Orthogonal"))
struct RIGVM_API FRigVMFunction_MathVectorOrthogonal : public FRigVMFunction_MathVectorBase
{
	GENERATED_BODY()

	FRigVMFunction_MathVectorOrthogonal()
	{
		A = FVector(1.f, 0.f, 0.f);
		B = FVector(0.f, 1.f, 0.f);
		Result = true;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	FVector A;

	UPROPERTY(meta=(Input))
	FVector B;

	UPROPERTY(meta=(Output))
	bool Result;
};

/**
 * Returns the 4 point bezier interpolation
 */
USTRUCT(meta=(DisplayName="Bezier Four Point", Deprecated = "5.0"))
struct RIGVM_API FRigVMFunction_MathVectorBezierFourPoint : public FRigVMFunction_MathVectorBase
{
	GENERATED_BODY()

	FRigVMFunction_MathVectorBezierFourPoint()
	{
		Bezier = FRigVMFourPointBezier();
		T = 0.f;
		Result = Tangent = FVector::ZeroVector;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	FRigVMFourPointBezier Bezier;

	UPROPERTY(meta=(Input))
	float T;

	UPROPERTY(meta=(Output))
	FVector Result;

	UPROPERTY(meta=(Output))
	FVector Tangent;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
 * Creates a bezier four point
 */
USTRUCT(meta = (DisplayName = "Make Bezier Four Point", Constant, Deprecated = "5.0"))
struct RIGVM_API FRigVMFunction_MathVectorMakeBezierFourPoint : public FRigVMFunction_MathVectorBase
{
	GENERATED_BODY()

	FRigVMFunction_MathVectorMakeBezierFourPoint()
	{
		Bezier = FRigVMFourPointBezier();
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input, Output))
	FRigVMFourPointBezier Bezier;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
 * Clamps a position using a plane collision, cylindric collision or spherical collision.
 * The collision happens both towards an inner envelope (minimum) and towards an outer envelope (maximum).
 * You can disable the inner / outer envelope / collision by setting the minimum / maximum to 0.0.
 */
USTRUCT(meta = (DisplayName = "Clamp Spatially", TemplateName="ClampSpatially", Keywords="Collide,Collision"))
struct RIGVM_API FRigVMFunction_MathVectorClampSpatially: public FRigVMFunction_MathVectorBase
{
	GENERATED_BODY()

	FRigVMFunction_MathVectorClampSpatially()
	{
		Value = Result = FVector::ZeroVector;
		Axis = EAxis::X;
		Type = ERigVMClampSpatialMode::Plane;
		Minimum = 0.f;
		Maximum = 100.f;
		Space = FTransform::Identity;
		bDrawDebug = false;
		DebugColor = FLinearColor::Red;
		DebugThickness = 1.f;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	FVector Value;

	UPROPERTY(meta = (Input))
	TEnumAsByte<EAxis::Type> Axis;

	UPROPERTY(meta = (Input))
	TEnumAsByte<ERigVMClampSpatialMode::Type> Type;

	// The minimum allowed distance at which a collision occurs.
	// Note: For capsule this represents the radius.
	// Disable by setting to 0.0.
	UPROPERTY(meta = (Input))
	float Minimum;

	// This maximum allowed distance.
	// A collision will occur towards the center at this wall.
	// Note: For capsule this represents the length.
	// Disable by setting to 0.0.
	UPROPERTY(meta = (Input))
	float Maximum;

	// The space this spatial clamp happens within.
	// The input position will be projected into this space.
	UPROPERTY(meta = (Input))
	FTransform Space;

	UPROPERTY(meta = (Input))
	bool bDrawDebug;

	UPROPERTY(meta = (Input))
	FLinearColor DebugColor;

	UPROPERTY(meta = (Input))
	float DebugThickness;

	UPROPERTY(meta = (Output))
	FVector Result;
};

/**
 * Intersects a plane with a vector given a start and direction
 */
USTRUCT(meta = (DisplayName = "Intersect Plane", Keywords = "Collide,Intersect,Raycast"))
struct RIGVM_API FRigVMFunction_MathIntersectPlane : public FRigVMFunction_MathVectorBase
{
	GENERATED_BODY()

	FRigVMFunction_MathIntersectPlane()
	{
		Start = PlanePoint = Result = FVector::ZeroVector;
		Direction = PlaneNormal = FVector(0.f, 0.f, 1.f);
		Distance = 0.f;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	FVector Start;

	UPROPERTY(meta = (Input))
	FVector Direction;

	UPROPERTY(meta = (Input))
	FVector PlanePoint;

	UPROPERTY(meta = (Input))
	FVector PlaneNormal;

	UPROPERTY(meta = (Output))
	FVector Result;

	UPROPERTY(meta = (Output))
	float Distance;
};

/**
 * Find the point on the plane that is closest to the given point and the distance between them.
 */
USTRUCT(meta = (DisplayName = "Distance To Plane", Keywords = "Distance,Plane,Closest,Project"))
struct RIGVM_API FRigVMFunction_MathDistanceToPlane : public FRigVMFunction_MathVectorBase
{
	GENERATED_BODY()

	FRigVMFunction_MathDistanceToPlane()
	{
		Point = PlanePoint = ClosestPointOnPlane = FVector::ZeroVector;
		PlaneNormal = FVector(0.f, 0.f, 1.f);
		SignedDistance = 0.f;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	FVector Point;

	UPROPERTY(meta = (Input))
	FVector PlanePoint;

	UPROPERTY(meta = (Input))
	FVector PlaneNormal;

	UPROPERTY(meta = (Output))
	FVector ClosestPointOnPlane;

	UPROPERTY(meta = (Output))
	float SignedDistance;
};

/**
 * Returns the relative local vector within a parent's vector
 */
USTRUCT(meta=(DisplayName="Make Relative", TemplateName="Make Relative", Keywords="Local,Global,Absolute"))
struct RIGVM_API FRigVMFunction_MathVectorMakeRelative : public FRigVMFunction_MathVectorBase
{
	GENERATED_BODY()

	FRigVMFunction_MathVectorMakeRelative()
	{
		Global = Parent = Local = FVector::ZeroVector;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	FVector Global;

	UPROPERTY(meta=(Input))
	FVector Parent;

	UPROPERTY(meta=(Output))
	FVector Local;
};

/**
 * Returns the absolute global vector within a parent's vector
 */
USTRUCT(meta = (DisplayName = "Make Absolute", TemplateName="Make Absolute", Keywords = "Local,Global,Relative"))
struct RIGVM_API FRigVMFunction_MathVectorMakeAbsolute : public FRigVMFunction_MathVectorBase
{
	GENERATED_BODY()

	FRigVMFunction_MathVectorMakeAbsolute()
	{
		Global = Parent = Local = FVector::ZeroVector;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	FVector Local;

	UPROPERTY(meta = (Input))
	FVector Parent;

	UPROPERTY(meta = (Output))
	FVector Global;
};

/**
 * Mirror a vector about a central transform.
 */
USTRUCT(meta=(DisplayName="Mirror", TemplateName="Mirror"))
struct RIGVM_API FRigVMFunction_MathVectorMirrorTransform : public FRigVMFunction_MathVectorBase
{
	GENERATED_BODY()

	FRigVMFunction_MathVectorMirrorTransform()
	{
		Value = Result = FVector::ZeroVector;
		MirrorAxis = EAxis::X;
		AxisToFlip = EAxis::Z;
		CentralTransform = FTransform::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	FVector Value;

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
	FVector Result;
};

/**
 * Returns the sum of the given array
 */
USTRUCT(meta = (DisplayName = "Array Sum", TemplateName = "ArraySum"))
struct RIGVM_API FRigVMFunction_MathVectorArraySum : public FRigVMFunction_MathVectorBase
{
	GENERATED_BODY()

	FRigVMFunction_MathVectorArraySum()
	{
		Sum = FVector::ZeroVector;
	}

	/** Execute logic for this rig unit */
	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	TArray<FVector> Array;

	UPROPERTY(meta = (Output))
	FVector Sum;
};


/**
 * Returns the average of the given array
 */
USTRUCT(meta = (DisplayName = "Array Average", TemplateName = "ArrayAverage"))
struct RIGVM_API FRigVMFunction_MathVectorArrayAverage : public FRigVMFunction_MathVectorBase
{
	GENERATED_BODY()

	FRigVMFunction_MathVectorArrayAverage()
	{
		Average = FVector::ZeroVector;
	}

	/** Execute logic for this rig unit */
	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	TArray<FVector> Array;

	UPROPERTY(meta = (Output))
	FVector Average;
};