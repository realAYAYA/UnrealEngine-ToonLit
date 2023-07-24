// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMFunctions/Debug/RigVMFunction_DebugLineStrip.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMFunction_DebugLineStrip)

FRigVMFunction_DebugLineStrip_Execute()
{
	FRigVMFunction_DebugLineStripNoSpace::StaticExecute(
		ExecuteContext, 
		Points,
		Color,
		Thickness,
		WorldOffset, 
		bEnabled);
}

FRigVMStructUpgradeInfo FRigVMFunction_DebugLineStrip::GetUpgradeInfo() const
{
	FRigVMFunction_DebugLineStripNoSpace NewNode;
	NewNode.Points = Points;
	NewNode.Color = Color;
	NewNode.Thickness = Thickness;
	NewNode.WorldOffset = WorldOffset;
	NewNode.bEnabled = bEnabled;

	FRigVMStructUpgradeInfo Info(*this, NewNode);
	return Info;
}

FRigVMFunction_DebugLineStripNoSpace_Execute()
{
	if (ExecuteContext.GetDrawInterface() == nullptr || !bEnabled)
	{
		return;
	}

	ExecuteContext.GetDrawInterface()->DrawLineStrip(WorldOffset, TArrayView<const FVector>(Points.GetData(), Points.Num()), Color, Thickness);
}

