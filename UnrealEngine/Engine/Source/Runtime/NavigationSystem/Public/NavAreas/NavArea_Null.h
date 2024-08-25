// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_4
#include "CoreMinimal.h"
#endif
#include "NavAreas/NavArea.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "NavArea_Null.generated.h"

class UObject;

/** In general represents an empty area, that cannot be traversed by anyone. Ever.*/
UCLASS(Config=Engine, MinimalAPI)
class UNavArea_Null : public UNavArea
{
	GENERATED_UCLASS_BODY()
};
