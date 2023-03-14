// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"

#include "HandlerComponentFactory.generated.h"

class HandlerComponent;

/**
 * A UObject alternative for loading HandlerComponents without strict module dependency
 */
UCLASS(abstract)
class PACKETHANDLER_API UHandlerComponentFactory : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	virtual TSharedPtr<HandlerComponent> CreateComponentInstance(FString& Options)
		PURE_VIRTUAL(UHandlerComponentFactory::CreateComponentInstance, return nullptr;);
};
