// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/Drawing/RigUnit_DrawContainer.h"
#include "Units/RigUnitContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_DrawContainer)

FRigUnit_DrawContainerGetInstruction_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

    if(Context.DrawContainer == nullptr)
    {
    	return;
    }
	
	switch(Context.State)
	{
		case EControlRigState::Update:
		{
			int32 Index = Context.DrawContainer->GetIndex(InstructionName);
			if (Index != INDEX_NONE)
			{
				const FControlRigDrawInstruction& Instruction = (*Context.DrawContainer)[Index];
				Color = Instruction.Color;
				Transform = Instruction.Transform;
			}
			else
			{
				Color = FLinearColor::Red;
				Transform = FTransform::Identity;
			}
			break;
		}
	}
}

FRigVMStructUpgradeInfo FRigUnit_DrawContainerGetInstruction::GetUpgradeInfo() const
{
	// this node is no longer supported
	return FRigVMStructUpgradeInfo();
}

FRigUnit_DrawContainerSetThickness_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

	if (Context.DrawContainer == nullptr)
	{
		return;
	}

	switch (Context.State)
	{
		case EControlRigState::Update:
		{
			int32 Index = Context.DrawContainer->GetIndex(InstructionName);
			if (Index != INDEX_NONE)
			{
				FControlRigDrawInstruction& Instruction = (*Context.DrawContainer)[Index];
				Instruction.Thickness = Thickness;
			}
			break;
		}
	}
}

FRigVMStructUpgradeInfo FRigUnit_DrawContainerSetThickness::GetUpgradeInfo() const
{
	// this node is no longer supported
	return FRigVMStructUpgradeInfo();
}

FRigUnit_DrawContainerSetColor_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

	if (Context.DrawContainer == nullptr)
	{
		return;
	}

	switch (Context.State)
	{
		case EControlRigState::Update:
		{
			int32 Index = Context.DrawContainer->GetIndex(InstructionName);
			if (Index != INDEX_NONE)
			{
				FControlRigDrawInstruction& Instruction = (*Context.DrawContainer)[Index];
				Instruction.Color = Color;
			}
			break;
		}
	}
}

FRigVMStructUpgradeInfo FRigUnit_DrawContainerSetColor::GetUpgradeInfo() const
{
	// this node is no longer supported
	return FRigVMStructUpgradeInfo();
}

FRigUnit_DrawContainerSetTransform_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

	if (Context.DrawContainer == nullptr)
	{
		return;
	}

	switch (Context.State)
	{
		case EControlRigState::Update:
		{
			int32 Index = Context.DrawContainer->GetIndex(InstructionName);
			if (Index != INDEX_NONE)
			{
				FControlRigDrawInstruction& Instruction = (*Context.DrawContainer)[Index];
				Instruction.Transform = Transform;
			}
			break;
		}
	}
}

FRigVMStructUpgradeInfo FRigUnit_DrawContainerSetTransform::GetUpgradeInfo() const
{
	// this node is no longer supported
	return FRigVMStructUpgradeInfo();
}

