// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMFunction_MathBase.h"
#include "RigVMFunction_MathDouble.generated.h"

USTRUCT(meta=(Abstract, Category="Math|Double", MenuDescSuffix="(Double)"))
struct RIGVM_API FRigVMFunction_MathDoubleBase : public FRigVMFunction_MathBase
{
	GENERATED_BODY()
};

USTRUCT(meta=(Abstract))
struct RIGVM_API FRigVMFunction_MathDoubleConstant : public FRigVMFunction_MathDoubleBase
{
	GENERATED_BODY()

	FRigVMFunction_MathDoubleConstant()
	{
		Value = 0.0;
	}

	UPROPERTY(meta=(Output))
	double Value;
};

USTRUCT(meta=(Abstract))
struct RIGVM_API FRigVMFunction_MathDoubleUnaryOp : public FRigVMFunction_MathDoubleBase
{
	GENERATED_BODY()

	FRigVMFunction_MathDoubleUnaryOp()
	{
		Value = Result = 0.0;
	}

	UPROPERTY(meta=(Input))
	double Value;

	UPROPERTY(meta=(Output))
	double Result;
};

USTRUCT(meta=(Abstract))
struct RIGVM_API FRigVMFunction_MathDoubleBinaryOp : public FRigVMFunction_MathDoubleBase
{
	GENERATED_BODY()

	FRigVMFunction_MathDoubleBinaryOp()
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
struct RIGVM_API FRigVMFunction_MathDoubleBinaryAggregateOp : public FRigVMFunction_MathDoubleBase
{
	GENERATED_BODY()

	FRigVMFunction_MathDoubleBinaryAggregateOp()
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
 * A double constant
 */
USTRUCT(meta=(DisplayName="Double", Keywords="Make,Construct,Constant"))
struct RIGVM_API FRigVMFunction_MathDoubleMake : public FRigVMFunction_MathDoubleBase
{
	GENERATED_BODY()

	FRigVMFunction_MathDoubleMake()
	{
		Value = 0.0;
	}
	
	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input, Output))
	double Value;
};

/**
 * Returns PI
 */
USTRUCT(meta=(DisplayName="Pi", TemplateName="Pi"))
struct RIGVM_API FRigVMFunction_MathDoubleConstPi : public FRigVMFunction_MathDoubleConstant
{
	GENERATED_BODY()
	
	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns PI * 0.5
 */
USTRUCT(meta=(DisplayName="Half Pi", TemplateName="HalfPi"))
struct RIGVM_API FRigVMFunction_MathDoubleConstHalfPi : public FRigVMFunction_MathDoubleConstant
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns PI * 2.0
 */
USTRUCT(meta=(DisplayName="Two Pi", TemplateName="TwoPi", Keywords="Tau"))
struct RIGVM_API FRigVMFunction_MathDoubleConstTwoPi : public FRigVMFunction_MathDoubleConstant
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns E
 */
USTRUCT(meta=(DisplayName="E", TemplateName="E", Keywords="Euler"))
struct RIGVM_API FRigVMFunction_MathDoubleConstE : public FRigVMFunction_MathDoubleConstant
{
	GENERATED_BODY()
	
	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the sum of the two values
 */
USTRUCT(meta=(DisplayName="Add", TemplateName="Add", Keywords="Sum,+"))
struct RIGVM_API FRigVMFunction_MathDoubleAdd : public FRigVMFunction_MathDoubleBinaryAggregateOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the difference of the two values
 */
USTRUCT(meta=(DisplayName="Subtract", TemplateName="Subtract", Keywords="-"))
struct RIGVM_API FRigVMFunction_MathDoubleSub : public FRigVMFunction_MathDoubleBinaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the product of the two values
 */
USTRUCT(meta=(DisplayName="Multiply", TemplateName="Multiply", Keywords="Product,*"))
struct RIGVM_API FRigVMFunction_MathDoubleMul : public FRigVMFunction_MathDoubleBinaryAggregateOp
{
	GENERATED_BODY()

