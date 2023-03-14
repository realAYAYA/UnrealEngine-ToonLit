// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"

#include "CameraShakeSourceActor.generated.h"


class UCameraShakeSourceComponent;
class USceneComponent;
class UWorld;


UCLASS()
class ENGINE_API ACameraShakeSourceActor : public AActor
{
	GENERATED_BODY()

public:
	ACameraShakeSourceActor(const FObjectInitializer& ObjectInitializer);

	UCameraShakeSourceComponent* GetCameraShakeSourceComponent() const { return CameraShakeSourceComponent; }

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = CameraShakeSourceActor, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UCameraShakeSourceComponent> CameraShakeSourceComponent;
};
