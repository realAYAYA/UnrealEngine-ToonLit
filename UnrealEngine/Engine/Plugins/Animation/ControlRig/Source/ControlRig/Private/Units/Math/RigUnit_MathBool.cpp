// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/Math/RigUnit_MathBool.h"
#include "Units/RigUnitContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_MathBool)

FRigUnit_MathBoolConstTrue_Execute()
{
	Value = true;
}

FRigUnit_MathBoolConstFalse_Execute()
{
	Value = false;
}

FRigUnit_MathBoolNot_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = !Value;
}

FRigUnit_MathBoolAnd_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = A && B;
}

FRigUnit_MathBoolNand_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = (!A) && (!B);
}

FRigUnit_MathBoolNand2_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = !(A && B);
}

FRigVMStructUpgradeInfo FRigUnit_MathBoolNand::GetUpgradeInfo() const
{
	FRigUnit_MathBoolNand2 NewNode;
	NewNode.A = A;
	NewNode.B = B;

	return FRigVMStructUpgradeInfo(*this, NewNode);
}

FRigUnit_MathBoolOr_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = A || B;
}

FRigUnit_MathBoolEquals_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = A == B;
}

FRigVMStructUpgradeInfo FRigUnit_MathBoolEquals::GetUpgradeInfo() const
{
	return FRigVMStructUpgradeInfo::MakeFromStructToFactory(StaticStruct(), FRigDispatch_CoreEquals::StaticStruct());
}

FRigUnit_MathBoolNotEquals_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	Result = A != B;
}

FRigVMStructUpgradeInfo FRigUnit_MathBoolNotEquals::GetUpgradeInfo() const
{
	return FRigVMStructUpgradeInfo::MakeFromStructToFactory(StaticStruct(), FRigDispatch_CoreNotEquals::StaticStruct());
}

FRigUnit_MathBoolToggled_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

	if(Context.State == EControlRigState::Init)
	{
		Initialized = false;
		LastValue = Value;
		Toggled = false;
		return;
	}

	if(!Initialized)
	{
		Initialized = true;
		Toggled = false;
	}
	else
	{
		Toggled = LastValue != Value;
	}

	LastValue = Value;
}

FRigUnit_MathBoolFlipFlop_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

	if(Context.State == EControlRigState::Init)
	{
		Result = LastValue = !StartValue;
		TimeLeft = 0.f;
		return;
	}

	TimeLeft = TimeLeft - Context.DeltaTime;
	if (TimeLeft <= 0.f)
	{
		LastValue = !LastValue;
		TimeLeft = Duration;
	}

	Result = LastValue;
}

FRigUnit_MathBoolOnce_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

	if(Context.State == EControlRigState::Init)
	{
		Result = false;
		LastValue = true;
		TimeLeft = 0.f;
		return;
	}

	if(!Result && LastValue)
	{
		TimeLeft = Duration;
	}

	Result = LastValue;

	TimeLeft -= Context.DeltaTime;
	if(TimeLeft <= 0.f)
	{
		LastValue = false;
	}
}

FRigUnit_MathBoolToFloat_Execute()
{
	Result = Value ? 1.f : 0.f;
}

FRigUnit_MathBoolToInteger_Execute()
{
	Result = Value ? 1 : 0;
}