	FRigVMFunction_MathDoubleMul()
	{
		A = 1.0;
		B = 1.0;
	}

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the division of the two values
 */
USTRUCT(meta=(DisplayName="Divide", TemplateName="Divide", Keywords="Division,Divisor,/"))
struct RIGVM_API FRigVMFunction_MathDoubleDiv : public FRigVMFunction_MathDoubleBinaryOp
{
	GENERATED_BODY()

	FRigVMFunction_MathDoubleDiv()
	{
		B = 1.0;
	}

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the modulo of the two values
 */
USTRUCT(meta=(DisplayName="Modulo", TemplateName="Modulo", Keywords="%,fmod"))
struct RIGVM_API FRigVMFunction_MathDoubleMod : public FRigVMFunction_MathDoubleBinaryOp
{
	GENERATED_BODY()

	FRigVMFunction_MathDoubleMod()
	{
		A = 0.0;
		B = 1.0;
	}

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the smaller of the two values
 */
USTRUCT(meta=(DisplayName="Minimum", TemplateName="Minimum"))
struct RIGVM_API FRigVMFunction_MathDoubleMin : public FRigVMFunction_MathDoubleBinaryAggregateOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the larger of the two values
 */
USTRUCT(meta=(DisplayName="Maximum", TemplateName="Maximum"))
struct RIGVM_API FRigVMFunction_MathDoubleMax : public FRigVMFunction_MathDoubleBinaryAggregateOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the value of A raised to the power of B.
 */
USTRUCT(meta=(DisplayName="Power", TemplateName="Power"))
struct RIGVM_API FRigVMFunction_MathDoublePow : public FRigVMFunction_MathDoubleBinaryOp
{
	GENERATED_BODY()

	FRigVMFunction_MathDoublePow()
	{
		A = 1.0;
		B = 1.0;
	}

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the square root of the given value
 */
USTRUCT(meta=(DisplayName="Sqrt", TemplateName="Sqrt", Keywords="Root,Square"))
struct RIGVM_API FRigVMFunction_MathDoubleSqrt : public FRigVMFunction_MathDoubleUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the negative value
 */
USTRUCT(meta=(DisplayName="Negate", TemplateName="Negate", Keywords="-,Abs"))
struct RIGVM_API FRigVMFunction_MathDoubleNegate : public FRigVMFunction_MathDoubleUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the absolute (positive) value
 */
USTRUCT(meta=(DisplayName="Absolute", TemplateName="Absolute", Keywords="Abs,Neg"))
struct RIGVM_API FRigVMFunction_MathDoubleAbs : public FRigVMFunction_MathDoubleUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the closest lower full number (integer) of the value
 */
USTRUCT(meta=(DisplayName="Floor", TemplateName="Floor", Keywords="Round"))
struct RIGVM_API FRigVMFunction_MathDoubleFloor : public FRigVMFunction_MathDoubleBase
{
	GENERATED_BODY()

