// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_4
#include "CoreMinimal.h"
#endif
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"

#include "NavigationGraphNode.generated.h"

UCLASS(config=Engine, MinimalAPI, NotBlueprintable, abstract)
class ANavigationGraphNode : public AActor
{
	GENERATED_UCLASS_BODY()
};
