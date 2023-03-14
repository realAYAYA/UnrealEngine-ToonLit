// Copyright Epic Games, Inc. All Rights Reserved.

#include "ScalableConeGizmo.h"
#include "Components/SphereComponent.h"
#include "BaseGizmos/GizmoBoxComponent.h"
#include "Engine/CollisionProfile.h"
#include "BaseGizmos/GizmoMath.h"
#include "BaseGizmos/GizmoRenderingUtil.h"
#include "BaseBehaviors/MouseHoverBehavior.h"
#include "SceneManagement.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ScalableConeGizmo)

#define LOCTEXT_NAMESPACE "UScalableSphereGizmo"

// UScalableConeGizmoBuilder

UInteractiveGizmo* UScalableConeGizmoBuilder::BuildGizmo(const FToolBuilderState& SceneState) const
{
	UScalableConeGizmo* NewGizmo = NewObject<UScalableConeGizmo>(SceneState.GizmoManager);
	return NewGizmo;
}
// UScalableConeGizmo

void UScalableConeGizmo::Setup()
{
	UInteractiveGizmo::Setup();

	Length = 1000.0f;
	Angle = 45.f;
	MaxAngle = 90.f;
	MinAngle = 0.f;
	ConeColor = FColor(200, 255, 255);

	TransactionDescription = LOCTEXT("ScalableConeGizmo", "Scale Cone Gizmo");

	UScalableConeGizmoInputBehavior* ScalableConeBehavior = NewObject<UScalableConeGizmoInputBehavior>(this);
	ScalableConeBehavior->Initialize(this);
	AddInputBehavior(ScalableConeBehavior);

	UMouseHoverBehavior* HoverBehavior = NewObject<UMouseHoverBehavior>(this);
	HoverBehavior->Initialize(this);
	AddInputBehavior(HoverBehavior);
}

void UScalableConeGizmo::Render(IToolsContextRenderAPI* RenderAPI)
{
	if (ActiveTarget)
	{
		DrawWireSphereCappedCone(RenderAPI->GetPrimitiveDrawInterface(), ActiveTarget->GetTransform(), Length, Angle, 32, 8, 10, ConeColor, SDPG_Foreground);

		// Draw the yellow circle as a hightlight when hovering or dragging
		if (bIsHovering || bIsDragging)
		{
			FVector WorldOrigin = ActiveTarget->GetTransform().GetLocation();
			FVector CircleNormal = ActiveTarget->GetTransform().GetRotation().Vector();

			float ConeHeight = Length * FMath::Cos(FMath::DegreesToRadians(Angle));
			float ConeRadius = Length * FMath::Sin(FMath::DegreesToRadians(Angle));

			// The center of the circle at the base of the cone
			FVector CircleOrigin = WorldOrigin + CircleNormal * ConeHeight;

			FVector CircleX;
			FVector CircleY;

			// Direction vectors to represent the circle
			GizmoMath::MakeNormalPlaneBasis(CircleNormal, CircleX, CircleY);

			DrawCircle(RenderAPI->GetPrimitiveDrawInterface(), CircleOrigin, CircleX, CircleY, FLinearColor::Yellow, ConeRadius, 32, SDPG_Foreground, 0, 0.1f);
		}

		// Draw the lines to show which direction to drag in while hovering
		if (bIsHovering)
		{
			// Parameters for the line that shows the drag direction
			FVector LineStart = DragCurrentPositionProjected;
			float LineLength = 30.f;
			FVector LineEnd = LineStart + HitAxis * LineLength;

			// Get the Pixel to World scale of the line
			const FSceneView* View = RenderAPI->GetSceneView();
			float PixelToWorld = GizmoRenderingUtil::CalculateLocalPixelToWorldScale(View, LineEnd);

			// Draw the lines in both directions
			RenderAPI->GetPrimitiveDrawInterface()->DrawLine(LineStart, LineStart + HitAxis * PixelToWorld * LineLength, FLinearColor::Red, SDPG_Foreground);
			RenderAPI->GetPrimitiveDrawInterface()->DrawLine(LineStart, LineStart - HitAxis * PixelToWorld * LineLength, FLinearColor::Red, SDPG_Foreground);
		}
	}
}

void UScalableConeGizmo::SetTarget(UTransformProxy* InTarget)
{
	ActiveTarget = InTarget;
}

void UScalableConeGizmo::SetAngleDegrees(float InAngle)
{
	Angle = InAngle;

	Angle = FMath::Clamp<float>(Angle, MinAngle, MaxAngle);

	if (UpdateAngleFunc)
	{
		UpdateAngleFunc(InAngle);
	}
}

void UScalableConeGizmo::SetLength(float InLength)
{
	Length = InLength;
}

float UScalableConeGizmo::GetLength()
{
	return Length;
}

