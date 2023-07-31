// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CineCameraActor.h"
#include "VCamComponent.h"
#include "SimpleVirtualCamera.generated.h"

/**
 * A simple native virtual camera actor
 */
UCLASS(Abstract)
class UE_DEPRECATED(4.26, "This class is deprecated in favor of the Blueprint version")  ASimpleVirtualCamera : public ACineCameraActor
{
	GENERATED_BODY()

public:
	virtual void PostActorCreated() override;

private:
	UPROPERTY(EditAnywhere, Category = "VirtualCamera")
	TObjectPtr<UVCamComponent> VCamComponent = nullptr;
};
