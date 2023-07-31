// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseGizmos/PlanePositionGizmo.h"
#include "InteractiveGizmoManager.h"
#include "BaseBehaviors/ClickDragBehavior.h"
#include "BaseBehaviors/MouseHoverBehavior.h"
#include "BaseGizmos/GizmoMath.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PlanePositionGizmo)



UInteractiveGizmo* UPlanePositionGizmoBuilder::BuildGizmo(const FToolBuilderState& SceneState) const
{
	UPlanePositionGizmo* NewGizmo = NewObject<UPlanePositionGizmo>(SceneState.GizmoManager);
	return NewGizmo;
}




void UPlanePositionGizmo::Setup()
{
	UInteractiveGizmo::Setup();

	// Add default mouse input behavior
	MouseBehavior = NewObject<UClickDragInputBehavior>();
	MouseBehavior->Initialize(this);
	MouseBehavior->SetDefaultPriority(FInputCapturePriority(FInputCapturePriority::DEFAULT_GIZMO_PRIORITY));
	AddInputBehavior(MouseBehavior);
	UMouseHoverBehavior* HoverBehavior = NewObject<UMouseHoverBehavior>();
	HoverBehavior->Initialize(this);
	HoverBehavior->SetDefaultPriority(FInputCapturePriority(FInputCapturePriority::DEFAULT_GIZMO_PRIORITY));
	AddInputBehavior(HoverBehavior);

	AxisSource = NewObject<UGizmoConstantAxisSource>(this);
	ParameterSource = NewObject<UGizmoLocalVec2ParameterSource>(this);
	HitTarget = NewObject<UGizmoComponentHitTarget>(this);
	StateTarget = NewObject<UGizmoNilStateTarget>(this);

	bInInteraction = false;
}

FInputRayHit UPlanePositionGizmo::CanBeginClickDragSequence(const FInputDeviceRay& PressPos)
{
	FInputRayHit GizmoHit;
	if (HitTarget && AxisSource && ParameterSource)
	{
		GizmoHit = HitTarget->IsHit(PressPos);
		if (GizmoHit.bHit)
		{
			LastHitPosition = PressPos.WorldRay.PointAt(GizmoHit.HitDepth);
		}
	}
	return GizmoHit;
}

void UPlanePositionGizmo::OnClickPress(const FInputDeviceRay& PressPos)
{
	InteractionOrigin = LastHitPosition;
	InteractionNormal = AxisSource->GetDirection();
	if (AxisSource->HasTangentVectors())
	{
		AxisSource->GetTangentVectors(InteractionAxisX, InteractionAxisY);
	}
	else
	{
		GizmoMath::MakeNormalPlaneBasis(InteractionNormal, InteractionAxisX, InteractionAxisY);
	}

	bool bIntersects; FVector IntersectionPoint;
	GizmoMath::RayPlaneIntersectionPoint(
		InteractionOrigin, InteractionNormal,
		PressPos.WorldRay.Origin, PressPos.WorldRay.Direction,
		bIntersects, IntersectionPoint);
	if (!bIntersects)
	{
		// Generally should not happen since the user clicked the plane to start the interaction, but could happen in a floating point error edge case
		bInInteraction = false;
		return;
	}

	InteractionStartPoint = InteractionCurPoint = IntersectionPoint;

	FVector AxisOrigin = AxisSource->GetOrigin();

	double DirectionSignX = FVector::DotProduct(InteractionStartPoint - AxisOrigin, InteractionAxisX);
	ParameterSigns.X = (bEnableSignedAxis && DirectionSignX < 0) ? -1.0 : 1.0;
	ParameterSigns.X *= (bFlipX) ? -1.0 : 1.0;
	double DirectionSignY = FVector::DotProduct(InteractionStartPoint - AxisOrigin, InteractionAxisY);
	ParameterSigns.Y = (bEnableSignedAxis && DirectionSignY < 0) ? -1.0 : 1.0;
	ParameterSigns.Y *= (bFlipY) ? -1.0 : 1.0;

	InteractionStartParameter = GizmoMath::ComputeCoordinatesInPlane(IntersectionPoint,
			InteractionOrigin, InteractionNormal, InteractionAxisX, InteractionAxisY);

	// Figure out how the parameters would need to be adjusted to bring the axis origin to the
	// interaction start point. This is used when aligning the axis origin to a custom destination.
	FVector2D OriginParamValue = GizmoMath::ComputeCoordinatesInPlane(AxisOrigin,
		InteractionOrigin, InteractionNormal, InteractionAxisX, InteractionAxisY);
	InteractionStartOriginParameterOffset = InteractionStartParameter - OriginParamValue;

	InteractionStartParameter.X *= ParameterSigns.X;
	InteractionStartParameter.Y *= ParameterSigns.Y;
	InteractionCurParameter = InteractionStartParameter;

	InitialTargetParameter = ParameterSource->GetParameter();
	ParameterSource->BeginModify();

	bInInteraction = true;

	if (StateTarget)
	{
		StateTarget->BeginUpdate();
	}
}

