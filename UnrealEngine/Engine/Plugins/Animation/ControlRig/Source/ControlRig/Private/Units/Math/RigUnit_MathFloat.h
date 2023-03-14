// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigUnit_MathBase.h"
#include "RigUnit_MathFloat.generated.h"

USTRUCT(meta=(Abstract, Category="Math|Float", MenuDescSuffix="(Float)"))
struct CONTROLRIG_API FRigUnit_MathFloatBase : public FRigUnit_MathBase
{
	GENERATED_BODY()
};

USTRUCT(meta=(Abstract))
struct CONTROLRIG_API FRigUnit_MathFloatConstant : public FRigUnit_MathFloatBase
{
	GENERATED_BODY()

	FRigUnit_MathFloatConstant()
	{
		Value = 0.f;
	}

	UPROPERTY(meta=(Output))
	float Value;
};

USTRUCT(meta=(Abstract))
struct CONTROLRIG_API FRigUnit_MathFloatUnaryOp : public FRigUnit_MathFloatBase
{
	GENERATED_BODY()

	FRigUnit_MathFloatUnaryOp()
	{
		Value = Result = 0.f;
	}

	UPROPERTY(meta=(Input))
	float Value;

	UPROPERTY(meta=(Output))
	float Result;
};

USTRUCT(meta=(Abstract))
struct CONTROLRIG_API FRigUnit_MathFloatBinaryOp : public FRigUnit_MathFloatBase
{
	GENERATED_BODY()

	FRigUnit_MathFloatBinaryOp()
	{
		A = B = Result = 0.f;
	}

	UPROPERTY(meta=(Input))
	float A;

	UPROPERTY(meta=(Input))
	float B;

	UPROPERTY(meta=(Output))
	float Result;
};

USTRUCT(meta=(Abstract))
struct CONTROLRIG_API FRigUnit_MathFloatBinaryAggregateOp : public FRigUnit_MathFloatBase
{
	GENERATED_BODY()

	FRigUnit_MathFloatBinaryAggregateOp()
	{
		A = B = Result = 0.f;
	}

	UPROPERTY(meta=(Input, Aggregate))
	float A;

	UPROPERTY(meta=(Input, Aggregate))
	float B;

	UPROPERTY(meta=(Output, Aggregate))
	float Result;
};

/**
 * Returns PI
 */
USTRUCT(meta=(DisplayName="Pi", TemplateName="Pi"))
struct CONTROLRIG_API FRigUnit_MathFloatConstPi : public FRigUnit_MathFloatConstant
{
	GENERATED_BODY()
	
	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns PI * 0.5
 */
USTRUCT(meta=(DisplayName="Half Pi", TemplateName="HalfPi"))
struct CONTROLRIG_API FRigUnit_MathFloatConstHalfPi : public FRigUnit_MathFloatConstant
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns PI * 2.0
 */
USTRUCT(meta=(DisplayName="Two Pi", TemplateName="TwoPi", Keywords="Tau"))
struct CONTROLRIG_API FRigUnit_MathFloatConstTwoPi : public FRigUnit_MathFloatConstant
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns E
 */
USTRUCT(meta=(DisplayName="E", TemplateName="E", Keywords="Euler"))
struct CONTROLRIG_API FRigUnit_MathFloatConstE : public FRigUnit_MathFloatConstant
{
	GENERATED_BODY()
	
	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the sum of the two values
 */
USTRUCT(meta=(DisplayName="Add", TemplateName="Add", Keywords="Sum,+"))
struct CONTROLRIG_API FRigUnit_MathFloatAdd : public FRigUnit_MathFloatBinaryAggregateOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the difference of the two values
 */
USTRUCT(meta=(DisplayName="Subtract", TemplateName="Subtract", Keywords="-"))
struct CONTROLRIG_API FRigUnit_MathFloatSub : public FRigUnit_MathFloatBinaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the product of the two values
 */
USTRUCT(meta=(DisplayName="Multiply", TemplateName="Multiply", Keywords="Product,*"))
struct CONTROLRIG_API FRigUnit_MathFloatMul : public FRigUnit_MathFloatBinaryAggregateOp
{
	GENERATED_BODY()

