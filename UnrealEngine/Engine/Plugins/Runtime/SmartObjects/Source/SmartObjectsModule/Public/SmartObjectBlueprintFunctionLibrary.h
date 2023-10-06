// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "SmartObjectRuntime.h"
#include "SmartObjectBlueprintFunctionLibrary.generated.h"

struct FBlackboardKeySelector;
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

	/**
	 * Adds to the simulation all smart objects for an actor or removes them according to 'bAdd'.
	 * @param SmartObjectActor The actor containing the smart objects to add or remove from the simulation
	 * @param bAdd Whether the smart objects should be added or removed from the simulation
	 * @return True if the requested operation succeeded; false otherwise
	 * @note Removing a smart object from the simulation will interrupt all active interactions. If you simply need
	 * to make the object unavailable for queries consider using one of the SetSmartObjectEnabled functions so active
	 * interactions can be gracefully completed.
	 * @see SetSmartObjectEnabled, SetMultipleSmartObjectsEnabled
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	static bool AddOrRemoveSmartObject(UPARAM(DisplayName = "SmartObjectActor") AActor* SmartObject, UPARAM(DisplayName = "bAdd") const bool bEnabled);

	/**
	 * Adds to the simulation all smart objects for multiple actors or removes them according to 'bAdd'.
	 * @param SmartObjectActors The actors containing the smart objects to add or remove from the simulation
	 * @param bAdd Whether the smart objects should be added or removed from the simulation
	 * @return True if all actors were valid and the requested operation succeeded; false otherwise
	 * @note Removing a smart object from the simulation will interrupt all active interactions. If you simply need
	 * to make the object unavailable for queries consider using one of the SetSmartObjectEnabled functions so active
	 * interactions can be gracefully completed.
	 * @see SetSmartObjectEnabled, SetMultipleSmartObjectsEnabled
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	static bool AddOrRemoveMultipleSmartObjects(const TArray<AActor*>& SmartObjectActors, const bool bAdd);
	
	/**
	 * Adds to the simulation all smart objects for an actor.
	 * @param SmartObjectActor The actor containing the smart objects to add to the simulation
	 * @return True if the requested operation succeeded; false otherwise
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	static bool AddSmartObject(AActor* SmartObjectActor);

	/**
	 * Adds to the simulation all smart objects for multiple actors.
	 * @param SmartObjectActors The actors containing the smart objects to add to the simulation
	 * @return True if the requested operation succeeded; false otherwise
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	static bool AddMultipleSmartObjects(const TArray<AActor*>& SmartObjectActors);

	/**
	 * Removes from the simulation all smart objects for an actor.
	 * @param SmartObjectActor The actor containing the smart objects to add or remove from the simulation
	 * @return True if the requested operation succeeded; false otherwise
	 * @note Removing a smart object from the simulation will interrupt all active interactions. If you simply need
	 * to make the object unavailable for queries consider using one of the SetSmartObjectEnabled functions so active
	 * interactions can be gracefully completed.
	 * @see SetSmartObjectEnabled, SetMultipleSmartObjectsEnabled
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	static bool RemoveSmartObject(AActor* SmartObjectActor);

	/**
	 * Removes from the simulation all smart objects for multiple actors.
	 * @param SmartObjectActors The actors containing the smart objects to remove from the simulation
	 * @return True if the requested operation succeeded; false otherwise
	 * @note Removing a smart object from the simulation will interrupt all active interactions. If you simply need
	 * to make the object unavailable for queries consider using one of the SetSmartObjectEnabled functions so active
	 * interactions can be gracefully completed.
	 * @see SetSmartObjectEnabled, SetMultipleSmartObjectsEnabled
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	static bool RemoveMultipleSmartObjects(const TArray<AActor*>& SmartObjectActors);

	/**
	 * Marks all smart objects for an actor as enabled or not according to 'bEnabled'. A smart object marked as Enabled is available for queries.
	 * @param SmartObjectActor The actor containing the smart objects to enable/disable
	 * @param bEnabled Whether the smart objects should be enabled or not
	 * @return True if the requested operation succeeded; false otherwise
	 * @note Disabling a smart object will not interrupt active interactions, it will simply
	 * mark the object unavailable for new queries and broadcast an event that can be handled
	 * by the interacting agent to complete earlier. If the object should not be consider usable anymore
	 * and the interactions aborted then consider using one of the Add/RemoveSmartObject functions.
	 * @see AddOrRemoveSmartObject, AddOrRemoveMultipleSmartObjects, AddSmartObject, AddMultipleSmartObjects, RemoveSmartObject, RemoveMultipleSmartObjects
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	static bool SetSmartObjectEnabled(AActor* SmartObjectActor, const bool bEnabled);

	/**
	 * Marks a smart object slot from a request result as claimed.
	 * @param WorldContextObject Object used to fetch the SmartObjectSubsystem of its associated world.
	 * @param SlotHandle Handle to a smart object slot.
	 * @param UserActor Actor claiming the smart object
	 * @return A handle binding the claimed smart object, its slot and a user id.
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject", meta = (WorldContext = "WorldContextObject"))
	static FSmartObjectClaimHandle MarkSmartObjectSlotAsClaimed(UObject* WorldContextObject, const FSmartObjectSlotHandle SlotHandle, const AActor* UserActor = nullptr);
	
	/**
	 * Marks a previously claimed smart object slot as occupied.
	 * @param WorldContextObject Object used to fetch the SmartObjectSubsystem of its associated world.
	 * @param ClaimHandle Handle to a claimed slot returned by any of the Claim methods.
	 * @param DefinitionClass The type of behavior definition the user wants to use.
	 * @return The base class pointer of the requested behavior definition class associated to the slot
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject", meta = (WorldContext = "WorldContextObject"))
	static const USmartObjectBehaviorDefinition* MarkSmartObjectSlotAsOccupied(UObject* WorldContextObject, const FSmartObjectClaimHandle ClaimHandle, TSubclassOf<USmartObjectBehaviorDefinition> DefinitionClass);

	/**
	 * Marks a claimed or occupied smart object as free.
	 * @param WorldContextObject Object used to fetch the SmartObjectSubsystem of its associated world.
	 * @param ClaimHandle Handle to a claimed slot returned by any of the Claim methods.
	 * @return Whether the claim was successfully released or not
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject", meta = (WorldContext = "WorldContextObject"))
	static bool MarkSmartObjectSlotAsFree(UObject* WorldContextObject, const FSmartObjectClaimHandle ClaimHandle);

	/**
	 * Marks all smart objects for a list of actors as enabled or not according to 'bEnabled'. A smart object marked as Enabled is available for queries.
	 * @param SmartObjectActors The actors containing the smart objects to enable/disable
	 * @param bEnabled Whether the smart objects should be in the simulation (added) or not (removed) 
	 * @return True if all actors were valid and the requested operation succeeded; false otherwise
	 * @note Disabling a smart object will not interrupt active interactions, it will simply
	 * mark the object unavailable for new queries and broadcast an event that can be handled
	 * by the interacting agent to complete earlier. If the object should not be consider usable anymore
	 * and the interactions aborted then consider using one of the Add/RemoveSmartObject functions.
	 * @see AddOrRemoveSmartObject, AddOrRemoveMultipleSmartObjects, AddSmartObject, AddMultipleSmartObjects, RemoveSmartObject, RemoveMultipleSmartObjects
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject", meta = (DisplayName = "SetMultipleSmartObjectsEnabled"))
	static bool SetMultipleSmartObjectsEnabled(const TArray<AActor*>& SmartObjectActors, const bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "AI|BehaviorTree", meta = (HidePin = "NodeOwner", DefaultToSelf = "NodeOwner", DisplayName = "Set Blackboard Value As Smart Object Claim Handle"))
	static void SetBlackboardValueAsSOClaimHandle(UBTNode* NodeOwner, const FBlackboardKeySelector& Key, const FSmartObjectClaimHandle& Value);

	UFUNCTION(BlueprintPure, Category = "AI|BehaviorTree", meta = (HidePin = "NodeOwner", DefaultToSelf = "NodeOwner", DisplayName = "Get Blackboard Value As Smart Object Claim Handle"))
	static FSmartObjectClaimHandle GetBlackboardValueAsSOClaimHandle(UBTNode* NodeOwner, const FBlackboardKeySelector& Key);
};
