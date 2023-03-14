// Copyright Epic Games, Inc. All Rights Reserved.

#include "InputAction.h"

#include "EnhancedPlayerInput.h"
#include "InputModifiers.h"
#include "InputTriggers.h"
#include "UObject/UObjectIterator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InputAction)

#define LOCTEXT_NAMESPACE "EnhancedInputAction"

FInputActionInstance::FInputActionInstance(const UInputAction* InSourceAction)
	: SourceAction(InSourceAction)
{
	if (ensureMsgf(SourceAction != nullptr, TEXT("Trying to create an FInputActionInstance without a source action")))
	{
		Value = FInputActionValue(SourceAction->ValueType, FVector::ZeroVector);

		Triggers.Reserve(SourceAction->Triggers.Num());
		for (UInputTrigger* ToDuplicate : SourceAction->Triggers)
		{
			if (ToDuplicate)
			{
				Triggers.Add(DuplicateObject<UInputTrigger>(ToDuplicate, nullptr));
			}
		}

		for (UInputModifier* ToDuplicate : SourceAction->Modifiers)
		{
			if (ToDuplicate)
			{
				Modifiers.Add(DuplicateObject<UInputModifier>(ToDuplicate, nullptr));
			}
		}

	}
}

// Calculate a collective representation of trigger state from evaluations of all triggers in one or more trigger groups.
ETriggerState FTriggerStateTracker::EvaluateTriggers(const UEnhancedPlayerInput* PlayerInput, const TArray<UInputTrigger*>& Triggers, FInputActionValue ModifiedValue, float DeltaTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(EnhPIS_Triggers);

	// Note: No early outs permitted (e.g. whilst bBlocking)! All triggers must be evaluated to update their internal state/delta time.

	for (UInputTrigger* Trigger : Triggers)
	{
		if (!Trigger)
		{
			continue;
		}

		bEvaluatedTriggers = true;

		ETriggerState CurrentState = Trigger->UpdateState(PlayerInput, ModifiedValue, DeltaTime);

		// Automatically update the last value, avoiding the trigger having to track it.
		Trigger->LastValue = ModifiedValue;

		switch (Trigger->GetTriggerType())
		{
		case ETriggerType::Explicit:
			bFoundExplicit = true;
			bAnyExplictTriggered |= (CurrentState == ETriggerState::Triggered);
			bFoundActiveTrigger |= (CurrentState != ETriggerState::None);
			break;
		case ETriggerType::Implicit:
			bAllImplicitsTriggered &= (CurrentState == ETriggerState::Triggered);
			bFoundActiveTrigger |= (CurrentState != ETriggerState::None);
			break;
		case ETriggerType::Blocker:
			bBlocking |= (CurrentState == ETriggerState::Triggered);
			// Ongoing blockers don't count as active triggers
			break;
		}
	}

	return GetState();
}

ETriggerState FTriggerStateTracker::GetState() const
{
	if(!bEvaluatedTriggers)
	{
		return NoTriggerState;
	}

	if (bBlocking)
	{
		return ETriggerState::None;
	}

	bool bTriggered = ((!bFoundExplicit || bAnyExplictTriggered) && bAllImplicitsTriggered);
	return bTriggered ? ETriggerState::Triggered : (bFoundActiveTrigger ? ETriggerState::Ongoing : ETriggerState::None);
}

// TODO: Hacky. This is the state we should return if we have evaluated no valid triggers. Set during action evaluation based on final ModifiedValue.
void FTriggerStateTracker::SetStateForNoTriggers(ETriggerState State)
{
	NoTriggerState = State;
}

#if WITH_EDITOR
// Record input action property changes for later processing

TSet<const UInputAction*> UInputAction::ActionsWithModifiedValueTypes;

void UInputAction::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// If our value type changes we need to inform any blueprint InputActionEx nodes that refer to this action
	if (PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UInputAction, ValueType) ||
		PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UInputAction, Triggers))
	{
		ActionsWithModifiedValueTypes.Add(this);
	}
}

EDataValidationResult UInputAction::IsDataValid(TArray<FText>& ValidationErrors)
{
	EDataValidationResult Result = CombineDataValidationResults(Super::IsDataValid(ValidationErrors), EDataValidationResult::Valid);

	// Validate the triggers
	for (const TObjectPtr<UInputTrigger> Trigger : Triggers)
	{
		if (Trigger)
		{
			Result = CombineDataValidationResults(Result, Trigger->IsDataValid(ValidationErrors));
		}
		else
		{
			Result = EDataValidationResult::Invalid;
			ValidationErrors.Add(LOCTEXT("NullInputTrigger", "There cannot be a null Input Trigger on an Input Action!"));
		}
	}
	
	// Validate the modifiers
	for (const TObjectPtr<UInputModifier> Modifier : Modifiers)
	{
		if (Modifier)
		{
			Result = CombineDataValidationResults(Result, Modifier->IsDataValid(ValidationErrors));	
		}
		else
		{
			Result = EDataValidationResult::Invalid;
			ValidationErrors.Add(LOCTEXT("NullInputModifier", "There cannot be a null Input Modifier on an Input Action!"));
		}		
	}

	return Result;
}

ETriggerEventsSupported UInputAction::GetSupportedTriggerEvents() const
{
	ETriggerEventsSupported SupportedTriggers = ETriggerEventsSupported::None;

	bool bTriggersAdded = false;
	
	if(!Triggers.IsEmpty())
	{
		for(const UInputTrigger* Trigger : Triggers)
		{
			if(Trigger)
			{
				EnumAddFlags(SupportedTriggers, Trigger->GetSupportedTriggerEvents());
				bTriggersAdded = true;
			}
		}
	}

	// If there are no triggers on an action, then it can be instant (a key is pressed/release) or happening over time(key is held down)
	if(!bTriggersAdded)
	{
		EnumAddFlags(SupportedTriggers, ETriggerEventsSupported::Instant | ETriggerEventsSupported::Uninterruptible);
	}

	return SupportedTriggers;
}

#endif

#undef LOCTEXT_NAMESPACE
