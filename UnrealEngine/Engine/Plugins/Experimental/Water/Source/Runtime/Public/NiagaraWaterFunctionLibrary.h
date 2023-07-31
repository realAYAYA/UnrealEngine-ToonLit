// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NiagaraWaterFunctionLibrary.generated.h"

class UNiagaraComponent;
class AWaterBody;
class UWaterBodyComponent;

UCLASS()
class WATER_API UNiagaraWaterFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	/** Sets the water body on the Niagra water data interface on a Niagara particle system*/
	UE_DEPRECATED(4.27, "Use SetWaterBodyComponent instead.")
	UFUNCTION(BlueprintCallable, Category = Water, meta = (DisplayName = "Set Water Body"), meta = (DeprecatedFunction, DeprecationMessage="Use SetWaterBodyComponent instead"))
	static void SetWaterBody(UNiagaraComponent* NiagaraSystem, const FString& OverrideName, AWaterBody* WaterBody) {}

	/** Sets the water body component on the Niagra water data interface on a Niagara particle system*/
	UFUNCTION(BlueprintCallable, Category = Water)
	static void SetWaterBodyComponent(UNiagaraComponent* NiagaraSystem, const FString& OverrideName, UWaterBodyComponent* WaterBodyComponent);
};