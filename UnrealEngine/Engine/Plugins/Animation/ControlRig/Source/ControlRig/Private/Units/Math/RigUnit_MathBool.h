// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigUnit_MathBase.h"
#include "Units/Core/RigUnit_CoreDispatch.h"
#include "RigUnit_MathBool.generated.h"

USTRUCT(meta=(Abstract, Category="Math|Boolean"))
struct CONTROLRIG_API FRigUnit_MathBoolBase : public FRigUnit_MathBase
{
	GENERATED_BODY()
};

USTRUCT(meta=(Abstract))
struct CONTROLRIG_API FRigUnit_MathBoolConstant : public FRigUnit_MathBoolBase
{
	GENERATED_BODY()

	FRigUnit_MathBoolConstant()
	{
		Value = false;
	}

	UPROPERTY(meta=(Output))
	bool Value;
};

USTRUCT(meta=(Abstract))
struct CONTROLRIG_API FRigUnit_MathBoolUnaryOp : public FRigUnit_MathBoolBase
{
	GENERATED_BODY()

	FRigUnit_MathBoolUnaryOp()
	{
		Value = Result = 0.f;
	}

	UPROPERTY(meta=(Input))
	bool Value;

	UPROPERTY(meta=(Output))
	bool Result;
};

USTRUCT(meta=(Abstract))
struct CONTROLRIG_API FRigUnit_MathBoolBinaryOp : public FRigUnit_MathBoolBase
{
	GENERATED_BODY()

	FRigUnit_MathBoolBinaryOp()
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
struct CONTROLRIG_API FRigUnit_MathBoolBinaryAggregateOp : public FRigUnit_MathBoolBase
{
	GENERATED_BODY()

	FRigUnit_MathBoolBinaryAggregateOp()
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
 * Returns true
 */
USTRUCT(meta=(DisplayName="True", Keywords="Yes"))
struct CONTROLRIG_API FRigUnit_MathBoolConstTrue : public FRigUnit_MathBoolConstant
{
	GENERATED_BODY()
	
	FRigUnit_MathBoolConstTrue()
	{
		Value = true;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns false
 */
USTRUCT(meta=(DisplayName="False", Keywords="No"))
struct CONTROLRIG_API FRigUnit_MathBoolConstFalse : public FRigUnit_MathBoolConstant
{
	GENERATED_BODY()
	
	FRigUnit_MathBoolConstFalse()
	{
		Value = false;
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns true if the condition is false
 */
USTRUCT(meta=(DisplayName="Not", TemplateName="Not", Keywords="!"))
struct CONTROLRIG_API FRigUnit_MathBoolNot : public FRigUnit_MathBoolUnaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns true if both conditions are true
 */
USTRUCT(meta=(DisplayName="And", TemplateName="And", Keywords="&&"))
struct CONTROLRIG_API FRigUnit_MathBoolAnd : public FRigUnit_MathBoolBinaryAggregateOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns true if both conditions are false
 */
USTRUCT(meta=(DisplayName="Nand", Deprecated="5.1"))
struct CONTROLRIG_API FRigUnit_MathBoolNand : public FRigUnit_MathBoolBinaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	RIGVM_METHOD()
	virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;
};

/**
 * Returns false if both conditions are true
 */
USTRUCT(meta=(DisplayName="Nand", TemplateName="Nand"))
struct CONTROLRIG_API FRigUnit_MathBoolNand2 : public FRigUnit_MathBoolBinaryOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns true if one of the conditions is true
 */
USTRUCT(meta=(DisplayName="Or", TemplateName="Or", Keywords="||"))
struct CONTROLRIG_API FRigUnit_MathBoolOr : public FRigUnit_MathBoolBinaryAggregateOp
{
	GENERATED_BODY()

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;
};

/**
 * Returns true if the value A equals B
 */
USTRUCT(meta=(DisplayName="Equals", TemplateName="Equals", Keywords="Same,==", Deprecated="5.1"))
struct CONTROLRIG_API FRigUnit_MathBoolEquals : public FRigUnit_MathBoolBase
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	FRigUnit_MathBoolEquals()
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
struct CONTROLRIG_API FRigUnit_MathBoolNotEquals : public FRigUnit_MathBoolBase
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	FRigUnit_MathBoolNotEquals()
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
struct CONTROLRIG_API FRigUnit_MathBoolToggled : public FRigUnit_MathBoolBase
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	FRigUnit_MathBoolToggled()
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
struct CONTROLRIG_API FRigUnit_MathBoolFlipFlop : public FRigUnit_MathBoolBase
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	FRigUnit_MathBoolFlipFlop()
	{
		Duration = TimeLeft = 0.f;
		Result = LastValue = StartValue = false;
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
struct CONTROLRIG_API FRigUnit_MathBoolOnce : public FRigUnit_MathBoolBase
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	FRigUnit_MathBoolOnce()
	{
		Duration = TimeLeft = 0.f;
		Result = LastValue = false;
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
USTRUCT(meta=(DisplayName="To Float", TemplateName="Cast"))
struct CONTROLRIG_API FRigUnit_MathBoolToFloat : public FRigUnit_MathBoolBase
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	FRigUnit_MathBoolToFloat()
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
USTRUCT(meta=(DisplayName="To Integer", TemplateName="Cast"))
struct CONTROLRIG_API FRigUnit_MathBoolToInteger : public FRigUnit_MathBoolBase
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	FRigUnit_MathBoolToInteger()
	{
		Value = false;
		Result = 0;
	}

	UPROPERTY(meta = (Input))
	bool Value;

	UPROPERTY(meta= (Output))
	int32 Result;
};
