// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimationBlueprintToolMenuContext.generated.h"

class FAnimationBlueprintEditor;

UCLASS()
class UAnimationBlueprintToolMenuContext : public UObject
{
	GENERATED_BODY()

public:
	TWeakPtr<FAnimationBlueprintEditor> AnimationBlueprintEditor;
};