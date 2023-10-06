// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnhancedPlayerInput.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "EnhancedInputDeveloperSettings.h"
#include "InputMappingContext.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include "EnhancedInputModule.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EnhancedPlayerInput)

namespace UE
{
	namespace Input
	{
		static int32 ShouldOnlyTriggerLastActionInChord = 1;
		static FAutoConsoleVariableRef CVarShouldOnlyTriggerLastActionInChord(TEXT("EnhancedInput.OnlyTriggerLastActionInChord"),
			ShouldOnlyTriggerLastActionInChord,
			TEXT("Should only the last action in a ChordedAction trigger be fired? If this is disabled, then the dependant chords will be fired as well"));

		static int32 EnableDefaultMappingContexts = 1;
		static FAutoConsoleVariableRef CVarEnableDefaultMappingContexts(TEXT("EnhancedInput.EnableDefaultMappingContexts"),
			EnableDefaultMappingContexts,
			TEXT("Should the UEnhancedInputDeveloperSettings::DefaultMappingContexts be applied to every UEnhancedPlayerInput?"));
	}
}

UEnhancedPlayerInput::UEnhancedPlayerInput()
	: Super()
	, bIsFlushingInputThisFrame(false)
	, CurrentlyInUseAnyKeySubstitute(NAME_None)
{
	if (UE::Input::EnableDefaultMappingContexts)
	{
		for (const FDefaultContextSetting& DefaultContext : GetDefault<UEnhancedInputDeveloperSettings>()->DefaultMappingContexts)
		{
			if (const UInputMappingContext* IMC = DefaultContext.InputMappingContext.LoadSynchronous())
			{
				AppliedInputContexts.Add(IMC, DefaultContext.Priority);
			}
		}
	}
}

// NOTE: Enum order represents firing priority(lowest to highest) and is important as multiple keys bound to the same action may generate differing trigger event states.
enum class ETriggerEventInternal : uint8
{
	None,					// No significant trigger state changes occurred
	Completed,				// Triggering stopped after one or more triggered ticks										ETriggerState (Triggered -> None)
	Started,				// Triggering has begun																		ETriggerState (None -> Ongoing)
	Ongoing,				// Triggering is still being processed														ETriggerState (Ongoing -> Ongoing)
	Canceled,				// Triggering has been canceled	mid processing												ETriggerState (Ongoing -> None)
	StartedAndTriggered,	// Triggering occurred in a single tick (fires both started and triggered events)			ETriggerState (None -> Triggered)
	Triggered,				// Triggering occurred after one or more processing ticks									ETriggerState (Ongoing -> Triggered, Triggered -> Triggered)
};

ETriggerEventInternal UEnhancedPlayerInput::GetTriggerStateChangeEvent(ETriggerState LastTriggerState, ETriggerState NewTriggerState) const
{
	// LastTState	NewTState     Event

	// None		 -> Ongoing		= Started
	// None		 -> Triggered	= Started + Triggered
	// Ongoing	 -> None		= Canceled
	// Ongoing	 -> Ongoing		= Ongoing
	// Ongoing	 -> Triggered	= Triggered
	// Triggered -> Triggered	= Triggered
	// Triggered -> Ongoing		= Ongoing
	// Triggered -> None	    = Completed

	switch (LastTriggerState)
	{
	case ETriggerState::None:
		if (NewTriggerState == ETriggerState::Ongoing)
		{
			return ETriggerEventInternal::Started;
		}
		else if (NewTriggerState == ETriggerState::Triggered)
		{
			return ETriggerEventInternal::StartedAndTriggered;
		}
		break;
	case ETriggerState::Ongoing:
		if (NewTriggerState == ETriggerState::None)
		{
			return ETriggerEventInternal::Canceled;
		}
		else if (NewTriggerState == ETriggerState::Ongoing)
		{
			return ETriggerEventInternal::Ongoing;
		}
		else if (NewTriggerState == ETriggerState::Triggered)
		{
			return ETriggerEventInternal::Triggered;
		}
		break;
	case ETriggerState::Triggered:
		if (NewTriggerState == ETriggerState::Triggered)
		{
			return ETriggerEventInternal::Triggered;	// Don't re-raise Started event for multiple completed ticks.
		}
		else if (NewTriggerState == ETriggerState::Ongoing)
		{
			return ETriggerEventInternal::Ongoing;
		}
		else if (NewTriggerState == ETriggerState::None)
		{
			return ETriggerEventInternal::Completed;
		}
		break;
	}

	return ETriggerEventInternal::None;
}

