// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigUnit_MathBase.h"
#include "RigUnit_MathDouble.generated.h"

USTRUCT(meta=(Abstract, Category="Math|Double", MenuDescSuffix="(Double)"))
struct CONTROLRIG_API FRigUnit_MathDoubleBase : public FRigUnit_MathBase
{
	GENERATED_BODY()
};

USTRUCT(meta=(Abstract))
struct CONTROLRIG_API FRigUnit_MathDoubleConstant : public FRigUnit_MathDoubleBase
{
	GENERATED_BODY()

	FRigUnit_MathDoubleConstant()
	{
		Value = 0.0;
	}

	UPROPERTY(meta=(Output))
	double Value;
};

USTRUCT(meta=(Abstract))
struct CONTROLRIG_API FRigUnit_MathDoubleUnaryOp : public FRigUnit_MathDoubleBase
{
	GENERATED_BODY()

	FRigUnit_MathDoubleUnaryOp()
	{
		Value = Result = 0.0;
	}

	UPROPERTY(meta=(Input))
	double Value;

	UPROPERTY(meta=(Output))
	double Result;
};

USTRUCT(meta=(Abstract))
struct CONTROLRIG_API FRigUnit_MathDoubleBinaryOp : public FRigUnit_MathDoubleBase
{
	GENERATED_BODY()

	FRigUnit_MathDoubleBinaryOp()
	{
		A = B = Result = 0.0;
	}

	UPROPERTY(meta=(Input))
	double A;

	UPROPERTY(meta=(Input))
	double B;

	UPROPERTY(meta=(Output))
	double Result;
};


USTRUCT(meta=(Abstract))
struct CONTROLRIG_API FRigUnit_MathDoubleBinaryAggregateOp : public FRigUnit_MathDoubleBase
{
	GENERATED_BODY()

	FRigUnit_MathDoubleBinaryAggregateOp()
	{
		A = B = Result = 0.0;
	}

	UPROPERTY(meta=(Input, Aggregate))
	double A;

	UPROPERTY(meta=(Input, Aggregate))
	double B;

	UPROPERTY(meta=(Output, Aggregate))
	double Result;
};

/**
 * Returns PI
 */
USTRUCT(meta=(DisplayName="Pi", TemplateName="Pi"))
struct CONTROLRIG_API FRigUnit_MathDoubleConstPi : public FRigUnit_MathDoubleConstant
{
	GENERATED_BODY()
	
	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns PI * 0.5
 */
USTRUCT(meta=(DisplayName="Half Pi", TemplateName="HalfPi"))
struct CONTROLRIG_API FRigUnit_MathDoubleConstHalfPi : public FRigUnit_MathDoubleConstant
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns PI * 2.0
 */
USTRUCT(meta=(DisplayName="Two Pi", TemplateName="TwoPi", Keywords="Tau"))
struct CONTROLRIG_API FRigUnit_MathDoubleConstTwoPi : public FRigUnit_MathDoubleConstant
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns E
 */
USTRUCT(meta=(DisplayName="E", TemplateName="E", Keywords="Euler"))
struct CONTROLRIG_API FRigUnit_MathDoubleConstE : public FRigUnit_MathDoubleConstant
{
	GENERATED_BODY()
	
	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the sum of the two values
 */
USTRUCT(meta=(DisplayName="Add", TemplateName="Add", Keywords="Sum,+"))
struct CONTROLRIG_API FRigUnit_MathDoubleAdd : public FRigUnit_MathDoubleBinaryAggregateOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the difference of the two values
 */
USTRUCT(meta=(DisplayName="Subtract", TemplateName="Subtract", Keywords="-"))
struct CONTROLRIG_API FRigUnit_MathDoubleSub : public FRigUnit_MathDoubleBinaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the product of the two values
 */
USTRUCT(meta=(DisplayName="Multiply", TemplateName="Multiply", Keywords="Product,*"))
struct CONTROLRIG_API FRigUnit_MathDoubleMul : public FRigUnit_MathDoubleBinaryAggregateOp
{
	GENERATED_BODY()

