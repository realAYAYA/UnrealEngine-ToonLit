// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMFunction_MathBase.h"
#include "RigVMFunction_MathInt.generated.h"

USTRUCT(meta=(Abstract, Category="Math|Int", MenuDescSuffix="(Int)"))
struct RIGVM_API FRigVMFunction_MathIntBase : public FRigVMFunction_MathBase
{
	GENERATED_BODY()
};

USTRUCT(meta=(Abstract))
struct RIGVM_API FRigVMFunction_MathIntUnaryOp : public FRigVMFunction_MathIntBase
{
	GENERATED_BODY()

	FRigVMFunction_MathIntUnaryOp()
	{
		Value = Result = 0;
	}

	UPROPERTY(meta=(Input))
	int32 Value;

	UPROPERTY(meta=(Output))
	int32 Result;
};

USTRUCT(meta=(Abstract))
struct RIGVM_API FRigVMFunction_MathIntBinaryOp : public FRigVMFunction_MathIntBase
{
	GENERATED_BODY()

	FRigVMFunction_MathIntBinaryOp()
	{
		A = B = Result = 0;
	}

	UPROPERTY(meta=(Input))
	int32 A;

	UPROPERTY(meta=(Input))
	int32 B;

	UPROPERTY(meta=(Output))
	int32 Result;
};

USTRUCT(meta=(Abstract))
struct RIGVM_API FRigVMFunction_MathIntBinaryAggregateOp : public FRigVMFunction_MathIntBase
{
	GENERATED_BODY()

	FRigVMFunction_MathIntBinaryAggregateOp()
	{
		A = B = Result = 0;
	}

	UPROPERTY(meta=(Input, Aggregate))
	int32 A;

	UPROPERTY(meta=(Input, Aggregate))
	int32 B;

	UPROPERTY(meta=(Output, Aggregate))
	int32 Result;
};

/**
 * A integer constant
 */
USTRUCT(meta=(DisplayName="Integer", Keywords="Make,Construct,Constant"))
struct RIGVM_API FRigVMFunction_MathIntMake : public FRigVMFunction_MathIntBase
{
	GENERATED_BODY()

	FRigVMFunction_MathIntMake()
	{
		Value = 0;
	}
	
	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input, Output))
	int32 Value;
};

/**
 * Returns the sum of the two values
 */
USTRUCT(meta=(DisplayName="Add", TemplateName="Add", Keywords="Sum,+"))
struct RIGVM_API FRigVMFunction_MathIntAdd : public FRigVMFunction_MathIntBinaryAggregateOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the difference of the two values
 */
USTRUCT(meta=(DisplayName="Subtract", TemplateName="Subtract", Keywords="-"))
struct RIGVM_API FRigVMFunction_MathIntSub : public FRigVMFunction_MathIntBinaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the product of the two values
 */
USTRUCT(meta=(DisplayName="Multiply", TemplateName="Multiply", Keywords="Product,*"))
struct RIGVM_API FRigVMFunction_MathIntMul : public FRigVMFunction_MathIntBinaryAggregateOp
{
	GENERATED_BODY()

	FRigVMFunction_MathIntMul()
	{
		A = 1;
		B = 1;
	}

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the division of the two values
 */
USTRUCT(meta=(DisplayName="Divide", TemplateName="Divide", Keywords="Division,Divisor,/"))
struct RIGVM_API FRigVMFunction_MathIntDiv : public FRigVMFunction_MathIntBinaryOp
{
	GENERATED_BODY()

	FRigVMFunction_MathIntDiv()
	{
		B = 1;
	}

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the modulo of the two values
 */
USTRUCT(meta=(DisplayName="Modulo", TemplateName="Modulo", Keywords="%,fmod"))
struct RIGVM_API FRigVMFunction_MathIntMod : public FRigVMFunction_MathIntBinaryOp
{
	GENERATED_BODY()

	FRigVMFunction_MathIntMod()
	{
		A = 0;
		B = 1;
	}

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the smaller of the two values
 */
USTRUCT(meta=(DisplayName="Minimum", TemplateName="Minimum"))
struct RIGVM_API FRigVMFunction_MathIntMin : public FRigVMFunction_MathIntBinaryAggregateOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the larger of the two values
 */
USTRUCT(meta=(DisplayName="Maximum", TemplateName="Maximum"))
struct RIGVM_API FRigVMFunction_MathIntMax : public FRigVMFunction_MathIntBinaryAggregateOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the value of A raised to the power of B.
 */
USTRUCT(meta=(DisplayName="Power", TemplateName="Power"))
struct RIGVM_API FRigVMFunction_MathIntPow : public FRigVMFunction_MathIntBinaryOp
{
	GENERATED_BODY()

