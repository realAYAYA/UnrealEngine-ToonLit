// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CineCameraActor.h"
#include "VCamBaseActor.generated.h"

class UVCamComponent;

/**
 * Utility base class which sets up a UVCamComponent with a Cine Camera.
 * Acts as base class for Blueprint VCamActor, which has some default properties set already. Therefore Abstract and NotPlaceable. 
 */
UCLASS(Abstract, Blueprintable, HideCategories = (Input, VCamComponent, SceneComponent), ShowCategories=("Input|MouseInput", "Input|TouchInput"), ComponentWrapperClass)
class VCAMCORE_API AVCamBaseActor : public ACineCameraActor
{
	GENERATED_BODY()
public:
	
	AVCamBaseActor(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintPure, Category = VCam)
	UVCamComponent* GetVCamComponent() const { return VCamComponent; }
	
private:
	
	UPROPERTY(VisibleAnywhere, Category = VCamBaseActor, BlueprintGetter = GetVCamComponent, meta = (DisplayAfter = "AutoActivateForPlayer", DisplayPriority = 1))
	TObjectPtr<UVCamComponent> VCamComponent = nullptr;
};