ETriggerEvent UEnhancedPlayerInput::ConvertInternalTriggerEvent(ETriggerEventInternal InternalEvent) const
{
	switch (InternalEvent)
	{
	case ETriggerEventInternal::None:
		return ETriggerEvent::None;
	case ETriggerEventInternal::Started:
		return ETriggerEvent::Started;
	case ETriggerEventInternal::Ongoing:
		return ETriggerEvent::Ongoing;
	case ETriggerEventInternal::Canceled:
		return ETriggerEvent::Canceled;
	case ETriggerEventInternal::StartedAndTriggered:
	case ETriggerEventInternal::Triggered:
		return ETriggerEvent::Triggered;
	case ETriggerEventInternal::Completed:
		return ETriggerEvent::Completed;
	}
	return ETriggerEvent::None;
}

enum class EKeyEvent : uint8
{
	None,		// Key did not generate an event this tick and is not being held
	Actuated,	// Key has generated an event this tick
	Held,		// Key generated no event, but is in a held state and wants to continue applying modifiers and triggers
};

void UEnhancedPlayerInput::ProcessActionMappingEvent(TObjectPtr<const UInputAction> Action, float DeltaTime, bool bGamePaused, FInputActionValue RawKeyValue, EKeyEvent KeyEvent, const TArray<UInputModifier*>& Modifiers, const TArray<UInputTrigger*>& Triggers)
{
	FInputActionInstance& ActionData = FindOrAddActionEventData(Action);

	// Update values and triggers for all actionable mappings each frame
	FTriggerStateTracker TriggerStateTracker;

	// Reset action data on the first event processed for the action this tick.
	bool bResetActionData = !ActionsWithEventsThisTick.Contains(Action);
	bool bMappingTriggersApplied = false;

	bool bHasAnyAlwaysTickTriggers = false;
	// checking the input mapping context triggers for any triggers that should tick every frame
	for (const UInputTrigger* Trigger : Triggers)
	{
		if (Trigger && Trigger->bShouldAlwaysTick)
		{
			bHasAnyAlwaysTickTriggers = true;
			break;
		}
	}
	// we also need to check the triggers of the Input Action itself - only if we haven't already found an AlwaysTickTrigger
	if (!bHasAnyAlwaysTickTriggers)
	{
		for (const UInputTrigger* Trigger : Action->Triggers)
		{
			if (Trigger && Trigger->bShouldAlwaysTick)
			{
				bHasAnyAlwaysTickTriggers = true;
				break;
			}
		}
	}
	
	// If the key state is changing or the key is actuated and being held (and not coming back up this tick) recalculate its value and resulting trigger state.
	if (KeyEvent != EKeyEvent::None || bHasAnyAlwaysTickTriggers)
	{
		if (bResetActionData)
		{
			ActionsWithEventsThisTick.Add(Action);
			ActionData.Value.Reset();	// TODO: what if default value isn't 0 (e.g. bool value with negate modifier). Move reset out to a pre-pass? This may be confusing as triggering requires key interaction for value processing for performance reasons.
		}

		// Apply modifications to the raw value
		EInputActionValueType ValueType = ActionData.Value.GetValueType();
		FInputActionValue ModifiedValue = ApplyModifiers(Modifiers, FInputActionValue(ValueType, RawKeyValue.Get<FVector>()), DeltaTime);
		//UE_CLOG(RawKeyValue.GetMagnitudeSq(), LogEnhancedInput, Warning, TEXT("Modified %s -> %s"), *RawKeyValue.ToString(), *ModifiedValue.ToString());

		// Derive an initial trigger state for this mapping using all applicable triggers
		ETriggerState CalcedState = TriggerStateTracker.EvaluateTriggers(this, Triggers, ModifiedValue, DeltaTime);
		// Do this only for no triggers?
		TriggerStateTracker.SetStateForNoTriggers(ModifiedValue.IsNonZero() ? ETriggerState::Triggered : ETriggerState::None);	
		bMappingTriggersApplied = Triggers.Num() > 0;

		const EInputActionAccumulationBehavior AccumulationBehavior = ActionData.GetSourceAction()->AccumulationBehavior;

		// Combine values for active events only, selecting the input with the greatest magnitude for each component in each tick.
		if(ModifiedValue.GetMagnitudeSq())
		{
			const int32 NumComponents = FMath::Max(1, int32(ValueType));
			FVector Modified = ModifiedValue.Get<FVector>();
			FVector Merged = ActionData.Value.Get<FVector>();
			for (int32 Component = 0; Component < NumComponents; ++Component)
			{
				switch (AccumulationBehavior)
				{
				// Sometimes you may want to cumulatively merge input. This would allow you to, for example, map WASD to movement and have pressing W and S at the same time
				// completely cancel out input because "W" is a value of +1.0, and "S" is a value of -1.0
				case EInputActionAccumulationBehavior::Cumulative:
				{
					Merged[Component] += Modified[Component];
				}										
				break;

				// By default, we will accept the input with the highest absolute value
				case EInputActionAccumulationBehavior::TakeHighestAbsoluteValue:
				default:
				{
					if (FMath::Abs(Modified[Component]) >= FMath::Abs(Merged[Component]))
					{
						Merged[Component] = Modified[Component];
					}
				}
				break;
				}			
			}
			ActionData.Value = FInputActionValue(ValueType, Merged);
		}
	}

	// Retain the most interesting/triggered tracker.
	ActionData.TriggerStateTracker = FMath::Max(ActionData.TriggerStateTracker, TriggerStateTracker);
	ActionData.TriggerStateTracker.SetMappingTriggerApplied(bMappingTriggersApplied);
}

