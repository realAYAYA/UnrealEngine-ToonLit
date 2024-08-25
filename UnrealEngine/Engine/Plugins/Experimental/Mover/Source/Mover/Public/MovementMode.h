// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MoverSimulationTypes.h"
#include "MoverTypes.h"
#include "MoveLibrary/MoverBlackboard.h"
#include "MovementModeTransition.h"
#include "UObject/Interface.h"
#include "Templates/SubclassOf.h"
#include "MovementMode.generated.h"


/**
 * UMovementSettingsInterface: interface that must be implemented for any settings object to be shared between modes
 */
UINTERFACE(MinimalAPI, BlueprintType)
class UMovementSettingsInterface : public UInterface
{
	GENERATED_BODY()
};

class IMovementSettingsInterface
{
	GENERATED_BODY()

public:
	virtual FString GetDisplayName() const = 0;
};



/**
 * Base class for all movement modes, exposing simulation update methods for both C++ and blueprint extension
 */
UCLASS(Abstract, Blueprintable, BlueprintType, EditInlineNew)
class MOVER_API UBaseMovementMode : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	
	void DoRegister(const FName ModeName);
	void DoUnregister();
	void DoGenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const;
	void DoSimulationTick(const FSimulationTickParams& Params, FMoverTickEndData& OutputState);

	UFUNCTION(BlueprintCallable, Category=Mover, meta=(DisplayName="Get Mover Component"))
	UMoverComponent* GetMoverComponent() const;

	UFUNCTION(BlueprintCallable, Category=Mover, meta=(DisplayName="Get Blackboard"))
	const UMoverBlackboard* GetBlackboard() const;

	UFUNCTION(BlueprintCallable, Category=Mover, meta=(DisplayName="Get Blackboard"))
	UMoverBlackboard* GetBlackboard_Mutable() const;


#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif // WITH_EDITOR

	/** Settings object type that this mode depends on. May be shared with other movement modes. When the mode is added to a Mover Component, it will create a shared instance of this settings class. */
	UPROPERTY(EditDefaultsOnly, Category = Mover, meta = (MustImplement = "/Script/Mover.MovementSettingsInterface"))
	TSubclassOf<UObject> SharedSettingsClass = nullptr;

	/** Transition checks for the current mode. Evaluated in order, stopping at the first successful transition check */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = Mover, meta = (FullyExpand = true))
	TArray<TObjectPtr<UBaseMovementModeTransition>> Transitions;

protected:

	virtual void OnRegistered(const FName ModeName);

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "OnRegistered", ScriptName = "OnRegistered"))
	void K2_OnRegistered(const FName ModeName);


	virtual void OnUnregistered();

	UFUNCTION(BlueprintImplementableEvent, meta = (DisplayName = "OnUnregistered", ScriptName = "OnUnregistered"))
	void K2_OnUnregistered();


	virtual void OnGenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const;

	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnGenerateMove", meta = (ScriptName = "OnGenerateMove"))
	FProposedMove K2_OnGenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep) const;

	virtual void OnSimulationTick(const FSimulationTickParams& Params, FMoverTickEndData& OutputState);
	
	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnSimulationTick", meta = (ScriptName = "OnSimulationTick"))
	FMoverTickEndData K2_OnSimulationTick(const FSimulationTickParams& Params);



private:
	bool bHasBlueprintGenerateMove = false;
	bool bHasBlueprintSimulationTick = false;
};