// Copyright Epic Games, Inc. All Rights Reserved.
 
#include "BaseGizmos/GizmoElementLineBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GizmoElementLineBase)

DEFINE_LOG_CATEGORY_STATIC(LogGizmoElementLineBase, Log, All);

bool UGizmoElementLineBase::UpdateRenderState(IToolsContextRenderAPI* RenderAPI, const FVector& InLocalOrigin, FRenderTraversalState& InOutRenderState)
{
	InOutRenderState.LineRenderState.Update(LineRenderAttributes);

	return Super::UpdateRenderState(RenderAPI, InLocalOrigin, InOutRenderState);
}

float UGizmoElementLineBase::GetCurrentLineThickness(bool bPerspectiveView, float InViewFOV) const
{
	float CurrentLineThickness;

	if (ElementInteractionState == EGizmoElementInteractionState::Hovering)
	{
		CurrentLineThickness = (LineThickness > 0.0f ? LineThickness * HoverLineThicknessMultiplier : HoverLineThicknessMultiplier);
	}
	else if (ElementInteractionState == EGizmoElementInteractionState::Interacting)
	{
		CurrentLineThickness = (LineThickness > 0.0f ? LineThickness * InteractLineThicknessMultiplier : InteractLineThicknessMultiplier);
	}
	else
	{
		CurrentLineThickness = LineThickness;
	}

	if (bPerspectiveView)
	{
		CurrentLineThickness *= (InViewFOV / 90.0f);		// compensate for FOV scaling in Gizmos...
	}

	return CurrentLineThickness;
}

void UGizmoElementLineBase::SetLineThickness(float InLineThickness)
{
	if (InLineThickness < 0.0f)
	{
		UE_LOG(LogGizmoElementLineBase, Warning, TEXT("Invalid gizmo element line thickness %f, will be set to 0.0."), InLineThickness);
		LineThickness = 0.0f;
	}
	else
	{
		LineThickness = InLineThickness;
	}
}

float UGizmoElementLineBase::GetLineThickness() const
{
	return LineThickness;
}

void UGizmoElementLineBase::SetHoverLineThicknessMultiplier(float InHoverLineThicknessMultiplier)
{
	HoverLineThicknessMultiplier = InHoverLineThicknessMultiplier;
}

float UGizmoElementLineBase::GetHoverLineThicknessMultiplier() const
{
	return HoverLineThicknessMultiplier;
}

void UGizmoElementLineBase::SetInteractLineThicknessMultiplier(float InInteractLineThicknessMultiplier)
{
	InteractLineThicknessMultiplier = InInteractLineThicknessMultiplier;
}

float UGizmoElementLineBase::GetInteractLineThicknessMultiplier() const
{
	return InteractLineThicknessMultiplier;
}

void UGizmoElementLineBase::SetScreenSpaceLine(bool bInScreenSpaceLine)
{
	bScreenSpaceLine = bInScreenSpaceLine;
}

bool UGizmoElementLineBase::GetScreenSpaceLine() const
{
	return bScreenSpaceLine;
}

void UGizmoElementLineBase::SetLineColor(FLinearColor InLineColor, bool InOverridesChildState)
{
	LineRenderAttributes.LineColor.SetColor(InLineColor, InOverridesChildState);
}

FLinearColor UGizmoElementLineBase::GetLineColor() const
{
	return LineRenderAttributes.LineColor.GetColor();
}

bool UGizmoElementLineBase::HasLineColor() const
{
	return LineRenderAttributes.LineColor.bHasValue;
}

bool UGizmoElementLineBase::DoesLineColorOverrideChildState() const
{
	return LineRenderAttributes.LineColor.bOverridesChildState;
}

void UGizmoElementLineBase::ClearLineColor()
{
	LineRenderAttributes.LineColor.Reset();
}

void UGizmoElementLineBase::SetHoverLineColor(FLinearColor InHoverLineColor, bool InOverridesChildState)
{
	LineRenderAttributes.HoverLineColor.SetColor(InHoverLineColor, InOverridesChildState);
}

FLinearColor UGizmoElementLineBase::GetHoverLineColor() const
{
	return LineRenderAttributes.HoverLineColor.GetColor();
}
bool UGizmoElementLineBase::HasHoverLineColor() const
{
	return LineRenderAttributes.HoverLineColor.bHasValue;
}

bool UGizmoElementLineBase::DoesHoverLineColorOverrideChildState() const
{
	return LineRenderAttributes.HoverLineColor.bOverridesChildState;
}

void UGizmoElementLineBase::ClearHoverLineColor()
{
	LineRenderAttributes.HoverLineColor.Reset();
}

void UGizmoElementLineBase::SetInteractLineColor(FLinearColor InInteractLineColor, bool InOverridesChildState)
{
	LineRenderAttributes.InteractLineColor.SetColor(InInteractLineColor, InOverridesChildState);
}

FLinearColor UGizmoElementLineBase::GetInteractLineColor() const
{
	return LineRenderAttributes.InteractLineColor.GetColor();
}
bool UGizmoElementLineBase::HasInteractLineColor() const
{
	return LineRenderAttributes.InteractLineColor.bHasValue;
}

bool UGizmoElementLineBase::DoesInteractLineColorOverrideChildState() const
{
	return LineRenderAttributes.InteractLineColor.bOverridesChildState;
}

void UGizmoElementLineBase::ClearInteractLineColor()
{
	LineRenderAttributes.InteractLineColor.Reset();
}



