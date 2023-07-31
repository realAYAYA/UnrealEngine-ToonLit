// Copyright Epic Games, Inc. All Rights Reserved.

#include "InputTriggers.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "EnhancedPlayerInput.h"
#include "GameFramework/PlayerController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InputTriggers)

#define LOCTEXT_NAMESPACE "EnhancedInputTriggers"

// Abstract trigger bases
ETriggerState UInputTrigger::UpdateState_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue ModifiedValue, float DeltaTime)
{
	return IsActuated(ModifiedValue) ? ETriggerState::Triggered : ETriggerState::None;
};

bool UInputTrigger::IsSupportedTriggerEvent(const ETriggerEventsSupported SupportedEvents, const ETriggerEvent Event)
{
	if(SupportedEvents == ETriggerEventsSupported::All)
	{
		return true;
	}
	else if(SupportedEvents == ETriggerEventsSupported::None)
	{
		return false;
	}
	
	// Check the bitmask of SupportedEvent types for each ETriggerEvent
	switch (Event)
	{
	case ETriggerEvent::Started:
		return EnumHasAnyFlags(SupportedEvents, ETriggerEventsSupported::Uninterruptible | ETriggerEventsSupported::Ongoing);
		break;
	case ETriggerEvent::Ongoing:
		return EnumHasAnyFlags(SupportedEvents, ETriggerEventsSupported::Uninterruptible | ETriggerEventsSupported::Ongoing);
		break;
	case ETriggerEvent::Canceled:
		return EnumHasAnyFlags(SupportedEvents, ETriggerEventsSupported::Ongoing);
		break;
	// Triggered can happen from Instant, Overtime, or Cancelable trigger events.
	case ETriggerEvent::Triggered:
		return EnumHasAnyFlags(SupportedEvents, (ETriggerEventsSupported::Instant | ETriggerEventsSupported::Uninterruptible | ETriggerEventsSupported::Ongoing));
		break;
		// Completed is supported by every UInputTrigger
	case ETriggerEvent::Completed:
		return EnumHasAnyFlags(SupportedEvents, ETriggerEventsSupported::All);
		break;
	case ETriggerEvent::None:
	default:
		return false;
	}	
	
	return false;
}

ETriggerState UInputTriggerTimedBase::UpdateState_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue ModifiedValue, float DeltaTime)
{
	ETriggerState State = ETriggerState::None;

	// Transition to Ongoing on actuation. Update the held duration.
	if (IsActuated(ModifiedValue))
	{
		State = ETriggerState::Ongoing;
		HeldDuration = CalculateHeldDuration(PlayerInput, DeltaTime);
	}
	else
	{
		// Reset duration
		HeldDuration = 0.0f;
	}

	return State;
}

float UInputTriggerTimedBase::CalculateHeldDuration(const UEnhancedPlayerInput* const PlayerInput, const float DeltaTime) const
{
	if (ensureMsgf(PlayerInput, TEXT("No Player Input was given to Calculate with! Returning 1.0")))
	{
		const float TimeDilation = PlayerInput->GetEffectiveTimeDilation();
	
		// Calculates the new held duration, applying time dilation if desired
		return HeldDuration + (!bAffectedByTimeDilation ? DeltaTime : DeltaTime * TimeDilation);
	}

	return 1.0f;
}


// Implementations

ETriggerState UInputTriggerDown::UpdateState_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue ModifiedValue, float DeltaTime)
{
	// Triggered on down.
	return IsActuated(ModifiedValue) ? ETriggerState::Triggered : ETriggerState::None;
}

ETriggerState UInputTriggerPressed::UpdateState_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue ModifiedValue, float DeltaTime)
{
	// Triggered on transition to actuated.
	return IsActuated(ModifiedValue) && !IsActuated(LastValue) ? ETriggerState::Triggered : ETriggerState::None;
}

ETriggerState UInputTriggerReleased::UpdateState_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue ModifiedValue, float DeltaTime)
{
	// Ongoing on hold
	if (IsActuated(ModifiedValue))
	{
		return ETriggerState::Ongoing;
	}

	// Triggered on release
	if (IsActuated(LastValue))
	{
		return ETriggerState::Triggered;
	}

	return ETriggerState::None;
}

