// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseGizmos/GizmoElementGroup.h"
#include "BaseGizmos/GizmoViewContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GizmoElementGroup)

void UGizmoElementGroup::ApplyUniformConstantScaleToTransform(double PixelToWorldScale, FTransform& InOutLocalToWorldTransform) const
{
	double Scale = InOutLocalToWorldTransform.GetScale3D().X;
	if (bConstantScale)
	{
		Scale *= PixelToWorldScale;
	}
	InOutLocalToWorldTransform.SetScale3D(FVector(Scale, Scale, Scale));
}

void UGizmoElementGroup::Render(IToolsContextRenderAPI* RenderAPI, const FRenderTraversalState& RenderState)
{
	FRenderTraversalState CurrentRenderState(RenderState);
	bool bVisibleViewDependent = UpdateRenderState(RenderAPI, FVector::ZeroVector, CurrentRenderState);

	if (bVisibleViewDependent)
	{
		ApplyUniformConstantScaleToTransform(CurrentRenderState.PixelToWorldScale, CurrentRenderState.LocalToWorldTransform);

		// Continue render even if not visible so all transforms will be cached 
		// for subsequent line tracing.
		for (UGizmoElementBase* Element : Elements)
		{
			if (Element)
			{
				Element->Render(RenderAPI, CurrentRenderState);
			}
		}
	}
}

FInputRayHit UGizmoElementGroup::LineTrace(const UGizmoViewContext* ViewContext, const FLineTraceTraversalState& LineTraceState, const FVector& RayOrigin, const FVector& RayDirection)
{
	FInputRayHit Hit;

	FLineTraceTraversalState CurrentLineTraceState(LineTraceState);
	bool bHittableViewDependent = UpdateLineTraceState(ViewContext, FVector::ZeroVector, CurrentLineTraceState);

	if (bHittableViewDependent)
	{
		ApplyUniformConstantScaleToTransform(CurrentLineTraceState.PixelToWorldScale, CurrentLineTraceState.LocalToWorldTransform);

		for (UGizmoElementBase* Element : Elements)
		{
			if (Element)
			{
				FInputRayHit NewHit = Element->LineTrace(ViewContext, CurrentLineTraceState, RayOrigin, RayDirection);
				if (!Hit.bHit || NewHit.HitDepth < Hit.HitDepth)
				{
					Hit = NewHit;
					if (bHitOwner)
					{
						Hit.SetHitObject(this);
						Hit.HitIdentifier = PartIdentifier;
					}
				}
			}
		}
	}
	return Hit;
}

void UGizmoElementGroup::Add(UGizmoElementBase* InElement)
{
	if (!Elements.Contains(InElement))
	{
		Elements.Add(InElement);
	}
}

void UGizmoElementGroup::Remove(UGizmoElementBase* InElement)
{
	if (InElement)
	{
		int32 Index;
		if (Elements.Find(InElement, Index))
		{
			Elements.RemoveAtSwap(Index);
		}
	}
}

bool UGizmoElementGroup::UpdatePartVisibleState(bool bVisible, uint32 InPartIdentifier)
{
	if (Super::UpdatePartVisibleState(bVisible, InPartIdentifier))
	{
		return true;
	}

	for (UGizmoElementBase* Element : Elements)
	{
		if (Element && Element->UpdatePartVisibleState(bVisible, InPartIdentifier))
		{
			return true;
		}
	}

	return false;
}

TOptional<bool> UGizmoElementGroup::GetPartVisibleState(uint32 InPartIdentifier) const
{
	TOptional<bool> Result = Super::GetPartVisibleState(InPartIdentifier);
	if (Result.IsSet())
	{
		return Result;
	}

	for (UGizmoElementBase* Element : Elements)
	{
		if (Element)
		{
			Result = Element->GetPartVisibleState(InPartIdentifier);
			if (Result.IsSet())
			{
				return Result;
			}
		}
	}

	return TOptional<bool>();
}


bool UGizmoElementGroup::UpdatePartHittableState(bool bHittable, uint32 InPartIdentifier)
{
	if (Super::UpdatePartHittableState(bHittable, InPartIdentifier))
	{
		return true;
	}

	for (UGizmoElementBase* Element : Elements)
	{
		if (Element && Element->UpdatePartHittableState(bHittable, InPartIdentifier))
		{
			return true;
		}
	}

	return false;
}

TOptional<bool> UGizmoElementGroup::GetPartHittableState(uint32 InPartIdentifier) const
{
	TOptional<bool> Result = Super::GetPartHittableState(InPartIdentifier);
	if (Result.IsSet())
	{
		return Result;
	}

	for (UGizmoElementBase* Element : Elements)
	{
		if (Element)
		{
			Result = Element->GetPartHittableState(InPartIdentifier);
			if (Result.IsSet())
			{
				return Result;
			}
		}
	}

	return TOptional<bool>();
}

bool UGizmoElementGroup::UpdatePartInteractionState(EGizmoElementInteractionState InInteractionState, uint32 InPartIdentifier)
{
	if (Super::UpdatePartInteractionState(InInteractionState, InPartIdentifier))
	{
		return true;
	}

	for (UGizmoElementBase* Element : Elements)
	{
		if (Element && Element->UpdatePartInteractionState(InInteractionState, InPartIdentifier))
		{
			return true;
		}
	}

	return false;
}


TOptional<EGizmoElementInteractionState> UGizmoElementGroup::GetPartInteractionState(uint32 InPartIdentifier) const
{
	TOptional<EGizmoElementInteractionState> Result = Super::GetPartInteractionState(InPartIdentifier);
	if (Result.IsSet())
	{
		return Result;
	}

	for (UGizmoElementBase* Element : Elements)
	{
		if (Element)
		{ 
			Result = Element->GetPartInteractionState(InPartIdentifier);
			if (Result.IsSet())
			{
				return Result;
			}
		}
	}

	return TOptional<EGizmoElementInteractionState>();
}

void UGizmoElementGroup::SetConstantScale(bool bInConstantScale)
{
	bConstantScale = bInConstantScale;
}

bool UGizmoElementGroup::GetConstantScale() const
{
	return bConstantScale;
}

void UGizmoElementGroup::SetHitOwner(bool bInHitOwner)
{
	bHitOwner = bInHitOwner;
}

bool UGizmoElementGroup::GetHitOwner() const
{
	return bHitOwner;
}