	FRigUnit_MathDoubleMul()
	{
		A = 1.0;
		B = 1.0;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the division of the two values
 */
USTRUCT(meta=(DisplayName="Divide", TemplateName="Divide", Keywords="Division,Divisor,/"))
struct CONTROLRIG_API FRigUnit_MathDoubleDiv : public FRigUnit_MathDoubleBinaryOp
{
	GENERATED_BODY()

	FRigUnit_MathDoubleDiv()
	{
		B = 1.0;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the modulo of the two values
 */
USTRUCT(meta=(DisplayName="Modulo", TemplateName="Modulo", Keywords="%,fmod"))
struct CONTROLRIG_API FRigUnit_MathDoubleMod : public FRigUnit_MathDoubleBinaryOp
{
	GENERATED_BODY()

	FRigUnit_MathDoubleMod()
	{
		A = 0.0;
		B = 1.0;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the smaller of the two values
 */
USTRUCT(meta=(DisplayName="Minimum", TemplateName="Minimum"))
struct CONTROLRIG_API FRigUnit_MathDoubleMin : public FRigUnit_MathDoubleBinaryAggregateOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the larger of the two values
 */
USTRUCT(meta=(DisplayName="Maximum", TemplateName="Maximum"))
struct CONTROLRIG_API FRigUnit_MathDoubleMax : public FRigUnit_MathDoubleBinaryAggregateOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the value of A raised to the power of B.
 */
USTRUCT(meta=(DisplayName="Power", TemplateName="Power"))
struct CONTROLRIG_API FRigUnit_MathDoublePow : public FRigUnit_MathDoubleBinaryOp
{
	GENERATED_BODY()

	FRigUnit_MathDoublePow()
	{
		A = 1.0;
		B = 1.0;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the square root of the given value
 */
USTRUCT(meta=(DisplayName="Sqrt", TemplateName="Sqrt", Keywords="Root,Square"))
struct CONTROLRIG_API FRigUnit_MathDoubleSqrt : public FRigUnit_MathDoubleUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the negative value
 */
USTRUCT(meta=(DisplayName="Negate", TemplateName="Negate", Keywords="-,Abs"))
struct CONTROLRIG_API FRigUnit_MathDoubleNegate : public FRigUnit_MathDoubleUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the absolute (positive) value
 */
USTRUCT(meta=(DisplayName="Absolute", TemplateName="Absolute", Keywords="Abs,Neg"))
struct CONTROLRIG_API FRigUnit_MathDoubleAbs : public FRigUnit_MathDoubleUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the closest lower full number (integer) of the value
 */
USTRUCT(meta=(DisplayName="Floor", TemplateName="Floor", Keywords="Round"))
struct CONTROLRIG_API FRigUnit_MathDoubleFloor : public FRigUnit_MathDoubleBase
{
	GENERATED_BODY()

	FRigUnit_MathDoubleFloor()
	{
		Value = Result = 0.0;
		Int = 0;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	double Value;

	UPROPERTY(meta=(Output))
	double Result;

	UPROPERTY(meta=(Output))
	int32 Int;
};

/**
 * Returns the closest higher full number (integer) of the value
 */
USTRUCT(meta=(DisplayName="Ceiling", TemplateName="Ceiling", Keywords="Round"))
struct CONTROLRIG_API FRigUnit_MathDoubleCeil : public FRigUnit_MathDoubleBase
{
	GENERATED_BODY()

	FRigUnit_MathDoubleCeil()
	{
		Value = Result = 0.0;
		Int = 0;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	double Value;

	UPROPERTY(meta=(Output))
	double Result;

	UPROPERTY(meta=(Output))
	int32 Int;
};

/**
 * Returns the closest higher full number (integer) of the value
 */
USTRUCT(meta=(DisplayName="Round", TemplateName="Round"))
struct CONTROLRIG_API FRigUnit_MathDoubleRound : public FRigUnit_MathDoubleBase
{
	GENERATED_BODY()

	FRigUnit_MathDoubleRound()
	{
		Value = Result = 0.0;
		Int = 0;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	double Value;

	UPROPERTY(meta=(Output))
	double Result;

	UPROPERTY(meta=(Output))
	int32 Int;
};

/**
 * Returns the double cast to an int (this uses floor)
 */
USTRUCT(meta=(DisplayName="To Int", TemplateName="Cast"))
struct CONTROLRIG_API FRigUnit_MathDoubleToInt : public FRigUnit_MathDoubleBase
{
	GENERATED_BODY()

	FRigUnit_MathDoubleToInt()
	{
		Value = 0.0;
		Result = 0;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	double Value;

	UPROPERTY(meta=(Output))
	int32 Result;
};

/**
 * Returns the sign of the value (+1 for >= 0.0, -1 for < 0.0)
 */
USTRUCT(meta=(DisplayName="Sign", TemplateName="Sign"))
struct CONTROLRIG_API FRigUnit_MathDoubleSign : public FRigUnit_MathDoubleUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Clamps the given value within the range provided by minimum and maximum
 */
USTRUCT(meta=(DisplayName="Clamp", TemplateName="Clamp", Keywords="Range,Remap"))
struct CONTROLRIG_API FRigUnit_MathDoubleClamp : public FRigUnit_MathDoubleBase
{
	GENERATED_BODY()

	FRigUnit_MathDoubleClamp()
	{
		Value = Minimum = Maximum = Result = 0.0;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	double Value;

	UPROPERTY(meta=(Input))
	double Minimum;

	UPROPERTY(meta=(Input))
	double Maximum;

	UPROPERTY(meta=(Output))
	double Result;
};

/**
 * Linearly interpolates between A and B using the ratio T
 */
USTRUCT(meta=(DisplayName="Interpolate", TemplateName="Interpolate", Keywords="Lerp,Mix,Blend"))
struct CONTROLRIG_API FRigUnit_MathDoubleLerp : public FRigUnit_MathDoubleBase
{
	GENERATED_BODY()

	FRigUnit_MathDoubleLerp()
	{
		A = B = T = Result = 0.0;
		B = 1.0;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	double A;

	UPROPERTY(meta=(Input))
	double B;

	UPROPERTY(meta=(Input))
	double T;

	UPROPERTY(meta=(Output))
	double Result;
};

/**
 * Remaps the given value from a source range to a target range.
 */
USTRUCT(meta=(DisplayName="Remap", TemplateName="Remap", Keywords="Rescale,Scale"))
struct CONTROLRIG_API FRigUnit_MathDoubleRemap : public FRigUnit_MathDoubleBase
{
	GENERATED_BODY()

	FRigUnit_MathDoubleRemap()
	{
		Value = SourceMinimum = TargetMinimum = Result = 0.0;
		SourceMaximum = TargetMaximum = 1.0;
		bClamp = false;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	double Value;

	UPROPERTY(meta=(Input))
	double SourceMinimum;

	UPROPERTY(meta=(Input))
	double SourceMaximum;

	UPROPERTY(meta=(Input))
	double TargetMinimum;

	UPROPERTY(meta=(Input))
	double TargetMaximum;

	/** If set to true the result is clamped to the target range */
	UPROPERTY(meta=(Input))
	bool bClamp;

	UPROPERTY(meta=(Output))
	double Result;
};

/**
 * Returns true if the value A equals B
 */
USTRUCT(meta=(DisplayName="Equals", TemplateName="Equals", Keywords="Same,==", Deprecated="5.1"))
struct CONTROLRIG_API FRigUnit_MathDoubleEquals : public FRigUnit_MathDoubleBase
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	FRigUnit_MathDoubleEquals()
	{
		A = B = 0.0;
		Result = true;
	}

	UPROPERTY(meta=(Input))
	double A;

	UPROPERTY(meta=(Input))
	double B;

	UPROPERTY(meta=(Output))
	bool Result;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
 * Returns true if the value A does not equal B
 */
USTRUCT(meta=(DisplayName="Not Equals", TemplateName="NotEquals", Keywords="Different,!=", Deprecated="5.1"))
struct CONTROLRIG_API FRigUnit_MathDoubleNotEquals : public FRigUnit_MathDoubleBase
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	FRigUnit_MathDoubleNotEquals()
	{
		A = B = 0.0;
		Result = false;
	}

	UPROPERTY(meta=(Input))
	double A;

	UPROPERTY(meta=(Input))
	double B;

	UPROPERTY(meta=(Output))
	bool Result;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
 * Returns true if the value A is greater than B
 */
USTRUCT(meta=(DisplayName="Greater", TemplateName="Greater", Keywords="Larger,Bigger,>"))
struct CONTROLRIG_API FRigUnit_MathDoubleGreater : public FRigUnit_MathDoubleBase
{
	GENERATED_BODY()

	FRigUnit_MathDoubleGreater()
	{
		A = B = 0.0;
		Result = false;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	double A;

	UPROPERTY(meta=(Input))
	double B;

	UPROPERTY(meta=(Output))
	bool Result;
};

/**
 * Returns true if the value A is less than B
 */
USTRUCT(meta=(DisplayName="Less", TemplateName="Less", Keywords="Smaller,<"))
struct CONTROLRIG_API FRigUnit_MathDoubleLess : public FRigUnit_MathDoubleBase
{
	GENERATED_BODY()
	
	FRigUnit_MathDoubleLess()
	{
		A = B = 0.0;
		Result = false;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	double A;

	UPROPERTY(meta=(Input))
	double B;

	UPROPERTY(meta=(Output))
	bool Result;
};

/**
 * Returns true if the value A is greater than or equal to B
 */
USTRUCT(meta=(DisplayName="Greater Equal", TemplateName="GreaterEqual", Keywords="Larger,Bigger,>="))
struct CONTROLRIG_API FRigUnit_MathDoubleGreaterEqual : public FRigUnit_MathDoubleBase
{
	GENERATED_BODY()

	FRigUnit_MathDoubleGreaterEqual()
	{
		A = B = 0.0;
		Result = true;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	double A;

	UPROPERTY(meta=(Input))
	double B;

	UPROPERTY(meta=(Output))
	bool Result;
};

/**
 * Returns true if the value A is less than or equal to B
 */
USTRUCT(meta=(DisplayName="Less Equal", TemplateName="LessEqual", Keywords="Smaller,<="))
struct CONTROLRIG_API FRigUnit_MathDoubleLessEqual : public FRigUnit_MathDoubleBase
{
	GENERATED_BODY()

	FRigUnit_MathDoubleLessEqual()
	{
		A = B = 0.0;
		Result = true;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	double A;

	UPROPERTY(meta=(Input))
	double B;

	UPROPERTY(meta=(Output))
	bool Result;
};

/**
 * Returns true if the value is nearly zero
 */
USTRUCT(meta=(DisplayName="Is Nearly Zero", TemplateName="IsNearlyZero", Keywords="AlmostZero,0"))
struct CONTROLRIG_API FRigUnit_MathDoubleIsNearlyZero : public FRigUnit_MathDoubleBase
{
	GENERATED_BODY()
	
	FRigUnit_MathDoubleIsNearlyZero()
	{
		Value = Tolerance = 0.0;
		Result = true;
	}
	
	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	double Value;

	UPROPERTY(meta=(Input))
	double Tolerance;

	UPROPERTY(meta=(Output))
	bool Result;
};

/**
 * Returns true if the value A is almost equal to B
 */
USTRUCT(meta=(DisplayName="Is Nearly Equal", TemplateName="IsNearlyEqual", Keywords="AlmostEqual,=="))
struct CONTROLRIG_API FRigUnit_MathDoubleIsNearlyEqual : public FRigUnit_MathDoubleBase
{
	GENERATED_BODY()

	FRigUnit_MathDoubleIsNearlyEqual()
	{
		A = B = Tolerance = 0.0;
		Result = true;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	double A;

	UPROPERTY(meta=(Input))
	double B;

	UPROPERTY(meta=(Input))
	double Tolerance;

	UPROPERTY(meta=(Output))
	bool Result;
};

/**
 * Returns the degrees of a given value in radians
 */
USTRUCT(meta=(DisplayName="Degrees", TemplateName="Degrees"))
struct CONTROLRIG_API FRigUnit_MathDoubleDeg : public FRigUnit_MathDoubleUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the radians of a given value in degrees
 */
USTRUCT(meta=(DisplayName="Radians", TemplateName="Radians"))
struct CONTROLRIG_API FRigUnit_MathDoubleRad : public FRigUnit_MathDoubleUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the sinus value of the given value (in radians)
 */
USTRUCT(meta=(DisplayName="Sin", TemplateName="Sin"))
struct CONTROLRIG_API FRigUnit_MathDoubleSin : public FRigUnit_MathDoubleUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the cosinus value of the given value (in radians)
 */
USTRUCT(meta=(DisplayName="Cos", TemplateName="Cos"))
struct CONTROLRIG_API FRigUnit_MathDoubleCos : public FRigUnit_MathDoubleUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the tangens value of the given value (in radians)
 */
USTRUCT(meta=(DisplayName="Tan", TemplateName="Tan"))
struct CONTROLRIG_API FRigUnit_MathDoubleTan : public FRigUnit_MathDoubleUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the inverse sinus value (in radians) of the given value
 */
USTRUCT(meta=(DisplayName="Asin", TemplateName="Asin", Keywords="Arcsin"))
struct CONTROLRIG_API FRigUnit_MathDoubleAsin : public FRigUnit_MathDoubleUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the inverse cosinus value (in radians) of the given value
 */
USTRUCT(meta=(DisplayName="Acos", TemplateName="Acos", Keywords="Arccos"))
struct CONTROLRIG_API FRigUnit_MathDoubleAcos : public FRigUnit_MathDoubleUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the inverse tangens value (in radians) of the given value
 */
USTRUCT(meta=(DisplayName="Atan", TemplateName="Atan", Keywords="Arctan"))
struct CONTROLRIG_API FRigUnit_MathDoubleAtan : public FRigUnit_MathDoubleUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Computes the angles alpha, beta and gamma (in radians) between the three sides A, B and C
 */
USTRUCT(meta = (DisplayName = "Law Of Cosine", TemplateName="LawOfCosine"))
struct CONTROLRIG_API FRigUnit_MathDoubleLawOfCosine : public FRigUnit_MathDoubleBase
{
	GENERATED_BODY()

	FRigUnit_MathDoubleLawOfCosine()
	{
		A = B = C = AlphaAngle = BetaAngle = GammaAngle = 0.0;
		bValid = false;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	double A;

	UPROPERTY(meta = (Input))
	double B;

	UPROPERTY(meta = (Input))
	double C;

	UPROPERTY(meta = (Output))
	double AlphaAngle;

	UPROPERTY(meta = (Output))
	double BetaAngle;

	UPROPERTY(meta = (Output))
	double GammaAngle;

	UPROPERTY(meta = (Output))
	bool bValid;
};

/**
 * Computes the base-e exponential of the given value 
 */
USTRUCT(meta = (DisplayName = "Exponential", TemplateName="Exponential"))
struct CONTROLRIG_API FRigUnit_MathDoubleExponential : public FRigUnit_MathDoubleUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};