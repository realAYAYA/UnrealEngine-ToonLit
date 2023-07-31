// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "VCamModifierInterface.generated.h"

class UVCamComponent;

UINTERFACE(MinimalAPI,BlueprintType,Blueprintable)
class UVCamModifierInterface : public UInterface
{
	GENERATED_BODY()
};

/**
   The interface for all VCam modifier Blueprints to inherit so that we can enforce some common behaviors.
 */
class VCAMCORE_API IVCamModifierInterface
{
	GENERATED_BODY()
public:
	//Function used to monitor the status of a VCam. If it changed, function will be triggered.
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Active VCam Update")
	void OnVCamComponentChanged(UVCamComponent* VCam);

	virtual void OnVCamComponentChanged_Implementation(UVCamComponent* VCam);
};
