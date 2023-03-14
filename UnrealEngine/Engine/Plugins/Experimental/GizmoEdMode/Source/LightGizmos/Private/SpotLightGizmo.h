// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ScalableConeGizmo.h"
#include "Engine/SpotLight.h"
#include "InteractiveGizmo.h"
#include "SubTransformProxy.h"
#include "BaseBehaviors/BehaviorTargetInterfaces.h"
#include "BaseGizmos/GizmoBaseComponent.h"

#include "SpotLightGizmo.generated.h"

class UGizmoViewContext;

UCLASS()
class USpotLightGizmoBuilder : public UInteractiveGizmoBuilder
{
	GENERATED_BODY()

public:
	virtual UInteractiveGizmo* BuildGizmo(const FToolBuilderState& SceneState) const override;
};

UCLASS()
class ASpotLightGizmoActor : public AGizmoActor
{
	GENERATED_BODY()

public:

	ASpotLightGizmoActor();

	 // The handle to drag and scale the attenuation
	UGizmoBaseComponent* AttenuationScaleHandle;
};


/**
 * USpotLightGizmo provides a gizmo to allow for editing spot light properties in viewport
 * Currently supports changing the inner and outer cone angle and scaling the attenuation radius
 *
 */
UCLASS()
class USpotLightGizmo : public UInteractiveGizmo, public IHoverBehaviorTarget
{
	GENERATED_BODY()

public:
	// UInteractiveGizmo interface

	virtual void Setup() override;

	virtual void Tick(float DeltaTime) override;

	virtual void Shutdown() override;

	// IHoverBehaviorTarget interface
	virtual FInputRayHit BeginHoverSequenceHitTest(const FInputDeviceRay& PressPos) override;
	virtual void OnBeginHover(const FInputDeviceRay& DevicePos) override {}
	virtual bool OnUpdateHover(const FInputDeviceRay& DevicePos) override;
	virtual void OnEndHover() override;

	USpotLightGizmo();

	void SetSelectedObject(ASpotLight* InLight);

	/** Create a cone gizmo to change the outer angle of the spotlight */
	void CreateOuterAngleGizmo();

	/** Create a cone gizmo to change the inner angle of the spotlight */
	void CreateInnerAngleGizmo();

	/** Create a  gizmo to change the attenuation */
	void CreateAttenuationScaleGizmo();

	void SetWorld(UWorld* InWorld);
	void SetGizmoViewContext(UGizmoViewContext* GizmoViewContextIn);

	USubTransformProxy* GetTransformProxy();

	virtual void OnBeginDrag(const FInputDeviceRay& Ray);
	virtual void OnUpdateDrag(const FInputDeviceRay& Ray);
	virtual void OnEndDrag(const FInputDeviceRay& Ray);

	/** Check if the Input ray hit any of the components of the gizmo*/
	bool HitTest(const FRay& Ray, FHitResult& OutHit, FTransform& OutTransform);

private:

	/** The current target light the gizmo is attached to*/
	UPROPERTY()
	TObjectPtr<ASpotLight> LightActor;

	UPROPERTY()
	TObjectPtr<UWorld> World;

	/** A transform proxy to use with other gizmos*/
	UPROPERTY()
	TObjectPtr<USubTransformProxy> TransformProxy;

	/** The gizmo to change the outer angle of the spotlight */
	UPROPERTY()
	TObjectPtr<UScalableConeGizmo> OuterAngleGizmo;

	/** The gizmo to change the inner angle of the spotlight */
	UPROPERTY()
	TObjectPtr<UScalableConeGizmo> InnerAngleGizmo;

	/** The internal gizmo actor that is used by the gizmo 
	 *  We need a GizmoActor separate from the 2 UScalableConeGizmo's
	 *  so that we can have one handle that scales the attenuation for
	 *  both of them simultaneously
	 */
	UPROPERTY()
	TObjectPtr<ASpotLightGizmoActor> GizmoActor;

	/** Used to properly render the handle gizmo. */
	UPROPERTY()
	TObjectPtr<UGizmoViewContext> GizmoViewContext;

	UPROPERTY()
	FVector DragStartWorldPosition;

	UPROPERTY()
	float InteractionStartParameter;

	UPROPERTY()
	bool bIsHovering{ false };

	UPROPERTY()
	bool bIsDragging{ false };

	void OnOuterAngleUpdate(float NewAngle);
	void OnInnerAngleUpdate(float NewAngle);
	void OnTransformChanged(UTransformProxy*, FTransform);

	void UpdateHandleColors();
};

/**
 * A behavior that forwards clicking and dragging to the gizmo.
 */
UCLASS()
class USpotLightGizmoInputBehavior : public UAnyButtonInputBehavior
{
	GENERATED_BODY()

public:
	virtual FInputCapturePriority GetPriority() override { return FInputCapturePriority(FInputCapturePriority::DEFAULT_GIZMO_PRIORITY); }

	virtual void Initialize(USpotLightGizmo* Gizmo);

	virtual FInputCaptureRequest WantsCapture(const FInputDeviceState& input) override;
	virtual FInputCaptureUpdate BeginCapture(const FInputDeviceState& input, EInputCaptureSide eSide) override;
	virtual FInputCaptureUpdate UpdateCapture(const FInputDeviceState& input, const FInputCaptureData& data) override;
	virtual void ForceEndCapture(const FInputCaptureData& data) override;

protected:
	USpotLightGizmo* Gizmo;
	FRay LastWorldRay;
	FVector2D LastScreenPosition;
	bool bInputDragCaptured;

};