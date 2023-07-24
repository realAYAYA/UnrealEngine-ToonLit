// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMFunctions/Debug/RigVMFunction_DebugTransform.h"
#include "RigVMFunctions/Debug/RigVMFunction_VisualDebug.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMFunction_DebugTransform)

FRigVMFunction_DebugTransform_Execute()
{
	if (ExecuteContext.GetDrawInterface() == nullptr || !bEnabled)
	{
		return;
	}

	FTransform DrawTransform = Transform;

	switch (Mode)
	{
		case ERigUnitDebugTransformMode::Axes:
		{
			ExecuteContext.GetDrawInterface()->DrawAxes(WorldOffset, DrawTransform, Scale, Thickness);
			break;
		}
		case ERigUnitDebugTransformMode::Point:
		{
			ExecuteContext.GetDrawInterface()->DrawPoint(WorldOffset, DrawTransform.GetLocation(), Scale, Color);
			break;
		}
		case ERigUnitDebugTransformMode::Box:
		{
			DrawTransform.SetScale3D(DrawTransform.GetScale3D() * Scale);
			ExecuteContext.GetDrawInterface()->DrawBox(WorldOffset, DrawTransform, Color, Thickness);
			break;
		}
	}
}

FRigVMStructUpgradeInfo FRigVMFunction_DebugTransform::GetUpgradeInfo() const
{
	FRigVMFunction_VisualDebugTransformNoSpace NewNode;
	NewNode.Value = Transform;
	NewNode.Scale = Scale;
	NewNode.Thickness = Thickness;
	NewNode.bEnabled = bEnabled;

	FRigVMStructUpgradeInfo Info(*this, NewNode);
	Info.AddRemappedPin(TEXT("Transform"), TEXT("Value"));
	return Info;
}

FRigVMFunction_DebugTransformMutable_Execute()
{
	FRigVMFunction_DebugTransformMutableNoSpace::StaticExecute(
		ExecuteContext, 
		Transform,
		Mode,
		Color,
		Thickness,
		Scale,
		WorldOffset, 
		bEnabled);
}

FRigVMStructUpgradeInfo FRigVMFunction_DebugTransformMutable::GetUpgradeInfo() const
{
	FRigVMFunction_DebugTransformMutableNoSpace NewNode;
	NewNode.Transform = Transform;
	NewNode.Mode = Mode;
	NewNode.Color = Color;
	NewNode.Thickness = Thickness;
	NewNode.Scale = Scale;
	NewNode.WorldOffset = WorldOffset;
	NewNode.bEnabled = bEnabled;

	FRigVMStructUpgradeInfo Info(*this, NewNode);
	return Info;
}

FRigVMFunction_DebugTransformMutableNoSpace_Execute()
{
	if (ExecuteContext.GetDrawInterface() == nullptr || !bEnabled)
	{
		return;
	}

	FTransform DrawTransform = Transform;

	switch (Mode)
	{
		case ERigUnitDebugTransformMode::Axes:
		{
			ExecuteContext.GetDrawInterface()->DrawAxes(WorldOffset, DrawTransform, Scale, Thickness);
			break;
		}
		case ERigUnitDebugTransformMode::Point:
		{
			ExecuteContext.GetDrawInterface()->DrawPoint(WorldOffset, DrawTransform.GetLocation(), Scale, Color);
			break;
		}
		case ERigUnitDebugTransformMode::Box:
		{
			DrawTransform.SetScale3D(DrawTransform.GetScale3D() * Scale);
			ExecuteContext.GetDrawInterface()->DrawBox(WorldOffset, DrawTransform, Color, Thickness);
			break;
		}
	}
}

FRigVMFunction_DebugTransformArrayMutable_Execute()
{
	if (ExecuteContext.GetDrawInterface() == nullptr || !bEnabled)
	{
		return;
	}

	WorkData.DrawTransforms.SetNumUninitialized(Transforms.Num());
	for(int32 Index=0;Index<Transforms.Num();Index++)
	{
		WorkData.DrawTransforms[Index] = Transforms[Index];
	}

	for(FTransform& DrawTransform : WorkData.DrawTransforms)
	{
		switch (Mode)
		{
			case ERigUnitDebugTransformMode::Axes:
			{
				ExecuteContext.GetDrawInterface()->DrawAxes(WorldOffset, DrawTransform, Scale, Thickness);
				break;
			}
			case ERigUnitDebugTransformMode::Point:
			{
				ExecuteContext.GetDrawInterface()->DrawPoint(WorldOffset, DrawTransform.GetLocation(), Scale, Color);
				break;
			}
			case ERigUnitDebugTransformMode::Box:
			{
				DrawTransform.SetScale3D(DrawTransform.GetScale3D() * Scale);
				ExecuteContext.GetDrawInterface()->DrawBox(WorldOffset, DrawTransform, Color, Thickness);
				break;
			}
		}
	}
}

FRigVMStructUpgradeInfo FRigVMFunction_DebugTransformArrayMutable::GetUpgradeInfo() const
{
	FRigVMFunction_DebugTransformArrayMutableNoSpace NewNode;
	NewNode.Transforms = Transforms;
	NewNode.Mode = Mode;
	NewNode.Color = Color;
	NewNode.Thickness = Thickness;
	NewNode.Scale = Scale;
	NewNode.WorldOffset = WorldOffset;
	NewNode.bEnabled = bEnabled;

	FRigVMStructUpgradeInfo Info(*this, NewNode);
	return Info;
}

FRigVMFunction_DebugTransformArrayMutableNoSpace_Execute()
{
	if (ExecuteContext.GetDrawInterface() == nullptr || !bEnabled || Transforms.Num() == 0)
	{
		return;
	}

	for(const FTransform& Transform : Transforms)
	{
		FRigVMFunction_DebugTransformMutableNoSpace::StaticExecute(
			ExecuteContext, 
			Transform,
			Mode,
			Color,
			Thickness,
			Scale,
			WorldOffset, 
			bEnabled);
	}

	if(ParentIndices.Num() == Transforms.Num())
	{
		for(int32 Index = 0; Index < ParentIndices.Num(); Index++)
		{
			if(Transforms.IsValidIndex(ParentIndices[Index]))
			{
				ExecuteContext.GetDrawInterface()->DrawLine(
					WorldOffset,
					Transforms[Index].GetTranslation(),
					Transforms[ParentIndices[Index]].GetTranslation(),
					Color,
					Thickness
				);
			}
		}
	}
}

