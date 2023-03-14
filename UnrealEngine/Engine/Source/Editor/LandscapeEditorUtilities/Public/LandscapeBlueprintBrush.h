// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"
#include "LandscapeBlueprintBrushBase.h"

#include "LandscapeBlueprintBrush.generated.h"

UCLASS(Abstract, Blueprintable, hidecategories = (Replication, Input, LOD, Actor, Rendering), showcategories = (Cooking))
class LANDSCAPEEDITORUTILITIES_API ALandscapeBlueprintBrush : public ALandscapeBlueprintBrushBase
{
	GENERATED_UCLASS_BODY()
};