	FRigVMFunction_MathDoubleFloor()
	{
		Value = Result = 0.0;
		Int = 0;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

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
struct RIGVM_API FRigVMFunction_MathDoubleCeil : public FRigVMFunction_MathDoubleBase
{
	GENERATED_BODY()

	FRigVMFunction_MathDoubleCeil()
	{
		Value = Result = 0.0;
		Int = 0;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

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
struct RIGVM_API FRigVMFunction_MathDoubleRound : public FRigVMFunction_MathDoubleBase
{
	GENERATED_BODY()

	FRigVMFunction_MathDoubleRound()
	{
		Value = Result = 0.0;
		Int = 0;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

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
USTRUCT(meta=(DisplayName="To Int", TemplateName="Cast", ExecuteContext="FRigVMExecuteContext"))
struct RIGVM_API FRigVMFunction_MathDoubleToInt : public FRigVMFunction_MathDoubleBase
{
	GENERATED_BODY()

	FRigVMFunction_MathDoubleToInt()
	{
		Value = 0.0;
		Result = 0;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	double Value;

	UPROPERTY(meta=(Output))
	int32 Result;
};

/**
 * Returns the sign of the value (+1 for >= 0.0, -1 for < 0.0)
 */
USTRUCT(meta=(DisplayName="Sign", TemplateName="Sign"))
struct RIGVM_API FRigVMFunction_MathDoubleSign : public FRigVMFunction_MathDoubleUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Clamps the given value within the range provided by minimum and maximum
 */
USTRUCT(meta=(DisplayName="Clamp", TemplateName="Clamp", Keywords="Range,Remap"))
struct RIGVM_API FRigVMFunction_MathDoubleClamp : public FRigVMFunction_MathDoubleBase
{
	GENERATED_BODY()

	FRigVMFunction_MathDoubleClamp()
	{
		Value = Minimum = Maximum = Result = 0.0;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

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
struct RIGVM_API FRigVMFunction_MathDoubleLerp : public FRigVMFunction_MathDoubleBase
{
	GENERATED_BODY()

	FRigVMFunction_MathDoubleLerp()
	{
		A = B = T = Result = 0.0;
		B = 1.0;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

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
struct RIGVM_API FRigVMFunction_MathDoubleRemap : public FRigVMFunction_MathDoubleBase
{
	GENERATED_BODY()

	FRigVMFunction_MathDoubleRemap()
	{
		Value = SourceMinimum = TargetMinimum = Result = 0.0;
		SourceMaximum = TargetMaximum = 1.0;
		bClamp = false;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

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
struct RIGVM_API FRigVMFunction_MathDoubleEquals : public FRigVMFunction_MathDoubleBase
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute() override;

	FRigVMFunction_MathDoubleEquals()
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
struct RIGVM_API FRigVMFunction_MathDoubleNotEquals : public FRigVMFunction_MathDoubleBase
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute() override;

	FRigVMFunction_MathDoubleNotEquals()
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
struct RIGVM_API FRigVMFunction_MathDoubleGreater : public FRigVMFunction_MathDoubleBase
{
	GENERATED_BODY()

	FRigVMFunction_MathDoubleGreater()
	{
		A = B = 0.0;
		Result = false;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

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
struct RIGVM_API FRigVMFunction_MathDoubleLess : public FRigVMFunction_MathDoubleBase
{
	GENERATED_BODY()
	
	FRigVMFunction_MathDoubleLess()
	{
		A = B = 0.0;
		Result = false;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

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
struct RIGVM_API FRigVMFunction_MathDoubleGreaterEqual : public FRigVMFunction_MathDoubleBase
{
	GENERATED_BODY()

	FRigVMFunction_MathDoubleGreaterEqual()
	{
		A = B = 0.0;
		Result = true;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

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
struct RIGVM_API FRigVMFunction_MathDoubleLessEqual : public FRigVMFunction_MathDoubleBase
{
	GENERATED_BODY()

	FRigVMFunction_MathDoubleLessEqual()
	{
		A = B = 0.0;
		Result = true;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

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
struct RIGVM_API FRigVMFunction_MathDoubleIsNearlyZero : public FRigVMFunction_MathDoubleBase
{
	GENERATED_BODY()
	
	FRigVMFunction_MathDoubleIsNearlyZero()
	{
		Value = Tolerance = 0.0;
		Result = true;
	}
	
	RIGVM_METHOD()
	virtual void Execute() override;

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
struct RIGVM_API FRigVMFunction_MathDoubleIsNearlyEqual : public FRigVMFunction_MathDoubleBase
{
	GENERATED_BODY()

	FRigVMFunction_MathDoubleIsNearlyEqual()
	{
		A = B = Tolerance = 0.0;
		Result = true;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

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
struct RIGVM_API FRigVMFunction_MathDoubleDeg : public FRigVMFunction_MathDoubleUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the radians of a given value in degrees
 */
USTRUCT(meta=(DisplayName="Radians", TemplateName="Radians"))
struct RIGVM_API FRigVMFunction_MathDoubleRad : public FRigVMFunction_MathDoubleUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the sinus value of the given value (in radians)
 */
USTRUCT(meta=(DisplayName="Sin", TemplateName="Sin"))
struct RIGVM_API FRigVMFunction_MathDoubleSin : public FRigVMFunction_MathDoubleUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the cosinus value of the given value (in radians)
 */
USTRUCT(meta=(DisplayName="Cos", TemplateName="Cos"))
struct RIGVM_API FRigVMFunction_MathDoubleCos : public FRigVMFunction_MathDoubleUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the tangens value of the given value (in radians)
 */
USTRUCT(meta=(DisplayName="Tan", TemplateName="Tan"))
struct RIGVM_API FRigVMFunction_MathDoubleTan : public FRigVMFunction_MathDoubleUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the inverse sinus value (in radians) of the given value
 */
USTRUCT(meta=(DisplayName="Asin", TemplateName="Asin", Keywords="Arcsin"))
struct RIGVM_API FRigVMFunction_MathDoubleAsin : public FRigVMFunction_MathDoubleUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the inverse cosinus value (in radians) of the given value
 */
USTRUCT(meta=(DisplayName="Acos", TemplateName="Acos", Keywords="Arccos"))
struct RIGVM_API FRigVMFunction_MathDoubleAcos : public FRigVMFunction_MathDoubleUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the inverse tangens value (in radians) of the given value
 */
USTRUCT(meta=(DisplayName="Atan", TemplateName="Atan", Keywords="Arctan"))
struct RIGVM_API FRigVMFunction_MathDoubleAtan : public FRigVMFunction_MathDoubleUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the arctangent of the specified A and B coordinates.
 */
USTRUCT(meta=(DisplayName="Atan2", TemplateName="Atan2", Keywords="Arctan"))
struct RIGVM_API FRigVMFunction_MathDoubleAtan2 : public FRigVMFunction_MathDoubleBinaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Computes the angles alpha, beta and gamma (in radians) between the three sides A, B and C
 */
USTRUCT(meta = (DisplayName = "Law Of Cosine", TemplateName="LawOfCosine"))
struct RIGVM_API FRigVMFunction_MathDoubleLawOfCosine : public FRigVMFunction_MathDoubleBase
{
	GENERATED_BODY()

	FRigVMFunction_MathDoubleLawOfCosine()
	{
		A = B = C = AlphaAngle = BetaAngle = GammaAngle = 0.0;
		bValid = false;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

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
struct RIGVM_API FRigVMFunction_MathDoubleExponential : public FRigVMFunction_MathDoubleUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the sum of the given array
 */
USTRUCT(meta = (DisplayName = "Array Sum", TemplateName = "ArraySum"))
struct RIGVM_API FRigVMFunction_MathDoubleArraySum : public FRigVMFunction_MathDoubleBase
{
	GENERATED_BODY()

	FRigVMFunction_MathDoubleArraySum()
	{
		Sum = 0.0;
	}

	/** Execute logic for this rig unit */
	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	TArray<double> Array;

	UPROPERTY(meta = (Output))
	double Sum;
};

/**
 * Returns the average of the given array
 */
USTRUCT(meta = (DisplayName = "Array Average", TemplateName = "ArrayAverage"))
struct RIGVM_API FRigVMFunction_MathDoubleArrayAverage : public FRigVMFunction_MathDoubleBase
{
	GENERATED_BODY()

	FRigVMFunction_MathDoubleArrayAverage()
	{
		Average = 0.0;
	}

	/** Execute logic for this rig unit */
	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	TArray<double> Array;

	UPROPERTY(meta = (Output))
	double Average;
};