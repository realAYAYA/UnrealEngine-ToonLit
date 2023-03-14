// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/Debug/RigUnit_VisualDebug.h"
#include "Units/RigUnitContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_VisualDebug)

FRigUnit_VisualDebugVector_Execute()
{
	FRigUnit_VisualDebugVectorItemSpace::StaticExecute(RigVMExecuteContext, Value, bEnabled, Mode, Color, Thickness, Scale, FRigElementKey(BoneSpace, ERigElementType::Bone), Context);
}

FRigVMStructUpgradeInfo FRigUnit_VisualDebugVector::GetUpgradeInfo() const
{
	FRigUnit_VisualDebugVectorItemSpace NewNode;
	NewNode.Value = Value;
	NewNode.Color = Color;
	NewNode.Thickness = Thickness;
	NewNode.Space = FRigElementKey(BoneSpace, ERigElementType::Bone);
	NewNode.Scale = Scale;
	NewNode.bEnabled = bEnabled;

	FRigVMStructUpgradeInfo Info(*this, NewNode);
	Info.AddRemappedPin(TEXT("BoneSpace"), TEXT("Space.Name"));
	return Info;
}

FRigUnit_VisualDebugVectorItemSpace_Execute()
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

	FTransform WorldOffset = FTransform::Identity;
	if (Space.IsValid())
	{
		WorldOffset = Context.Hierarchy->GetGlobalTransform(Space);
	}

	switch(Mode)
	{
		case ERigUnitVisualDebugPointMode::Point:
		{
			Context.DrawInterface->DrawPoint(WorldOffset, Value, Thickness, Color);
			break;
		}
		case ERigUnitVisualDebugPointMode::Vector:
		{
			Context.DrawInterface->DrawLine(WorldOffset, FVector::ZeroVector, Value * Scale, Color, Thickness);
			break;
		}
		default:
		{
			checkNoEntry();
		}
	}
}

FRigUnit_VisualDebugQuat_Execute()
{
	FRigUnit_VisualDebugQuatItemSpace::StaticExecute(RigVMExecuteContext, Value, bEnabled, Thickness, Scale, FRigElementKey(BoneSpace, ERigElementType::Bone), Context);
}

FRigVMStructUpgradeInfo FRigUnit_VisualDebugQuat::GetUpgradeInfo() const
{
	FRigUnit_VisualDebugQuatItemSpace NewNode;
	NewNode.Value = Value;
	NewNode.Thickness = Thickness;
	NewNode.Space = FRigElementKey(BoneSpace, ERigElementType::Bone);
	NewNode.Scale = Scale;
	NewNode.bEnabled = bEnabled;

	FRigVMStructUpgradeInfo Info(*this, NewNode);
	Info.AddRemappedPin(TEXT("BoneSpace"), TEXT("Space.Name"));
	return Info;
}

FRigUnit_VisualDebugQuatItemSpace_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

    FTransform Transform = FTransform::Identity;
    Transform.SetRotation(Value);

	FRigUnit_VisualDebugTransformItemSpace::StaticExecute(RigVMExecuteContext, Transform, bEnabled, Thickness, Scale, Space, Context);
}

FRigUnit_VisualDebugTransform_Execute()
{
	FRigUnit_VisualDebugTransformItemSpace::StaticExecute(RigVMExecuteContext, Value, bEnabled, Thickness, Scale, FRigElementKey(BoneSpace, ERigElementType::Bone), Context);
}

FRigVMStructUpgradeInfo FRigUnit_VisualDebugTransform::GetUpgradeInfo() const
{
	FRigUnit_VisualDebugTransformItemSpace NewNode;
	NewNode.Value = Value;
	NewNode.Thickness = Thickness;
	NewNode.Space = FRigElementKey(BoneSpace, ERigElementType::Bone);
	NewNode.Scale = Scale;
	NewNode.bEnabled = bEnabled;

	FRigVMStructUpgradeInfo Info(*this, NewNode);
	Info.AddRemappedPin(TEXT("BoneSpace"), TEXT("Space.Name"));
	return Info;
}

FRigUnit_VisualDebugTransformItemSpace_Execute()
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

	FTransform WorldOffset = FTransform::Identity;
	if (Space.IsValid())
	{
		WorldOffset = Context.Hierarchy->GetGlobalTransform(Space);
	}

	Context.DrawInterface->DrawAxes(WorldOffset, Value, Scale, Thickness);
}
