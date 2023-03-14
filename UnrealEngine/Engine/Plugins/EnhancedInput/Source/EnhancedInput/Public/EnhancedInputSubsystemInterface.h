// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputActionValue.h"
#include "InputMappingQuery.h"
#include "UObject/Interface.h"
#include "EnhancedActionKeyMapping.h"
#include "EnhancedPlayerInput.h"

#include "EnhancedInputSubsystemInterface.generated.h"

class APlayerController;
class UInputMappingContext;
class UInputAction;
class UEnhancedPlayerInput;
class UInputModifier;
class UInputTrigger;
class UPlayerMappableInputConfig;

// Subsystem interface
UINTERFACE(MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class UEnhancedInputSubsystemInterface : public UInterface
{
	GENERATED_BODY()
};

UENUM()
enum class EInputMappingRebuildType : uint8
{
	// No rebuild required.
	None,
	// Standard mapping rebuild. Retains existing triggers and modifiers for actions that were previously mapped.
	Rebuild,
	// If you have made changes to the triggers/modifiers associated with a UInputAction that was previously mapped a flush is required to reset the tracked data for that action.
	RebuildWithFlush,
};

/** Passed in as params for Adding/Remove input contexts */
USTRUCT(BlueprintType)
struct FModifyContextOptions
{
	GENERATED_BODY()
	
	FModifyContextOptions()
		: bIgnoreAllPressedKeysUntilRelease(true)
		, bForceImmediately(false)
	{}

	// If true than any keys that are pressed during the rebuild of control mappings will be ignored until they are released.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	uint8 bIgnoreAllPressedKeysUntilRelease : 1;

	// The mapping changes will be applied synchronously, rather than at the end of the frame, making them available to the input system on the same frame.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	uint8 bForceImmediately : 1;
};

// Includes native functionality shared between all subsystems
class ENHANCEDINPUT_API IEnhancedInputSubsystemInterface
{
	friend class FEnhancedInputModule;

	GENERATED_BODY()

public:

	virtual UEnhancedPlayerInput* GetPlayerInput() const = 0;

	/**
	 * Input simulation via injection. Runs modifiers and triggers delegates as if the input had come through the underlying input system as FKeys.
	 * Applies action modifiers and triggers on top.
	 *
	 * @param Action		The Input Action to set inject input for
	 * @param RawValue		The value to set the action to
	 * @param Modifiers		The modifiers to apply to the injected input.
	 * @param Triggers		The triggers to apply to the injected input.
	 */
	UFUNCTION(BlueprintCallable, Category="Input", meta=(AutoCreateRefTerm="Modifiers,Triggers"))
	virtual void InjectInputForAction(const UInputAction* Action, FInputActionValue RawValue, const TArray<UInputModifier*>& Modifiers, const TArray<UInputTrigger*>& Triggers);

	/**
	 * Input simulation via injection. Runs modifiers and triggers delegates as if the input had come through the underlying input system as FKeys.
	 * Applies action modifiers and triggers on top.
	 *
	 * @param Action		The Input Action to set inject input for
	 * @param Value			The value to set the action to (the type will be controlled by the Action)
	 * @param Modifiers		The modifiers to apply to the injected input.
	 * @param Triggers		The triggers to apply to the injected input.
	 */
	UFUNCTION(BlueprintCallable, Category="Input", meta=(AutoCreateRefTerm="Modifiers,Triggers"))
	virtual void InjectInputVectorForAction(const UInputAction* Action, FVector Value, const TArray<UInputModifier*>& Modifiers, const TArray<UInputTrigger*>& Triggers);

	/**
	 * Remove all applied mapping contexts.
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Input")
	virtual void ClearAllMappings();
	
	/**
	 * Add a control mapping context.
	 * @param MappingContext		A set of key to action mappings to apply to this player
	 * @param Priority				Higher priority mappings will be applied first and, if they consume input, will block lower priority mappings.
	 * @param Options				Options to consider when adding this mapping context.
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Input", meta=(AutoCreateRefTerm = "Options"))
	virtual void AddMappingContext(const UInputMappingContext* MappingContext, int32 Priority, const FModifyContextOptions& Options = FModifyContextOptions());

	/**
	* Remove a specific control context. 
	* This is safe to call even if the context is not applied.
	* @param MappingContext		Context to remove from the player
	* @param Options			Options to consider when removing this input mapping context
	*/
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Input", meta=(AutoCreateRefTerm = "Options"))
	virtual void RemoveMappingContext(const UInputMappingContext* MappingContext, const FModifyContextOptions& Options = FModifyContextOptions());

	/**
	* Flag player for reapplication of all mapping contexts at the end of this frame.
	* This is called automatically when adding or removing mappings contexts.
	*
	* @param Options		Options to consider when removing this input mapping context
	*/
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Input", meta=(AutoCreateRefTerm = "Options"))
	virtual void RequestRebuildControlMappings(const FModifyContextOptions& Options = FModifyContextOptions(), EInputMappingRebuildType RebuildType = EInputMappingRebuildType::Rebuild);

	/**
	 * Check if a key mapping is safe to add to a given mapping context within the set of active contexts currently applied to the player controller.
	 * @param InputContext		Mapping context to which the action/key mapping is intended to be added
	 * @param Action			Action that can be triggered by the key
	 * @param Key				Key that will provide input values towards triggering the action
	 * @param OutIssues			Issues that may cause this mapping to be invalid (at your discretion). Any potential issues will be recorded, even if not present in FatalIssues.
	 * @param BlockingIssues	All issues that should be considered fatal as a bitset.
	 * @return					Summary of resulting issues.
	 * @see QueryMapKeyInContextSet
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Input|Mapping Queries")
	virtual EMappingQueryResult QueryMapKeyInActiveContextSet(const UInputMappingContext* InputContext, const UInputAction* Action, FKey Key, TArray<FMappingQueryIssue>& OutIssues, EMappingQueryIssue BlockingIssues/* = DefaultMappingIssues::StandardFatal*/);

	/**
	 * Check if a key mapping is safe to add to a collection of mapping contexts
	 * @param PrioritizedActiveContexts	Set of mapping contexts to test against ordered by priority such that earlier entries take precedence over later ones.
	 * @param InputContext		Mapping context to which the action/key mapping is intended to be applied. NOTE: This context must be present in PrioritizedActiveContexts.
	 * @param Action			Action that is triggered by the key
	 * @param Key				Key that will provide input values towards triggering the action
	 * @param OutIssues			Issues that may cause this mapping to be invalid (at your discretion). Any potential issues will be recorded, even if not present in FatalIssues.
	 * @param BlockingIssues	All issues that should be considered fatal as a bitset.
	 * @return					Summary of resulting issues.
	 * @see QueryMapKeyInActiveContextSet
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Input|Mapping Queries")
	virtual EMappingQueryResult QueryMapKeyInContextSet(const TArray<UInputMappingContext*>& PrioritizedActiveContexts, const UInputMappingContext* InputContext, const UInputAction* Action, FKey Key, TArray<FMappingQueryIssue>& OutIssues, EMappingQueryIssue BlockingIssues/* = DefaultMappingIssues::StandardFatal*/);

	/**
	 * Check if a mapping context is applied to this subsystem's owner.
	 */
	virtual bool HasMappingContext(const UInputMappingContext* MappingContext) const;

	/**
	 * Check if a mapping context is applied to this subsystem's owner.
	 *
	 * @param MappingContext		The mapping context to search for on the subsystem's owner.
	 * @param OutFoundPriority		The priority of the mapping context if it is applied. -1 if the context is not applied	
	 * @return	True if the mapping context is applied
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Input|Mapping Queries")
	virtual bool HasMappingContext(const UInputMappingContext* MappingContext, int32& OutFoundPriority) const;

	/**
	 * Returns the keys mapped to the given action in the active input mapping contexts.
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Input|Mapping Queries")
	virtual TArray<FKey> QueryKeysMappedToAction(const UInputAction* Action) const;

	/**
	 * Get an array of the currently applied key mappings that are marked as Player Mappable.
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Input|Mapping Queries")
	virtual TArray<FEnhancedActionKeyMapping> GetAllPlayerMappableActionKeyMappings() const;
	
	/**
	 * Replace any currently applied mappings to this key mapping with the given new one.
	 * Requests a rebuild of the player mappings. 
	 *
	 * @return The number of mappings that have been replaced
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Input|PlayerMappable", meta=(AutoCreateRefTerm = "Options"))
	virtual int32 AddPlayerMappedKey(const FName MappingName, const FKey NewKey, const FModifyContextOptions& Options = FModifyContextOptions());

	/**
	 * Remove any player mappings with to the given action
	 * Requests a rebuild of the player mappings. 
	 *
	 * @return The number of mappings that have been removed
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Input|PlayerMappable", meta=(AutoCreateRefTerm = "Options"))
	virtual int32 RemovePlayerMappedKey(const FName MappingName, const FModifyContextOptions& Options = FModifyContextOptions());

	/**
	 * Get the player mapped key to the given mapping name. If there is not a player mapped key, then this will return
	 * EKeys::Invalid.
	 *
	 * @param MappingName	The FName of the mapped key that would have been set with the AddPlayerMappedKey function.
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Input|PlayerMappable")
	virtual FKey GetPlayerMappedKey(const FName MappingName) const;
	
	/**
	 * Remove All PlayerMappedKeys
	 * Requests a rebuild of the player mappings. 
	 *
	 */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Input|PlayerMappable", meta=(AutoCreateRefTerm = "Options"))
	virtual void RemoveAllPlayerMappedKeys(const FModifyContextOptions& Options = FModifyContextOptions());
	
	/** Adds all the input mapping contexts inside of this mappable config. */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Input|PlayerMappable", meta=(AutoCreateRefTerm = "Options"))
	virtual void AddPlayerMappableConfig(const UPlayerMappableInputConfig* Config, const FModifyContextOptions& Options = FModifyContextOptions());

	/** Removes all the input mapping contexts inside of this mappable config. */
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category = "Input|PlayerMappable", meta=(AutoCreateRefTerm = "Options"))
	virtual void RemovePlayerMappableConfig(const UPlayerMappableInputConfig* Config, const FModifyContextOptions& Options = FModifyContextOptions());

