// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseGizmos/ScalableSphereGizmo.h"
#include "Engine/PointLight.h"
#include "InteractiveGizmo.h"
#include "SubTransformProxy.h"

#include "PointLightGizmo.generated.h"

UCLASS()
class UPointLightGizmoBuilder : public UInteractiveGizmoBuilder
{
	GENERATED_BODY()

public:
	virtual UInteractiveGizmo* BuildGizmo(const FToolBuilderState& SceneState) const override;
};

/**
 * UPointLightGizmo provides a gizmo to allow for editing point light properties in viewport
 * Currently supports changing the attenuation radius using a USacalableSphereGizmo
 *
 */
UCLASS()
class UPointLightGizmo : public UInteractiveGizmo
{
	GENERATED_BODY()

public:
	// UInteractiveGizmo interface

	virtual void Setup() override;

	virtual void Tick(float DeltaTime) override;

	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;

	virtual void Shutdown() override;

	UPointLightGizmo();

	/**
	 * Set the Target to which the gizmo will be attached
	 */
	void SetSelectedObject(APointLight*InLight);
	
	/**
	 * Create the Attenuation Edit Gizmo
	 */
	void CreateLightGizmo();

	void SetWorld(UWorld *InWorld);

	USubTransformProxy* GetTransformProxy();

private:

	/** The Gizmo that is used to scale the attenuation */
	UPROPERTY()
	TObjectPtr<UScalableSphereGizmo> AttenuationGizmo;

	/** The target Point Light */
	UPROPERTY()
	TObjectPtr<APointLight> LightActor;

	UPROPERTY()
	TObjectPtr<UWorld> World;

	/** A transform proxy to represent the light actor in gizmos */
	UPROPERTY()
	TObjectPtr<USubTransformProxy> TransformProxy;
	
	/**
	 * Function that is called when the radius of the sphere is changed
	 */
	void OnAttenuationUpdate(float NewRadius);

};