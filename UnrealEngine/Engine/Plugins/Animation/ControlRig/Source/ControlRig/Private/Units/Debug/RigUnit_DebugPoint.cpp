// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/Debug/RigUnit_DebugPoint.h"
#include "Units/Debug/RigUnit_VisualDebug.h"
#include "Units/RigUnitContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_DebugPoint)

FRigUnit_DebugPoint_Execute()
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

	FVector Center = FVector::ZeroVector;
	FVector DrawVector = Vector;
	if (Space != NAME_None && Context.Hierarchy != nullptr)
	{
		const FTransform Transform = Context.Hierarchy->GetGlobalTransform(FRigElementKey(Space, ERigElementType::Bone));
		Center = Transform.GetLocation();
		DrawVector = Transform.TransformPosition(DrawVector);
	}

	switch (Mode)
	{
		case ERigUnitDebugPointMode::Point:
		{
			Context.DrawInterface->DrawPoint(WorldOffset, DrawVector, Scale, Color);
			break;
		}
		case ERigUnitDebugPointMode::Vector:
		{
			Context.DrawInterface->DrawLine(WorldOffset, Center, DrawVector, Color, Thickness);
			break;
		}
	}
}

FRigVMStructUpgradeInfo FRigUnit_DebugPoint::GetUpgradeInfo() const
{
	FRigUnit_VisualDebugVector NewNode;
	NewNode.Value = Vector;
	NewNode.Color = Color;
	NewNode.Thickness = Thickness;
	NewNode.BoneSpace = Space;
	NewNode.Scale = Scale;
	NewNode.bEnabled = bEnabled;

	FRigVMStructUpgradeInfo Info(*this, NewNode);
	Info.AddRemappedPin(TEXT("Vector"), TEXT("Value"));
	Info.AddRemappedPin(TEXT("Space"), TEXT("BoneSpace"));
	return Info;
}

FRigUnit_DebugPointMutable_Execute()
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

	FVector Center = FVector::ZeroVector;
	FVector DrawVector = Vector;
	if (Space != NAME_None && Context.Hierarchy != nullptr)
	{
		const FTransform Transform = Context.Hierarchy->GetGlobalTransform(FRigElementKey(Space, ERigElementType::Bone));
		Center = Transform.GetLocation();
		DrawVector = Transform.TransformPosition(DrawVector);
	}

	switch (Mode)
	{
		case ERigUnitDebugPointMode::Point:
		{
			Context.DrawInterface->DrawPoint(WorldOffset, DrawVector, Scale, Color);
			break;
		}
		case ERigUnitDebugPointMode::Vector:
		{
			Context.DrawInterface->DrawLine(WorldOffset, Center, DrawVector, Color, Thickness);
			break;
		}
	}
}

FRigVMStructUpgradeInfo FRigUnit_DebugPointMutable::GetUpgradeInfo() const
{
	// this node is no longer supported
	return FRigVMStructUpgradeInfo();
}

