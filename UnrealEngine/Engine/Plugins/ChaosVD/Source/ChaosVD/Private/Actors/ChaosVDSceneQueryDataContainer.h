// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosVDSceneObjectBase.h"
#include "GameFramework/Actor.h"
#include "UObject/ObjectPtr.h"

#include "ChaosVDSceneQueryDataContainer.generated.h"

class UChaosVDSceneQueryDataComponent;

/** Actor that contains Scene Queries components data and any related logic used to visualize recording scene queries */
UCLASS()
class CHAOSVD_API AChaosVDSceneQueryDataContainer : public AActor, public FChaosVDSceneObjectBase
{
	GENERATED_BODY()

public:
	AChaosVDSceneQueryDataContainer();

	UChaosVDSceneQueryDataComponent* GetSceneQueryDataComponent() const { return SceneQueryDataComponent.Get(); }

protected:
	UPROPERTY()
	TObjectPtr<UChaosVDSceneQueryDataComponent> SceneQueryDataComponent;
};
