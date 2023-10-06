// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"

#include "OpenXRBlueprintFunctionLibrary.generated.h"

class FOpenXRHMD;

UCLASS()
class OPENXRHMD_API UOpenXRBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()
	/**
	 * Allows to change the environment blend mode at runtime.
	 * 
	 * @param NewBlendMode		The new blend mode to be set, if supported.
	 */
	UFUNCTION(BlueprintCallable, Category="OpenXR|Experimental")
	static void SetEnvironmentBlendMode(int32 NewBlendMode);

private:
	static FOpenXRHMD* GetOpenXRHMD();
};

