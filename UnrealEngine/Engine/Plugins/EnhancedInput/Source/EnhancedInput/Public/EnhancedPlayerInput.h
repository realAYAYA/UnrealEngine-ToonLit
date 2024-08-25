// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/PlayerInput.h"
#include "InputAction.h"

#include "EnhancedPlayerInput.generated.h"

class UInputModifier;
class UInputTrigger;
enum class ETriggerEvent : uint8;
enum class ETriggerState : uint8;
struct FEnhancedActionKeyMapping;
class UEnhancedInputUserSettings;

// Internal representation containing event variants
enum class ETriggerEventInternal : uint8;
enum class EKeyEvent : uint8;
class UInputMappingContext;

// ContinuouslyInjectedInputs Map is not managed.
// Continuous input injections seem to be getting garbage collected and
// crashing in UObject::ProcessEvent when calling ModifyRaw.
// Band-aid fix: Making these managed references. Also check modifications to
// IEnhancedInputSubsystemInterface::Start/StopContinuousInputInjectionForAction.
USTRUCT()
struct FInjectedInput
{
	GENERATED_BODY()

	FInputActionValue RawValue;

	UPROPERTY(Transient)
	TArray<UInputTrigger*> Triggers;
	UPROPERTY(Transient)
	TArray<UInputModifier*> Modifiers;
};

USTRUCT()
struct FKeyConsumptionOptions
{
	GENERATED_BODY()
	
	/** Keys that should be consumed if the trigger state is reached */
	TArray<FKey> KeysToConsume;
		
	/** A bitmask of trigger events that when reached, should cause the key to be marked as consumed. */
	ETriggerEvent EventsToCauseConsumption = ETriggerEvent::None;
};

USTRUCT()
struct FInjectedInputArray
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TArray<FInjectedInput> Injected;
};

/**
* UEnhancedPlayerInput : UPlayerInput extensions for enhanced player input system
*/
UCLASS(config = Input, transient)
class ENHANCEDINPUT_API UEnhancedPlayerInput : public UPlayerInput
{
	friend class IEnhancedInputSubsystemInterface;
	friend class UEnhancedInputLibrary;
	friend struct FInputTestHelper;

	GENERATED_BODY()

public:

	UEnhancedPlayerInput();

	//~ Begin UPlayerInput interface
	virtual void FlushPressedKeys() override;
	//~ End UPlayerInput interface

	/**
	* Returns the action instance data for the given input action if there is any. Returns nullptr if the action is not available.
	*/
	const FInputActionInstance* FindActionInstanceData(TObjectPtr<const UInputAction> ForAction) const { return ActionInstanceData.Find(ForAction); }

	/** Retrieve the current value of an action for this player.
	* Note: If the action is not currently triggering this will return a zero value of the appropriate value type, ignoring any ongoing inputs.
	*/
	FInputActionValue GetActionValue(TObjectPtr<const UInputAction> ForAction) const;

	// Input simulation via injection. Runs modifiers and triggers delegates as if the input had come through the underlying input system as FKeys. Applies action modifiers and triggers on top.
	void InjectInputForAction(TObjectPtr<const UInputAction> Action, FInputActionValue RawValue, const TArray<UInputModifier*>& Modifiers = {}, const TArray<UInputTrigger*>& Triggers = {});

	virtual bool InputKey(const FInputKeyParams& Params) override;
	
	// Applies modifiers and triggers without affecting keys read by the base input system
	virtual void ProcessInputStack(const TArray<UInputComponent*>& InputComponentStack, const float DeltaTime, const bool bGamePaused) override;

	/** Returns the Time Dilation value that is currently effecting this input. */
	float GetEffectiveTimeDilation() const;
	
protected:

	virtual void EvaluateKeyMapState(const float DeltaTime, const bool bGamePaused, OUT TArray<TPair<FKey, FKeyState*>>& KeysWithEvents) override;
	virtual void EvaluateInputDelegates(const TArray<UInputComponent*>& InputComponentStack, const float DeltaTime, const bool bGamePaused, const TArray<TPair<FKey, FKeyState*>>& KeysWithEvents) override;
	
	// Causes key to be consumed if it is affecting an action.
	virtual bool IsKeyHandledByAction(FKey Key) const override;
	
	/** Note: Source reference only. Use EnhancedActionMappings for the actual mappings (with properly instanced triggers/modifiers) */
	const TMap<TObjectPtr<const UInputMappingContext>, int32>& GetAppliedInputContexts() const { return AppliedInputContexts; }

	/** This player's version of the Action Mappings */
	const TArray<FEnhancedActionKeyMapping>& GetEnhancedActionMappings() const { return EnhancedActionMappings; }

	/** Array of data that represents what keys should be consumed if an enhanced input action is in a specific triggered state */
	UPROPERTY()
	TMap<TObjectPtr<const UInputAction>, FKeyConsumptionOptions> KeyConsumptionData;

private:

	/** Add a player specific action mapping.
	* Returns index into EnhancedActionMappings array.
	*/
	int32 AddMapping(const FEnhancedActionKeyMapping& Mapping);
	void ClearAllMappings();

	virtual void ConditionalBuildKeyMappings_Internal() const override;

	// Perform a first pass run of modifiers on an action instance
	void InitializeMappingActionModifiers(const FEnhancedActionKeyMapping& Mapping);

	FInputActionValue ApplyModifiers(const TArray<UInputModifier*>& Modifiers, FInputActionValue RawValue, float DeltaTime) const;						// Pre-modified (raw) value
	ETriggerEventInternal GetTriggerStateChangeEvent(ETriggerState LastTriggerState, ETriggerState NewTriggerState) const;
	ETriggerEvent ConvertInternalTriggerEvent(ETriggerEventInternal Event) const;	// Collapse a detailed internal trigger event into a friendly representation
	void ProcessActionMappingEvent(TObjectPtr<const UInputAction> Action, float DeltaTime, bool bGamePaused, FInputActionValue RawValue, EKeyEvent KeyEvent, const TArray<UInputModifier*>& Modifiers, const TArray<UInputTrigger*>& Triggers);

	FInputActionInstance& FindOrAddActionEventData(TObjectPtr<const UInputAction> Action) const;

	template<typename T>
	void GatherActionEventDataForActionMap(const T& ActionMap, TMap<TObjectPtr<const UInputAction>, FInputActionInstance>& FoundActionEventData) const;

	/** Currently applied key mappings
	 * Note: Source reference only. Use EnhancedActionMappings for the actual mappings (with properly instanced triggers/modifiers)
	 */
	UPROPERTY(Transient)
	TMap<TObjectPtr<const UInputMappingContext>, int32> AppliedInputContexts;

	/** This player's version of the Action Mappings */
	UPROPERTY(Transient)
	TArray<FEnhancedActionKeyMapping> EnhancedActionMappings;

	// Number of active binds by key
	TMap<FKey, int32> EnhancedKeyBinds;

	/** Tracked action values. Queryable. */
	UPROPERTY(Transient)
	mutable TMap<TObjectPtr<const UInputAction>, FInputActionInstance> ActionInstanceData;

	/** Actions which had actuated events at the last call to ProcessInputStack (held/pressed/released) */
	TSet<TObjectPtr<const UInputAction>> ActionsWithEventsThisTick;

	/** Actions that have been triggered this tick and have a delegate that may be fired */
	TSet<TObjectPtr<const UInputAction>> TriggeredActionsThisTick;

	/**
	 * A map of Keys to the amount they were depressed this frame. This is reset with each call to ProcessInputStack
	 * and is populated within UEnhancedPlayerInput::InputKey.
	 */
	UPROPERTY(Transient)
	TMap<FKey, FVector> KeysPressedThisTick;

	/** Inputs injected since the last call to ProcessInputStack */
	UPROPERTY(Transient)
	TMap<TObjectPtr<const UInputAction>, FInjectedInputArray> InputsInjectedThisTick;

	/** Last frame's injected inputs */
	UPROPERTY(Transient)
	TSet<TObjectPtr<const UInputAction>> LastInjectedActions;

	/** Used to keep track of Input Actions that have UInputTriggerChordAction triggers on them */
	struct FDependentChordTracker
	{
		/** The Input Action that has the UInputTriggerChordAction on it */
		TObjectPtr<const UInputAction> SourceAction;
		
		/** The action that is referenced by the SourceAction's Chord trigger */
		TObjectPtr<const UInputAction> DependantAction;
	};
	
	/**
	 * Array of all dependant Input Action's with Chord triggers on them.
	 * Populated by IEnhancedInputSubsystemInterface::ReorderMappings
	 */
	TArray<FDependentChordTracker> DependentChordActions;

protected:

	// We need to grab the down states of all keys before calling Super::ProcessInputStack as it will leave bDownPrevious in the same state as bDown (i.e. this frame, not last).
	TMap<FKey, bool> KeyDownPrevious;
	
	/** 
	* If true, then FlushPressedKeys has been called and the input key state map has been flushed.
	* 
	* This will be set to true in UEnhancedPlayerInput::FlushPressedKeys, and reset to false at the end of
	* UEnhancedPlayerInput::ProcessInputStack
	*/
	uint8 bIsFlushingInputThisFrame : 1;

	/**
	* If there is a key mapping to EKeys::AnyKey, we will keep track of what key was used when we first found a "Pressed"
	* event. That way we can use the same key when we wait for a "Released" event.
	*/
	FName CurrentlyInUseAnyKeySubstitute;

private:

	/** The last time of the last frame that was processed in ProcessPlayerInput */
	float LastFrameTime = 0.0f;

	/** Delta seconds between frames calculated with UWorld::GetRealTimeSeconds */
	float RealTimeDeltaSeconds = 0.0f;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "EnhancedActionKeyMapping.h"
#endif
