// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/CachedAnimData.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CachedAnimDataLibrary.generated.h"

class UAnimInstance;

/**
 *	A library of commonly used functionality from the CachedAnimData family, exposed to blueprint.
 */
UCLASS(meta = (ScriptName = "CachedAnimDataLibrary"), MinimalAPI)
class UCachedAnimDataLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	// Returns whether a state is relevant (specified in the provided FCachedAnimStateData)
	UFUNCTION(BlueprintPure, Category = "Utilities|Animation|StateMachine")
	static ENGINE_API bool StateMachine_IsStateRelevant(UAnimInstance* InAnimInstance, UPARAM(ref) const FCachedAnimStateData& CachedAnimStateData);

	// Returns the weight of a state, relative to its state machine (specified in the provided FCachedAnimStateData)
	UFUNCTION(BlueprintPure, Category = "Utilities|Animation|StateMachine")
	static ENGINE_API float StateMachine_GetLocalWeight(UAnimInstance* InAnimInstance, UPARAM(ref) const FCachedAnimStateData& CachedAnimStateData);

	// Returns the weight of a state, relative to the graph (specified in the provided FCachedAnimStateData)
	UFUNCTION(BlueprintPure, Category = "Utilities|Animation|StateMachine")
	static ENGINE_API float StateMachine_GetGlobalWeight(UAnimInstance* InAnimInstance, UPARAM(ref) const FCachedAnimStateData& CachedAnimStateData);
};

