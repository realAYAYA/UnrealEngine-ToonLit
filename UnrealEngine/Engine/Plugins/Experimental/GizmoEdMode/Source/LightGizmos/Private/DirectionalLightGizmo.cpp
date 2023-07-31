// Copyright Epic Games, Inc. All Rights Reserved.

#include "DirectionalLightGizmo.h"
#include "BaseGizmos/GizmoBoxComponent.h"
#include "BaseGizmos/GizmoViewContext.h"
#include "Components/SphereComponent.h"
#include "ContextObjectStore.h"
#include "Engine/CollisionProfile.h"
#include "Kismet/KismetMathLibrary.h"
#include "BaseGizmos/GizmoMath.h"
#include "BaseGizmos/GizmoCircleComponent.h"
#include "BaseGizmos/GizmoLineHandleComponent.h"
#include "BaseGizmos/GizmoRenderingUtil.h"
#include "SceneManagement.h"
#include "BaseBehaviors/MouseHoverBehavior.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DirectionalLightGizmo)

#define LOCTEXT_NAMESPACE "UDirectionalLightGizmo"


// UDirectionalLightGizmoBuilder

UInteractiveGizmo* UDirectionalLightGizmoBuilder::BuildGizmo(const FToolBuilderState& SceneState) const
{
	UDirectionalLightGizmo* NewGizmo = NewObject<UDirectionalLightGizmo>(SceneState.GizmoManager);
	NewGizmo->SetWorld(SceneState.World);
	
	UGizmoViewContext* GizmoViewContext = SceneState.ToolManager->GetContextObjectStore()->FindContext<UGizmoViewContext>();
	check(GizmoViewContext && GizmoViewContext->IsValidLowLevel());
	NewGizmo->SetGizmoViewContext(GizmoViewContext);

	return NewGizmo;
}

// ADirectionalLightGizmoActor

ADirectionalLightGizmoActor::ADirectionalLightGizmoActor()
{
	// root component is a hidden sphere
	USphereComponent* SphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("GizmoCenter"));
	RootComponent = SphereComponent;
	SphereComponent->InitSphereRadius(1.0f);
	SphereComponent->SetVisibility(false);
	SphereComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

}

// UDirectionalLightGizmo

UDirectionalLightGizmo::UDirectionalLightGizmo()
{
	LightActor = nullptr;
	GizmoActor = nullptr;
	TransformProxy = nullptr;
}

void UDirectionalLightGizmo::Setup()
{
	UInteractiveGizmo::Setup();

	UDirectionalLightGizmoInputBehavior* DirectionalLightBehavior = NewObject<UDirectionalLightGizmoInputBehavior>(this);
	DirectionalLightBehavior->Initialize(this);
	AddInputBehavior(DirectionalLightBehavior);

	UMouseHoverBehavior* HoverBehavior = NewObject<UMouseHoverBehavior>(this);
	HoverBehavior->Initialize(this);
	AddInputBehavior(HoverBehavior);

	CreateGizmoHandles();

	CreateZRotationGizmo();

	// The Gizmo is being rotated around the Y axis
	RotationPlaneX = FVector::XAxisVector;
	RotationPlaneZ = FVector::ZAxisVector;
	
}

void UDirectionalLightGizmo::Render(IToolsContextRenderAPI* RenderAPI)
{
	FVector Start = GizmoActor->GetActorLocation();
	
	// Parameters of the handle to rotate around y axis
	FVector ArrowDir = GizmoActor->GetActorRotation().Vector();
	FVector ArrowEnd = Start + ArrowDir * ArrowLength;

	FVector ZAxis;
	
	// Checking if the rotation normal is positive or negative Z axis
	if (ArrowEnd.Z < Start.Z)
	{
		ZAxis = -FVector::ZAxisVector;
	}
	else
	{
		ZAxis = FVector::ZAxisVector;
	}

	// Project the arrow end onto the XY plane to get it's direction along that plane
	FVector EndProjection = GizmoMath::ProjectPointOntoPlane(ArrowEnd, Start, ZAxis);

	FVector LineDir = EndProjection - Start;
	LineDir.Normalize();

	// The end point of the line
	FVector LineEnd = Start + LineDir * ArrowLength;

	const FSceneView* View = RenderAPI->GetSceneView();
	float PixelToWorld = GizmoRenderingUtil::CalculateLocalPixelToWorldScale(View, LineEnd);

	// Calculate the "true" end point in world space (We want the line to be the same length in screen space)
	FVector LineEndWorld = Start + LineDir * ArrowLength * PixelToWorld;

	// Draw the line that represents the arrow projected onto the circle
	RenderAPI->GetPrimitiveDrawInterface()->DrawLine(Start, LineEndWorld, FLinearColor::Red, SDPG_Foreground);

	// Figure out the angle of the arc between the line and the arrow
	float ArcAngle = FMath::Acos(FVector::DotProduct(LineDir, ArrowDir));
	ArcAngle = FMath::RadiansToDegrees(ArcAngle);

	// Draw an arc between the arrow and the line
	DrawArc(RenderAPI->GetPrimitiveDrawInterface(), Start, LineDir, ZAxis, 0, ArcAngle, ArrowLength * PixelToWorld, 32, FLinearColor(0.f, 0.f, 1.f), SDPG_Foreground);
}