void UEnhancedPlayerInput::InjectInputForAction(TObjectPtr<const UInputAction> Action, FInputActionValue RawValue, const TArray<UInputModifier*>& Modifiers, const TArray<UInputTrigger*>& Triggers)
{
	FInjectedInput Input;
	Input.RawValue = RawValue;
	Input.Modifiers = Modifiers;
	Input.Triggers = Triggers;

	InputsInjectedThisTick.FindOrAdd(Action).Injected.Emplace(MoveTemp(Input));
}

bool UEnhancedPlayerInput::InputKey(const FInputKeyParams& Params)
{
	const bool bResult = Super::InputKey(Params);

	if (Params.Key.IsButtonAxis() && Params.Event == IE_Pressed)
	{
		KeysPressedThisTick.FindOrAdd(Params.Key, Params.Delta);
	}
	
	return bResult;
}

float UEnhancedPlayerInput::GetEffectiveTimeDilation() const
{
	if (const APlayerController* PC = GetOuterAPlayerController())
	{
		return PC->GetActorTimeDilation();
	}
	else if (const UWorld* World = GetWorld())
	{
		if (const AWorldSettings* WorldSettings = World->GetWorldSettings())
		{
			return WorldSettings->GetEffectiveTimeDilation();	
		}
	}
	return 1.0f;
}

void UEnhancedPlayerInput::EvaluateKeyMapState(const float DeltaTime, const bool bGamePaused, OUT TArray<TPair<FKey, FKeyState*>>& KeysWithEvents)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(EnhPIS_KeyDownPrev);

	const UEnhancedInputDeveloperSettings* Settings = GetDefault<UEnhancedInputDeveloperSettings>();
	bool bWasAnyKeyDownLastFrame = false;

	KeyDownPrevious.Reset();
	KeyDownPrevious.Reserve(GetKeyStateMap().Num());
	for (TPair<FKey, FKeyState>& KeyPair : GetKeyStateMap())
	{
		const FKeyState& KeyState = KeyPair.Value;
		// TODO: Can't just use bDownPrevious as paired axis event edges may not fire due to axial deadzoning/missing axis properties. Need to change how this is detected in PlayerInput.cpp.
		bool bWasDown = KeyState.bDownPrevious || KeyState.EventCounts[IE_Pressed].Num() || KeyState.EventCounts[IE_Repeat].Num();
		bWasDown |= KeyPair.Key.IsAnalog() && KeyState.RawValue.SizeSquared() != 0;	// Analog inputs should pulse every (non-zero) tick to retain compatibility with UE4.

		// When UPlayerInput::FlushPressedKeys is called any keys that are down will have their RawValue set to 0, and their bDown/bDownPrevious state will be reset to false.
		// However, their "Value" will not be reset until UPlayerInput::ProcessInputStack. We need to detect if this key was down previously after a flush
		// so that the Enhanced Input action will correctly fire the triggered values.
		const bool bKeyWasJustFlushed = 
			bIsFlushingInputThisFrame && 
			Settings->bSendTriggeredEventsWhenInputIsFlushed && 
			!KeyState.Value.IsZero() && 
			KeyState.RawValue.IsZero() && 
			!KeyState.bDown;

		bWasDown |= bKeyWasJustFlushed;
			
		// Keep track of the state of every key so that when we are done iterating we can have a meaningful value for EKeys::AnyKey
		bWasAnyKeyDownLastFrame |= bWasDown;

		KeyDownPrevious.Emplace(KeyPair.Key, bWasDown);
	}

	KeyDownPrevious.Emplace(EKeys::AnyKey, bWasAnyKeyDownLastFrame);
	
	
	Super::EvaluateKeyMapState(DeltaTime, bGamePaused, KeysWithEvents);
}

