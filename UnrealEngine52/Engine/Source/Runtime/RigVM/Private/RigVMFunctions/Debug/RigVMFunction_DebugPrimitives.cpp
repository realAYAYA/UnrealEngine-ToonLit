// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMFunctions/Debug/RigVMFunction_DebugPrimitives.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMFunction_DebugPrimitives)

FRigVMFunction_DebugRectangle_Execute()
{
	FRigVMFunction_DebugRectangleNoSpace::StaticExecute(
		ExecuteContext, 
		Transform,
		Color,
		Scale,
		Thickness,
		WorldOffset, 
		bEnabled);
}

FRigVMStructUpgradeInfo FRigVMFunction_DebugRectangle::GetUpgradeInfo() const
{
	FRigVMFunction_DebugRectangleNoSpace NewNode;
	NewNode.Transform = Transform;
	NewNode.Color = Color;
	NewNode.Scale = Scale;
	NewNode.Thickness = Thickness;
	NewNode.WorldOffset = WorldOffset;
	NewNode.bEnabled = bEnabled;

	FRigVMStructUpgradeInfo Info(*this, NewNode);
	return Info;
}

FRigVMFunction_DebugRectangleNoSpace_Execute()
{
	if (ExecuteContext.GetDrawInterface() == nullptr || !bEnabled)
	{
		return;
	}

	ExecuteContext.GetDrawInterface()->DrawRectangle(WorldOffset, Transform, Scale, Color, Thickness);
}

FRigVMFunction_DebugArc_Execute()
{
	FRigVMFunction_DebugArcNoSpace::StaticExecute(
		ExecuteContext, 
		Transform,
		Color,
		Radius,
		MinimumDegrees,
		MaximumDegrees,
		Thickness,
		Detail,
		WorldOffset, 
		bEnabled);
}

FRigVMStructUpgradeInfo FRigVMFunction_DebugArc::GetUpgradeInfo() const
{
	FRigVMFunction_DebugArcNoSpace NewNode;
	NewNode.Transform = Transform;
	NewNode.Color = Color;
	NewNode.Radius = Radius;
	NewNode.MinimumDegrees = MinimumDegrees;
	NewNode.MaximumDegrees = MaximumDegrees;
	NewNode.Thickness = Thickness;
	NewNode.Detail = Detail;
	NewNode.WorldOffset = WorldOffset;
	NewNode.bEnabled = bEnabled;

	FRigVMStructUpgradeInfo Info(*this, NewNode);
	return Info;
}

FRigVMFunction_DebugArcNoSpace_Execute()
{
	if (ExecuteContext.GetDrawInterface() == nullptr || !bEnabled)
	{
		return;
	}

	ExecuteContext.GetDrawInterface()->DrawArc(WorldOffset, Transform, Radius, FMath::DegreesToRadians(MinimumDegrees), FMath::DegreesToRadians(MaximumDegrees), Color, Thickness, Detail);
}
