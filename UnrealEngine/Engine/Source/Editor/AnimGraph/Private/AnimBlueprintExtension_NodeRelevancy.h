// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimBlueprintExtension.h"
#include "Animation/AnimSubsystem_NodeRelevancy.h"
#include "AnimBlueprintExtension_NodeRelevancy.generated.h"

UCLASS(MinimalAPI)
class UAnimBlueprintExtension_NodeRelevancy : public UAnimBlueprintExtension
{
	GENERATED_BODY()

private:
	UPROPERTY()
	FAnimSubsystemInstance_NodeRelevancy Subsystem;
};