void UEnhancedPlayerInput::EvaluateInputDelegates(const TArray<UInputComponent*>& InputComponentStack, const float DeltaTime, const bool bGamePaused, const TArray<TPair<FKey, FKeyState*>>& KeysWithEvents)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(EnhPIS_Main);

	// Process Action bindings
	ActionsWithEventsThisTick.Reset();

	// Calculate the current delta time as a fallback in case the Time Dilation is set to 0.0
	// This will ensure that Timed Triggers are calculated in real time
	const UWorld* World = GetWorld();
	// If there is no world, then add 1/60 to the last frame time to try and recover at least some
	// semblance of real time passing. 
	float CurrentTime = World ? World->GetRealTimeSeconds() : LastFrameTime + (1.0f / 60.0f);
	RealTimeDeltaSeconds = CurrentTime - LastFrameTime;
	
	// Use non-dilated delta time for processing
	const float Dilation = GetEffectiveTimeDilation();
	const float NonDilatedDeltaTime = Dilation != 0.0f ? DeltaTime / Dilation : RealTimeDeltaSeconds;

	// Handle input devices, applying modifiers and triggers
	for (FEnhancedActionKeyMapping& Mapping : EnhancedActionMappings)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(EnhPIS_Devices);

		if (!Mapping.Action)
		{
			continue;
		}

		FKeyState* KeyState = nullptr;

		// If the mapping was to AnyKey, then it won't be in the KeyStateMap and we need to handle it specially. 
		if (Mapping.Key == EKeys::AnyKey)
		{
			// We can just get the first value in the key state map that has been pressed or released, that's what we really care about with this key type
			for (TPair<FKey, FKeyState>& KeyStatePair : KeyStateMap)
			{
				// EKeys::AnyKey will only use non-analog keys. the same as the legacy system.
				if (!KeyStatePair.Key.IsDigital())
				{
					continue;
				}

				// If we have no Substitute key, then we can simply use the key state of the first available key with pressed events.
				if (CurrentlyInUseAnyKeySubstitute == NAME_None && KeyStatePair.Value.EventCounts[IE_Pressed].Num() > 0)
				{
					KeyState = &KeyStatePair.Value;
					CurrentlyInUseAnyKeySubstitute = KeyStatePair.Key.GetFName();
					break;
				}

				// If we have a substitute key already, then we can just look for the key of this name in the map
				else if (KeyStatePair.Key.GetFName() == CurrentlyInUseAnyKeySubstitute)
				{
					// If the substitute key was just released, then we can reset our currently in 
					// use substitute key so that it can be replaced by something else on the next key press.					
					if (KeyStatePair.Value.EventCounts[IE_Released].Num() > 0)
					{
						CurrentlyInUseAnyKeySubstitute = NAME_None;
					}

					KeyState = &KeyStatePair.Value;
					break;
				}
			}
		}
		else
		{
			KeyState = GetKeyState(Mapping.Key);
		}
		
		FVector RawKeyValue = KeyState ? KeyState->RawValue : FVector::ZeroVector;
		//UE_CLOG(RawKeyValue.SizeSquared(), LogEnhancedInput, Warning, TEXT("Key %s - state %s"), *Mapping.Key.GetDisplayName().ToString(), *RawKeyValue.ToString());

		// Should this key be ignored because it was down during a context switch?
		// If so, check if it is back up, otherwise ignore it.
		if(Mapping.bShouldBeIgnored && KeyState)
		{
			if(KeyState->bDown)
			{
				continue;				
			}
			else
			{
				Mapping.bShouldBeIgnored = false;	
			}
		}
		
		// Establish update type.
		bool bDownLastTick = KeyDownPrevious.FindRef(Mapping.Key);
		// TODO: Can't just use bDown as paired axis event edges may not fire due to axial deadzoning/missing axis properties. Need to change how this is detected in PlayerInput.cpp.
		bool bKeyIsDown = KeyState && (KeyState->bDown || KeyState->EventCounts[IE_Pressed].Num() || KeyState->EventCounts[IE_Repeat].Num());
		// Analog inputs should pulse every (non-zero) tick to retain compatibility with UE4. TODO: This would be better handled at the device level.
		bKeyIsDown |= Mapping.Key.IsAnalog() && RawKeyValue.SizeSquared() > 0;

		bool bKeyIsReleased = !bKeyIsDown && bDownLastTick;
		bool bKeyIsHeld = bKeyIsDown && bDownLastTick;

		EKeyEvent KeyEvent = bKeyIsHeld ? EKeyEvent::Held : ((bKeyIsDown || bKeyIsReleased) ? EKeyEvent::Actuated : EKeyEvent::None);

		FVector* PressedThisTickValue = KeysPressedThisTick.Find(Mapping.Key);
		
		// For keys that were pressed and released within the same frame, set their RawValue so that
		// InputTriggers are aware that they have been pressed
		if(PressedThisTickValue && bKeyIsDown && KeyState->EventCounts[IE_Pressed].Num() && KeyState->EventCounts[IE_Released].Num() && RawKeyValue.IsZero())
		{
			RawKeyValue = *PressedThisTickValue;
		}

		// Perform update
		ProcessActionMappingEvent(Mapping.Action, NonDilatedDeltaTime, bGamePaused, RawKeyValue, KeyEvent, Mapping.Modifiers, Mapping.Triggers);
	}


	// Strip stored injected input states that weren't re-injected this tick
	for (auto It = LastInjectedActions.CreateIterator(); It; ++It)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(EnhPIS_InjectedStrip);
		TObjectPtr<const UInputAction> InjectedAction = *It;

		if (!InjectedAction)
		{
			It.RemoveCurrent();
		}
		else if (!InputsInjectedThisTick.Contains(InjectedAction))
		{
			// Reset action state by "releasing the key".
			ProcessActionMappingEvent(InjectedAction, NonDilatedDeltaTime, bGamePaused, FInputActionValue(), EKeyEvent::Actuated, {}, {});
			It.RemoveCurrent();
		}
	}

	// Handle injected inputs, applying modifiers and triggers
	for (TPair<TObjectPtr<const UInputAction>, FInjectedInputArray>& InjectedPair : InputsInjectedThisTick)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(EnhPIS_Injected);
		TObjectPtr<const UInputAction> InjectedAction = InjectedPair.Key;
		if (!InjectedAction)
		{
			continue;
		}

		// Update last injection status data
		bool bDownLastTick = false;
		LastInjectedActions.Emplace(InjectedAction, &bDownLastTick);

		EKeyEvent KeyEvent = bDownLastTick ? EKeyEvent::Held : EKeyEvent::Actuated;
		for (FInjectedInput& InjectedInput : InjectedPair.Value.Injected)
		{
			// Perform update
			ProcessActionMappingEvent(InjectedAction, NonDilatedDeltaTime, bGamePaused, InjectedInput.RawValue, KeyEvent, InjectedInput.Modifiers, InjectedInput.Triggers);
		}
	}
	InputsInjectedThisTick.Reset();


	// Post tick action instance updates
	for (TPair<TObjectPtr<const UInputAction>, FInputActionInstance>& ActionPair : ActionInstanceData)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(EnhPIS_PostTick);

		TObjectPtr<const UInputAction> Action = ActionPair.Key;
		FInputActionInstance& ActionData = ActionPair.Value;
		ETriggerState TriggerState = ETriggerState::None;

		if (ActionsWithEventsThisTick.Contains(Action))
		{
			// Apply action modifiers
			FInputActionValue RawValue = ActionData.Value; 
			ActionData.Value = ApplyModifiers(ActionData.Modifiers, ActionData.Value, NonDilatedDeltaTime);

			// Update what state to use for this data in the case of there being no triggers, otherwise we can get incorrect triggered
			// states even if the modified value is Zero
			if(ActionData.Value.Get<FVector>() != RawValue.Get<FVector>())
			{
				ActionData.TriggerStateTracker.SetStateForNoTriggers(ActionData.Value.IsNonZero() ? ETriggerState::Triggered : ETriggerState::None);	
			}

			ETriggerState PrevState = ActionData.TriggerStateTracker.GetState();
			// Evaluate action triggers. We must always call EvaluateTriggers to update any internal state, even when paused.
			TriggerState = ActionData.TriggerStateTracker.EvaluateTriggers(this, ActionData.Triggers, ActionData.Value, NonDilatedDeltaTime);
			TriggerState = ActionData.TriggerStateTracker.GetMappingTriggerApplied() ? FMath::Min(TriggerState, PrevState) : TriggerState;
			
			// However, if the game is paused invalidate trigger unless the action allows it.
			// TODO: Potential issues with e.g. hold event that's canceled due to pausing, but jumps straight back to its "triggered" state on unpause if the user continues to hold the key.
			if (bGamePaused && !Action->bTriggerWhenPaused)
			{
				TriggerState = ETriggerState::None;
			}
		}

		// Use the new trigger state to determine a trigger event based on changes from the previous trigger state.
		ActionData.TriggerEventInternal = GetTriggerStateChangeEvent(ActionData.LastTriggerState, TriggerState);
		ActionData.TriggerEvent = ConvertInternalTriggerEvent(ActionData.TriggerEventInternal);
		ActionData.LastTriggerState = TriggerState;
		// Evaluate time per action after establishing the internal trigger state across all mappings
		ActionData.ElapsedProcessedTime += TriggerState != ETriggerState::None ? NonDilatedDeltaTime : 0.f;
		ActionData.ElapsedTriggeredTime += (ActionData.TriggerEvent == ETriggerEvent::Triggered) ? NonDilatedDeltaTime : 0.f;
		// Track the time that this trigger was last used
		if(TriggerState == ETriggerState::Triggered)
		{
			ActionData.LastTriggeredWorldTime = CurrentTime;
		}
	}


	// Execute appropriate delegates

	// Cache modifier key states for debug key bindings
	const bool bAlt = IsAltPressed(), bCtrl = IsCtrlPressed(), bShift = IsShiftPressed(), bCmd = IsCmdPressed();

	// TODO: Process APlayerController::InputComponent only!
	// Walk the stack, top to bottom, grabbing actions and firing triggered delegates
	int32 StackIndex = InputComponentStack.Num() - 1;
	for (; StackIndex >= 0; --StackIndex)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(EnhPIS_Delegates);

		UEnhancedInputComponent* IC = Cast<UEnhancedInputComponent>(InputComponentStack[StackIndex]);
		if (!IC)
		{
			continue;
		}

		// Trigger bound event delegates
		static TArray<TUniquePtr<FEnhancedInputActionEventBinding>> TriggeredDelegates;
		for (const TUniquePtr<FEnhancedInputActionEventBinding>& Binding : IC->GetActionEventBindings())
		{
			// PERF: Lots of map lookups! Group EnhancedActionBindings by Action?
			if (const FInputActionInstance* ActionData = FindActionInstanceData(Binding->GetAction()))
			{
				const ETriggerEvent BoundTriggerEvent = Binding->GetTriggerEvent();
				// Raise appropriate delegate to report on event state
				if (ActionData->TriggerEvent == BoundTriggerEvent ||
					(BoundTriggerEvent == ETriggerEvent::Started && ActionData->TriggerEventInternal == ETriggerEventInternal::StartedAndTriggered))	// Triggering in a single tick should also fire the started event.
				{
					// Record intent to trigger started as well as triggered
					// EmplaceAt 0 for the "Started" event it is always guaranteed to fire before Triggered
					if (BoundTriggerEvent == ETriggerEvent::Started)
					{
						TriggeredDelegates.EmplaceAt(0, Binding->Clone());
					}
					else
					{
						TriggeredDelegates.Emplace(Binding->Clone());
					}

					// Keep track of the triggered actions this tick so that we can quickly look them up later when determining chorded action state
					if (BoundTriggerEvent == ETriggerEvent::Triggered)
					{
						TriggeredActionsThisTick.Add(ActionData->GetSourceAction());
					}
				}
			}
		}

		// Action all delegates that triggered this tick, in the order in which they triggered.
		for (TUniquePtr<FEnhancedInputActionEventBinding>& Delegate : TriggeredDelegates)
		{
			TObjectPtr<const UInputAction> DelegateAction = Delegate->GetAction();
			bool bCanTrigger = true;

			if (UE::Input::ShouldOnlyTriggerLastActionInChord)
			{
				// If this delegate is referenced by a UInputTriggerChordAction::ChordAction
				// then we only want to trigger it the referencing action is not triggered
				for (const UEnhancedPlayerInput::FDependentChordTracker& DepAction : DependentChordActions)
				{
					if (DepAction.DependantAction && DepAction.DependantAction == DelegateAction)
					{
						bCanTrigger &= !TriggeredActionsThisTick.Contains(DepAction.SourceAction);
						if(!bCanTrigger)
						{
							UE_LOG(LogEnhancedInput, Warning, TEXT("'%s' action was cancelled, its dependant on '%s'"), *DelegateAction->GetName(), *DepAction.SourceAction->GetName());
						}
					}
				}
			}

			if (bCanTrigger)
			{
				// Search for the action instance data a second time as a previous delegate call may have deleted it.
				if (const FInputActionInstance* ActionData = FindActionInstanceData(DelegateAction))
				{
					// If this enhanced input delegate has triggered and is flagged to consume legacy keys, then mark it as such.
					if (const FKeyConsumptionOptions* ConsumptionData = KeyConsumptionData.Find(ActionData->GetSourceAction()))
					{
						if (static_cast<uint8>(ConsumptionData->EventsToCauseConsumption & Delegate->GetTriggerEvent()) != 0)
						{
							// Consume all keys that are mapped to this input action with the proper trigger values
							for (const FKey& KeyToConsume : ConsumptionData->KeysToConsume)
							{
								ConsumeKey(KeyToConsume);	
							}
						}
					}
					Delegate->Execute(*ActionData);
				}
			}
		}
		TriggeredDelegates.Reset();
		TriggeredActionsThisTick.Reset();

		// Update action value bindings
		for (const FEnhancedInputActionValueBinding& Binding : IC->GetActionValueBindings())
		{
			if (const UInputAction* Action = Binding.GetAction())
			{
				// PERF: Lots of map lookups! Group EnhancedActionBindings by Action?
				if (const FInputActionInstance* ActionData = FindActionInstanceData(Action))
				{
					Binding.CurrentValue = ActionData->GetValue();
				}
				// If there is no action instance data related to this action, then reset the binding's value to zero
				// to ensure that it gets its Completed state sent to any listeners
				else
				{
					Binding.CurrentValue = FInputActionValue(Action->ValueType, FVector::ZeroVector);
				}
			}
		}