	FRigVMFunction_MathIntPow()
	{
		A = 1;
		B = 1;
	}

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the negative value
 */
USTRUCT(meta=(DisplayName="Negate", TemplateName="Negate", Keywords="-,Abs"))
struct RIGVM_API FRigVMFunction_MathIntNegate : public FRigVMFunction_MathIntUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the absolute (positive) value
 */
USTRUCT(meta=(DisplayName="Absolute", TemplateName="Absolute", Keywords="Abs,Neg"))
struct RIGVM_API FRigVMFunction_MathIntAbs : public FRigVMFunction_MathIntUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns the int cast to a float
 */
USTRUCT(meta=(DisplayName="To Float", TemplateName="Cast", ExecuteContext="FRigVMExecuteContext"))
struct RIGVM_API FRigVMFunction_MathIntToFloat : public FRigVMFunction_MathIntBase
{
	GENERATED_BODY()

	FRigVMFunction_MathIntToFloat()
	{
		Value = 0;
		Result = 0.f;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	int32 Value;

	UPROPERTY(meta=(Output))
	float Result;
};

/**
 * Returns the int cast to a float
 */
USTRUCT(meta=(DisplayName="To Double", TemplateName="Cast", ExecuteContext="FRigVMExecuteContext"))
struct RIGVM_API FRigVMFunction_MathIntToDouble : public FRigVMFunction_MathIntBase
{
	GENERATED_BODY()

	FRigVMFunction_MathIntToDouble()
	{
		Value = 0;
		Result = 0.0;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	int32 Value;

	UPROPERTY(meta=(Output))
	double Result;
};

/**
 * Returns the sign of the value (+1 for >= 0, -1 for < 0)
 */
USTRUCT(meta=(DisplayName="Sign", TemplateName="Sign"))
struct RIGVM_API FRigVMFunction_MathIntSign : public FRigVMFunction_MathIntUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Clamps the given value within the range provided by minimum and maximum
 */
USTRUCT(meta=(DisplayName="Clamp", TemplateName="Clamp", Keywords="Range,Remap"))
struct RIGVM_API FRigVMFunction_MathIntClamp : public FRigVMFunction_MathIntBase
{
	GENERATED_BODY()

	FRigVMFunction_MathIntClamp()
	{
		Value = Minimum = Maximum = Result = 0;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	int32 Value;

	UPROPERTY(meta=(Input))
	int32 Minimum;

	UPROPERTY(meta=(Input))
	int32 Maximum;

	UPROPERTY(meta=(Output))
	int32 Result;
};

/**
 * Returns true if the value A equals B
 */
USTRUCT(meta=(DisplayName="Equals", TemplateName="Equals", Keywords="Same,==", Deprecated="5.1"))
struct RIGVM_API FRigVMFunction_MathIntEquals : public FRigVMFunction_MathIntBase
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute() override;

	FRigVMFunction_MathIntEquals()
	{
		A = B = 0;
		Result = true;
	}

	UPROPERTY(meta=(Input))
	int32 A;

	UPROPERTY(meta=(Input))
	int32 B;

	UPROPERTY(meta=(Output))
	bool Result;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
 * Returns true if the value A does not equal B
 */
USTRUCT(meta=(DisplayName="Not Equals", TemplateName="NotEquals", Keywords="Different,!=", Deprecated="5.1"))
struct RIGVM_API FRigVMFunction_MathIntNotEquals : public FRigVMFunction_MathIntBase
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute() override;

	FRigVMFunction_MathIntNotEquals()
	{
		A = B = 0;
		Result = false;
	}

	UPROPERTY(meta=(Input))
	int32 A;

	UPROPERTY(meta=(Input))
	int32 B;

	UPROPERTY(meta=(Output))
	bool Result;
	
	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
 * Returns true if the value A is greater than B
 */
USTRUCT(meta=(DisplayName="Greater", TemplateName="Greater", Keywords="Larger,Bigger,>"))
struct RIGVM_API FRigVMFunction_MathIntGreater : public FRigVMFunction_MathIntBase
{
	GENERATED_BODY()

