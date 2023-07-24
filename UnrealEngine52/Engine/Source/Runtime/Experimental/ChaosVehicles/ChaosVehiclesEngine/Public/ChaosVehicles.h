// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Physics/Experimental/PhysScene_Chaos.h"

#include "ChaosVehicles.generated.h"

class UChaosVehicles;

/**
* UChaosVehicles (UObject)
*
*/
UCLASS(customconstructor)
class CHAOSVEHICLESENGINE_API UChaosVehicles : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	UChaosVehicles(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	bool IsVisible() { return true; }
};



