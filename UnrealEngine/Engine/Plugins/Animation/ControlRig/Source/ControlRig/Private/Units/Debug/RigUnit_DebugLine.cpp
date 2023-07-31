// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/Debug/RigUnit_DebugLine.h"
#include "Units/RigUnitContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_DebugLine)

FRigUnit_DebugLine_Execute()
{
	FRigUnit_DebugLineItemSpace::StaticExecute(
		RigVMExecuteContext, 
		A,
		B,
		Color,
		Thickness,
		FRigElementKey(Space, ERigElementType::Bone), 
		WorldOffset, 
		bEnabled,
		ExecuteContext, 
		Context);
}

FRigVMStructUpgradeInfo FRigUnit_DebugLine::GetUpgradeInfo() const
{
	FRigUnit_DebugLineItemSpace NewNode;
	NewNode.A = A;
	NewNode.B = B;
	NewNode.Color = Color;
	NewNode.Thickness = Thickness;
	NewNode.Space = FRigElementKey(Space, ERigElementType::Bone);
	NewNode.WorldOffset = WorldOffset;
	NewNode.bEnabled = bEnabled;

	FRigVMStructUpgradeInfo Info(*this, NewNode);
	Info.AddRemappedPin(TEXT("Space"), TEXT("Space.Name"));
	return Info;
}

FRigUnit_DebugLineItemSpace_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	if (Context.State == EControlRigState::Init)
	{
		return;
	}

	if (Context.DrawInterface == nullptr || !bEnabled)
	{
		return;
	}

	FVector DrawA = A, DrawB = B;
	if (Space.IsValid())
	{
		FTransform Transform = Context.Hierarchy->GetGlobalTransform(Space);
		DrawA = Transform.TransformPosition(DrawA);
		DrawB = Transform.TransformPosition(DrawB);
	}

	Context.DrawInterface->DrawLine(WorldOffset, DrawA, DrawB, Color, Thickness);
}