void UDirectionalLightGizmo::Shutdown()
{
	if (GizmoActor)
	{
		GizmoActor->Destroy();
		GizmoActor = nullptr;
	}
}

USubTransformProxy* UDirectionalLightGizmo::GetTransformProxy()
{
	return TransformProxy;
}

FInputRayHit UDirectionalLightGizmo::BeginHoverSequenceHitTest(const FInputDeviceRay& PressPos)
{
	FHitResult HitResult;
	FTransform DragTransform;

	if (HitTest(PressPos.WorldRay, HitResult, DragTransform, HitComponent))
	{
		bIsHovering = true;
		UpdateHandleColors();
		return FInputRayHit(HitResult.Distance);
	}

	bIsHovering = false;
	// Return invalid ray hit to say we don't want to listen to hover input
	return FInputRayHit();
}

bool UDirectionalLightGizmo::OnUpdateHover(const FInputDeviceRay& DevicePos)
{
	if (!LightActor)
	{
		bIsHovering = false;
		return false;
	}

	FVector Start = DevicePos.WorldRay.Origin;
	const float MaxRaycastDistance = 1e6f;
	FVector End = DevicePos.WorldRay.Origin + DevicePos.WorldRay.Direction * MaxRaycastDistance;

	FRay HitCheckRay(Start, End - Start);
	FHitResult HitResult;
	FTransform DragTransform;

	if (HitTest(HitCheckRay, HitResult, DragTransform, HitComponent))
	{
		bIsHovering = true;
		return true;
	}

	bIsHovering = false;
	return false;
}

void UDirectionalLightGizmo::OnEndHover()
{
	bIsHovering = false;

	UpdateHandleColors();
}

void UDirectionalLightGizmo::SetSelectedObject(ADirectionalLight* InLight)
{
	LightActor = InLight;

	// TODO: Cannot remove a component from Transform Proxy
	if (!TransformProxy)
	{
		TransformProxy = NewObject<USubTransformProxy>(this);
	}

	USceneComponent* SceneComponent = LightActor->GetRootComponent();

	TransformProxy->AddComponent(SceneComponent);

	TransformProxy->OnTransformChanged.AddUObject(this, &UDirectionalLightGizmo::OnTransformChanged);

	OnTransformChanged(TransformProxy, TransformProxy->GetTransform());

}

void UDirectionalLightGizmo::SetWorld(UWorld* InWorld)
{
	World = InWorld;
}

void UDirectionalLightGizmo::SetGizmoViewContext(UGizmoViewContext* GizmoViewContextIn)
{
	GizmoViewContext = GizmoViewContextIn;
}