void UPlanePositionGizmo::OnClickDrag(const FInputDeviceRay& DragPos)
{
	if (!bInInteraction)
	{
		return;
	}

	FVector HitPoint;
	FVector2D NewParamValue;

	// See if we should use the custom destination function.
	FCustomDestinationParams Params;
	Params.WorldRay = &DragPos.WorldRay;
	if (ShouldUseCustomDestinationFunc() && CustomDestinationFunc(Params, HitPoint))
	{
		InteractionCurPoint = GizmoMath::ProjectPointOntoPlane(HitPoint, InteractionOrigin, InteractionNormal);
		InteractionCurParameter = GizmoMath::ComputeCoordinatesInPlane(InteractionCurPoint,
			InteractionOrigin, InteractionNormal, InteractionAxisX, InteractionAxisY);

		InteractionCurParameter += InteractionStartOriginParameterOffset;
	}
	else
	{
		bool bIntersects; 
		GizmoMath::RayPlaneIntersectionPoint(
			InteractionOrigin, InteractionNormal,
			DragPos.WorldRay.Origin, DragPos.WorldRay.Direction,
			bIntersects, HitPoint);

		if (bIntersects == false)
		{
			return;
		}
		InteractionCurPoint = HitPoint;

		InteractionCurParameter = GizmoMath::ComputeCoordinatesInPlane(InteractionCurPoint,
			InteractionOrigin, InteractionNormal, InteractionAxisX, InteractionAxisY);
		InteractionCurParameter.X *= ParameterSigns.X;
		InteractionCurParameter.Y *= ParameterSigns.Y;
	}

	FVector2D DeltaParam = InteractionCurParameter - InteractionStartParameter;
	NewParamValue = InitialTargetParameter + DeltaParam;

	ParameterSource->SetParameter(NewParamValue);
}

void UPlanePositionGizmo::OnClickRelease(const FInputDeviceRay& ReleasePos)
{
	if (!bInInteraction)
	{
		return;
	}

	ParameterSource->EndModify();
	if (StateTarget)
	{
		StateTarget->EndUpdate();
	}
	bInInteraction = false;
}


void UPlanePositionGizmo::OnTerminateDragSequence()
{
	if (!bInInteraction)
	{
		return;
	}

	ParameterSource->EndModify();
	if (StateTarget)
	{
		StateTarget->EndUpdate();
	}
	bInInteraction = false;
}





FInputRayHit UPlanePositionGizmo::BeginHoverSequenceHitTest(const FInputDeviceRay& PressPos)
{
	FInputRayHit GizmoHit;
	if (HitTarget)
	{
		GizmoHit = HitTarget->IsHit(PressPos);
	}
	return GizmoHit;
}

void UPlanePositionGizmo::OnBeginHover(const FInputDeviceRay& DevicePos)
{
	HitTarget->UpdateHoverState(true);
}

bool UPlanePositionGizmo::OnUpdateHover(const FInputDeviceRay& DevicePos)
{
	// not necessary...
	HitTarget->UpdateHoverState(true);
	return true;
}

void UPlanePositionGizmo::OnEndHover()
{
	HitTarget->UpdateHoverState(false);
}

