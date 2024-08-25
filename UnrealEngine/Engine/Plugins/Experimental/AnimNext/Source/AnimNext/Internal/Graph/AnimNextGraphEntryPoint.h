// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigUnit_AnimNextGraphEvaluator.h"
#include "DecoratorBase/EntryPointHandle.h"
#include "AnimNextGraphEntryPoint.generated.h"

USTRUCT()
struct FAnimNextGraphEntryPoint
{
	GENERATED_BODY()

	// The name of the entry point
	UPROPERTY()
	FName EntryPointName;

	// This is a handle to the root decorator for a graph
	UPROPERTY()
	FAnimNextEntryPointHandle RootDecoratorHandle;
};

