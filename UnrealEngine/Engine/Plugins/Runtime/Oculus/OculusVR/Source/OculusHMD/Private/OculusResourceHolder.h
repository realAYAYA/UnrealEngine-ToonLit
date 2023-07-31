// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Materials/MaterialInterface.h"
#include "OculusResourceHolder.generated.h"

/**
 *
 */
UCLASS()
class UOculusResourceHolder : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY()
	TObjectPtr<UMaterial> PokeAHoleMaterial;
};