ETriggerState UInputTriggerHold::UpdateState_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue ModifiedValue, float DeltaTime)
{
	// Update HeldDuration and derive base state
	ETriggerState State = Super::UpdateState_Implementation(PlayerInput, ModifiedValue, DeltaTime);

	// Trigger when HeldDuration reaches the threshold
	bool bIsFirstTrigger = !bTriggered;
	bTriggered = HeldDuration >= HoldTimeThreshold;
	if (bTriggered)
	{
		return (bIsFirstTrigger || !bIsOneShot) ? ETriggerState::Triggered : ETriggerState::None;
	}

	return State;
}

ETriggerState UInputTriggerHoldAndRelease::UpdateState_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue ModifiedValue, float DeltaTime)
{
	// Evaluate the updated held duration prior to calling Super to update the held timer
	// This stops us failing to trigger if the input is released on the threshold frame due to HeldDuration being 0.
	const float TickHeldDuration = CalculateHeldDuration(PlayerInput, DeltaTime);

	// Update HeldDuration and derive base state
	ETriggerState State = Super::UpdateState_Implementation(PlayerInput, ModifiedValue, DeltaTime);

	// Trigger if we've passed the threshold and released
	if (TickHeldDuration >= HoldTimeThreshold && State == ETriggerState::None)
	{
		State = ETriggerState::Triggered;
	}

	return State;
}

ETriggerState UInputTriggerTap::UpdateState_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue ModifiedValue, float DeltaTime)
{
	float LastHeldDuration = HeldDuration;

	// Updates HeldDuration
	ETriggerState State = Super::UpdateState_Implementation(PlayerInput, ModifiedValue, DeltaTime);

	// Only trigger if pressed then released quickly enough
	if (IsActuated(LastValue) && State == ETriggerState::None && LastHeldDuration < TapReleaseTimeThreshold)
	{
		State = ETriggerState::Triggered;
	}
	else if (HeldDuration >= TapReleaseTimeThreshold)
	{
		// Once we pass the threshold halt all triggering until released
		State = ETriggerState::None;
	}

	return State;
}

ETriggerState UInputTriggerPulse::UpdateState_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue ModifiedValue, float DeltaTime)
{
	// Update HeldDuration and derive base state
	ETriggerState State = Super::UpdateState_Implementation(PlayerInput, ModifiedValue, DeltaTime);

	if (State == ETriggerState::Ongoing)
	{
		// If the repeat count limit has not been reached
		if (TriggerLimit == 0 || TriggerCount < TriggerLimit)
		{
			// Trigger when HeldDuration exceeds the interval threshold, optionally trigger on initial actuation
			if (HeldDuration > (Interval * (bTriggerOnStart ? TriggerCount : TriggerCount + 1)))
			{
				++TriggerCount;
				State = ETriggerState::Triggered;
			}
		}
		else
		{
			State = ETriggerState::None;
		}
	}
	else
	{
		// Reset repeat count
		TriggerCount = 0;
	}

	return State;
}

#if WITH_EDITOR
EDataValidationResult UInputTriggerChordAction::IsDataValid(TArray<FText>& ValidationErrors)
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(ValidationErrors), EDataValidationResult::Valid);
	
	// You can't evaluate the combo if there are no combo steps!
	if (!ChordAction)
	{
		Result = EDataValidationResult::Invalid;
		ValidationErrors.Add(LOCTEXT("NullChordedAction", "A valid action is required for the Chorded Action input trigger!"));
	}

	return Result;
}
#endif

ETriggerState UInputTriggerChordAction::UpdateState_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue ModifiedValue, float DeltaTime)
{
	// Inherit state from the chorded action
	const FInputActionInstance* EventData = PlayerInput->FindActionInstanceData(ChordAction);
	return EventData ? EventData->TriggerStateTracker.GetState() : ETriggerState::None;
}

