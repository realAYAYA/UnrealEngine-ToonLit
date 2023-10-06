// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "CableActor.generated.h"

/** An actor that renders a simulated cable */
UCLASS(hidecategories=(Input,Replication), showcategories=("Input|MouseInput", "Input|TouchInput"))
class CABLECOMPONENT_API ACableActor : public AActor
{
	GENERATED_UCLASS_BODY()

	/** Cable component that performs simulation and rendering */
	UPROPERTY(Category=Cable, VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<class UCableComponent> CableComponent;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
