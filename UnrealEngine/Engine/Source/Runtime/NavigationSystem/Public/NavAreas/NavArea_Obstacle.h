// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NavAreas/NavArea.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "NavArea_Obstacle.generated.h"

class UObject;

/** In general represents a high cost area, that shouldn't be traversed by anyone unless no other path exist.*/
UCLASS(Config = Engine)
class NAVIGATIONSYSTEM_API UNavArea_Obstacle : public UNavArea
{
	GENERATED_UCLASS_BODY()
};