protected:

	/**
	 * Ticks any forced/injected input to the subsystem's player input. If you want support for ApplyForcedInput
	 * in a subsytem implementing this interface, then you must call this function.
	 */
	void TickForcedInput(float DeltaTime);

	/**
	 * Called each tick from the Enhanced Input module.
	 * 
	 * If bControlMappingsRebuiltThisTick is true this tick, then call the ControlMappingsRebuiltThisFrame
	 * function and reset the bControlMappingsRebuiltThisTick flag to false.
	 * 
	 * This gives an implementor of this interface an oppurtunity to add in a nice blueprint useable delegate.
	 */
	void HandleControlMappingRebuildDelegate();

	/** Function that will be called when Control Mappings have been rebuilt this tick. */
	virtual void ControlMappingsRebuiltThisFrame() {}
	
private:

	// Forced actions/keys for debug. These will be applied each tick once set even if zeroed, until removed. 
	void ApplyForcedInput(const UInputAction* Action, FInputActionValue Value);
	void ApplyForcedInput(FKey Key, FInputActionValue Value);
	void RemoveForcedInput(const UInputAction* Action);
	void RemoveForcedInput(FKey Key);

	void InjectChordBlockers(const TArray<int32>& ChordedMappings);
	bool HasTriggerWith(TFunctionRef<bool(const class UInputTrigger*)> TestFn, const TArray<class UInputTrigger*>& Triggers);	

	/**
	 * Reorder the given UnordedMappings such that chording mappings > chorded mappings > everything else.
	 * This is used to ensure mappings within a single context are evaluated in the correct order to support chording.
	 * Populate the DependentChordActions array with any chorded triggers so that we can detect which ones should be triggered
	 * later. 
	 */
	TArray<FEnhancedActionKeyMapping> ReorderMappings(const TArray<FEnhancedActionKeyMapping>& UnorderedMappings, TArray<UEnhancedPlayerInput::FDependentChordTracker>& OUT DependentChordActions);
	
	/**
	 * Reapply all control mappings to players pending a rebuild
	 */
	void RebuildControlMappings();

	/** Convert input settings axis config to modifiers for a given mapping */
	void ApplyAxisPropertyModifiers(UEnhancedPlayerInput* PlayerInput, struct FEnhancedActionKeyMapping& Mapping) const;

	TMap<TWeakObjectPtr<const UInputAction>, FInputActionValue> ForcedActions;
	TMap<FKey, FInputActionValue> ForcedKeys;

	/** A map of any player mapped keys to the key that they should redirect to instead */
	TMap<FName, FKey> PlayerMappedSettings;

	/**
	 * A map of input actions with a Chorded trigger, mapped to the action they are dependent on.
	 * The Key is the Input Action with the Chorded Trigger, and the value is the action it is dependant on.
	 */
	TMap<TObjectPtr<const UInputAction>, TObjectPtr<const UInputAction>> ChordedActionDependencies;

	/**
	 * A map of the currently applied mapping context redirects. This is populated in RebuildControlMappings
	 * with any InputMappingContexts that have been redirected on this local player from the platform settings.
	 */
	TMap<TObjectPtr<const UInputMappingContext>, TObjectPtr<const UInputMappingContext>> AppliedContextRedirects;

	EInputMappingRebuildType MappingRebuildPending = EInputMappingRebuildType::None;

	/**
	 * A flag that will be set when adding/removing a mapping context.
	 *
	 * If this is true, then any keys that are pressed when control mappings are rebuilt will be ignored 
	 * by the new Input context after being until the key is lifted
	 */
	bool bIgnoreAllPressedKeysUntilReleaseOnRebuild = true;

	bool bMappingRebuildPending = false;

	/**
	 * If true then the control mappings have been rebuilt on this frame.
	 * This is reset to false every tick in the EnhancedInputModule
	 */
	bool bControlMappingsRebuiltThisTick = false;

	// Debug visualization implemented in EnhancedInputSubsystemsDebug.cpp
	void ShowDebugInfo(class UCanvas* Canvas);
	
	// Debug visualization of any platform input devices (IPlatformInputDeviceMapper)
	// TODO: We should get a better place to put this instead of the Enhanced Input plugin.
	// It can't go in InputCore because that doesn't depend on Engine for debug drawing
	void ShowPlatformInputDebugInfo(class UCanvas* Canvas);
	
	void ShowDebugActionModifiers(UCanvas* Canvas, const UInputAction* Action);
	static void PurgeDebugVisualizations();
};