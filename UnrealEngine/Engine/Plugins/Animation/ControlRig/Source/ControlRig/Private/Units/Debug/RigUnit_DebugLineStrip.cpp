// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/Debug/RigUnit_DebugLineStrip.h"
#include "Units/RigUnitContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_DebugLineStrip)

FRigUnit_DebugLineStrip_Execute()
{
	FRigUnit_DebugLineStripItemSpace::StaticExecute(
		RigVMExecuteContext, 
		Points,
		Color,
		Thickness,
		FRigElementKey(Space, ERigElementType::Bone), 
		WorldOffset, 
		bEnabled,
		ExecuteContext, 
		Context);
}

FRigVMStructUpgradeInfo FRigUnit_DebugLineStrip::GetUpgradeInfo() const
{
	FRigUnit_DebugLineStripItemSpace NewNode;
	NewNode.Points = Points;
	NewNode.Color = Color;
	NewNode.Thickness = Thickness;
	NewNode.Space = FRigElementKey(Space, ERigElementType::Bone);
	NewNode.WorldOffset = WorldOffset;
	NewNode.bEnabled = bEnabled;

	FRigVMStructUpgradeInfo Info(*this, NewNode);
	Info.AddRemappedPin(TEXT("Space"), TEXT("Space.Name"));
	return Info;
}

FRigUnit_DebugLineStripItemSpace_Execute()
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

	if (Space.IsValid())
	{
		FTransform Transform = Context.Hierarchy->GetGlobalTransform(Space);
		TArray<FVector> PointsTransformed;
		PointsTransformed.Reserve(Points.Num());
		for(const FVector& Point : Points)
		{
			PointsTransformed.Add(Transform.TransformPosition(Point));
		}
		Context.DrawInterface->DrawLineStrip(WorldOffset, PointsTransformed, Color, Thickness);
	}
	else
	{
		Context.DrawInterface->DrawLineStrip(WorldOffset, TArrayView<const FVector>(Points.GetData(), Points.Num()), Color, Thickness);
	}
}