float UScalableConeGizmo::GetAngleDegrees()
{
	return Angle;
}

FInputRayHit UScalableConeGizmo::BeginHoverSequenceHitTest(const FInputDeviceRay& PressPos)
{
	FHitResult HitResult;
	FVector TestHitAxis;
	FTransform DragTransform;

	if (HitTest(PressPos.WorldRay, HitResult, TestHitAxis, DragTransform))
	{
		bIsHovering = true;
		DragStartWorldPosition = DragTransform.GetLocation();
		DragCurrentPositionProjected = DragStartWorldPosition;
		HitAxis = TestHitAxis;

		return FInputRayHit(HitResult.Distance);
	}

	bIsHovering = false;
	// Return invalid ray hit to say we don't want to listen to hover input
	return FInputRayHit();
}

bool UScalableConeGizmo::OnUpdateHover(const FInputDeviceRay& DevicePos)
{
	if (!ActiveTarget)
	{
		bIsHovering = false;
		return false;
	}

	FVector Start = DevicePos.WorldRay.Origin;
	const float MaxRaycastDistance = 1e6f;
	FVector End = DevicePos.WorldRay.Origin + DevicePos.WorldRay.Direction * MaxRaycastDistance;

	FRay HitCheckRay(Start, End - Start);
	FHitResult HitResult;
	FVector TestHitAxis;
	FTransform DragTransform;

	if (HitTest(HitCheckRay, HitResult, TestHitAxis, DragTransform))
	{
		bIsHovering = true;
		DragStartWorldPosition = DragTransform.GetLocation();
		DragCurrentPositionProjected = DragStartWorldPosition;
		HitAxis = TestHitAxis;
		return true;
	}

	bIsHovering = false;
	return false;
}

void UScalableConeGizmo::OnEndHover()
{
	bIsHovering = false;
}

bool UScalableConeGizmo::HitTest(const FRay& Ray, FHitResult& OutHit, FVector& OutAxis, FTransform& OutTransform)
{
	if (!ActiveTarget)
	{
		return false;
	}

	FVector Start = Ray.Origin;
	const float MaxRaycastDistance = 1e6f;
	FVector End = Ray.Origin + Ray.Direction * MaxRaycastDistance;

	// Find the intresection with the circle plane. Note that unlike the FMath version, GizmoMath::RayPlaneIntersectionPoint() 
	// checks that the ray isn't parallel to the plane.
	FVector WorldOrigin = ActiveTarget->GetTransform().GetLocation();
	FVector CircleNormal = ActiveTarget->GetTransform().GetRotation().Vector();

	float ConeHeight = Length * FMath::Cos(FMath::DegreesToRadians(Angle));
	float ConeRadius = Length * FMath::Sin(FMath::DegreesToRadians(Angle));

	// The center of the base of the cone
	FVector CircleOrigin = WorldOrigin + CircleNormal * ConeHeight;
	
	FVector ConeBottom = WorldOrigin + CircleNormal * Length;

	FVector HitPos;
	bool bIntersects = false;

	// Figure out if the ray interesects with the plane at the base of the cone
	GizmoMath::RayPlaneIntersectionPoint(CircleOrigin, CircleNormal, Ray.Origin, Ray.Direction, bIntersects, HitPos);

	if (!bIntersects || Ray.GetParameter(HitPos) > Ray.GetParameter(End))
	{
		return false;
	}

	// Find the closest point on the circle of the intersection
	FVector NearestCircle;
	GizmoMath::ClosetPointOnCircle(HitPos, CircleOrigin, CircleNormal, ConeRadius, NearestCircle);

	FVector NearestRay = Ray.ClosestPoint(NearestCircle);

	// Make sure the distance to the ray is within a certain threshold
	double Distance = FVector::Distance(NearestCircle, NearestRay);

	if (Distance > HitErrorThreshold)
	{
		return false;
	}

	OutAxis = NearestCircle - ConeBottom;
	OutAxis.Normalize();

	OutTransform.SetIdentity();
	OutTransform.SetTranslation(NearestCircle);
	return true;
}

void UScalableConeGizmo::OnBeginDrag(const FInputDeviceRay& Ray)
{
	FVector Start = Ray.WorldRay.Origin;
	const float MaxRaycastDistance = 1e6f;
	FVector End = Ray.WorldRay.Origin + Ray.WorldRay.Direction * MaxRaycastDistance;

	FRay HitCheckRay(Start, End - Start);
	FHitResult HitResult;
	FTransform DragTransform;
	FVector HitTestAxis;

	 // Check if any component was hit
	if (HitTest(HitCheckRay, HitResult, HitTestAxis, DragTransform))
	{
		HitAxis = HitTestAxis;

		FVector RayNearestPt; float RayNearestParam;

		// Get the initial interaction parameters
		GizmoMath::NearestPointOnLineToRay(DragTransform.GetLocation(), HitAxis,
			Ray.WorldRay.Origin, Ray.WorldRay.Direction,
			InteractionStartPoint, InteractionStartParameter,
			RayNearestPt, RayNearestParam);

		DragStartWorldPosition = DragTransform.GetLocation();
		DragCurrentPositionProjected = DragStartWorldPosition;

		bIsDragging = true;

		GetGizmoManager()->BeginUndoTransaction(TransactionDescription);
	}
}

