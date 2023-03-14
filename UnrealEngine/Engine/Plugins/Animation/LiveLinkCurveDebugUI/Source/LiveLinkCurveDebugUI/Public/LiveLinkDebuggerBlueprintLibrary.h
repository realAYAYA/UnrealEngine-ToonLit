// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LiveLinkDebuggerBlueprintLibrary.generated.h"

UCLASS()
class ULiveLinkDebuggerBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	UFUNCTION(BlueprintCallable, Category = "LiveLinkDebug")
	static void DisplayLiveLinkDebugger(FString SubjectName);

	UFUNCTION(BlueprintCallable, Category = "LiveLinkDebug")
	static void HideLiveLinkDebugger();
};