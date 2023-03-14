// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigUnit_MathBase.h"
#include "Math/ControlRigMathLibrary.h"
#include "RigUnit_MathVector.generated.h"

USTRUCT(meta=(Abstract, Category="Math|Vector", MenuDescSuffix="(Vector)"))
struct CONTROLRIG_API FRigUnit_MathVectorBase : public FRigUnit_MathBase
{
	GENERATED_BODY()
};

USTRUCT(meta=(Abstract))
struct CONTROLRIG_API FRigUnit_MathVectorUnaryOp : public FRigUnit_MathVectorBase
{
	GENERATED_BODY()
	
	FRigUnit_MathVectorUnaryOp()
	{
		Value = Result = FVector::ZeroVector;
	}

	UPROPERTY(meta=(Input))
	FVector Value;

	UPROPERTY(meta=(Output))
	FVector Result;
};

USTRUCT(meta=(Abstract))
struct CONTROLRIG_API FRigUnit_MathVectorBinaryOp : public FRigUnit_MathVectorBase
{
	GENERATED_BODY()

	FRigUnit_MathVectorBinaryOp()
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
struct CONTROLRIG_API FRigUnit_MathVectorBinaryAggregateOp : public FRigUnit_MathVectorBase
{
	GENERATED_BODY()

	FRigUnit_MathVectorBinaryAggregateOp()
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
 * Makes a vector from a single float
 */
USTRUCT(meta=(DisplayName="From Float", TemplateName="Cast", Keywords="Make,Construct"))
struct CONTROLRIG_API FRigUnit_MathVectorFromFloat : public FRigUnit_MathVectorBase
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	FRigUnit_MathVectorFromFloat()
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
 * Returns the sum of the two values
 */
USTRUCT(meta=(DisplayName="Add", TemplateName="Add", Keywords="Sum,+"))
struct CONTROLRIG_API FRigUnit_MathVectorAdd : public FRigUnit_MathVectorBinaryAggregateOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the difference of the two values
 */
USTRUCT(meta=(DisplayName="Subtract", TemplateName="Subtract", Keywords="-"))
struct CONTROLRIG_API FRigUnit_MathVectorSub : public FRigUnit_MathVectorBinaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the product of the two values
 */
USTRUCT(meta=(DisplayName="Multiply", TemplateName="Multiply", Keywords="Product,*"))
struct CONTROLRIG_API FRigUnit_MathVectorMul : public FRigUnit_MathVectorBinaryAggregateOp
{
	GENERATED_BODY()

