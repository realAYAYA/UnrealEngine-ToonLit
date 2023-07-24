// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMFunction_MathBase.h"
#include "RigVMFunction_MathBool.generated.h"

USTRUCT(meta=(Abstract, Category="Math|Boolean"))
struct RIGVM_API FRigVMFunction_MathBoolBase : public FRigVMFunction_MathBase
{
	GENERATED_BODY()
};

USTRUCT(meta=(Abstract))
struct RIGVM_API FRigVMFunction_MathBoolConstant : public FRigVMFunction_MathBoolBase
{
	GENERATED_BODY()

	FRigVMFunction_MathBoolConstant()
	{
		Value = false;
	}

	UPROPERTY(meta=(Output))
	bool Value;
};

USTRUCT(meta=(Abstract))
struct RIGVM_API FRigVMFunction_MathBoolUnaryOp : public FRigVMFunction_MathBoolBase
{
	GENERATED_BODY()

	FRigVMFunction_MathBoolUnaryOp()
	{
		Value = Result = 0.f;
	}

	UPROPERTY(meta=(Input))
	bool Value;

	UPROPERTY(meta=(Output))
	bool Result;
};

USTRUCT(meta=(Abstract))
struct RIGVM_API FRigVMFunction_MathBoolBinaryOp : public FRigVMFunction_MathBoolBase
{
	GENERATED_BODY()

	FRigVMFunction_MathBoolBinaryOp()
	{
		A = B = Result = 0.f;
	}

	UPROPERTY(meta=(Input))
	bool A;

	UPROPERTY(meta=(Input))
	bool B;

	UPROPERTY(meta=(Output))
	bool Result;
};

USTRUCT(meta=(Abstract))
struct RIGVM_API FRigVMFunction_MathBoolBinaryAggregateOp : public FRigVMFunction_MathBoolBase
{
	GENERATED_BODY()

	FRigVMFunction_MathBoolBinaryAggregateOp()
	{
		A = B = Result = 0.f;
	}

	UPROPERTY(meta=(Input, Aggregate))
	bool A;

	UPROPERTY(meta=(Input, Aggregate))
	bool B;

	UPROPERTY(meta=(Output, Aggregate))
	bool Result;
};

/**
 * A boolean constant
 */
USTRUCT(meta=(DisplayName="Bool", Keywords="Make,Construct,Constant"))
struct RIGVM_API FRigVMFunction_MathBoolMake : public FRigVMFunction_MathBoolBase
{
	GENERATED_BODY()

	FRigVMFunction_MathBoolMake()
	{
		Value = false;
	}
	
	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input, Output))
	bool Value;
};

/**
 * Returns true
 */
USTRUCT(meta=(DisplayName="True", Keywords="Yes"))
struct RIGVM_API FRigVMFunction_MathBoolConstTrue : public FRigVMFunction_MathBoolConstant
{
	GENERATED_BODY()
	
	FRigVMFunction_MathBoolConstTrue()
	{
		Value = true;
	}

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns false
 */
USTRUCT(meta=(DisplayName="False", Keywords="No"))
struct RIGVM_API FRigVMFunction_MathBoolConstFalse : public FRigVMFunction_MathBoolConstant
{
	GENERATED_BODY()
	
	FRigVMFunction_MathBoolConstFalse()
	{
		Value = false;
	}

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns true if the condition is false
 */
USTRUCT(meta=(DisplayName="Not", TemplateName="Not", Keywords="!"))
struct RIGVM_API FRigVMFunction_MathBoolNot : public FRigVMFunction_MathBoolUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns true if both conditions are true
 */
USTRUCT(meta=(DisplayName="And", TemplateName="And", Keywords="&&"))
struct RIGVM_API FRigVMFunction_MathBoolAnd : public FRigVMFunction_MathBoolBinaryAggregateOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns true if both conditions are false
 */
USTRUCT(meta=(DisplayName="Nand", Deprecated="5.1"))
struct RIGVM_API FRigVMFunction_MathBoolNand : public FRigVMFunction_MathBoolBinaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
 * Returns false if both conditions are true
 */
USTRUCT(meta=(DisplayName="Nand", TemplateName="Nand"))
struct RIGVM_API FRigVMFunction_MathBoolNand2 : public FRigVMFunction_MathBoolBinaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns true if one of the conditions is true
 */
USTRUCT(meta=(DisplayName="Or", TemplateName="Or", Keywords="||"))
struct RIGVM_API FRigVMFunction_MathBoolOr : public FRigVMFunction_MathBoolBinaryAggregateOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute() override;
};

/**
 * Returns true if the value A equals B
 */
USTRUCT(meta=(DisplayName="Equals", TemplateName="Equals", Keywords="Same,==", Deprecated="5.1"))
struct RIGVM_API FRigVMFunction_MathBoolEquals : public FRigVMFunction_MathBoolBase
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute() override;