void UDirectionalLightGizmo::OnBeginDrag(const FInputDeviceRay& Ray)
{
	FVector Start = Ray.WorldRay.Origin;
	const float MaxRaycastDistance = 1e6f;
	FVector End = Ray.WorldRay.Origin + Ray.WorldRay.Direction * MaxRaycastDistance;

	FRay HitCheckRay(Start, End - Start);
	FHitResult HitResult;

	FTransform DragTransform;
	HitComponent = nullptr;

	// Check if any component was hit
	if (HitTest(HitCheckRay, HitResult, DragTransform, HitComponent))
	{
		bIsDragging = true;
		UpdateHandleColors();

		// Rotate around y axis if the arrow was hit
		if (HitComponent == GizmoActor->Arrow)
		{
			HitAxis = LightActor->GetActorRotation().RotateVector(FVector::YAxisVector);

			// Get the rotated plane vectors for the interaction
			GizmoMath::MakeNormalPlaneBasis(HitAxis, RotationPlaneX, RotationPlaneZ);
			RotationPlaneX = LightActor->GetActorRotation().RotateVector(FVector::XAxisVector);
			RotationPlaneZ = LightActor->GetActorRotation().RotateVector(FVector::ZAxisVector);

			GetGizmoManager()->BeginUndoTransaction(LOCTEXT("DirectionalLightYRotation", "Directional Light Y Rotation"));
			
		}
		// Rotate around Z axis if the circle was hit
		else
		{
			HitAxis = FVector::ZAxisVector;
			RotationPlaneX = FVector::XAxisVector;
			RotationPlaneZ = FVector::YAxisVector;

			GetGizmoManager()->BeginUndoTransaction(LOCTEXT("DirectionalLightZRotation", "Directional Light Z Rotation"));
		}

		// Calculate initial hit position
		DragStartWorldPosition = GizmoMath::ProjectPointOntoLine(Ray.WorldRay.PointAt(HitResult.Distance), DragTransform.GetLocation(), HitAxis);

		// Calculate initial hit parameters
		bool bIntersects; 
		FVector IntersectionPoint;

		GizmoMath::RayPlaneIntersectionPoint(
			DragStartWorldPosition, HitAxis,
			Ray.WorldRay.Origin, Ray.WorldRay.Direction,
			bIntersects, IntersectionPoint);

		check(bIntersects);  // need to handle this case...

		InteractionStartPoint = IntersectionPoint;

		InteractionStartParameter = GizmoMath::ComputeAngleInPlane(InteractionStartPoint,
			DragStartWorldPosition, HitAxis, RotationPlaneX, RotationPlaneZ);
	}
}

void UDirectionalLightGizmo::OnUpdateDrag(const FInputDeviceRay& Ray)
{
	bool bIntersects; 
	FVector IntersectionPoint;

	// Calculate current hit parameters
	GizmoMath::RayPlaneIntersectionPoint(
		DragStartWorldPosition, HitAxis,
		Ray.WorldRay.Origin, Ray.WorldRay.Direction,
		bIntersects, IntersectionPoint);

	if (!bIntersects)
	{
		return;
	}

	FVector InteractionCurPoint = IntersectionPoint;

	float InteractionCurAngle = GizmoMath::ComputeAngleInPlane(InteractionCurPoint,
		DragStartWorldPosition, HitAxis, RotationPlaneX, RotationPlaneZ);

	float DeltaAngle = InteractionCurAngle - InteractionStartParameter;

	LightActor->Modify();

	// Rotate around y axis if the arrow was hit
	if (HitComponent == GizmoActor->Arrow)
	{
		FRotator Rotation = FRotator::ZeroRotator;

		Rotation.Pitch = FMath::RadiansToDegrees(DeltaAngle);

		LightActor->AddActorLocalRotation(Rotation);
	}
	// Rotate around Z axis if the circle was hit
	else
	{
		FQuat Rotation(FVector(0, 0, 1), DeltaAngle);
		LightActor->AddActorWorldRotation(Rotation);
	}

	TransformProxy->SetTransform(LightActor->GetTransform());

	InteractionStartPoint = InteractionCurPoint;
	InteractionStartParameter = InteractionCurAngle;

}

void UDirectionalLightGizmo::OnEndDrag(const FInputDeviceRay& Ray)
{
	bIsDragging = false;

	UpdateHandleColors();

	GetGizmoManager()->EndUndoTransaction();
}

template<typename PtrType>
bool UDirectionalLightGizmo::HitTest(const FRay& Ray, FHitResult& OutHit, FTransform& OutTransform, PtrType& OutHitComponent)
{
	FVector Start = Ray.Origin;
	const float MaxRaycastDistance = 1e6f;
	FVector End = Ray.Origin + Ray.Direction * MaxRaycastDistance;

	FCollisionQueryParams Params;

	if (GizmoActor->Arrow->LineTraceComponent(OutHit, Start, End, Params))
	{
		OutTransform = GizmoActor->Arrow->GetComponentTransform();
		OutHitComponent = GizmoActor->Arrow;
		return true;
	}
	else if (GizmoActor->RotationZCircle && GizmoActor->RotationZCircle->LineTraceComponent(OutHit, Start, End, Params))
	{
		OutTransform = GizmoActor->GetTransform();
		OutHitComponent = GizmoActor->RotationZCircle;
		return true;
	}

	return false;
}

