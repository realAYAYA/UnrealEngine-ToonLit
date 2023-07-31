// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"

#include "ReflexBlueprint.generated.h"

UENUM(BlueprintType)
enum class EReflexMode : uint8
{
	Disabled = 0 UMETA(DisplayName="Disabled"),
	Enabled = 1 UMETA(DisplayName="Enabled"),
	EnabledPlusBoost = 3 UMETA(DisplayName="Enabled + Boost")
};


UCLASS()
class REFLEX_API UReflexBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	UFUNCTION(BlueprintPure, Category="Reflex")
	static bool GetReflexAvailable();

	UFUNCTION(BlueprintCallable, Category="Reflex")
	static void SetReflexMode(const EReflexMode Mode);
	UFUNCTION(BlueprintPure, Category="Reflex")
	static EReflexMode GetReflexMode();

	UFUNCTION(BlueprintCallable, Category="Reflex")
	static void SetFlashIndicatorEnabled(const bool bEnabled);
	UFUNCTION(BlueprintPure, Category="Reflex")
	static bool GetFlashIndicatorEnabled();
	
	UFUNCTION(BlueprintPure, Category="Reflex")
	static float GetGameToRenderLatencyInMs();
	UFUNCTION(BlueprintPure, Category="Reflex")
	static float GetGameLatencyInMs();
	UFUNCTION(BlueprintPure, Category="Reflex")
	static float GetRenderLatencyInMs();
};
