// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/Light.h"
#include "DirectionalLight.generated.h"

class UArrowComponent;
class UDirectionalLightComponent;

/**
 * Implements a directional light actor.
 */
UCLASS(ClassGroup=(Lights, DirectionalLights), MinimalAPI, meta=(ChildCanTick))
class ADirectionalLight
	: public ALight
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITORONLY_DATA
	// Reference to editor visualization arrow
private:
	UPROPERTY()
	TObjectPtr<UArrowComponent> ArrowComponent;

	/* EditorOnly reference to the light component to allow it to be displayed in the details panel correctly */
	UPROPERTY(VisibleAnywhere, Category="Light")
	TObjectPtr<UDirectionalLightComponent> DirectionalLightComponent;
#endif

public:

	// UObject Interface
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void LoadedFromAnotherClass(const FName& OldClassName) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual bool CanChangeIsSpatiallyLoadedFlag() const override { return false; }
	virtual FBox GetStreamingBounds() const override { return FBox(ForceInit); }
#endif

public:

#if WITH_EDITORONLY_DATA
	/** Returns ArrowComponent subobject **/
	UArrowComponent* GetArrowComponent() const { return ArrowComponent; }

	/** Returns SkyAtmosphereComponent subobject */
	UDirectionalLightComponent* GetComponent() const { return DirectionalLightComponent; }
#endif
};
