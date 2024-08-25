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
struct FTargetingRequestHandle;

UCLASS(meta = (ScriptName = "SmartObjectLibrary"))
class SMARTOBJECTSMODULE_API USmartObjectBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "SmartObject", meta=(ReturnDisplayName="Claim Handle"))
	static FSmartObjectClaimHandle GetValueAsSOClaimHandle(UBlackboardComponent* BlackboardComponent, const FName& KeyName);
	
	UFUNCTION(BlueprintCallable, Category = "SmartObject")
	static void SetValueAsSOClaimHandle(UBlackboardComponent* BlackboardComponent, const FName& KeyName, FSmartObjectClaimHandle Value);
	
	UFUNCTION(BlueprintCallable, Category = "SmartObject", meta=(DisplayName="Is Valid (Smart Object Claim Handle)", ReturnDisplayName="Is Valid"))
	static bool IsValidSmartObjectClaimHandle(const FSmartObjectClaimHandle Handle)	{ return Handle.IsValid(); }

	/** Returns the invalid smart object claim handle. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "SmartObject", meta=(ReturnDisplayName="Invalid Claim Handle"))
	static FSmartObjectClaimHandle SmartObjectClaimHandle_Invalid();

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
	UFUNCTION(BlueprintCallable, Category = "SmartObject", meta=(ReturnDisplayName="bSuccess"))
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
	UFUNCTION(BlueprintCallable, Category = "SmartObject", meta=(ReturnDisplayName="bSuccess"))
	static bool AddOrRemoveMultipleSmartObjects(const TArray<AActor*>& SmartObjectActors, const bool bAdd);
	
	/**
	 * Adds to the simulation all smart objects for an actor.
	 * @param SmartObjectActor The actor containing the smart objects to add to the simulation
	 * @return True if the requested operation succeeded; false otherwise
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject", meta=(ReturnDisplayName="bSuccess"))
	static bool AddSmartObject(AActor* SmartObjectActor);

	/**
	 * Adds to the simulation all smart objects for multiple actors.
	 * @param SmartObjectActors The actors containing the smart objects to add to the simulation
	 * @return True if the requested operation succeeded; false otherwise
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject", meta=(ReturnDisplayName="bSuccess"))
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
	UFUNCTION(BlueprintCallable, Category = "SmartObject", meta=(ReturnDisplayName="bSuccess"))
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
	UFUNCTION(BlueprintCallable, Category = "SmartObject", meta=(ReturnDisplayName="bSuccess"))
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
	UFUNCTION(BlueprintCallable, Category = "SmartObject", meta=(ReturnDisplayName="bSuccess"))
	static bool SetSmartObjectEnabled(AActor* SmartObjectActor, const bool bEnabled);

	/**
	 * Marks a smart object slot from a request result as claimed.
	 * @param WorldContextObject Object used to fetch the SmartObjectSubsystem of its associated world.
	 * @param SlotHandle Handle to a smart object slot.
	 * @param UserActor Actor claiming the smart object
	 * @param ClaimPriority Claim priority, a slot claimed at lower priority can be claimed by higher priority (unless already in use).
	 * @return A handle binding the claimed smart object, its slot and a user id.
	 */
	UFUNCTION(BlueprintCallable, Category = "SmartObject", meta = (WorldContext = "WorldContextObject", ReturnDisplayName="Claim Handle"))
	static FSmartObjectClaimHandle MarkSmartObjectSlotAsClaimed(UObject* WorldContextObject, const FSmartObjectSlotHandle SlotHandle, const AActor* UserActor = nullptr, ESmartObjectClaimPriority ClaimPriority = ESmartObjectClaimPriority::Normal);

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
	UFUNCTION(BlueprintCallable, Category = "SmartObject", meta = (WorldContext = "WorldContextObject", ReturnDisplayName="bSuccess"))
	static bool MarkSmartObjectSlotAsFree(UObject* WorldContextObject, const FSmartObjectClaimHandle ClaimHandle);
	
	/**
	 * Search a given Smart Object Component for slot candidates respecting the request criteria and selection conditions.
	 * 
	 * @param Filter Parameters defining the search area and criteria
	 * @param SmartObjectComponent The component to search
	 * @param OutResults List of smart object slot candidates found in range
	 * @param UserActor  Used to create additional data that could be provided to bind values in the conditions evaluation context
	 * 
	 * @return True if at least one candidate was found.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure = False, Category = "SmartObject", Meta = (ReturnDisplayName = "bSuccess"))
	static bool FindSmartObjectsInComponent(const FSmartObjectRequestFilter& Filter, USmartObjectComponent* SmartObjectComponent, TArray<FSmartObjectRequestResult>& OutResults, const AActor* UserActor = nullptr);

	/**
	 * Search a given Actor for slot candidates respecting the request criteria and selection conditions.
	 * 
	 * @param Filter Parameters defining the search area and criteria
	 * @param SearchActor The actor to search
	 * @param OutResults List of smart object slot candidates found in range
	 * @param UserActor  Used to create additional data that could be provided to bind values in the conditions evaluation context
	 * 
	 * @return True if at least one candidate was found.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure = False, Category = "SmartObject", Meta = (ReturnDisplayName = "bSuccess"))
	static bool FindSmartObjectsInActor(const FSmartObjectRequestFilter& Filter, AActor* SearchActor, TArray<FSmartObjectRequestResult>& OutResults, const AActor* UserActor = nullptr);

	/**
	 * Search the results of the given targeting handle request for smart objects that match the request criteria
	 *
	 * @param WorldContextObject Object used to fetch the SmartObjectSubsystem of its associated world.
	 * @param Filter Parameters defining the search area and criteria
	 * @param TargetingHandle The targeting handle of the request that will have its results searched for smart objects
	 * @param OutResults List of smart object slot candidates found in range
	 * @param UserActor Used to create additional data that could be provided to bind values in the conditions evaluation context
	 *
	 * @return True if at least one candidate was found.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure = False, Category = "SmartObject", Meta = (WorldContext="WorldContextObject", ReturnDisplayName = "bSuccess"))
	static bool FindSmartObjectsInTargetingRequest(UObject* WorldContextObject, const FSmartObjectRequestFilter& Filter, const FTargetingRequestHandle TargetingHandle, TArray<FSmartObjectRequestResult>& OutResults, const AActor* UserActor = nullptr);
	
	/**
	 * Search list of specific actors (often from a physics query) for slot candidates respecting request criteria and selection conditions.
	 *
	 * @param WorldContextObject Object used to fetch the SmartObjectSubsystem of its associated world.
	 * @param Filter Parameters defining the search area and criteria
	 * @param ActorList Ordered list of actors to search
	 * @param OutResults List of smart object slot candidates found in range
	 * @param UserData Additional data that could be provided to bind values in the conditions evaluation context
	 *
	 * @return True if at least one candidate was found.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure = False, Category = "SmartObject", Meta = (WorldContext="WorldContextObject", ReturnDisplayName = "bSuccess"))
	static bool FindSmartObjectsInList(UObject* WorldContextObject, const FSmartObjectRequestFilter& Filter, const TArray<AActor*>& ActorList, TArray<FSmartObjectRequestResult>& OutResults, const AActor* UserActor = nullptr);
	
	/** Converts a SmartObjectClaimHandle value to a string */
	UFUNCTION(BlueprintPure, Category = "Utilities|String", meta = (DisplayName = "To String (SmartObjectClaimHandle)", CompactNodeTitle = "->", BlueprintAutocast))
	static FString Conv_SmartObjectClaimHandleToString(const FSmartObjectClaimHandle& Result);
	
	/** Converts a SmartObjectRequestResult value to a string */
	UFUNCTION(BlueprintPure, Category = "Utilities|String", meta = (DisplayName = "To String (SmartObjectRequestResult)", CompactNodeTitle = "->", BlueprintAutocast))
	static FString Conv_SmartObjectRequestResultToString(const FSmartObjectRequestResult& Result);

	/** Converts a SmartObjectDefinition value to a string */
	UFUNCTION(BlueprintPure, Category = "Utilities|String", meta = (DisplayName = "To String (SmartObjectDefinition)", CompactNodeTitle = "->", BlueprintAutocast))
	static FString Conv_SmartObjectDefinitionToString(const USmartObjectDefinition* Definition);
	
	//
	// FSmartObjectHandle operators
	//
	
	/** Converts a SmartObjectHandle value to a string */
	UFUNCTION(BlueprintPure, Category = "Utilities|String", meta = (DisplayName = "To String (SmartObjectHandle)", CompactNodeTitle = "->", BlueprintAutocast))
	static FString Conv_SmartObjectHandleToString(const FSmartObjectHandle& Handle);

	/** Returns true if SmartObjectHandle A is NOT equal to SmartObjectHandle B (A != B) */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Not Equal (SmartObjectHandle)", CompactNodeTitle = "!=", ScriptMethod = "NotEqual", ScriptOperator = "!=", Keywords = "!= not equal"), Category = "SmartObjects")
	static bool NotEqual_SmartObjectHandleSmartObjectHandle(const FSmartObjectHandle& A, const FSmartObjectHandle& B);

	/** Returns true if SmartObjectHandle A is equal to SmartObjectHandle B (A == B) */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Equal (SmartObjectHandle)", CompactNodeTitle = "==", ScriptMethod = "Equals", ScriptOperator = "==", Keywords = "== equal"), Category = "SmartObjects")
	static bool Equal_SmartObjectHandleSmartObjectHandle(const FSmartObjectHandle& A, const FSmartObjectHandle& B);

	/** Returns true if the given handle is valid */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="SmartObjects", meta=(DisplayName="Is Valid (Smart Object Handle)", ReturnDisplayName = "Is Valid"))
	static bool IsValidSmartObjectHandle(const FSmartObjectHandle& Handle);
	
	//
	// FSmartObjectSlotHandle operators
	//
	
	/** Converts a SmartObjectSlotHandle value to a string */
	UFUNCTION(BlueprintPure, Category = "Utilities|String", meta = (DisplayName = "To String (SmartObjectSlotHandle)", CompactNodeTitle = "->", BlueprintAutocast))
	static FString Conv_SmartObjectSlotHandleToString(const FSmartObjectSlotHandle& Handle);
	
	/** Returns true if SmartObjectSlotHandle A is equal to SmartObjectSlotHandle B (A == B) */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Equal (SmartObjectSlotHandle)", CompactNodeTitle = "==", ScriptMethod = "Equals", ScriptOperator = "==", Keywords = "== equal"), Category = "SmartObjects")
	static bool Equal_SmartObjectSlotHandleSmartObjectSlotHandle(const FSmartObjectSlotHandle& A, const FSmartObjectSlotHandle& B);
	
	/** Returns true if SmartObjectSlotHandle A is NOT equal to SmartObjectSlotHandle B (A != B) */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Not Equal (SmartObjectSlotHandle)", CompactNodeTitle = "!=", ScriptMethod = "NotEqual", ScriptOperator = "!=", Keywords = "!= not equal"), Category = "SmartObjects")
	static bool NotEqual_SmartObjectSlotHandleSmartObjectSlotHandle(const FSmartObjectSlotHandle& A, const FSmartObjectSlotHandle& B);

	/** Returns true if the given Smart Object Slot Handle is valid. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category="SmartObjects", meta=(DisplayName="Is Valid (Smart Object Slot Handle)",  ReturnDisplayName = "Is Valid"))
	static bool IsValidSmartObjectSlotHandle(const FSmartObjectSlotHandle& Handle);

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
	UFUNCTION(BlueprintCallable, Category = "SmartObject", meta = (DisplayName = "SetMultipleSmartObjectsEnabled", ReturnDisplayName="bSuccess"))
	static bool SetMultipleSmartObjectsEnabled(const TArray<AActor*>& SmartObjectActors, const bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "AI|BehaviorTree", meta = (HidePin = "NodeOwner", DefaultToSelf = "NodeOwner", DisplayName = "Set Blackboard Value As Smart Object Claim Handle"))
	static void SetBlackboardValueAsSOClaimHandle(UBTNode* NodeOwner, const FBlackboardKeySelector& Key, const FSmartObjectClaimHandle& Value);

	UFUNCTION(BlueprintPure, Category = "AI|BehaviorTree", meta = (HidePin = "NodeOwner", DefaultToSelf = "NodeOwner", DisplayName = "Get Blackboard Value As Smart Object Claim Handle"))
	static FSmartObjectClaimHandle GetBlackboardValueAsSOClaimHandle(UBTNode* NodeOwner, const FBlackboardKeySelector& Key);
};
