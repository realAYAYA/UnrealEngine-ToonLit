// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/Debug/RigUnit_DebugPrimitives.h"
#include "Units/RigUnitContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_DebugPrimitives)

FRigUnit_DebugRectangle_Execute()
{
	FRigUnit_DebugRectangleItemSpace::StaticExecute(
		RigVMExecuteContext, 
		Transform,
		Color,
		Scale,
		Thickness,
		FRigElementKey(Space, ERigElementType::Bone), 
		WorldOffset, 
		bEnabled,
		ExecuteContext, 
		Context);
}

FRigVMStructUpgradeInfo FRigUnit_DebugRectangle::GetUpgradeInfo() const
{
	FRigUnit_DebugRectangleItemSpace NewNode;
	NewNode.Transform = Transform;
	NewNode.Color = Color;
	NewNode.Scale = Scale;
	NewNode.Thickness = Thickness;
	NewNode.Space = FRigElementKey(Space, ERigElementType::Bone);
	NewNode.WorldOffset = WorldOffset;
	NewNode.bEnabled = bEnabled;

	FRigVMStructUpgradeInfo Info(*this, NewNode);
	Info.AddRemappedPin(TEXT("Space"), TEXT("Space.Name"));
	return Info;
}

FRigUnit_DebugRectangleItemSpace_Execute()
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

	FTransform DrawTransform = Transform;
	if (Space.IsValid())
	{
		DrawTransform = DrawTransform * Context.Hierarchy->GetGlobalTransform(Space);
	}

	Context.DrawInterface->DrawRectangle(WorldOffset, DrawTransform, Scale, Color, Thickness);
}

FRigUnit_DebugArc_Execute()
{
	FRigUnit_DebugArcItemSpace::StaticExecute(
		RigVMExecuteContext, 
		Transform,
		Color,
		Radius,
		MinimumDegrees,
		MaximumDegrees,
		Thickness,
		Detail,
		FRigElementKey(Space, ERigElementType::Bone), 
		WorldOffset, 
		bEnabled,
		ExecuteContext, 
		Context);
}

FRigVMStructUpgradeInfo FRigUnit_DebugArc::GetUpgradeInfo() const
{
	FRigUnit_DebugArcItemSpace NewNode;
	NewNode.Transform = Transform;
	NewNode.Color = Color;
	NewNode.Radius = Radius;
	NewNode.MinimumDegrees = MinimumDegrees;
	NewNode.MaximumDegrees = MaximumDegrees;
	NewNode.Thickness = Thickness;
	NewNode.Detail = Detail;
	NewNode.Space = FRigElementKey(Space, ERigElementType::Bone);
	NewNode.WorldOffset = WorldOffset;
	NewNode.bEnabled = bEnabled;

	FRigVMStructUpgradeInfo Info(*this, NewNode);
	Info.AddRemappedPin(TEXT("Space"), TEXT("Space.Name"));
	return Info;
}

FRigUnit_DebugArcItemSpace_Execute()
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

	FTransform DrawTransform = Transform;
	if (Space.IsValid())
	{
		DrawTransform = DrawTransform * Context.Hierarchy->GetGlobalTransform(Space);
	}

	Context.DrawInterface->DrawArc(WorldOffset, DrawTransform, Radius, FMath::DegreesToRadians(MinimumDegrees), FMath::DegreesToRadians(MaximumDegrees), Color, Thickness, Detail);
}