#if DEV_ONLY_KEY_BINDINGS_AVAILABLE
		// DebugKeyBindings are intended to be used to enable/toggle debug functionality only and have reduced functionality compared to old style key bindings. Limitations/differences include:
		// No support for the 'Any Key' concept. Explicit key binds only.
		// They will always fire, and cannot mask each other or action bindings (i.e. no bConsumeInput option)
		// Chords are supported, but there is no chord masking protection. Exact chord combinations must be met. So a binding of Ctrl + A will not fire if Ctrl + Alt + A is pressed.
		static TArray<TUniquePtr<FInputDebugKeyBinding>> TriggeredDebugDelegates;
		for (const TUniquePtr<FInputDebugKeyBinding>& KeyBinding : IC->GetDebugKeyBindings())
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(EnhPIS_DebugKeys);

			ensureMsgf(KeyBinding->Chord.Key != EKeys::AnyKey, TEXT("Debug key bindings don't support 'any key'!"));

			// We match modifier key state here to explicitly block unmodified debug actions whilst modifier keys are held down, rather than allow e.g. E through on Alt + E.
			// This acts as a simplified version of chord masking.
			if (KeyBinding->Chord.bAlt == bAlt &&
				KeyBinding->Chord.bCtrl == bCtrl &&
				KeyBinding->Chord.bShift == bShift &&
				KeyBinding->Chord.bCmd == bCmd)
			{
				// TODO: Support full chord masking? Not worth the extra effort for debug keys?
				if (!bGamePaused || KeyBinding->bExecuteWhenPaused)
				{
					FKeyState* KeyState = KeyStateMap.Find(KeyBinding->Chord.Key);
					// We always want to update any analog debug events, like Gamepad axis 
					if ((KeyState && KeyState->EventCounts[KeyBinding->KeyEvent].Num() > 0) || KeyBinding->Chord.Key.IsAnalog())
					{
						// Record intent to trigger
						TriggeredDebugDelegates.Add(KeyBinding->Clone());
					}
				}
			}
		}

		// Action all debug delegates that triggered this tick, in the order in which they triggered.
		for (TUniquePtr<FInputDebugKeyBinding>& Delegate : TriggeredDebugDelegates)
		{
			const FKeyState* KeyState = GetKeyState(Delegate->Chord.Key);
			
			FInputActionValue ActionValue(KeyState ? KeyState->RawValue : FVector::ZeroVector);
			
			Delegate->Execute(ActionValue);
		}
		TriggeredDebugDelegates.Reset();
