// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Param/AnimNextClassExtensionLibrary.h"
#include "Scheduler/AnimNextTickFunctionBinding.h"
#include "AnimNextActorComponentLibrary.generated.h"

class UActorComponent;

// Access to non-UProperty/UFunction data on UActorComponent
UCLASS()
class UAnimNextActorComponentLibrary : public UAnimNextClassExtensionLibrary
{
	GENERATED_BODY()

	// UAnimNextClassProxy interface
	virtual UClass* GetSupportedClass() const override;

	// Returns the component's tick function
	UFUNCTION(BlueprintCallable, BlueprintInternalUseOnly)
	static FAnimNextTickFunctionBinding GetTick(UActorComponent* InComponent);
};