void UScalableConeGizmo::OnUpdateDrag(const FInputDeviceRay& Ray)
{
	FVector AxisNearestPt; float AxisNearestParam;
	FVector RayNearestPt; float RayNearestParam;

	// Get the current interaction parameters
	GizmoMath::NearestPointOnLineToRay(DragStartWorldPosition, HitAxis,
		Ray.WorldRay.Origin, Ray.WorldRay.Direction,
		AxisNearestPt, AxisNearestParam,
		RayNearestPt, RayNearestParam);

	FVector GizmoLocation = ActiveTarget->GetTransform().GetLocation();

	// Vector to the starting position of the interaction
	FVector Start = InteractionStartPoint - GizmoLocation;
	Start.Normalize();

	// Vector to the ending position of the interaction
	FVector End = AxisNearestPt - GizmoLocation;
	End.Normalize();

	float DotP = FVector::DotProduct(Start, End);

	float DeltaAngle = FMath::Acos(DotP);

	// Get the angle between the start and end vectors and the forward vector to check if the drag direction should be +ve or -ve

	FVector Forward = ActiveTarget->GetTransform().GetRotation().Vector();

	float StartAngle = FMath::Acos(FVector::DotProduct(Start, Forward));
	float EndAngle = FMath::Acos(FVector::DotProduct(End, Forward));

	if (StartAngle > EndAngle)
	{
		DeltaAngle = -DeltaAngle;
	}

	SetAngleDegrees(Angle + FMath::RadiansToDegrees(DeltaAngle));

	InteractionStartPoint = AxisNearestPt;
	DragCurrentPositionProjected = AxisNearestPt;
	InteractionStartParameter = AxisNearestParam;
}

void UScalableConeGizmo::OnEndDrag(const FInputDeviceRay& Ray)
{
	GetGizmoManager()->EndUndoTransaction();
	bIsDragging = false;
}

// UScalableConeGizmoInputBehavior

void UScalableConeGizmoInputBehavior::Initialize(UScalableConeGizmo* InGizmo)
{
	Gizmo = InGizmo;
}

FInputCaptureRequest UScalableConeGizmoInputBehavior::WantsCapture(const FInputDeviceState& input)
{
	if (IsPressed(input))
	{
		FHitResult HitResult;
		FVector HitTestAxis;
		FTransform DragTransform;
		if (Gizmo->HitTest(input.Mouse.WorldRay, HitResult, HitTestAxis, DragTransform))
		{
			return FInputCaptureRequest::Begin(this, EInputCaptureSide::Any, HitResult.Distance);
		}
	}

	return FInputCaptureRequest::Ignore();
}

FInputCaptureUpdate UScalableConeGizmoInputBehavior::BeginCapture(const FInputDeviceState& input, EInputCaptureSide eSide)
{
	FInputDeviceRay DeviceRay(input.Mouse.WorldRay, input.Mouse.Position2D);
	LastWorldRay = DeviceRay.WorldRay;
	LastScreenPosition = DeviceRay.ScreenPosition;

	Gizmo->OnBeginDrag(DeviceRay);
	bInputDragCaptured = true;
	return FInputCaptureUpdate::Begin(this, EInputCaptureSide::Any);
}

FInputCaptureUpdate UScalableConeGizmoInputBehavior::UpdateCapture(const FInputDeviceState& input, const FInputCaptureData& data)
{
	FInputDeviceRay DeviceRay(input.Mouse.WorldRay, input.Mouse.Position2D);
	LastWorldRay = DeviceRay.WorldRay;
	LastScreenPosition = DeviceRay.ScreenPosition;

	if (IsReleased(input))
	{
		bInputDragCaptured = false;
		Gizmo->OnEndDrag(FInputDeviceRay(LastWorldRay));
		return FInputCaptureUpdate::End();
	}

	Gizmo->OnUpdateDrag(FInputDeviceRay(LastWorldRay));

	return FInputCaptureUpdate::Continue();
}

void UScalableConeGizmoInputBehavior::ForceEndCapture(const FInputCaptureData& data)
{
	if (bInputDragCaptured)
	{
		bInputDragCaptured = false;
		Gizmo->OnEndDrag(FInputDeviceRay(LastWorldRay));
	}
}

#undef LOCTEXT_NAMESPACE