	FRigVMFunction_MathIntGreater()
	{
		A = B = 0;
		Result = false;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	int32 A;

	UPROPERTY(meta=(Input))
	int32 B;

	UPROPERTY(meta=(Output))
	bool Result;
};

/**
 * Returns true if the value A is less than B
 */
USTRUCT(meta=(DisplayName="Less", TemplateName="Less", Keywords="Smaller,<"))
struct RIGVM_API FRigVMFunction_MathIntLess : public FRigVMFunction_MathIntBase
{
	GENERATED_BODY()
	
	FRigVMFunction_MathIntLess()
	{
		A = B = 0;
		Result = false;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	int32 A;

	UPROPERTY(meta=(Input))
	int32 B;

	UPROPERTY(meta=(Output))
	bool Result;
};

/**
 * Returns true if the value A is greater than or equal to B
 */
USTRUCT(meta=(DisplayName="Greater Equal", TemplateName="GreaterEqual", Keywords="Larger,Bigger,>="))
struct RIGVM_API FRigVMFunction_MathIntGreaterEqual : public FRigVMFunction_MathIntBase
{
	GENERATED_BODY()

	FRigVMFunction_MathIntGreaterEqual()
	{
		A = B = 0;
		Result = true;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	int32 A;

	UPROPERTY(meta=(Input))
	int32 B;

	UPROPERTY(meta=(Output))
	bool Result;
};

/**
 * Returns true if the value A is less than or equal to B
 */
USTRUCT(meta=(DisplayName="Less Equal", TemplateName="LessEqual", Keywords="Smaller,<="))
struct RIGVM_API FRigVMFunction_MathIntLessEqual : public FRigVMFunction_MathIntBase
{
	GENERATED_BODY()

	FRigVMFunction_MathIntLessEqual()
	{
		A = B = 0;
		Result = true;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	int32 A;

	UPROPERTY(meta=(Input))
	int32 B;

	UPROPERTY(meta=(Output))
	bool Result;
};

/**
 * Returns the sum of the given array
 */
USTRUCT(meta = (DisplayName = "Array Sum", TemplateName = "ArraySum"))
struct RIGVM_API FRigVMFunction_MathIntArraySum : public FRigVMFunction_MathIntBase
{
	GENERATED_BODY()

	FRigVMFunction_MathIntArraySum()
	{
		Sum = 0;
	}

	/** Execute logic for this rig unit */
	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	TArray<int32> Array;

	UPROPERTY(meta = (Output))
	int32 Sum;
};


/**
 * Returns the average of the given array
 */
USTRUCT(meta = (DisplayName = "Array Average", TemplateName = "ArrayAverage"))
struct RIGVM_API FRigVMFunction_MathIntArrayAverage : public FRigVMFunction_MathIntBase
{
	GENERATED_BODY()

	FRigVMFunction_MathIntArrayAverage()
	{
		Average = 0;
	}

	/** Execute logic for this rig unit */
	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	TArray<int32> Array;

	UPROPERTY(meta = (Output))
	int32 Average;
};

/**
 * Converts an integer to a string
 */
USTRUCT(meta = (DisplayName = "Int to String", TemplateName="Int to Name"))
struct RIGVM_API FRigVMFunction_MathIntToString : public FRigVMFunction_MathIntBase
{
	GENERATED_BODY()

	FRigVMFunction_MathIntToString()
	{
		Number = PaddedSize = 0;
	}

	/** Execute logic for this function */
	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	int32 Number;

	/*
	 * For positive numbers you can pad the result to a required with
	 * so rather than '13' return '00013' for a padded size of 5.
	 */
	UPROPERTY(meta = (Input))
	int32 PaddedSize;

	UPROPERTY(meta = (Output))
	FString Result;
};

/**
 * Converts an integer to a name
 */
USTRUCT(meta = (DisplayName = "Int to Name", TemplateName="Int to Name"))
struct RIGVM_API FRigVMFunction_MathIntToName : public FRigVMFunction_MathIntBase
{
	GENERATED_BODY()

	FRigVMFunction_MathIntToName()
	{
		Number = PaddedSize = 0;
	}

	/** Execute logic for this function */
	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	int32 Number;

	/*
	 * For positive numbers you can pad the result to a required with
	 * so rather than '13' return '00013' for a padded size of 5.
	 */
	UPROPERTY(meta = (Input))
	int32 PaddedSize;

	UPROPERTY(meta = (Output))
	FName Result;
};