UInputTriggerCombo::UInputTriggerCombo()
{
	bShouldAlwaysTick = true;
}

ETriggerState UInputTriggerCombo::UpdateState_Implementation(const UEnhancedPlayerInput* PlayerInput, FInputActionValue ModifiedValue, float DeltaTime)
{
	if (ComboActions.IsEmpty())
	{
		ensureMsgf(false, TEXT("You must add combo actions to the UInputTriggerCombo for it work properly! Exiting..."));
		return ETriggerState::None;
	}
	
	if (const UInputAction* CurrentAction = ComboActions[CurrentComboStepIndex].ComboStepAction)
	{
		// loop through all cancel actions and check if they've fired
		for (const UInputAction* CancelAction : CancelActions)
		{
			if (CancelAction && CancelAction != CurrentAction)
			{
				const FInputActionInstance* CancelState = PlayerInput->FindActionInstanceData(CancelAction);
				if (CancelState && CancelState->GetTriggerEvent() != ETriggerEvent::None)
				{
					// Cancel action firing!
					CurrentComboStepIndex = 0;
					CurrentAction = ComboActions[CurrentComboStepIndex].ComboStepAction;	// Reset for fallthrough
					break;
				}
			}
		}
		// loop through all combo actions and check if a combo action fired out of order
		for (FInputComboStepData ComboStep : ComboActions)
		{
			if (ComboStep.ComboStepAction && ComboStep.ComboStepAction != CurrentAction)
			{
				const FInputActionInstance* CancelState = PlayerInput->FindActionInstanceData(ComboStep.ComboStepAction);
				if (CancelState && CancelState->GetTriggerEvent() != ETriggerEvent::None)
				{
					// Other combo action firing - should cancel
					CurrentComboStepIndex = 0;
					CurrentAction = ComboActions[CurrentComboStepIndex].ComboStepAction;	// Reset for fallthrough
					break;
				}
			}
		}

		// Reset if we take too long to hit the action
		if (CurrentComboStepIndex > 0)
		{
			CurrentTimeBetweenComboSteps += DeltaTime;
			if (CurrentTimeBetweenComboSteps >= ComboActions[CurrentComboStepIndex].TimeToPressKey)
			{
				CurrentComboStepIndex = 0;
				CurrentAction = ComboActions[CurrentComboStepIndex].ComboStepAction;	// Reset for fallthrough			
			}
		}

		const FInputActionInstance* CurrentState = PlayerInput->FindActionInstanceData(CurrentAction);
		// check to see if current action is completed - if so advance the combo to the next combo action
		if (CurrentState && CurrentState->GetTriggerEvent() == ETriggerEvent::Completed) // + possibly Triggered
		{
			CurrentComboStepIndex++;
			CurrentTimeBetweenComboSteps = 0;
			// check to see if we've completed all actions in the combo
			if (CurrentComboStepIndex >= ComboActions.Num())
			{
				CurrentComboStepIndex = 0;
				return ETriggerState::Triggered;
			}
		}

		if (CurrentComboStepIndex > 0)
		{
			return ETriggerState::Ongoing;
		}
	
		// Really should account for first combo action being mid-trigger...
		const FInputActionInstance* InitialState = PlayerInput->FindActionInstanceData(ComboActions[0].ComboStepAction);
		if (InitialState && InitialState->GetTriggerEvent() > ETriggerEvent::None) // || Cancelled!
		{
			return ETriggerState::Ongoing;
		}
		CurrentTimeBetweenComboSteps = 0;
	}
	return ETriggerState::None;
};

#if WITH_EDITOR
EDataValidationResult UInputTriggerCombo::IsDataValid(TArray<FText>& ValidationErrors)
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(ValidationErrors), EDataValidationResult::Valid);
	
	// You can't evaluate the combo if there are no combo steps!
	if (ComboActions.IsEmpty())
	{
		Result = EDataValidationResult::Invalid;
		ValidationErrors.Add(LOCTEXT("NoComboSteps", "There must be at least one combo step in the Combo Trigger!"));
	}

	return Result;
}

#endif

#undef LOCTEXT_NAMESPACE
