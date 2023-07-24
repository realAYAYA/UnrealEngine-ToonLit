// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMFunctions/Debug/RigVMFunction_DebugLine.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMFunction_DebugLine)

FRigVMFunction_DebugLine_Execute()
{
	FRigVMFunction_DebugLineNoSpace::StaticExecute(
		ExecuteContext, 
		A,
		B,
		Color,
		Thickness,
		WorldOffset, 
		bEnabled);
}

FRigVMStructUpgradeInfo FRigVMFunction_DebugLine::GetUpgradeInfo() const
{
	FRigVMFunction_DebugLineNoSpace NewNode;
	NewNode.A = A;
	NewNode.B = B;
	NewNode.Color = Color;
	NewNode.Thickness = Thickness;
	NewNode.WorldOffset = WorldOffset;
	NewNode.bEnabled = bEnabled;

	FRigVMStructUpgradeInfo Info(*this, NewNode);
	return Info;
}

FRigVMFunction_DebugLineNoSpace_Execute()
{
	if (ExecuteContext.GetDrawInterface() == nullptr || !bEnabled)
	{
		return;
	}

	ExecuteContext.GetDrawInterface()->DrawLine(WorldOffset, A, B, Color, Thickness);
}
