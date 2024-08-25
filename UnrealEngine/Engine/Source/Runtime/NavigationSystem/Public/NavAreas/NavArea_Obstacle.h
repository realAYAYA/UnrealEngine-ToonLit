// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_4
#include "CoreMinimal.h"
#endif
#include "NavAreas/NavArea.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "NavArea_Obstacle.generated.h"

class UObject;

/** In general represents a high cost area, that shouldn't be traversed by anyone unless no other path exist.*/
UCLASS(Config = Engine, MinimalAPI)
class UNavArea_Obstacle : public UNavArea
{
	GENERATED_UCLASS_BODY()
};
