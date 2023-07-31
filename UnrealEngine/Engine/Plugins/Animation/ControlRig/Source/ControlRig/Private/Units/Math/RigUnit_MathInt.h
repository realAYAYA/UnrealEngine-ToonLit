// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigUnit_MathBase.h"
#include "RigUnit_MathInt.generated.h"

USTRUCT(meta=(Abstract, Category="Math|Int", MenuDescSuffix="(Int)"))
struct CONTROLRIG_API FRigUnit_MathIntBase : public FRigUnit_MathBase
{
	GENERATED_BODY()
};

USTRUCT(meta=(Abstract))
struct CONTROLRIG_API FRigUnit_MathIntUnaryOp : public FRigUnit_MathIntBase
{
	GENERATED_BODY()

	FRigUnit_MathIntUnaryOp()
	{
		Value = Result = 0;
	}

	UPROPERTY(meta=(Input))
	int32 Value;

	UPROPERTY(meta=(Output))
	int32 Result;
};

USTRUCT(meta=(Abstract))
struct CONTROLRIG_API FRigUnit_MathIntBinaryOp : public FRigUnit_MathIntBase
{
	GENERATED_BODY()

	FRigUnit_MathIntBinaryOp()
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
struct CONTROLRIG_API FRigUnit_MathIntBinaryAggregateOp : public FRigUnit_MathIntBase
{
	GENERATED_BODY()

	FRigUnit_MathIntBinaryAggregateOp()
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
 * Returns the sum of the two values
 */
USTRUCT(meta=(DisplayName="Add", TemplateName="Add", Keywords="Sum,+"))
struct CONTROLRIG_API FRigUnit_MathIntAdd : public FRigUnit_MathIntBinaryAggregateOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the difference of the two values
 */
USTRUCT(meta=(DisplayName="Subtract", TemplateName="Subtract", Keywords="-"))
struct CONTROLRIG_API FRigUnit_MathIntSub : public FRigUnit_MathIntBinaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the product of the two values
 */
USTRUCT(meta=(DisplayName="Multiply", TemplateName="Multiply", Keywords="Product,*"))
struct CONTROLRIG_API FRigUnit_MathIntMul : public FRigUnit_MathIntBinaryAggregateOp
{
	GENERATED_BODY()

	FRigUnit_MathIntMul()
	{
		A = 1;
		B = 1;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the division of the two values
 */
USTRUCT(meta=(DisplayName="Divide", TemplateName="Divide", Keywords="Division,Divisor,/"))
struct CONTROLRIG_API FRigUnit_MathIntDiv : public FRigUnit_MathIntBinaryOp
{
	GENERATED_BODY()

	FRigUnit_MathIntDiv()
	{
		B = 1;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the modulo of the two values
 */
USTRUCT(meta=(DisplayName="Modulo", TemplateName="Modulo", Keywords="%,fmod"))
struct CONTROLRIG_API FRigUnit_MathIntMod : public FRigUnit_MathIntBinaryOp
{
	GENERATED_BODY()

	FRigUnit_MathIntMod()
	{
		A = 0;
		B = 1;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the smaller of the two values
 */
USTRUCT(meta=(DisplayName="Minimum", TemplateName="Minimum"))
struct CONTROLRIG_API FRigUnit_MathIntMin : public FRigUnit_MathIntBinaryAggregateOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the larger of the two values
 */
USTRUCT(meta=(DisplayName="Maximum", TemplateName="Maximum"))
struct CONTROLRIG_API FRigUnit_MathIntMax : public FRigUnit_MathIntBinaryAggregateOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the value of A raised to the power of B.
 */
USTRUCT(meta=(DisplayName="Power", TemplateName="Power"))
struct CONTROLRIG_API FRigUnit_MathIntPow : public FRigUnit_MathIntBinaryOp
{
	GENERATED_BODY()

	FRigUnit_MathIntPow()
	{
		A = 1;
		B = 1;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the negative value
 */
USTRUCT(meta=(DisplayName="Negate", TemplateName="Negate", Keywords="-,Abs"))
struct CONTROLRIG_API FRigUnit_MathIntNegate : public FRigUnit_MathIntUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the absolute (positive) value
 */
USTRUCT(meta=(DisplayName="Absolute", TemplateName="Absolute", Keywords="Abs,Neg"))
struct CONTROLRIG_API FRigUnit_MathIntAbs : public FRigUnit_MathIntUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns the int cast to a float
 */
USTRUCT(meta=(DisplayName="To Float", TemplateName="Cast"))
struct CONTROLRIG_API FRigUnit_MathIntToFloat : public FRigUnit_MathIntBase
{
	GENERATED_BODY()

	FRigUnit_MathIntToFloat()
	{
		Value = 0;
		Result = 0.f;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	int32 Value;

	UPROPERTY(meta=(Output))
	float Result;
};

/**
 * Returns the sign of the value (+1 for >= 0, -1 for < 0)
 */
USTRUCT(meta=(DisplayName="Sign", TemplateName="Sign"))
struct CONTROLRIG_API FRigUnit_MathIntSign : public FRigUnit_MathIntUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Clamps the given value within the range provided by minimum and maximum
 */
USTRUCT(meta=(DisplayName="Clamp", TemplateName="Clamp", Keywords="Range,Remap"))
struct CONTROLRIG_API FRigUnit_MathIntClamp : public FRigUnit_MathIntBase
{
	GENERATED_BODY()

	FRigUnit_MathIntClamp()
	{
		Value = Minimum = Maximum = Result = 0;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

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
struct CONTROLRIG_API FRigUnit_MathIntEquals : public FRigUnit_MathIntBase
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	FRigUnit_MathIntEquals()
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
struct CONTROLRIG_API FRigUnit_MathIntNotEquals : public FRigUnit_MathIntBase
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	FRigUnit_MathIntNotEquals()
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
struct CONTROLRIG_API FRigUnit_MathIntGreater : public FRigUnit_MathIntBase
{
	GENERATED_BODY()

	FRigUnit_MathIntGreater()
	{
		A = B = 0;
		Result = false;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

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
struct CONTROLRIG_API FRigUnit_MathIntLess : public FRigUnit_MathIntBase
{
	GENERATED_BODY()
	
	FRigUnit_MathIntLess()
	{
		A = B = 0;
		Result = false;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

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
struct CONTROLRIG_API FRigUnit_MathIntGreaterEqual : public FRigUnit_MathIntBase
{
	GENERATED_BODY()

	FRigUnit_MathIntGreaterEqual()
	{
		A = B = 0;
		Result = true;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

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
struct CONTROLRIG_API FRigUnit_MathIntLessEqual : public FRigUnit_MathIntBase
{
	GENERATED_BODY()

	FRigUnit_MathIntLessEqual()
	{
		A = B = 0;
		Result = true;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	UPROPERTY(meta=(Input))
	int32 A;

	UPROPERTY(meta=(Input))
	int32 B;

	UPROPERTY(meta=(Output))
	bool Result;
};
