// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/WeakObjectPtrTemplates.h"
#include "Scheduler/AnimNextExternalTaskBinding.h"
#include "AnimNextTickFunctionBinding.generated.h"

struct FTickFunction;

// Wrapper around an object and a tick function
// Transient structure used to communicate binding information
USTRUCT(BlueprintInternalUseOnly)
struct FAnimNextTickFunctionBinding : public FAnimNextExternalTaskBinding
{
	GENERATED_BODY()

	// The object the tick function exists on
	TWeakObjectPtr<UObject> Object;

	// The tick function itself
	FTickFunction* TickFunction = nullptr;
};
