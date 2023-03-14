// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "SmartObjectRuntime.h"
#include "SmartObjectBlueprintFunctionLibrary.generated.h"

struct FGameplayTagContainer;
class UBlackboardComponent;
class AAIController;
class UBTNode;

UCLASS(meta = (ScriptName = "SmartObjectLibrary"))
class SMARTOBJECTSMODULE_API USmartObjectBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	static FSmartObjectClaimHandle GetValueAsSOClaimHandle(UBlackboardComponent* BlackboardComponent, const FName& KeyName);
	
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	static void SetValueAsSOClaimHandle(UBlackboardComponent* BlackboardComponent, const FName& KeyName, FSmartObjectClaimHandle Value);
	
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	static bool IsValidSmartObjectClaimHandle(const FSmartObjectClaimHandle Handle)	{ return Handle.IsValid(); }

	UFUNCTION(BlueprintCallable, Category = "SmartObject", meta = (DisplayName = "SetSmartObjectEnabled"))
	static bool K2_SetSmartObjectEnabled(AActor* SmartObject, const bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "AI|BehaviorTree", meta = (HidePin = "NodeOwner", DefaultToSelf = "NodeOwner", DisplayName = "Set Blackboard Value As Smart Object Claim Handle"))
	static void SetBlackboardValueAsSOClaimHandle(UBTNode* NodeOwner, const FBlackboardKeySelector& Key, const FSmartObjectClaimHandle& Value);

	UFUNCTION(BlueprintPure, Category = "AI|BehaviorTree", meta = (HidePin = "NodeOwner", DefaultToSelf = "NodeOwner", DisplayName = "Get Blackboard Value As Smart Object Claim Handle"))
	static FSmartObjectClaimHandle GetBlackboardValueAsSOClaimHandle(UBTNode* NodeOwner, const FBlackboardKeySelector& Key);
};