	FRigUnit_MathFloatMul()
	{
		A = 1.f;
		B = 1.f;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the division of the two values
 */
USTRUCT(meta=(DisplayName="Divide", TemplateName="Divide", Keywords="Division,Divisor,/"))
struct CONTROLRIG_API FRigUnit_MathFloatDiv : public FRigUnit_MathFloatBinaryOp
{
	GENERATED_BODY()

	FRigUnit_MathFloatDiv()
	{
		B = 1.f;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the modulo of the two values
 */
USTRUCT(meta=(DisplayName="Modulo", TemplateName="Modulo", Keywords="%,fmod"))
struct CONTROLRIG_API FRigUnit_MathFloatMod : public FRigUnit_MathFloatBinaryOp
{
	GENERATED_BODY()

	FRigUnit_MathFloatMod()
	{
		A = 0.f;
		B = 1.f;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the smaller of the two values
 */
USTRUCT(meta=(DisplayName="Minimum", TemplateName="Minimum"))
struct CONTROLRIG_API FRigUnit_MathFloatMin : public FRigUnit_MathFloatBinaryAggregateOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the larger of the two values
 */
USTRUCT(meta=(DisplayName="Maximum", TemplateName="Maximum"))
struct CONTROLRIG_API FRigUnit_MathFloatMax : public FRigUnit_MathFloatBinaryAggregateOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the value of A raised to the power of B.
 */
USTRUCT(meta=(DisplayName="Power", TemplateName="Power"))
struct CONTROLRIG_API FRigUnit_MathFloatPow : public FRigUnit_MathFloatBinaryOp
{
	GENERATED_BODY()

	FRigUnit_MathFloatPow()
	{
		A = 1.f;
		B = 1.f;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the square root of the given value
 */
USTRUCT(meta=(DisplayName="Sqrt", TemplateName="Sqrt", Keywords="Root,Square"))
struct CONTROLRIG_API FRigUnit_MathFloatSqrt : public FRigUnit_MathFloatUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the negative value
 */
USTRUCT(meta=(DisplayName="Negate", TemplateName="Negate", Keywords="-,Abs"))
struct CONTROLRIG_API FRigUnit_MathFloatNegate : public FRigUnit_MathFloatUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the absolute (positive) value
 */
USTRUCT(meta=(DisplayName="Absolute", TemplateName="Absolute", Keywords="Abs,Neg"))
struct CONTROLRIG_API FRigUnit_MathFloatAbs : public FRigUnit_MathFloatUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the closest lower full number (integer) of the value
 */
USTRUCT(meta=(DisplayName="Floor", TemplateName="Floor", Keywords="Round"))
struct CONTROLRIG_API FRigUnit_MathFloatFloor : public FRigUnit_MathFloatBase
{
	GENERATED_BODY()

	FRigUnit_MathFloatFloor()
	{
		Value = Result = 0.f;
		Int = 0;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	float Value;

	UPROPERTY(meta=(Output))
	float Result;

	UPROPERTY(meta=(Output))
	int32 Int;
};

/**
 * Returns the closest higher full number (integer) of the value
 */
USTRUCT(meta=(DisplayName="Ceiling", TemplateName="Ceiling", Keywords="Round"))
struct CONTROLRIG_API FRigUnit_MathFloatCeil : public FRigUnit_MathFloatBase
{
	GENERATED_BODY()

	FRigUnit_MathFloatCeil()
	{
		Value = Result = 0.f;
		Int = 0;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	float Value;

	UPROPERTY(meta=(Output))
	float Result;

	UPROPERTY(meta=(Output))
	int32 Int;
};

/**
 * Returns the closest higher full number (integer) of the value
 */
USTRUCT(meta=(DisplayName="Round", TemplateName="Round"))
struct CONTROLRIG_API FRigUnit_MathFloatRound : public FRigUnit_MathFloatBase
{
	GENERATED_BODY()

	FRigUnit_MathFloatRound()
	{
		Value = Result = 0.f;
		Int = 0;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	float Value;

	UPROPERTY(meta=(Output))
	float Result;

	UPROPERTY(meta=(Output))
	int32 Int;
};

/**
 * Returns the float cast to an int (this uses floor)
 */
USTRUCT(meta=(DisplayName="To Int", TemplateName="Cast"))
struct CONTROLRIG_API FRigUnit_MathFloatToInt : public FRigUnit_MathFloatBase
{
	GENERATED_BODY()

	FRigUnit_MathFloatToInt()
	{
		Value = 0.f;
		Result = 0;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	float Value;

	UPROPERTY(meta=(Output))
	int32 Result;
};

/**
 * Returns the sign of the value (+1 for >= 0.f, -1 for < 0.f)
 */
USTRUCT(meta=(DisplayName="Sign", TemplateName="Sign"))
struct CONTROLRIG_API FRigUnit_MathFloatSign : public FRigUnit_MathFloatUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Clamps the given value within the range provided by minimum and maximum
 */
USTRUCT(meta=(DisplayName="Clamp", TemplateName="Clamp", Keywords="Range,Remap"))
struct CONTROLRIG_API FRigUnit_MathFloatClamp : public FRigUnit_MathFloatBase
{
	GENERATED_BODY()

	FRigUnit_MathFloatClamp()
	{
		Value = Minimum = Maximum = Result = 0.f;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	float Value;

	UPROPERTY(meta=(Input))
	float Minimum;

	UPROPERTY(meta=(Input))
	float Maximum;

	UPROPERTY(meta=(Output))
	float Result;
};

/**
 * Linearly interpolates between A and B using the ratio T
 */
USTRUCT(meta=(DisplayName="Interpolate", TemplateName="Interpolate", Keywords="Lerp,Mix,Blend"))
struct CONTROLRIG_API FRigUnit_MathFloatLerp : public FRigUnit_MathFloatBase
{
	GENERATED_BODY()

	FRigUnit_MathFloatLerp()
	{
		A = B = T = Result = 0.f;
		B = 1.f;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	float A;

	UPROPERTY(meta=(Input))
	float B;

	UPROPERTY(meta=(Input))
	float T;

	UPROPERTY(meta=(Output))
	float Result;
};

/**
 * Remaps the given value from a source range to a target range.
 */
USTRUCT(meta=(DisplayName="Remap", TemplateName="Remap", Keywords="Rescale,Scale"))
struct CONTROLRIG_API FRigUnit_MathFloatRemap : public FRigUnit_MathFloatBase
{
	GENERATED_BODY()

	FRigUnit_MathFloatRemap()
	{
		Value = SourceMinimum = TargetMinimum = Result = 0.f;
		SourceMaximum = TargetMaximum = 1.f;
		bClamp = false;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	float Value;

	UPROPERTY(meta=(Input))
	float SourceMinimum;

	UPROPERTY(meta=(Input))
	float SourceMaximum;

	UPROPERTY(meta=(Input))
	float TargetMinimum;

	UPROPERTY(meta=(Input))
	float TargetMaximum;

	/** If set to true the result is clamped to the target range */
	UPROPERTY(meta=(Input))
	bool bClamp;

	UPROPERTY(meta=(Output))
	float Result;
};

/**
 * Returns true if the value A equals B
 */
USTRUCT(meta=(DisplayName="Equals", TemplateName="Equals", Keywords="Same,==", Deprecated="5.1"))
struct CONTROLRIG_API FRigUnit_MathFloatEquals : public FRigUnit_MathFloatBase
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	FRigUnit_MathFloatEquals()
	{
		A = B = 0.f;
		Result = true;
	}

	UPROPERTY(meta=(Input))
	float A;

	UPROPERTY(meta=(Input))
	float B;

	UPROPERTY(meta=(Output))
	bool Result;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
 * Returns true if the value A does not equal B
 */
USTRUCT(meta=(DisplayName="Not Equals", TemplateName="NotEquals", Keywords="Different,!=", Deprecated="5.1"))
struct CONTROLRIG_API FRigUnit_MathFloatNotEquals : public FRigUnit_MathFloatBase
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	FRigUnit_MathFloatNotEquals()
	{
		A = B = 0.f;
		Result = false;
	}

	UPROPERTY(meta=(Input))
	float A;

	UPROPERTY(meta=(Input))
	float B;

	UPROPERTY(meta=(Output))
	bool Result;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
 * Returns true if the value A is greater than B
 */
USTRUCT(meta=(DisplayName="Greater", TemplateName="Greater", Keywords="Larger,Bigger,>"))
struct CONTROLRIG_API FRigUnit_MathFloatGreater : public FRigUnit_MathFloatBase
{
	GENERATED_BODY()

	FRigUnit_MathFloatGreater()
	{
		A = B = 0.f;
		Result = false;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	float A;

	UPROPERTY(meta=(Input))
	float B;

	UPROPERTY(meta=(Output))
	bool Result;
};

/**
 * Returns true if the value A is less than B
 */
USTRUCT(meta=(DisplayName="Less", TemplateName="Less", Keywords="Smaller,<"))
struct CONTROLRIG_API FRigUnit_MathFloatLess : public FRigUnit_MathFloatBase
{
	GENERATED_BODY()
	
	FRigUnit_MathFloatLess()
	{
		A = B = 0.f;
		Result = false;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	float A;

	UPROPERTY(meta=(Input))
	float B;

	UPROPERTY(meta=(Output))
	bool Result;
};

/**
 * Returns true if the value A is greater than or equal to B
 */
USTRUCT(meta=(DisplayName="Greater Equal", TemplateName="GreaterEqual", Keywords="Larger,Bigger,>="))
struct CONTROLRIG_API FRigUnit_MathFloatGreaterEqual : public FRigUnit_MathFloatBase
{
	GENERATED_BODY()

	FRigUnit_MathFloatGreaterEqual()
	{
		A = B = 0.f;
		Result = true;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	float A;

	UPROPERTY(meta=(Input))
	float B;

	UPROPERTY(meta=(Output))
	bool Result;
};

/**
 * Returns true if the value A is less than or equal to B
 */
USTRUCT(meta=(DisplayName="Less Equal", TemplateName="LessEqual", Keywords="Smaller,<="))
struct CONTROLRIG_API FRigUnit_MathFloatLessEqual : public FRigUnit_MathFloatBase
{
	GENERATED_BODY()

	FRigUnit_MathFloatLessEqual()
	{
		A = B = 0.f;
		Result = true;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	float A;

	UPROPERTY(meta=(Input))
	float B;

	UPROPERTY(meta=(Output))
	bool Result;
};

/**
 * Returns true if the value is nearly zero
 */
USTRUCT(meta=(DisplayName="Is Nearly Zero", TemplateName="IsNearlyZero", Keywords="AlmostZero,0"))
struct CONTROLRIG_API FRigUnit_MathFloatIsNearlyZero : public FRigUnit_MathFloatBase
{
	GENERATED_BODY()
	
	FRigUnit_MathFloatIsNearlyZero()
	{
		Value = Tolerance = 0.f;
		Result = true;
	}
	
	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	float Value;

	UPROPERTY(meta=(Input))
	float Tolerance;

	UPROPERTY(meta=(Output))
	bool Result;
};

/**
 * Returns true if the value A is almost equal to B
 */
USTRUCT(meta=(DisplayName="Is Nearly Equal", TemplateName="IsNearlyEqual", Keywords="AlmostEqual,=="))
struct CONTROLRIG_API FRigUnit_MathFloatIsNearlyEqual : public FRigUnit_MathFloatBase
{
	GENERATED_BODY()

	FRigUnit_MathFloatIsNearlyEqual()
	{
		A = B = Tolerance = 0.f;
		Result = true;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	float A;

	UPROPERTY(meta=(Input))
	float B;

	UPROPERTY(meta=(Input))
	float Tolerance;

	UPROPERTY(meta=(Output))
	bool Result;
};

/**
 * Return one of the two values based on the condition
 */
USTRUCT(meta=(DisplayName="Select", TemplateName="Select", Keywords="Pick,If", Deprecated = "4.26.0"))
struct CONTROLRIG_API FRigUnit_MathFloatSelectBool : public FRigUnit_MathFloatBase
{
	GENERATED_BODY()

	FRigUnit_MathFloatSelectBool()
	{
		Condition = false;
		IfTrue = IfFalse = Result = 0.f;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	bool Condition;

	UPROPERTY(meta=(Input))
	float IfTrue;

	UPROPERTY(meta=(Input))
	float IfFalse;

	UPROPERTY(meta=(Output))
	float Result;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
 * Returns the degrees of a given value in radians
 */
USTRUCT(meta=(DisplayName="Degrees", TemplateName="Degrees"))
struct CONTROLRIG_API FRigUnit_MathFloatDeg : public FRigUnit_MathFloatUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the radians of a given value in degrees
 */
USTRUCT(meta=(DisplayName="Radians", TemplateName="Radians"))
struct CONTROLRIG_API FRigUnit_MathFloatRad : public FRigUnit_MathFloatUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the sinus value of the given value (in radians)
 */
USTRUCT(meta=(DisplayName="Sin", TemplateName="Sin"))
struct CONTROLRIG_API FRigUnit_MathFloatSin : public FRigUnit_MathFloatUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the cosinus value of the given value (in radians)
 */
USTRUCT(meta=(DisplayName="Cos", TemplateName="Cos"))
struct CONTROLRIG_API FRigUnit_MathFloatCos : public FRigUnit_MathFloatUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the tangens value of the given value (in radians)
 */
USTRUCT(meta=(DisplayName="Tan", TemplateName="Tan"))
struct CONTROLRIG_API FRigUnit_MathFloatTan : public FRigUnit_MathFloatUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the inverse sinus value (in radians) of the given value
 */
USTRUCT(meta=(DisplayName="Asin", TemplateName="Asin", Keywords="Arcsin"))
struct CONTROLRIG_API FRigUnit_MathFloatAsin : public FRigUnit_MathFloatUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the inverse cosinus value (in radians) of the given value
 */
USTRUCT(meta=(DisplayName="Acos", TemplateName="Acos", Keywords="Arccos"))
struct CONTROLRIG_API FRigUnit_MathFloatAcos : public FRigUnit_MathFloatUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the inverse tangens value (in radians) of the given value
 */
USTRUCT(meta=(DisplayName="Atan", TemplateName="Atan", Keywords="Arctan"))
struct CONTROLRIG_API FRigUnit_MathFloatAtan : public FRigUnit_MathFloatUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Computes the angles alpha, beta and gamma (in radians) between the three sides A, B and C
 */
USTRUCT(meta = (DisplayName = "Law Of Cosine", TemplateName="LawOfCosine"))
struct CONTROLRIG_API FRigUnit_MathFloatLawOfCosine : public FRigUnit_MathFloatBase
{
	GENERATED_BODY()

	FRigUnit_MathFloatLawOfCosine()
	{
		A = B = C = AlphaAngle = BetaAngle = GammaAngle = 0.f;
		bValid = false;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta = (Input))
	float A;

	UPROPERTY(meta = (Input))
	float B;

	UPROPERTY(meta = (Input))
	float C;

	UPROPERTY(meta = (Output))
	float AlphaAngle;

	UPROPERTY(meta = (Output))
	float BetaAngle;

	UPROPERTY(meta = (Output))
	float GammaAngle;

	UPROPERTY(meta = (Output))
	bool bValid;
};

/**
 * Computes the base-e exponential of the given value 
 */
USTRUCT(meta = (DisplayName = "Exponential", TemplateName="Exponential"))
struct CONTROLRIG_API FRigUnit_MathFloatExponential : public FRigUnit_MathFloatUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};