// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BehaviorTreeTypes.h"
#include "GameplayBehavior.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UObject/ObjectMacros.h"
#include "GameplayBehaviorsBlueprintFunctionLibrary.generated.h"


class AActor;
class UBTNode;

UCLASS(meta = (ScriptName = "GameplayBehaviorLibrary"))
class GAMEPLAYBEHAVIORSMODULE_API UGameplayBehaviorsBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/** Will force-stop GameplayBehavior on given Avatar assuming the current 
	 *	behavior is of GameplayBehaviorClass class*/
	static bool StopGameplayBehavior(TSubclassOf<UGameplayBehavior> GameplayBehaviorClass,  AActor* Avatar);

	UFUNCTION(BlueprintPure, Category = "AI|BehaviorTree", Meta = (HidePin = "NodeOwner", DefaultToSelf = "NodeOwner"))
	static FGameplayTagContainer GetBlackboardValueAsGameplayTag(UBTNode* NodeOwner, const FBlackboardKeySelector& Key);

	UFUNCTION(BlueprintCallable, Category = "AI|BehaviorTree", Meta = (HidePin = "NodeOwner", DefaultToSelf = "NodeOwner"))
	static void SetBlackboardValueAsGameplayTag(UBTNode* NodeOwner, const FBlackboardKeySelector& Key, FGameplayTagContainer Value);

	UFUNCTION(BlueprintCallable, Category = "AI|BehaviorTree")
	static void AddGameplayTagFilterToBlackboardKeySelector(FBlackboardKeySelector& InSelector, UObject* Owner, FName PropertyName);

	UFUNCTION(BlueprintCallable, Category = "AI|BehaviorTree")
	static FGameplayTagContainer GetBlackboardValueAsGameplayTagFromBlackboardComp(UBlackboardComponent* BlackboardComp, const FName& KeyName);

	UFUNCTION(BlueprintCallable, Category = "AI|BehaviorTree")
	static void SetValueAsGameplayTagForBlackboardComp(UBlackboardComponent* BlackboardComp, const FName& KeyName, FGameplayTagContainer GameplayTagValue);

};
