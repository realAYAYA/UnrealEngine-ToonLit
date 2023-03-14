// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Constraint.h"
#include "ConstraintsScripting.generated.h"

class UConstraintsManager;
class UTransformableComponentHandle;
class UTickableConstraint;
class UTickableTransformConstraint;

/**
* This is a set of helper functions to access various parts of the Sequencer and Control Rig API via Python and Blueprints.
*/
UCLASS(meta = (Transient, ScriptName = "ConstraintsScriptingLibrary"))
class CONSTRAINTS_API UConstraintsScriptingLibrary : public UBlueprintFunctionLibrary
{

public:
	GENERATED_BODY()

public:

	/**
	* Get the manager of the constraints. This object contains delegates to listen to for when constraints are added,deleted,
	* and is also the outer used when creating custom transformable handles, for example this is used to create control rig transformable handles
	* Note this function will create the mananager and it's actor if one doesn't exist.
	* @param InWorld, the world you are in
	* @return Returns the mananger
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig | Constraints")
	static UConstraintsManager* GetManager(UWorld* InWorld);

	/**
	* Create the transformable handle that deals with getting and setting transforms on this scene component
	* @param InWorld, the world you are in
	* @param InSceneComponent World to create the constraint
	* @param InSocketName Optional name of the socket to get the transform 
	* @return returns the handle for this scene component
	**/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig | Constraints")
	static UTransformableComponentHandle* CreateTransformableComponentHandle(UWorld* InWorld, USceneComponent* InSceneComponent, const FName& InSocketName);

	/**
	* Create Constraint based on the specified type.
	* @param InWorld World to create the constraint
	* @param InType The type of constraint
	* @return return The constraint object
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig | Constraints")
	static UTickableTransformConstraint* CreateFromType(UWorld* InWorld, const ETransformConstraintType InType);

	/**
	* Add Constraint to the system using the incoming parent and child handles with the specified type.
	* @param InWorld World to create the constraint
	* @param InParentHandle The parent handle
	* @param InChildHandle The child handle
	* @param InConsrtaint The constraint
	* @return return If constraint added correctly
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig | Constraints")
	static bool AddConstraint(UWorld* InWorld, UTransformableHandle* InParentHandle, UTransformableHandle* InChildHandle, UTickableTransformConstraint *InConstraint,
			const bool bMaintainOffset);


	/* Get a copy of the constraints in the current world
	@param InWorld World we are in
	@return Copy of the constraints in the level
	*/
	static TArray<TWeakObjectPtr<UTickableConstraint>> GetConstraintsArray(UWorld* InWorld);

	/**
	* Remove constraint at specified index
	* @param InWorld World to create the constraint
	* @param InIndex Index to remove from
	* @return return If constraint removed correctly
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Sequencer Tools | Control Rig | Constraints")
	static bool RemoveConstraint(UWorld* InWorld, int32 InIndex);

};