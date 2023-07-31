// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InteractiveGizmo.h"
#include "InteractiveGizmoBuilder.h"
#include "SubTransformProxy.h"
#include "BaseGizmos/GizmoActor.h"
#include "BaseBehaviors/BehaviorTargetInterfaces.h"
#include "BaseBehaviors/AnyButtonInputBehavior.h"
#include "Engine/DirectionalLight.h"
#include "BaseGizmos/CombinedTransformGizmo.h"
#include "SubTransformProxy.h"
#include "BaseGizmos/GizmoBaseComponent.h"

#include "DirectionalLightGizmo.generated.h"

class UGizmoViewContext;

UCLASS()
class UDirectionalLightGizmoBuilder : public UInteractiveGizmoBuilder
{
	GENERATED_BODY()

public:
	virtual UInteractiveGizmo* BuildGizmo(const FToolBuilderState& SceneState) const override;
};

UCLASS()
class ADirectionalLightGizmoActor : public AGizmoActor
{
	GENERATED_BODY()

public:

	ADirectionalLightGizmoActor();

	// The handle to rotate around its y axis
	UGizmoBaseComponent* Arrow;

	// The handle to rotate around the world z axis
	UGizmoBaseComponent* RotationZCircle;
};

/**
 * UDirectionalLightGizmo provides a gizmo to allow for editing directional light properties in viewport
 * Currently supports rotating the light around the world Z axis and its Y Axis
 *
 */
UCLASS()
class UDirectionalLightGizmo : public UInteractiveGizmo, public IHoverBehaviorTarget
{
	GENERATED_BODY()

public:
	// UInteractiveGizmo interface

	UDirectionalLightGizmo();

	virtual void Setup() override;

	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;

	virtual void Shutdown() override;

	USubTransformProxy* GetTransformProxy();

	// IHoverBehaviorTarget interface
	virtual FInputRayHit BeginHoverSequenceHitTest(const FInputDeviceRay& PressPos) override;
	virtual void OnBeginHover(const FInputDeviceRay& DevicePos) override {}
	virtual bool OnUpdateHover(const FInputDeviceRay& DevicePos) override;
	virtual void OnEndHover() override;

	virtual void SetSelectedObject(ADirectionalLight* InLight);
	virtual void SetWorld(UWorld* InWorld);
	virtual void SetGizmoViewContext(UGizmoViewContext* GizmoViewContextIn);

	virtual void OnBeginDrag(const FInputDeviceRay& Ray);
	virtual void OnUpdateDrag(const FInputDeviceRay& Ray);
	virtual void OnEndDrag(const FInputDeviceRay& Ray);

	/** Check if any of the components are hit by the input ray */
	template<typename PtrType>
	bool HitTest(const FRay& Ray, FHitResult& OutHit, FTransform& OutTransform, PtrType& OutHitComponent);

private:

	void CreateGizmoHandles();
	void UpdateGizmoHandles();

	void UpdateHandleColors();

	void OnTransformChanged(UTransformProxy*, FTransform);

	/** Create the gizmo that rotates around the world z axis */
	void CreateZRotationGizmo();

	/** A transform proxy of the LightActor */
	UPROPERTY()
	TObjectPtr<USubTransformProxy> TransformProxy;

	UPROPERTY()
	TObjectPtr<UWorld> World;

	/** The internal actor used by the light gizmo */
	UPROPERTY()
	TObjectPtr<ADirectionalLightGizmoActor> GizmoActor;

	/** Used to properly render the handle gizmo. */
	UPROPERTY()
	TObjectPtr<UGizmoViewContext> GizmoViewContext;

	/** The current target of the gizmo */
	UPROPERTY()
	TObjectPtr<ADirectionalLight> LightActor;

	UPROPERTY()
	bool bIsHovering{ false };

	UPROPERTY()
	bool bIsDragging{ false };

	/** Parameters used during hit testing */
	UPROPERTY()
	FVector DragStartWorldPosition;

	UPROPERTY()
	FVector InteractionStartPoint;

	UPROPERTY()
	float InteractionStartParameter;

	UPROPERTY()
	FVector HitAxis;

	UPROPERTY()
	FVector RotationPlaneX;

	UPROPERTY()
	FVector RotationPlaneZ;

	UPROPERTY()
	TObjectPtr<UGizmoBaseComponent> HitComponent;

	UPROPERTY()
	float ArrowLength{ 120.f };
};


/**
 * A behavior that forwards clicking and dragging to the gizmo.
 */
UCLASS()
class UDirectionalLightGizmoInputBehavior : public UAnyButtonInputBehavior
{
	GENERATED_BODY()

public:
	virtual FInputCapturePriority GetPriority() override { return FInputCapturePriority(FInputCapturePriority::DEFAULT_GIZMO_PRIORITY); }

	virtual void Initialize(UDirectionalLightGizmo* Gizmo);

	virtual FInputCaptureRequest WantsCapture(const FInputDeviceState& input) override;
	virtual FInputCaptureUpdate BeginCapture(const FInputDeviceState& input, EInputCaptureSide eSide) override;
	virtual FInputCaptureUpdate UpdateCapture(const FInputDeviceState& input, const FInputCaptureData& data) override;
	virtual void ForceEndCapture(const FInputCaptureData& data) override;

protected:
	UDirectionalLightGizmo* Gizmo;
	FRay LastWorldRay;
	FVector2D LastScreenPosition;
	bool bInputDragCaptured;

};