	FRigVMFunction_MathBoolEquals()
	{
		A = B = false;
		Result = true;
	}

	UPROPERTY(meta=(Input))
	bool A;

	UPROPERTY(meta=(Input))
	bool B;

	UPROPERTY(meta=(Output))
	bool Result;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
 * Returns true if the value A does not equal B
 */
USTRUCT(meta=(DisplayName="Not Equals", TemplateName="NotEquals", Keywords="Different,!=,Xor", Deprecated="5.1"))
struct RIGVM_API FRigVMFunction_MathBoolNotEquals : public FRigVMFunction_MathBoolBase
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute() override;

	FRigVMFunction_MathBoolNotEquals()
	{
		A = B = false;
		Result = false;
	}

	UPROPERTY(meta=(Input))
	bool A;

	UPROPERTY(meta=(Input))
	bool B;

	UPROPERTY(meta=(Output))
	bool Result;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
* Returns true if the value has changed from the last run
*/
USTRUCT(meta=(DisplayName="Toggled", TemplateName="Toggled", Keywords="Changed,Different"))
struct RIGVM_API FRigVMFunction_MathBoolToggled : public FRigVMFunction_MathBoolBase
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute() override;

	FRigVMFunction_MathBoolToggled()
	{
		Value = Toggled = Initialized = LastValue = false;
	}

	UPROPERTY(meta=(Input))
	bool Value;

	UPROPERTY(meta=(Output))
	bool Toggled;

	UPROPERTY()
	bool Initialized;

	UPROPERTY()
	bool LastValue;
};

/**
 * Returns true and false as a sequence.
 */
USTRUCT(meta=(DisplayName="FlipFlop", Keywords="Toggle,Changed,Different", Varying))
struct RIGVM_API FRigVMFunction_MathBoolFlipFlop : public FRigVMFunction_MathBoolBase
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute() override;

	FRigVMFunction_MathBoolFlipFlop()
	{
		Duration = TimeLeft = 0.f;
		StartValue = false;
		Result = LastValue = !StartValue;
	}

	// The initial value to use for the flag
	UPROPERTY(meta=(Visible))
	bool StartValue;

	/**
	 * The duration in seconds at which the result won't change.
     * Use 0 for a different result every time.
	 */
	UPROPERTY(meta = (Input))
	float Duration;

	UPROPERTY(meta=(Output))
	bool Result;

	UPROPERTY()
	bool LastValue;

	UPROPERTY()
	float TimeLeft;
};

/**
 * Returns true once the first time this node is hit
 */
USTRUCT(meta=(DisplayName="Once", Keywords="FlipFlop,Toggle,Changed,Different", Varying))
struct RIGVM_API FRigVMFunction_MathBoolOnce : public FRigVMFunction_MathBoolBase
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute() override;

	FRigVMFunction_MathBoolOnce()
	{
		Duration = TimeLeft = 0.f;
		Result = false;
		LastValue = true;
	}

	/**
	 * The duration in seconds at which the result is true
	 * Use 0 for a different result every time.
	 */
	UPROPERTY(meta = (Visible))
	float Duration;

	UPROPERTY(meta=(Output))
	bool Result;

	UPROPERTY()
	bool LastValue;

	UPROPERTY()
	float TimeLeft;
};

/**
 * Turns the given bool into a float value
 */
USTRUCT(meta=(DisplayName="To Float", TemplateName="Cast", ExecuteContext="FRigVMExecuteContext"))
struct RIGVM_API FRigVMFunction_MathBoolToFloat : public FRigVMFunction_MathBoolBase
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute() override;

	FRigVMFunction_MathBoolToFloat()
	{
		Value = false;
		Result = 0.f;
	}

	UPROPERTY(meta = (Input))
	bool Value;

	UPROPERTY(meta= (Output))
	float Result;
};

/**
 * Turns the given bool into an integer value
 */
USTRUCT(meta=(DisplayName="To Integer", TemplateName="Cast", ExecuteContext="FRigVMExecuteContext"))
struct RIGVM_API FRigVMFunction_MathBoolToInteger : public FRigVMFunction_MathBoolBase
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute() override;

	FRigVMFunction_MathBoolToInteger()
	{
		Value = false;
		Result = 0;
	}

	UPROPERTY(meta = (Input))
	bool Value;

	UPROPERTY(meta= (Output))
	int32 Result;
};
