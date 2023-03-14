// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "PythonAutomationTest.generated.h"

UCLASS(meta = (ScriptName = "PyAutomationTest"))
class PYTHONAUTOMATIONTEST_API UPyAutomationTestLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	UFUNCTION(BlueprintCallable, Category = "PyAutomationTest")
	static void SetIsRunningPyLatentCommand(bool isRunning);

	UFUNCTION(BlueprintCallable, Category = "PyAutomationTest")
	static bool GetIsRunningPyLatentCommand();

	UFUNCTION(BlueprintCallable, Category = "PyAutomationTest")
	static void SetPyLatentCommandTimeout(float Seconds);

	UFUNCTION(BlueprintCallable, Category = "PyAutomationTest")
	static float GetPyLatentCommandTimeout();

	UFUNCTION(BlueprintCallable, Category = "PyAutomationTest")
	static void ResetPyLatentCommand();

private:
	static bool IsRunningPyLatentCommand;
	static float PyLatentCommandTimeout;
};