// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "UObject/ObjectMacros.h"

#include "GameplayCameraSystemActor.generated.h"

class APlayerController;
class UGameplayCameraSystemComponent;

/**
 * An actor that hosts a camera system.
 */
UCLASS(BlueprintType, MinimalAPI, ClassGroup=Camera, HideCategories=(Input, Rendering))
class AGameplayCameraSystemActor : public AActor
{
	GENERATED_BODY()

public:

	AGameplayCameraSystemActor(const FObjectInitializer& ObjectInit);

	UFUNCTION(BlueprintCallable, Category=Camera)
	void ActivateForPlayer(int32 PlayerIndex);

	UFUNCTION(BlueprintGetter, Category=Camera)
	UGameplayCameraSystemComponent* GetCameraSystemComponent() const { return CameraSystemComponent; }

public:

	// AActor interface
	virtual void BeginPlay() override;
	virtual void BecomeViewTarget(APlayerController* PC) override;
	virtual void CalcCamera(float DeltaTime, struct FMinimalViewInfo& OutResult) override;
	virtual void EndViewTarget(APlayerController* PC) override;

private:

	UPROPERTY(VisibleAnywhere, Category=Camera, BlueprintGetter="GetCameraSystemComponent", meta=(ExposeFunctionCategories="CameraSystem"))
	TObjectPtr<UGameplayCameraSystemComponent> CameraSystemComponent;

	UPROPERTY(EditAnywhere, Category=Camera)
	TEnumAsByte<EAutoReceiveInput::Type> AutoActivateForPlayer;
};

