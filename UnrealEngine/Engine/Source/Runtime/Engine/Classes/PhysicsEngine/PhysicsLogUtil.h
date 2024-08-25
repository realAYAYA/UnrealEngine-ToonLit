// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"

class AActor;

class FString;
class UActorComponent;

namespace PhysicsLogUtil
{
	ENGINE_API FString MakeComponentNetIDString(const UActorComponent* Component);
	ENGINE_API FString MakeComponentNameString(const UActorComponent* Component);
	ENGINE_API FString MakeActorNameString(const UActorComponent* ActorComponent);
	ENGINE_API FString MakeActorNameString(const AActor* Actor);
}
