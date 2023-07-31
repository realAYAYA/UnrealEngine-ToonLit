// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

class AActor;

namespace ActorsReferencesUtils
{
	/**
	 * Gather direct references to external actors from the root object.
	 */
	ENGINE_API TArray<AActor*> GetExternalActorReferences(UObject* Root, bool bRecursive = false);

	/**
	 * Gather direct references to actors from the root object.
	 */
	ENGINE_API TArray<AActor*> GetActorReferences(UObject* Root, EObjectFlags RequiredFlags = RF_NoFlags, bool bRecursive = false);
}