void UDirectionalLightGizmo::CreateGizmoHandles()
{
	FActorSpawnParameters SpawnInfo;
	GizmoActor = World->SpawnActor<ADirectionalLightGizmoActor>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnInfo);

	GizmoActor->Arrow = AGizmoActor::AddDefaultLineHandleComponent(World, GizmoActor, GizmoViewContext,
		FLinearColor::Red, FVector::YAxisVector, FVector::XAxisVector, ArrowLength, true);
}

void UDirectionalLightGizmo::UpdateGizmoHandles()
{
	if (GizmoActor && GizmoActor->RotationZCircle)
	{
		GizmoActor->RotationZCircle->SetRelativeRotation(GizmoActor->GetActorRotation().Quaternion().Inverse());
	}
}

void UDirectionalLightGizmo::UpdateHandleColors()
{
	if (bIsHovering || bIsDragging)
	{
		if (HitComponent)
		{
			HitComponent->Color = FLinearColor::Yellow;
			HitComponent->NotifyExternalPropertyUpdates();
		}
	}
	else
	{
		GizmoActor->Arrow->Color = FLinearColor::Red;
		GizmoActor->Arrow->NotifyExternalPropertyUpdates();

		GizmoActor->RotationZCircle->Color = FLinearColor::Blue;
		GizmoActor->RotationZCircle->NotifyExternalPropertyUpdates();

	}
}

void UDirectionalLightGizmo::OnTransformChanged(UTransformProxy*, FTransform)
{
	if (!GizmoActor)
	{
		return;
	}

	USceneComponent* GizmoComponent = GizmoActor->GetRootComponent();

	FTransform TargetTransform = TransformProxy->GetTransform();

	FTransform GizmoTransform = TargetTransform;
	GizmoTransform.SetScale3D(FVector(1, 1, 1));

	GizmoComponent->SetWorldTransform(GizmoTransform);

	UpdateGizmoHandles();
}


void UDirectionalLightGizmo::CreateZRotationGizmo()
{
	UGizmoCircleComponent* NewCircle = NewObject<UGizmoCircleComponent>(GizmoActor);
	GizmoActor->AddInstanceComponent(NewCircle);
	NewCircle->AttachToComponent(GizmoActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	NewCircle->Normal = FVector::ZAxisVector;
	NewCircle->Color = FLinearColor::Blue;
	NewCircle->Radius = 120.f;
	NewCircle->bDrawFullCircle = true;
	NewCircle->RegisterComponent();

	GizmoActor->RotationZCircle = NewCircle;

	UpdateGizmoHandles();
}

// UDirectionalLightGizmoInputBehavior

void UDirectionalLightGizmoInputBehavior::Initialize(UDirectionalLightGizmo* InGizmo)
{
	Gizmo = InGizmo;
}

FInputCaptureRequest UDirectionalLightGizmoInputBehavior::WantsCapture(const FInputDeviceState& input)
{
	if (IsPressed(input))
	{
		FHitResult HitResult;
		FTransform DragTransform;
		UGizmoBaseComponent* HitComponent = nullptr;
		if (Gizmo->HitTest(input.Mouse.WorldRay, HitResult, DragTransform, HitComponent))
		{
			return FInputCaptureRequest::Begin(this, EInputCaptureSide::Any, HitResult.Distance);
		}
	}

	return FInputCaptureRequest::Ignore();
}

FInputCaptureUpdate UDirectionalLightGizmoInputBehavior::BeginCapture(const FInputDeviceState& input, EInputCaptureSide eSide)
{
	FInputDeviceRay DeviceRay(input.Mouse.WorldRay, input.Mouse.Position2D);
	LastWorldRay = DeviceRay.WorldRay;
	LastScreenPosition = DeviceRay.ScreenPosition;

	Gizmo->OnBeginDrag(DeviceRay);
	bInputDragCaptured = true;
	return FInputCaptureUpdate::Begin(this, EInputCaptureSide::Any);
}

FInputCaptureUpdate UDirectionalLightGizmoInputBehavior::UpdateCapture(const FInputDeviceState& input, const FInputCaptureData& data)
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

void UDirectionalLightGizmoInputBehavior::ForceEndCapture(const FInputCaptureData& data)
{
	if (bInputDragCaptured)
	{
		Gizmo->OnEndDrag(FInputDeviceRay(LastWorldRay));
		bInputDragCaptured = false;
	}
}

#undef LOCTEXT_NAMESPACE