	FRigUnit_MathVectorMul()
	{
		A = B = FVector::OneVector;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the product of the the vector and the float value
 */
USTRUCT(meta = (DisplayName = "Scale", TemplateName="Scale", Keywords = "Multiply,Product,*,ByScalar,ByFloat"))
struct CONTROLRIG_API FRigUnit_MathVectorScale : public FRigUnit_MathVectorBase
{
	GENERATED_BODY()

		FRigUnit_MathVectorScale()
	{
		Value = Result = FVector::ZeroVector;
		Factor = 1.f;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

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
struct CONTROLRIG_API FRigUnit_MathVectorDiv : public FRigUnit_MathVectorBinaryOp
{
	GENERATED_BODY()

	FRigUnit_MathVectorDiv()
	{
		B = FVector::OneVector;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the modulo of the two values
 */
USTRUCT(meta=(DisplayName="Modulo", TemplateName="Modulo", Keywords="%,fmod"))
struct CONTROLRIG_API FRigUnit_MathVectorMod : public FRigUnit_MathVectorBinaryOp
{
	GENERATED_BODY()

	FRigUnit_MathVectorMod()
	{
		A = FVector::ZeroVector;
		B = FVector::OneVector;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the smaller of the two values for each component
 */
USTRUCT(meta=(DisplayName="Minimum", TemplateName="Minimum"))
struct CONTROLRIG_API FRigUnit_MathVectorMin : public FRigUnit_MathVectorBinaryAggregateOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the larger of the two values each component
 */
USTRUCT(meta=(DisplayName="Maximum", TemplateName="Maximum"))
struct CONTROLRIG_API FRigUnit_MathVectorMax : public FRigUnit_MathVectorBinaryAggregateOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the negative value
 */
USTRUCT(meta=(DisplayName="Negate", TemplateName="Negate", Keywords="-,Abs"))
struct CONTROLRIG_API FRigUnit_MathVectorNegate : public FRigUnit_MathVectorUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the absolute (positive) value
 */
USTRUCT(meta=(DisplayName="Absolute", TemplateName="Absolute", Keywords="Abs,Neg"))
struct CONTROLRIG_API FRigUnit_MathVectorAbs : public FRigUnit_MathVectorUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the closest lower full number (integer) of the value for each component
 */
USTRUCT(meta=(DisplayName="Floor", Keywords="Round"))
struct CONTROLRIG_API FRigUnit_MathVectorFloor : public FRigUnit_MathVectorUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the closest higher full number (integer) of the value for each component
 */
USTRUCT(meta=(DisplayName="Ceiling", Keywords="Round"))
struct CONTROLRIG_API FRigUnit_MathVectorCeil : public FRigUnit_MathVectorUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the closest higher full number (integer) of the value for each component
 */
USTRUCT(meta=(DisplayName="Round"))
struct CONTROLRIG_API FRigUnit_MathVectorRound : public FRigUnit_MathVectorUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the sign of the value (+1 for >= FVector(0.f, 0.f, 0.f), -1 for < 0.f) for each component
 */
USTRUCT(meta=(DisplayName="Sign", TemplateName="Sign"))
struct CONTROLRIG_API FRigUnit_MathVectorSign : public FRigUnit_MathVectorUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Clamps the given value within the range provided by minimum and maximum for each component
 */
USTRUCT(meta=(DisplayName="Clamp", TemplateName="Clamp", Keywords="Range,Remap"))
struct CONTROLRIG_API FRigUnit_MathVectorClamp : public FRigUnit_MathVectorBase
{
	GENERATED_BODY()

	FRigUnit_MathVectorClamp()
	{
		Value = Minimum = Result = FVector::ZeroVector;
		Maximum = FVector::OneVector;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

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
struct CONTROLRIG_API FRigUnit_MathVectorLerp : public FRigUnit_MathVectorBase
{
	GENERATED_BODY()

	FRigUnit_MathVectorLerp()
	{
		A = Result = FVector::ZeroVector;
		B = FVector::OneVector;
		T = 0.f;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

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
struct CONTROLRIG_API FRigUnit_MathVectorRemap : public FRigUnit_MathVectorBase
{
	GENERATED_BODY()

	FRigUnit_MathVectorRemap()
	{
		Value = SourceMinimum = TargetMinimum = Result = FVector::ZeroVector;
		SourceMaximum = TargetMaximum = FVector::OneVector;
		bClamp = false;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

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
struct CONTROLRIG_API FRigUnit_MathVectorEquals : public FRigUnit_MathVectorBase
{
	GENERATED_BODY()

	FRigUnit_MathVectorEquals()
	{
		A = B = FVector::ZeroVector;
		Result = true;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

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
struct CONTROLRIG_API FRigUnit_MathVectorNotEquals : public FRigUnit_MathVectorBase
{
	GENERATED_BODY()

	FRigUnit_MathVectorNotEquals()
	{
		A = B = FVector::ZeroVector;
		Result = false;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

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
struct CONTROLRIG_API FRigUnit_MathVectorIsNearlyZero : public FRigUnit_MathVectorBase
{
	GENERATED_BODY()

	FRigUnit_MathVectorIsNearlyZero()
	{
		Value = FVector::ZeroVector;
		Tolerance = 0.f;
		Result = true;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

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
struct CONTROLRIG_API FRigUnit_MathVectorIsNearlyEqual : public FRigUnit_MathVectorBase
{
	GENERATED_BODY()

	FRigUnit_MathVectorIsNearlyEqual()
	{
		A = B = FVector::ZeroVector;
		Tolerance = 0.f;
		Result = true;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

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
USTRUCT(meta=(DisplayName="Select", TemplateName="Select", Keywords="Pick,If", Deprecated = "4.26.0"))
struct CONTROLRIG_API FRigUnit_MathVectorSelectBool : public FRigUnit_MathVectorBase
{
	GENERATED_BODY()

	FRigUnit_MathVectorSelectBool()
	{
		Condition = false;
		IfTrue = IfFalse = Result = FVector::ZeroVector;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

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
struct CONTROLRIG_API FRigUnit_MathVectorDeg : public FRigUnit_MathVectorUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the radians of a given value in degrees
 */
USTRUCT(meta=(DisplayName="Radians", TemplateName="Radians"))
struct CONTROLRIG_API FRigUnit_MathVectorRad : public FRigUnit_MathVectorUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the squared length of the vector
 */
USTRUCT(meta=(DisplayName="Length Squared", TemplateName="LengthSquared", Keywords="Length,Size,Magnitude"))
struct CONTROLRIG_API FRigUnit_MathVectorLengthSquared : public FRigUnit_MathVectorBase
{
	GENERATED_BODY()

	FRigUnit_MathVectorLengthSquared()
	{
		Value = FVector::ZeroVector;
		Result = 0.f;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	FVector Value;

	UPROPERTY(meta=(Output))
	float Result;
};

/**
 * Returns the length of the vector
 */
USTRUCT(meta=(DisplayName="Length", TemplateName="Length", Keywords="Size,Magnitude"))
struct CONTROLRIG_API FRigUnit_MathVectorLength : public FRigUnit_MathVectorBase
{
	GENERATED_BODY()

	FRigUnit_MathVectorLength()
	{
		Value = FVector::ZeroVector;
		Result = 0.f;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	FVector Value;

	UPROPERTY(meta=(Output))
	float Result;
};

/**
 * Returns the distance from A to B
 */
USTRUCT(meta=(DisplayName="Distance Between", TemplateName="Distance"))
struct CONTROLRIG_API FRigUnit_MathVectorDistance : public FRigUnit_MathVectorBase
{
	GENERATED_BODY()

	FRigUnit_MathVectorDistance()
	{
		A = B = FVector::ZeroVector;
		Result = 0.f;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

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
struct CONTROLRIG_API FRigUnit_MathVectorCross : public FRigUnit_MathVectorBinaryOp
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the dot product between two vectors
 */
USTRUCT(meta=(DisplayName="Dot", TemplateName="Dot,|"))
struct CONTROLRIG_API FRigUnit_MathVectorDot : public FRigUnit_MathVectorBase
{
	GENERATED_BODY()

	FRigUnit_MathVectorDot()
	{
		A = B = FVector::ZeroVector;
		Result = 0.f;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

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
struct CONTROLRIG_API FRigUnit_MathVectorUnit : public FRigUnit_MathVectorUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Sets the length of a given vector
 */
USTRUCT(meta = (DisplayName = "SetLength", TemplateName="SetLength", Keywords = "Unit,Normalize,Scale"))
struct CONTROLRIG_API FRigUnit_MathVectorSetLength: public FRigUnit_MathVectorBase
{
	GENERATED_BODY()

	FRigUnit_MathVectorSetLength()
	{
		Value = Result = FVector(1.f, 0.f, 0.f);
		Length = 1.f;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

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
struct CONTROLRIG_API FRigUnit_MathVectorClampLength: public FRigUnit_MathVectorBase
{
	GENERATED_BODY()

	FRigUnit_MathVectorClampLength()
	{
		Value = Result = FVector(1.f, 0.f, 0.f);
		MinimumLength = 0.f;
		MaximumLength = 1.f;
	}


	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

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
struct CONTROLRIG_API FRigUnit_MathVectorMirror : public FRigUnit_MathVectorBase
{
	GENERATED_BODY()

	FRigUnit_MathVectorMirror()
	{
		Value = Result = FVector::ZeroVector;
		Normal = FVector(1.f, 0.f, 0.f);
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

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
struct CONTROLRIG_API FRigUnit_MathVectorAngle : public FRigUnit_MathVectorBase
{
	GENERATED_BODY()

		FRigUnit_MathVectorAngle()
	{
		A = B = FVector::ZeroVector;
		Result = 0.f;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

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
struct CONTROLRIG_API FRigUnit_MathVectorParallel : public FRigUnit_MathVectorBase
{
	GENERATED_BODY()

	FRigUnit_MathVectorParallel()
	{
		A = B = FVector(1.f, 0.f, 0.f);
		Result = true;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

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
struct CONTROLRIG_API FRigUnit_MathVectorOrthogonal : public FRigUnit_MathVectorBase
{
	GENERATED_BODY()

	FRigUnit_MathVectorOrthogonal()
	{
		A = FVector(1.f, 0.f, 0.f);
		B = FVector(0.f, 1.f, 0.f);
		Result = true;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

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
struct CONTROLRIG_API FRigUnit_MathVectorBezierFourPoint : public FRigUnit_MathVectorBase
{
	GENERATED_BODY()

	FRigUnit_MathVectorBezierFourPoint()
	{
		Bezier = FCRFourPointBezier();
		T = 0.f;
		Result = Tangent = FVector::ZeroVector;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	FCRFourPointBezier Bezier;

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
struct CONTROLRIG_API FRigUnit_MathVectorMakeBezierFourPoint : public FRigUnit_MathVectorBase
{
	GENERATED_BODY()

	FRigUnit_MathVectorMakeBezierFourPoint()
	{
		Bezier = FCRFourPointBezier();
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input, Output))
	FCRFourPointBezier Bezier;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
 * Clamps a position using a plane collision, cylindric collision or spherical collision.
 */
USTRUCT(meta = (DisplayName = "Clamp Spatially", TemplateName="ClampSpatially", Keywords="Collide,Collision"))
struct CONTROLRIG_API FRigUnit_MathVectorClampSpatially: public FRigUnit_MathVectorBase
{
	GENERATED_BODY()

	FRigUnit_MathVectorClampSpatially()
	{
		Value = Result = FVector::ZeroVector;
		Axis = EAxis::X;
		Type = EControlRigClampSpatialMode::Plane;
		Minimum = 0.f;
		Maximum = 100.f;
		Space = FTransform::Identity;
		bDrawDebug = false;
		DebugColor = FLinearColor::Red;
		DebugThickness = 1.f;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	FVector Value;

	UPROPERTY(meta = (Input))
	TEnumAsByte<EAxis::Type> Axis;

	UPROPERTY(meta = (Input))
	TEnumAsByte<EControlRigClampSpatialMode::Type> Type;

	UPROPERTY(meta = (Input))
	float Minimum;

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
struct CONTROLRIG_API FRigUnit_MathIntersectPlane : public FRigUnit_MathVectorBase
{
	GENERATED_BODY()

	FRigUnit_MathIntersectPlane()
	{
		Start = PlanePoint = Result = FVector::ZeroVector;
		Direction = PlaneNormal = FVector(0.f, 0.f, 1.f);
		Distance = 0.f;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

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
struct CONTROLRIG_API FRigUnit_MathDistanceToPlane : public FRigUnit_MathVectorBase
{
	GENERATED_BODY()

	FRigUnit_MathDistanceToPlane()
	{
		Point = PlanePoint = ClosestPointOnPlane = FVector::ZeroVector;
		PlaneNormal = FVector(0.f, 0.f, 1.f);
		SignedDistance = 0.f;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

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
struct CONTROLRIG_API FRigUnit_MathVectorMakeRelative : public FRigUnit_MathVectorBase
{
	GENERATED_BODY()

	FRigUnit_MathVectorMakeRelative()
	{
		Global = Parent = Local = FVector::ZeroVector;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

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
struct CONTROLRIG_API FRigUnit_MathVectorMakeAbsolute : public FRigUnit_MathVectorBase
{
	GENERATED_BODY()

	FRigUnit_MathVectorMakeAbsolute()
	{
		Global = Parent = Local = FVector::ZeroVector;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

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
struct CONTROLRIG_API FRigUnit_MathVectorMirrorTransform : public FRigUnit_MathVectorBase
{
	GENERATED_BODY()

	FRigUnit_MathVectorMirrorTransform()
	{
		Value = Result = FVector::ZeroVector;
		MirrorAxis = EAxis::X;
		AxisToFlip = EAxis::Z;
		CentralTransform = FTransform::Identity;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

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