#endif


		// Early termination if this component is blocking input.
		// TODO: Remove support for this?
		if (IC->bBlockInput)
		{
			// stop traversing the stack, all input has been consumed by this InputComponent
			--StackIndex;
			break;
		}
	}

	for (; StackIndex >= 0; --StackIndex)
	{
		if (UEnhancedInputComponent* IC = Cast<UEnhancedInputComponent>(InputComponentStack[StackIndex]))
		{
			for (const FEnhancedInputActionValueBinding& Binding : IC->GetActionValueBindings())
			{
				Binding.CurrentValue.Reset();
			}
		}
	}

	// Reset action instance timers where necessary post delegate calls
	for (TPair<TObjectPtr<const UInputAction>, FInputActionInstance>& ActionPair : ActionInstanceData)
	{
		FInputActionInstance& ActionData = ActionPair.Value;
		switch (ActionData.TriggerEvent)
		{
		case ETriggerEvent::None:
		case ETriggerEvent::Canceled:
		case ETriggerEvent::Completed:
			ActionData.ElapsedProcessedTime = 0.f;
			break;
		}
		if (ActionData.TriggerEvent != ETriggerEvent::Triggered)
		{
			ActionData.ElapsedTriggeredTime = 0.f;
		}

		// Delay MappingTriggerState reset until here to allow dependent triggers (e.g. chords) access to this tick's values.
		ActionData.TriggerStateTracker = FTriggerStateTracker();
	}

	LastFrameTime = CurrentTime;
	KeysPressedThisTick.Reset();
	bIsFlushingInputThisFrame = false;

	Super::EvaluateInputDelegates(InputComponentStack, DeltaTime, bGamePaused, KeysWithEvents);
}

