// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Animation/AnimNotifyQueue.h"
#include "AnimNotifyLibrary.generated.h"


struct FAnimNotifyEventReference;

/**
*	A library of commonly used functionality for Notifies, exposed to blueprint.
*/
UCLASS(meta = (ScriptName = "UAnimNotifyLibrary"))
class ENGINE_API UAnimNotifyLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	/** Get whether the notify state reached the end (was not cancelled)
	*
	* @param EventReference		The event to inspect
	*/
	UFUNCTION(BlueprintPure, Category = "Utilities|Animation|Notifies" , meta = (ScriptMethod))
	static bool NotifyStateReachedEnd(const FAnimNotifyEventReference& EventReference);
};
