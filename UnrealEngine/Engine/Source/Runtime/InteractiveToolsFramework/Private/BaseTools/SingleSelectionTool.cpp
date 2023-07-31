// Copyright Epic Games, Inc. All Rights Reserved.

#include "SingleSelectionTool.h"
#include "InteractiveToolManager.h"
#include "ToolTargets/PrimitiveComponentToolTarget.h"
#include "Components/PrimitiveComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SingleSelectionTool)

#define LOCTEXT_NAMESPACE "USingleSelectionTool"

bool USingleSelectionTool::SupportsWorldSpaceFocusBox()
{
	return Target && Cast<UPrimitiveComponentToolTarget>(Target) != nullptr;
}


FBox USingleSelectionTool::GetWorldSpaceFocusBox()
{
	if (Target)
	{
		UPrimitiveComponentToolTarget* PrimTarget = Cast<UPrimitiveComponentToolTarget>(Target);
		if (PrimTarget)
		{
			UPrimitiveComponent* Component = PrimTarget->GetOwnerComponent();
			if (Component)
			{
				return Component->Bounds.GetBox();
			}
		}
	}
	return FBox();
}


bool USingleSelectionTool::SupportsWorldSpaceFocusPoint()
{
	return Target && Cast<UPrimitiveComponentToolTarget>(Target) != nullptr;

}
bool USingleSelectionTool::GetWorldSpaceFocusPoint(const FRay& WorldRay, FVector& PointOut)
{
	if (Target)
	{
		UPrimitiveComponentToolTarget* PrimTarget = Cast<UPrimitiveComponentToolTarget>(Target);
		if (PrimTarget)
		{
			FHitResult HitResult;
			if (PrimTarget->HitTestComponent(WorldRay, HitResult))
			{
				PointOut = HitResult.ImpactPoint;
				return true;
			}
		}
	}
	return false;
}




#undef LOCTEXT_NAMESPACE