void UEnhancedPlayerInput::ProcessInputStack(const TArray<UInputComponent*>& InputComponentStack, const float DeltaTime, const bool bGamePaused)
{
	Super::ProcessInputStack(InputComponentStack, DeltaTime, bGamePaused);
}

void UEnhancedPlayerInput::FlushPressedKeys()
{
	Super::FlushPressedKeys();

	bIsFlushingInputThisFrame = true;
}

FInputActionValue UEnhancedPlayerInput::GetActionValue(TObjectPtr<const UInputAction> ForAction) const
{
	const FInputActionInstance* ActionData = FindActionInstanceData(ForAction);
	return ActionData ? ActionData->GetValue() : FInputActionValue(ForAction->ValueType, FInputActionValue::Axis3D::ZeroVector);
}


int32 UEnhancedPlayerInput::AddMapping(const FEnhancedActionKeyMapping& Mapping)
{
	int32 MappingIndex = EnhancedActionMappings.AddUnique(Mapping);
	++EnhancedKeyBinds.FindOrAdd(Mapping.Key);
	bKeyMapsBuilt = false;

	return MappingIndex;
}

void UEnhancedPlayerInput::ClearAllMappings()
{
	EnhancedActionMappings.Reset();
	EnhancedKeyBinds.Reset();

	bKeyMapsBuilt = false;
}

