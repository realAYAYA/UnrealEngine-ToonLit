// Copyright Epic Games, Inc. All Rights Reserved.

/**
 * Base class for FViewportClients that are also UObjects
 *
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "UnrealClient.h"
#endif
#include "ViewportClient.h"
#include "ScriptViewportClient.generated.h"

UCLASS(transient)
class UScriptViewportClient : public UObject, public FCommonViewportClient
{
	GENERATED_UCLASS_BODY()

};