template<typename T>
void UEnhancedPlayerInput::GatherActionEventDataForActionMap(const T& ActionMap, TMap<TObjectPtr<const UInputAction>, FInputActionInstance>& FoundActionEventData) const
{
	for (const typename T::ElementType& Pair : ActionMap)
	{
		TObjectPtr<const UInputAction> Action = Pair.Key;
		if (FInputActionInstance* ActionData = ActionInstanceData.Find(Action))
		{
			FoundActionEventData.Add(Action, *ActionData);
		}
	}
}

void UEnhancedPlayerInput::ConditionalBuildKeyMappings_Internal() const
{
	Super::ConditionalBuildKeyMappings_Internal();

	// Remove any ActionEventData without a corresponding entry in EnhancedActionMappings or the injection maps
	for (auto Itr = ActionInstanceData.CreateIterator(); Itr; ++Itr)
	{
		TObjectPtr<const UInputAction> Action = Itr.Key();

		auto HasActionMapping = [&Action](const FEnhancedActionKeyMapping& Mapping) { return Mapping.Action == Action; };

		if (!LastInjectedActions.Contains(Action) &&
			!InputsInjectedThisTick.Contains(Action) &&		// This will be empty for most calls, but could potentially contain data.
			//EngineDefinedActionMappings.ContainsByPredicate(HasActionMapping) && // TODO: EngineDefinedActionMappings are non-rebindable action/key pairings but we have our own systems to handle this...
			!EnhancedActionMappings.ContainsByPredicate(HasActionMapping))
		{
			Itr.RemoveCurrent();
		}
	}

	bKeyMapsBuilt = true;
}

FInputActionValue UEnhancedPlayerInput::ApplyModifiers(const TArray<UInputModifier*>& Modifiers, FInputActionValue RawValue, float DeltaTime) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(EnhPIS_Modifiers);

	FInputActionValue ModifiedValue = RawValue;
	for (UInputModifier* Modifier : Modifiers)
	{
		if (Modifier)
		{
			// Enforce that type is kept to RawValue type between modifiers.
			ModifiedValue = FInputActionValue(RawValue.GetValueType(), Modifier->ModifyRaw(this, ModifiedValue, DeltaTime).Get<FInputActionValue::Axis3D>());
		}
	}
	return ModifiedValue;
}

bool UEnhancedPlayerInput::IsKeyHandledByAction(FKey Key) const
{
	// Determines if the key event is handled or not.
	return EnhancedKeyBinds.Contains(Key) || Super::IsKeyHandledByAction(Key);
}

FInputActionInstance& UEnhancedPlayerInput::FindOrAddActionEventData(TObjectPtr<const UInputAction> Action) const
{
	FInputActionInstance* Instance = ActionInstanceData.Find(Action);
	if (!Instance)
	{
		Instance = &ActionInstanceData.Emplace(Action, FInputActionInstance(Action));
	}
	return *Instance;
}

void UEnhancedPlayerInput::InitializeMappingActionModifiers(const FEnhancedActionKeyMapping& Mapping)
{
	if (Mapping.Action)
	{
		// Perform a modifier calculation pass on default data to initialize values correctly.
		FInputActionInstance& EventData = FindOrAddActionEventData(Mapping.Action);
		EventData.Value = ApplyModifiers(Mapping.Modifiers, EventData.Value, 0.f);	// Uses EventData.Value to provide the correct EInputActionValueType
	}
}


