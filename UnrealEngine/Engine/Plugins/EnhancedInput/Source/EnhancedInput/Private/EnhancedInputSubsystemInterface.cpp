// Copyright Epic Games, Inc. All Rights Reserved.

#include "EnhancedInputSubsystemInterface.h"

#include "EnhancedInputModule.h"
#include "EnhancedInputPlatformSettings.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include "EnhancedInputDeveloperSettings.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "InputMappingContext.h"
#include "InputMappingQuery.h"
#include "PlayerMappableInputConfig.h"
#include "PlayerMappableKeySettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EnhancedInputSubsystemInterface)

/* Shared input subsystem functionality.
 * See EnhancedInputSubsystemInterfaceDebug.cpp for debug specific functionality.
 */

static constexpr int32 GGlobalAxisConfigMode_Default = 0;
static constexpr int32 GGlobalAxisConfigMode_All = 1;
static constexpr int32 GGlobalAxisConfigMode_None = 2;

static int32 GGlobalAxisConfigMode = 0;
static FAutoConsoleVariableRef GCVarGlobalAxisConfigMode(
	TEXT("input.GlobalAxisConfigMode"),
	GGlobalAxisConfigMode,
	TEXT("Whether or not to apply Global Axis Config settings. 0 = Default (Mouse Only), 1 = All, 2 = None")
);

template<typename T>
void DeepCopyPtrArray(const TArray<T*>& From, TArray<T*>& To)
{
	To.Empty(From.Num());
	for (T* ToDuplicate : From)
	{
		if (ToDuplicate)
		{
			To.Add(DuplicateObject<T>(ToDuplicate, nullptr));
		}
	}
}

void IEnhancedInputSubsystemInterface::InitalizeUserSettings()
{
	// Not every implementer of the EI subsystem wants user settings, so leave it up to them to determine if they want it or not
}

UEnhancedInputUserSettings* IEnhancedInputSubsystemInterface::GetUserSettings() const
{
	// Not every implementer of the EI subsystem wants user settings, so leave it up to them to determine if they want it or not
	return nullptr;
}

void IEnhancedInputSubsystemInterface::BindUserSettingDelegates()
{
	UEnhancedInputUserSettings* Settings = GetUserSettings();
	if (!Settings)
	{
		UE_LOG(LogEnhancedInput, Error, TEXT("Unable to get the user settings object!"));
		return;
	}

	// There is no need to bind to any delegates if the setting is turned off. We shouldn't even get here,
	// but do this in case someone implements this interface
	if (!GetDefault<UEnhancedInputDeveloperSettings>()->bEnableUserSettings)
	{
		UE_LOG(LogEnhancedInput, Error, TEXT("Attempting to bind to user settings delegates but they are disabled in UEnhancedInputDeveloperSettings!"));
		return;
	}

	Settings->OnSettingsChanged.AddUniqueDynamic(this, &IEnhancedInputSubsystemInterface::OnUserSettingsChanged);
	Settings->OnKeyProfileChanged.AddUniqueDynamic(this, &IEnhancedInputSubsystemInterface::OnUserKeyProfileChanged);
}

void IEnhancedInputSubsystemInterface::OnUserSettingsChanged(UEnhancedInputUserSettings* Settings)
{
	// We want to rebuild our control mappings whenever a setting has changed
	RequestRebuildControlMappings();
}

void IEnhancedInputSubsystemInterface::OnUserKeyProfileChanged(const UEnhancedPlayerMappableKeyProfile* InNewProfile)
{
	// We want to rebuild our control mappings whenever a setting has changed
	RequestRebuildControlMappings();
}

void IEnhancedInputSubsystemInterface::InjectInputForAction(const UInputAction* Action, FInputActionValue RawValue, const TArray<UInputModifier*>& Modifiers, const TArray<UInputTrigger*>& Triggers)
{
	if(UEnhancedPlayerInput* PlayerInput = GetPlayerInput())
	{
		PlayerInput->InjectInputForAction(Action, RawValue, Modifiers, Triggers);
	}
}

void IEnhancedInputSubsystemInterface::InjectInputVectorForAction(const UInputAction* Action, FVector Value, const TArray<UInputModifier*>& Modifiers, const TArray<UInputTrigger*>& Triggers)
{
	FInputActionValue RawValue((Action != nullptr) ? Action->ValueType : EInputActionValueType::Boolean, Value);
	InjectInputForAction(Action, RawValue, Modifiers, Triggers);
}

void IEnhancedInputSubsystemInterface::InjectInputForPlayerMapping(const FName MappingName, FInputActionValue RawValue, const TArray<UInputModifier*>& Modifiers, const TArray<UInputTrigger*>& Triggers)
{
	InjectInputVectorForPlayerMapping(MappingName, RawValue.Get<FVector>(), Modifiers, Triggers);
}

void IEnhancedInputSubsystemInterface::StartContinuousInputInjectionForAction(const UInputAction* Action, FInputActionValue RawValue, const TArray<UInputModifier*>& Modifiers, const TArray<UInputTrigger*>& Triggers)
{
	FInjectedInput& Injection = GetContinuouslyInjectedInputs().FindOrAdd(Action);

	Injection.RawValue = RawValue;
	DeepCopyPtrArray<UInputModifier>(Modifiers, Injection.Modifiers);
	DeepCopyPtrArray<UInputTrigger>(Triggers, Injection.Triggers);
}

void IEnhancedInputSubsystemInterface::StartContinuousInputInjectionForPlayerMapping(const FName MappingName, FInputActionValue RawValue, const TArray<UInputModifier*>& Modifiers, const TArray<UInputTrigger*>& Triggers)
{
	if (const UEnhancedInputUserSettings* UserSettings = GetUserSettings())
	{
		if (const UInputAction* Action = UserSettings->FindInputActionForMapping(MappingName))
		{
			StartContinuousInputInjectionForAction(Action, RawValue, Modifiers, Triggers);
		}
		else
		{
			UE_LOG(LogEnhancedInput, Warning, TEXT("Could not find a Input Action for mapping name '%s'"), *MappingName.ToString());
		}
	}
	else
	{
		UE_LOG(LogEnhancedInput, Warning, TEXT("Could not find a valid UEnhancedInputUserSettings object, is it enabled in the project settings?"));
	}
}

void IEnhancedInputSubsystemInterface::UpdateValueOfContinuousInputInjectionForAction(const UInputAction* Action, FInputActionValue RawValue)
{
	FInjectedInput& Injection = GetContinuouslyInjectedInputs().FindOrAdd(Action);
	Injection.RawValue = RawValue;

	// Do NOT update the triggers/modifiers here to preserve their state
}

void IEnhancedInputSubsystemInterface::UpdateValueOfContinuousInputInjectionForPlayerMapping(const FName MappingName, FInputActionValue RawValue)
{
	if (const UEnhancedInputUserSettings* UserSettings = GetUserSettings())
	{
		if (const UInputAction* Action = UserSettings->FindInputActionForMapping(MappingName))
		{
			UpdateValueOfContinuousInputInjectionForAction(Action, RawValue);
		}
		else
		{
			UE_LOG(LogEnhancedInput, Warning, TEXT("Could not find a Input Action for mapping name '%s'"), *MappingName.ToString());
		}
	}
	else
	{
		UE_LOG(LogEnhancedInput, Warning, TEXT("Could not find a valid UEnhancedInputUserSettings object, is it enabled in the project settings?"));
	}
}

void IEnhancedInputSubsystemInterface::StopContinuousInputInjectionForAction(const UInputAction* Action)
{
	GetContinuouslyInjectedInputs().Remove(Action);
}

void IEnhancedInputSubsystemInterface::StopContinuousInputInjectionForPlayerMapping(const FName MappingName)
{
	if (const UEnhancedInputUserSettings* UserSettings = GetUserSettings())
	{
		if (const UInputAction* Action = UserSettings->FindInputActionForMapping(MappingName))
		{
			StopContinuousInputInjectionForAction(Action);
		}
		else
		{
			UE_LOG(LogEnhancedInput, Warning, TEXT("Could not find a Input Action for mapping name '%s'"), *MappingName.ToString());
		}
	}
	else
	{
		UE_LOG(LogEnhancedInput, Warning, TEXT("Could not find a valid UEnhancedInputUserSettings object, is it enabled in the project settings?"));
	}
}

void IEnhancedInputSubsystemInterface::InjectInputVectorForPlayerMapping(const FName MappingName, FVector Value, const TArray<UInputModifier*>& Modifiers, const TArray<UInputTrigger*>& Triggers)
{
	if (const UEnhancedInputUserSettings* UserSettings = GetUserSettings())
	{
		if (const UInputAction* Action = UserSettings->FindInputActionForMapping(MappingName))
		{
			FInputActionValue RawValue(Action->ValueType, Value);
			InjectInputForAction(Action, RawValue, Modifiers, Triggers);
		}
		else
		{
			UE_LOG(LogEnhancedInput, Warning, TEXT("Could not find a Input Action for mapping name '%s'"), *MappingName.ToString());
		}
	}
	else
	{
		UE_LOG(LogEnhancedInput, Warning, TEXT("Could not find a valid UEnhancedInputUserSettings object, is it enabled in the project settings?"));
	}
}

void IEnhancedInputSubsystemInterface::ClearAllMappings()
{
	if (UEnhancedPlayerInput* PlayerInput = GetPlayerInput())
	{
		PlayerInput->AppliedInputContexts.Empty();
		RequestRebuildControlMappings();
	}
}

void IEnhancedInputSubsystemInterface::AddMappingContext(const UInputMappingContext* MappingContext, int32 Priority, const FModifyContextOptions& Options)
{
	// Layer mappings on top of existing mappings
	if (MappingContext)
	{
		if (UEnhancedPlayerInput* PlayerInput = GetPlayerInput())
		{
			PlayerInput->AppliedInputContexts.Add(MappingContext, Priority);
			RequestRebuildControlMappings(Options);
		}

		if (Options.bNotifyUserSettings)
		{
			if (UEnhancedInputUserSettings* Settings = GetUserSettings())
			{
				Settings->RegisterInputMappingContext(MappingContext);
			}
		}
	}
	else
	{
		UE_LOG(LogEnhancedInput, Warning, TEXT("Called AddMappingContext with a null Mapping Context! No changes have been applied."));
	}
}

void IEnhancedInputSubsystemInterface::RemoveMappingContext(const UInputMappingContext* MappingContext, const FModifyContextOptions& Options)
{
	if (MappingContext)
	{
		if (UEnhancedPlayerInput* PlayerInput = GetPlayerInput())
		{
			PlayerInput->AppliedInputContexts.Remove(MappingContext);
			RequestRebuildControlMappings(Options);
		}

		if (Options.bNotifyUserSettings)
		{
			if (UEnhancedInputUserSettings* Settings = GetUserSettings())
			{
				Settings->UnregisterInputMappingContext(MappingContext);
			}
		}
	}
}

void IEnhancedInputSubsystemInterface::RequestRebuildControlMappings(const FModifyContextOptions& Options, EInputMappingRebuildType MappingRebuildType)
{
	bMappingRebuildPending = true;
	bIgnoreAllPressedKeysUntilReleaseOnRebuild &= Options.bIgnoreAllPressedKeysUntilRelease;
	MappingRebuildPending = MappingRebuildType;
	
	if (Options.bForceImmediately)
	{
		RebuildControlMappings();
	}
}

EMappingQueryResult IEnhancedInputSubsystemInterface::QueryMapKeyInActiveContextSet(const UInputMappingContext* InputContext, const UInputAction* Action, FKey Key, TArray<FMappingQueryIssue>& OutIssues, EMappingQueryIssue BlockingIssues/* = DefaultMappingIssues::StandardFatal*/)
{
	UEnhancedPlayerInput* PlayerInput = GetPlayerInput();
	if (!PlayerInput)
	{
		return EMappingQueryResult::Error_EnhancedInputNotEnabled;
	}

	// TODO: Inefficient, but somewhat forgivable as the mapping context count is likely to be single figure.
	TMap<TObjectPtr<const UInputMappingContext>, int32> OrderedInputContexts = PlayerInput->AppliedInputContexts;
	OrderedInputContexts.ValueSort([](const int32& A, const int32& B) { return A > B; });

	TArray<UInputMappingContext*> Applied;
	Applied.Reserve(OrderedInputContexts.Num());
	for (const TPair<TObjectPtr<const UInputMappingContext>, int32>& ContextPair : OrderedInputContexts)
	{
		Applied.Add(const_cast<UInputMappingContext*>(ToRawPtr(ContextPair.Key)));
	}

	return QueryMapKeyInContextSet(Applied, InputContext, Action, Key, OutIssues, BlockingIssues);
}

EMappingQueryResult IEnhancedInputSubsystemInterface::QueryMapKeyInContextSet(const TArray<UInputMappingContext*>& PrioritizedActiveContexts, const UInputMappingContext* InputContext, const UInputAction* Action, FKey Key, TArray<FMappingQueryIssue>& OutIssues, EMappingQueryIssue BlockingIssues/* = DefaultMappingIssues::StandardFatal*/)
{
	if (!Action)
	{
		return EMappingQueryResult::Error_InvalidAction;
	}

	OutIssues.Reset();

	// Report on keys being bound that don't support the action's value type.
	EInputActionValueType KeyValueType = FInputActionValue(Key).GetValueType();
	if (Action->ValueType != KeyValueType)
	{
		// We exclude bool -> Axis1D promotions, as these are commonly used for paired mappings (e.g. W + S/Negate bound to a MoveForward action), and are fairly intuitive anyway.
		if (Action->ValueType != EInputActionValueType::Axis1D || KeyValueType != EInputActionValueType::Boolean)
		{
			OutIssues.Add(KeyValueType < Action->ValueType ? EMappingQueryIssue::ForcesTypePromotion : EMappingQueryIssue::ForcesTypeDemotion);
		}
	}

	enum class EStage : uint8
	{
		Pre,
		Main,
		Post,
	};
	EStage Stage = EStage::Pre;

	EMappingQueryResult Result = EMappingQueryResult::MappingAvailable;

	// These will be ordered by priority
	for (const UInputMappingContext* BlockingContext : PrioritizedActiveContexts)
	{
		if (!BlockingContext)
		{
			continue;
		}

		// Update stage
		if (Stage == EStage::Main)
		{
			Stage = EStage::Post;
		}
		else if (BlockingContext == InputContext)
		{
			Stage = EStage::Main;
		}

		for (const FEnhancedActionKeyMapping& Mapping : BlockingContext->GetMappings())
		{
			if (Mapping.Key == Key && Mapping.Action)
			{
				FMappingQueryIssue Issue;
				// Block mappings that would have an unintended effect with an existing mapping
				// TODO: This needs to apply chording input consumption rules
				if (Stage == EStage::Pre && Mapping.Action->bConsumeInput)
				{
					Issue.Issue = EMappingQueryIssue::HiddenByExistingMapping;
				}
				else if (Stage == EStage::Post && Action->bConsumeInput)
				{
					Issue.Issue = EMappingQueryIssue::HidesExistingMapping;
				}
				else if (Stage == EStage::Main)
				{
					Issue.Issue = EMappingQueryIssue::CollisionWithMappingInSameContext;
				}

				// Block mapping over any action that refuses it.
				if (Mapping.Action->bReserveAllMappings)
				{
					Issue.Issue = EMappingQueryIssue::ReservedByAction;
				}

				if (Issue.Issue != EMappingQueryIssue::NoIssue)
				{
					Issue.BlockingContext = BlockingContext;
					Issue.BlockingAction = Mapping.Action;
					OutIssues.Add(Issue);

					if ((Issue.Issue & BlockingIssues) != EMappingQueryIssue::NoIssue)
					{
						Result = EMappingQueryResult::NotMappable;
					}
				}
			}
		}
	}

	// Context must be part of the tested collection. If we didn't find it raise an error.
	if (Stage < EStage::Main)
	{
		return EMappingQueryResult::Error_InputContextNotInActiveContexts;
	}

	return Result;

}

bool IEnhancedInputSubsystemInterface::HasTriggerWith(TFunctionRef<bool(const UInputTrigger*)> TestFn, const TArray<UInputTrigger*>& Triggers)
{
	for (const UInputTrigger* Trigger : Triggers)
	{
		if (TestFn(Trigger))
		{
			return true;
		}
	}
	return false;
};

void IEnhancedInputSubsystemInterface::InjectChordBlockers(const TArray<int32>& ChordedMappings)
{
	UEnhancedPlayerInput* PlayerInput = GetPlayerInput();
	if (!PlayerInput)
	{
		return;
	}

	// Inject chord blockers into all lower priority action mappings with a shared key
	for (int32 MappingIndex : ChordedMappings)
	{
		FEnhancedActionKeyMapping& ChordMapping = PlayerInput->EnhancedActionMappings[MappingIndex];
		for (int32 i = MappingIndex + 1; i < PlayerInput->EnhancedActionMappings.Num(); ++i)
		{
			FEnhancedActionKeyMapping& Mapping = PlayerInput->EnhancedActionMappings[i];
			if (Mapping.Action && Mapping.Key == ChordMapping.Key)
			{
				// If we have no explicit triggers we can't inject an implicit as it may cause us to fire when we shouldn't.
				auto AnyExplicit = [](const UInputTrigger* Trigger) { return Trigger->GetTriggerType() == ETriggerType::Explicit; };
				if (!HasTriggerWith(AnyExplicit, Mapping.Triggers) && !HasTriggerWith(AnyExplicit, Mapping.Action->Triggers))
				{
					// Insert a down trigger to ensure we have valid rules for triggering when the chord blocker is active.
					Mapping.Triggers.Add(NewObject<UInputTriggerDown>());
					Mapping.Triggers.Last()->ActuationThreshold = SMALL_NUMBER;	// TODO: "No trigger" actuates on any non-zero value but Down has a threshold so this is a hack to reproduce no trigger behavior!
				}

				UInputTriggerChordBlocker* ChordBlocker = NewObject<UInputTriggerChordBlocker>(PlayerInput);
				ChordBlocker->ChordAction = ChordMapping.Action;
				// TODO: If the chording action is bound at a lower priority than the blocked action its trigger state will be evaluated too late, which may produce unintended effects on the first tick.
				Mapping.Triggers.Add(ChordBlocker);
			}
		}
	}
}

void IEnhancedInputSubsystemInterface::ApplyAxisPropertyModifiers(UEnhancedPlayerInput* PlayerInput, FEnhancedActionKeyMapping& Mapping) const
{
	// Axis properties are treated as per-key default modifier layouts.

	// TODO: Make this optional? Opt in or out? Per modifier type?
	//if (!EnhancedInputSettings.bApplyAxisPropertiesAsModifiers)
	//{
	//	return;
	//}

	if (GGlobalAxisConfigMode_None == GGlobalAxisConfigMode)
	{
		return;
	}

	// TODO: This function is causing issues with gamepads, applying a hidden 0.25 deadzone modifier by default. Apply it to mouse inputs only until a better system is in place.
	if (GGlobalAxisConfigMode_All != GGlobalAxisConfigMode &&
		!Mapping.Key.IsMouseButton())
	{
		return;
	}

	// Apply applicable axis property modifiers from the old input settings automatically.
	// TODO: This needs to live at the EnhancedInputSettings level.
	// TODO: Adopt this approach for all modifiers? Most of these are better done at the action level for most use cases.
	FInputAxisProperties AxisProperties;
	if (PlayerInput->GetAxisProperties(Mapping.Key, AxisProperties))
	{
		TArray<TObjectPtr<UInputModifier>> Modifiers;

		// If a modifier already exists it should override axis properties.
		auto HasExistingModifier = [&Mapping](UClass* OfType)
		{
			auto TypeMatcher = [&OfType](UInputModifier* Modifier) { return Modifier != nullptr && Modifier->IsA(OfType); };
			return Mapping.Modifiers.ContainsByPredicate(TypeMatcher) || Mapping.Action->Modifiers.ContainsByPredicate(TypeMatcher);
		};

		// Maintain old input system modification order.

		if (AxisProperties.DeadZone > 0.f &&
			!HasExistingModifier(UInputModifierDeadZone::StaticClass()))
		{
			UInputModifierDeadZone* DeadZone = NewObject<UInputModifierDeadZone>();
			DeadZone->LowerThreshold = AxisProperties.DeadZone;
			DeadZone->Type = EDeadZoneType::Axial;
			Modifiers.Add(DeadZone);
		}

		if (AxisProperties.Exponent != 1.f &&
			!HasExistingModifier(UInputModifierResponseCurveExponential::StaticClass()))
		{
			UInputModifierResponseCurveExponential* Exponent = NewObject<UInputModifierResponseCurveExponential>();
			Exponent->CurveExponent = FVector::OneVector * AxisProperties.Exponent;
			Modifiers.Add(Exponent);
		}

		// Sensitivity stacks with user defined.
		// TODO: Unexpected behavior but makes sense for most use cases. E.g. Mouse sensitivity, which is scaled by 0.07 in BaseInput.ini, would be broken by adding a Look action sensitivity.
		if (AxisProperties.Sensitivity != 1.f /* &&
			!HasExistingModifier(UInputModifierScalar::StaticClass())*/)
		{
			UInputModifierScalar* Sensitivity = NewObject<UInputModifierScalar>();
			Sensitivity->Scalar = FVector::OneVector * AxisProperties.Sensitivity;
			Modifiers.Add(Sensitivity);
		}

		if (AxisProperties.bInvert &&
			!HasExistingModifier(UInputModifierNegate::StaticClass()))
		{
			Modifiers.Add(NewObject<UInputModifierNegate>());
		}

		// Add to front of modifier list (these modifiers should be executed before any user defined modifiers)
		Swap(Mapping.Modifiers, Modifiers);
		Mapping.Modifiers.Append(Modifiers);
	}
}

bool IEnhancedInputSubsystemInterface::HasMappingContext(const UInputMappingContext* MappingContext) const
{
	int32 DummyPri = INDEX_NONE;
	return HasMappingContext(MappingContext, DummyPri);
}

bool IEnhancedInputSubsystemInterface::HasMappingContext(const UInputMappingContext* MappingContext, int32& OutFoundPriority) const
{
	bool bResult = false;
	OutFoundPriority = INDEX_NONE;
	
	if (const UEnhancedPlayerInput* const Input = GetPlayerInput())
	{
		if (const int32* const FoundPriority = Input->AppliedInputContexts.Find(MappingContext))
		{
			OutFoundPriority = *FoundPriority;
			bResult = true;
		}
	}

	return bResult;
}

TArray<FKey> IEnhancedInputSubsystemInterface::QueryKeysMappedToAction(const UInputAction* Action) const
{
	TArray<FKey> MappedKeys;

	if (Action)
	{
		if (const UEnhancedPlayerInput* const PlayerInput = GetPlayerInput())
		{
			for (const FEnhancedActionKeyMapping& Mapping : PlayerInput->EnhancedActionMappings)
			{
				if (Mapping.Action == Action)
				{
					MappedKeys.AddUnique(Mapping.Key);
				}
			}
		}
	}

	return MappedKeys;
}

TArray<FEnhancedActionKeyMapping> IEnhancedInputSubsystemInterface::GetAllPlayerMappableActionKeyMappings() const
{
	TArray<FEnhancedActionKeyMapping> PlayerMappableMappings;
	
	if (const UEnhancedPlayerInput* const PlayerInput = GetPlayerInput())
    {
    	for (const FEnhancedActionKeyMapping& Mapping : PlayerInput->EnhancedActionMappings)
		{
			if (Mapping.IsPlayerMappable())
			{
				PlayerMappableMappings.AddUnique(Mapping);
			}
		}
    }
	
	return PlayerMappableMappings;
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS

int32 IEnhancedInputSubsystemInterface::AddPlayerMappedKey(const FName MappingName, const FKey NewKey, const FModifyContextOptions& Options /*= FModifyContextOptions()*/)
{
	return AddPlayerMappedKeyInSlot(MappingName, NewKey, FPlayerMappableKeySlot::FirstKeySlot, Options);
}

int32 IEnhancedInputSubsystemInterface::K2_AddPlayerMappedKeyInSlot(const FName MappingName, const FKey NewKey, const FPlayerMappableKeySlot& KeySlot /*= FPlayerMappableKeySlot()*/, const FModifyContextOptions& Options /*= FModifyContextOptions()*/)
{
	return AddPlayerMappedKeyInSlot(MappingName, NewKey, KeySlot, Options);
}

int32 IEnhancedInputSubsystemInterface::AddPlayerMappedKeyInSlot(const FName MappingName, const FKey NewKey, const FPlayerMappableKeySlot& KeySlot /*= FPlayerMappableKeySlot::FirstKeySlot*/, const FModifyContextOptions& Options /*= FModifyContextOptions()*/)
{
	if (UEnhancedInputUserSettings* Settings = GetUserSettings())
	{
		FMapPlayerKeyArgs Args = {};
		Args.MappingName = MappingName;
		Args.NewKey = NewKey;
		Args.Slot = static_cast<EPlayerMappableKeySlot>(KeySlot.GetSlotNumber());

		FGameplayTagContainer FailureReason;
		Settings->MapPlayerKey(Args, FailureReason);

		if (!FailureReason.IsEmpty())
		{
			UE_LOG(LogEnhancedInput, Warning, TEXT("IEnhancedInputSubsystemInterface::AddPlayerMappedKeyInSlot Failed! Reasoning: %s"), *FailureReason.ToString());
			return 0;
		}
	}

	RequestRebuildControlMappings(Options);
	return 1;
}

int32 IEnhancedInputSubsystemInterface::RemovePlayerMappedKey(const FName MappingName, const FModifyContextOptions& Options /*= FModifyContextOptions()*/)
{
	return RemovePlayerMappedKeyInSlot(MappingName, FPlayerMappableKeySlot::FirstKeySlot, Options);
}

int32 IEnhancedInputSubsystemInterface::K2_RemovePlayerMappedKeyInSlot(const FName MappingName, const FPlayerMappableKeySlot& KeySlot /*= FPlayerMappableKeySlot()*/, const FModifyContextOptions& Options /*= FModifyContextOptions()*/)
{
	return RemovePlayerMappedKeyInSlot(MappingName, KeySlot, Options);
}

int32 IEnhancedInputSubsystemInterface::RemovePlayerMappedKeyInSlot(const FName MappingName, const FPlayerMappableKeySlot& KeySlot /*= FPlayerMappableKeySlot::FirstKeySlot*/, const FModifyContextOptions& Options /*= FModifyContextOptions()*/)
{
	if (UEnhancedInputUserSettings* Settings = GetUserSettings())
	{
		FMapPlayerKeyArgs Args = {};
		Args.MappingName = MappingName;
		Args.Slot = static_cast<EPlayerMappableKeySlot>(KeySlot.GetSlotNumber());
		
		FGameplayTagContainer FailureReason;
		Settings->UnMapPlayerKey(Args, FailureReason);

		if (!FailureReason.IsEmpty())
		{
			UE_LOG(LogEnhancedInput, Warning, TEXT("IEnhancedInputSubsystemInterface::RemovePlayerMappedKeyInSlot Failed! Reasoning: %s"), *FailureReason.ToString());
			return 0;
		}
	}
	
	RequestRebuildControlMappings(Options);

	return 1;
}

int32 IEnhancedInputSubsystemInterface::RemoveAllPlayerMappedKeysForMapping(const FName MappingName, const FModifyContextOptions& Options /*= FModifyContextOptions()*/)
{
	if (UEnhancedInputUserSettings* Settings = GetUserSettings())
	{
		FMapPlayerKeyArgs Args = {};
		Args.MappingName = MappingName;

		FGameplayTagContainer FailureReason;
		Settings->ResetAllPlayerKeysInRow(Args, FailureReason);
		
		if (!FailureReason.IsEmpty())
		{
			UE_LOG(LogEnhancedInput, Warning, TEXT("IEnhancedInputSubsystemInterface::RemovePlayerMappedKeyInSlot Failed! Reasoning: %s"), *FailureReason.ToString());
			return 0;
		}
	}

	RequestRebuildControlMappings(Options);

	return 1;
}

void IEnhancedInputSubsystemInterface::RemoveAllPlayerMappedKeys(const FModifyContextOptions& Options)
{
	if (UEnhancedInputUserSettings* Settings = GetUserSettings())
	{
		FGameplayTagContainer FailureReason;
		Settings->ResetKeyProfileToDefault(Settings->GetCurrentKeyProfileIdentifier(), FailureReason);
	}

	RequestRebuildControlMappings(Options);
}

FKey IEnhancedInputSubsystemInterface::GetPlayerMappedKey(const FName MappingName) const
{
	return GetPlayerMappedKeyInSlot(MappingName, FPlayerMappableKeySlot::FirstKeySlot);
}

FKey IEnhancedInputSubsystemInterface::K2_GetPlayerMappedKeyInSlot(const FName MappingName, const FPlayerMappableKeySlot& KeySlot /*= FPlayerMappableKeySlot()*/) const
{
	return GetPlayerMappedKeyInSlot(MappingName, KeySlot);
}

FKey IEnhancedInputSubsystemInterface::GetPlayerMappedKeyInSlot(const FName MappingName, const FPlayerMappableKeySlot& KeySlot /*= FPlayerMappableKeySlot()*/) const
{
	if (const UEnhancedInputUserSettings* Settings = GetUserSettings())
	{
		if (const UEnhancedPlayerMappableKeyProfile* KeyProfile = Settings->GetCurrentKeyProfile())
		{
			FPlayerMappableKeyQueryOptions Opts = {};
			Opts.MappingName = MappingName;
			Opts.SlotToMatch = static_cast<EPlayerMappableKeySlot>(KeySlot.GetSlotNumber());

			TArray<FKey> Keys;
			KeyProfile->QueryPlayerMappedKeys(Opts, OUT Keys);
			
			if (!Keys.IsEmpty())
			{
				return Keys[0];
			}
		}
	}
	
	return EKeys::Invalid;
}

TArray<FKey> IEnhancedInputSubsystemInterface::GetAllPlayerMappedKeys(const FName MappingName) const
{
	TArray<FKey> PlayerMappedKeys;
	
	if (const UEnhancedInputUserSettings* Settings = GetUserSettings())
	{
		if (const UEnhancedPlayerMappableKeyProfile* KeyProfile = Settings->GetCurrentKeyProfile())
		{
			FPlayerMappableKeyQueryOptions Opts = {};
			Opts.MappingName = MappingName;
			
			KeyProfile->QueryPlayerMappedKeys(Opts, OUT PlayerMappedKeys);
		}
	}
	
	return PlayerMappedKeys;
}

void IEnhancedInputSubsystemInterface::AddPlayerMappableConfig(const UPlayerMappableInputConfig* Config, const FModifyContextOptions& Options)
{
	if (Config)
	{
		if (GetDefault<UEnhancedInputDeveloperSettings>()->bLogOnDeprecatedConfigUsed && Config->IsDeprecated())
		{
			UE_LOG(LogEnhancedInput, Warning, TEXT("The Player Mappable Input Config '%s' is marked as deprecated, but is still being added!"), *Config->GetFName().ToString());
		}

		for (TPair<TObjectPtr<UInputMappingContext>, int32> Pair : Config->GetMappingContexts())
		{
			AddMappingContext(Pair.Key, Pair.Value, Options);
		}	
	}
}

void IEnhancedInputSubsystemInterface::RemovePlayerMappableConfig(const UPlayerMappableInputConfig* Config, const FModifyContextOptions& Options)
{
	if (Config)
	{
		if (GetDefault<UEnhancedInputDeveloperSettings>()->bLogOnDeprecatedConfigUsed && Config->IsDeprecated())
		{
			UE_LOG(LogEnhancedInput, Warning, TEXT("The Player Mappable Input Config '%s' is marked as deprecated, but is still being removed!"), *Config->GetFName().ToString());
		}

		for(TPair<TObjectPtr<UInputMappingContext>, int32> Pair : Config->GetMappingContexts())
		{
			RemoveMappingContext(Pair.Key, Options);
		}	
	}
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS

// TODO: This should be a delegate (along with InjectChordBlockers), moving chording out of the underlying subsystem and enabling implementation of custom mapping handlers.
/**
 * Reorder the given UnordedMappings such that chording mappings > chorded mappings > everything else.
 * This is used to ensure mappings within a single context are evaluated in the correct order to support chording.
 * Populate the DependentChordActions array with any chorded triggers so that we can detect which ones should be triggered
 * later. 
 */
TArray<FEnhancedActionKeyMapping> IEnhancedInputSubsystemInterface::ReorderMappings(const TArray<FEnhancedActionKeyMapping>& UnorderedMappings, TArray<UEnhancedPlayerInput::FDependentChordTracker>& OUT DependentChordActions)
{
	TSet<const UInputAction*> ChordingActions;

	// Gather all chording actions within a mapping's triggers.
	auto GatherChordingActions = [&ChordingActions, &DependentChordActions](const FEnhancedActionKeyMapping& Mapping)
	{
		bool bFoundChordTrigger = false;
		auto EvaluateTriggers = [&Mapping, &ChordingActions, &bFoundChordTrigger, &DependentChordActions](const TArray<UInputTrigger*>& Triggers)
		{
			for (const UInputTrigger* Trigger : Triggers)
			{
				if (const UInputTriggerChordAction* ChordTrigger = Cast<const UInputTriggerChordAction>(Trigger))
				{
					ChordingActions.Add(ChordTrigger->ChordAction);

					// Keep track of the action itself, and the action it is dependant on
					DependentChordActions.Emplace(UEnhancedPlayerInput::FDependentChordTracker { Mapping.Action, ChordTrigger->ChordAction });
					
					bFoundChordTrigger = true;
				}
			}
		};
		EvaluateTriggers(Mapping.Triggers);
		
		if(ensureMsgf(Mapping.Action, TEXT("A key mapping has no associated action!")))
		{
			EvaluateTriggers(Mapping.Action->Triggers);			
		}

		return bFoundChordTrigger;
	};

	// Split chorded mappings (second priority) from all others whilst building a list of chording actions to use for further prioritization.
	TArray<FEnhancedActionKeyMapping> ChordedMappings;
	TArray<FEnhancedActionKeyMapping> OtherMappings;
	OtherMappings.Reserve(UnorderedMappings.Num());		// Mappings will most likely be Other
	int32 NumEmptyMappings = 0;
	for (const FEnhancedActionKeyMapping& Mapping : UnorderedMappings)
	{
		if(Mapping.Action)
		{
			TArray<FEnhancedActionKeyMapping>& MappingArray = GatherChordingActions(Mapping) ? ChordedMappings : OtherMappings;
			MappingArray.Add(Mapping);
		}
		else
		{
			++NumEmptyMappings;
			UE_LOG(LogEnhancedInput, Warning, TEXT("A Key Mapping with a blank action has been added! Ignoring the key mapping to '%s'"), *Mapping.Key.ToString());
		}
	}

	TArray<FEnhancedActionKeyMapping> OrderedMappings;
	OrderedMappings.Reserve(UnorderedMappings.Num());

	// Move chording mappings to the front as they need to be evaluated before chord and blocker triggers
	// TODO: Further ordering of chording mappings may be required should one of them be chorded against another
	auto ExtractChords = [&OrderedMappings, &ChordingActions](TArray<FEnhancedActionKeyMapping>& Mappings) {
		for (int32 i = 0; i < Mappings.Num();)
		{
			if (ChordingActions.Contains(Mappings[i].Action))
			{
				OrderedMappings.Add(Mappings[i]);
				Mappings.RemoveAtSwap(i);	// TODO: Do we care about reordering underlying mappings?
			}
			else
			{
				++i;
			}
		}
	};
	ExtractChords(ChordedMappings);
	ExtractChords(OtherMappings);

	OrderedMappings.Append(MoveTemp(ChordedMappings));
	OrderedMappings.Append(MoveTemp(OtherMappings));
	checkf(OrderedMappings.Num() == UnorderedMappings.Num() - NumEmptyMappings, TEXT("Number of mappings unexpectedly changed during reorder."));

	return OrderedMappings;
}

void IEnhancedInputSubsystemInterface::RebuildControlMappings()
{
	if(MappingRebuildPending == EInputMappingRebuildType::None)
	{
		return;
	}

	UEnhancedPlayerInput* PlayerInput = GetPlayerInput();
	if (!PlayerInput)
	{
		// TODO: Prefer to reset MappingRebuildPending here?
		return;
	}

	// Clear existing mappings, but retain the mapping array for later processing
	TArray<FEnhancedActionKeyMapping> OldMappings(MoveTemp(PlayerInput->EnhancedActionMappings));
	PlayerInput->ClearAllMappings();
	PlayerInput->KeyConsumptionData.Reset();
	AppliedContextRedirects.Reset();

	// Order contexts by priority
	TMap<TObjectPtr<const UInputMappingContext>, int32> OrderedInputContexts = PlayerInput->AppliedInputContexts;
	
	// Replace any mapping contexts that may have a redirect on this platform
	if (UEnhancedInputPlatformSettings* PlatformSettings = UEnhancedInputPlatformSettings::Get())
	{
		TMap<TObjectPtr<const UInputMappingContext>, TObjectPtr<const UInputMappingContext>> ContextRedirects;
		PlatformSettings->GetAllMappingContextRedirects(ContextRedirects);
		for (const TPair<TObjectPtr<const UInputMappingContext>, TObjectPtr<const UInputMappingContext>>& Pair : ContextRedirects)
		{
			if (!Pair.Key || !Pair.Value)
			{
				UE_LOG(LogEnhancedInput, Error, TEXT("An invalid Mappping Context Redirect specified in '%s'"), PlatformSettings->GetConfigOverridePlatform());
				continue;
			}
			
			// Replace the existing IMC with the one that it should be redirected to on the PlayerInput 
			if (const int32* ExistingIMCPriority = OrderedInputContexts.Find(Pair.Key))
			{
				OrderedInputContexts.Remove(Pair.Key);
				OrderedInputContexts.Add(Pair.Value, *ExistingIMCPriority);
				AppliedContextRedirects.Add(Pair);

				// Optional logging that may be helpful for debugging purposes
				if (PlatformSettings->ShouldLogMappingContextRedirects())
				{
					UE_LOG(
						LogEnhancedInput,
						Log,
						TEXT("'%s' Redirecting Mapping Context '%s' -> '%s'"),
						PlatformSettings->GetConfigOverridePlatform(), *Pair.Key->GetName(), *Pair.Value->GetName()
					);
				}
			}
		}
	}
	
	// Order contexts by priority
	OrderedInputContexts.ValueSort([](const int32& A, const int32& B) { return A > B; });

	TSet<FKey> AppliedKeys;

	TArray<int32> ChordedMappings;

	// Reset the tracking of dependant chord actions on the player input
	PlayerInput->DependentChordActions.Reset();

	UEnhancedInputUserSettings* CurrentUserSettings = GetUserSettings();
	UEnhancedPlayerMappableKeyProfile* PlayerKeyProfile = CurrentUserSettings ? CurrentUserSettings->GetCurrentKeyProfile() : nullptr;

	// An array of keys that are mapped to a given Action.
	// This is populated by any player mapped keys if they exist, or the default mapping from
	// an input mapping context.
	TArray<FKey> MappedKeysToActionName;
	
	for (const TPair<TObjectPtr<const UInputMappingContext>, int32>& ContextPair : OrderedInputContexts)
	{
		// Don't apply context specific keys immediately, allowing multiple mappings to the same key within the same context if required.
		TArray<FKey> ContextAppliedKeys;
		const UInputMappingContext* MappingContext = ContextPair.Key;
		TArray<FEnhancedActionKeyMapping> OrderedMappings = ReorderMappings(MappingContext->GetMappings(), PlayerInput->DependentChordActions);

		for (FEnhancedActionKeyMapping& Mapping : OrderedMappings)
		{
			// Clear out mappings from the previous iteration
			MappedKeysToActionName.Reset();
			
			const UPlayerMappableKeySettings* KeySettings = Mapping.GetPlayerMappableKeySettings();

			// If this mapping has specified a specific key profile, and the current profile isn't it, then don't add this key mapping
			if (KeySettings && PlayerKeyProfile && !KeySettings->SupportedKeyProfiles.IsEmpty() && !KeySettings->SupportedKeyProfiles.HasTag(PlayerKeyProfile->GetProfileIdentifer()))
			{
				continue;			
			}
			
			// See if there are any player mapped keys to this action
			if (PlayerKeyProfile && GetDefault<UEnhancedInputDeveloperSettings>()->bEnableUserSettings)
			{
				PlayerKeyProfile->GetPlayerMappedKeysForRebuildControlMappings(Mapping, MappedKeysToActionName);
			}

			// True if there were any player mapped keys to this mapping and we are using those instead.
			const bool bIsPlayerMapping = !MappedKeysToActionName.IsEmpty();

			// If there aren't, then just use the default mapping for this action
			if (!bIsPlayerMapping)
			{
				MappedKeysToActionName.Add(Mapping.Key);
			}
			
			for (const FKey& PlayerMappedKey : MappedKeysToActionName)
			{
				Mapping.Key = PlayerMappedKey;

				// If this Input Action is flagged to consume input, then mark it's key state as being consumed every tick.
				// This has the affect where the base UPlayerInput class will not fire any legacy bindings
				if (Mapping.Action->bConsumesActionAndAxisMappings)
				{
					FKeyConsumptionOptions& Opts = PlayerInput->KeyConsumptionData.FindOrAdd(Mapping.Action);
					Opts.KeysToConsume.AddUnique(Mapping.Key);
					Opts.EventsToCauseConsumption |= static_cast<ETriggerEvent>(Mapping.Action->TriggerEventsThatConsumeLegacyKeys);
				}
				
				if (Mapping.Action && !AppliedKeys.Contains(Mapping.Key))
				{
					// TODO: Wasteful query as we've already established chord state within ReorderMappings. Store TOptional bConsumeInput per mapping, allowing override? Query override via delegate?
					auto IsChord = [](const UInputTrigger* Trigger)
					{
						return Cast<const UInputTriggerChordAction>(Trigger) != nullptr;
					};
					bool bHasActionChords = HasTriggerWith(IsChord, Mapping.Action->Triggers);
					bool bHasChords = bHasActionChords || HasTriggerWith(IsChord, Mapping.Triggers);

					// Chorded actions can't consume input or they would hide the action they are chording.
					if (!bHasChords && Mapping.Action->bConsumeInput)
					{
						ContextAppliedKeys.Add(Mapping.Key);
					}

					int32 NewMappingIndex = PlayerInput->AddMapping(Mapping);
					FEnhancedActionKeyMapping& NewMapping = PlayerInput->EnhancedActionMappings[NewMappingIndex];

					// Re-instance modifiers
					DeepCopyPtrArray<UInputModifier>(Mapping.Modifiers, MutableView(NewMapping.Modifiers));

					ApplyAxisPropertyModifiers(PlayerInput, NewMapping);

					// Re-instance triggers
					DeepCopyPtrArray<UInputTrigger>(Mapping.Triggers, MutableView(NewMapping.Triggers));

					if (bHasChords)
					{
						// TODO: Re-prioritize chorded mappings (within same context only?) by number of chorded actions, so Ctrl + Alt + [key] > Ctrl + [key] > [key].
						// TODO: Above example shouldn't block [key] if only Alt is down, as there is no direct Alt + [key] mapping.y
						ChordedMappings.Add(NewMappingIndex);

						// Action level chording triggers need to be evaluated at the mapping level to ensure they block early enough.
						// TODO: Continuing to evaluate these at the action level is redundant.
						if (bHasActionChords)
						{
							for (const UInputTrigger* Trigger : Mapping.Action->Triggers)
							{
								if (IsChord(Trigger))
								{
									NewMapping.Triggers.Add(DuplicateObject(Trigger, nullptr));
								}
							}
						}
					}
				}
			}
		}

		AppliedKeys.Append(MoveTemp(ContextAppliedKeys));
	}

	InjectChordBlockers(ChordedMappings);

	PlayerInput->ForceRebuildingKeyMaps(false);

	// Clean out invalidated actions
	if (MappingRebuildPending == EInputMappingRebuildType::RebuildWithFlush)
	{
		PlayerInput->ActionInstanceData.Empty();
	}
	else
	{
		
		// Remove action instance data for actions that are not referenced in the new action mappings
		TSet<const UInputAction*> RemovedActions;
		for (TPair<TObjectPtr<const UInputAction>, FInputActionInstance>& ActionInstance : PlayerInput->ActionInstanceData)
		{
			RemovedActions.Add(ActionInstance.Key.Get());
		}

		// Return true if the given FKey was in the old Player Input mappings
		auto WasInOldMapping = [&OldMappings](const FKey& InKey) -> bool
		{
			return OldMappings.ContainsByPredicate(
				[&InKey](const FEnhancedActionKeyMapping& OldMapping){ return OldMapping.Key == InKey; }
				);
		};
	
		for (FEnhancedActionKeyMapping& Mapping : PlayerInput->EnhancedActionMappings)
		{
			RemovedActions.Remove(Mapping.Action);

			// Was this key pressed last frame? If so, then we need to mark it to be ignored by PlayerInput
			// until it is released to avoid re-processing a triggered event.
			// This also prevents actions from triggering if the key is being held whilst the IMC is added and bIgnoreAllPressedKeysUntilReleaseOnRebuild
			// has been set by the user.
			if (bIgnoreAllPressedKeysUntilReleaseOnRebuild && Mapping.Action->ValueType == EInputActionValueType::Boolean)
			{				
				const FKeyState* KeyState = PlayerInput->GetKeyState(Mapping.Key);
				if(KeyState && KeyState->bDown)
				{
					Mapping.bShouldBeIgnored = true;
				}
			}

			// Retain old mapping trigger/modifier state for identical key -> action mappings.
			TArray<FEnhancedActionKeyMapping>::SizeType Idx = OldMappings.IndexOfByPredicate(
				[&Mapping](const FEnhancedActionKeyMapping& Other)
				{
					// Use Equals() to ignore Triggers' values. We want to keep their values from before remapping to
					// prevent resets. Otherwise, triggers like UInputTriggerPressed re-trigger when their value is
					// reset to 0; and time counting triggers, like UInputTriggerHold, restart their time.
					// But don't ignore Modifier and Trigger types and their order in the comparison. If we did, we'd
					// replace new mappings for old ones with different Trigger and Modifier settings.
					return Mapping.Equals(Other);
				});
			if (Idx != INDEX_NONE)
			{
				Mapping = MoveTemp(OldMappings[Idx]);
				OldMappings.RemoveAtSwap(Idx);
			}
		}
		for (const UInputAction* Action : RemovedActions)
		{
			PlayerInput->ActionInstanceData.Remove(Action);
		}	
	}

	// Perform a modifier calculation pass on the default data to initialize values correctly.
	// We do this at the end to ensure ActionInstanceData is accessible without requiring a tick for new/flushed actions.
	for (FEnhancedActionKeyMapping& Mapping : PlayerInput->EnhancedActionMappings)
	{
		PlayerInput->InitializeMappingActionModifiers(Mapping);
	}
	
	MappingRebuildPending = EInputMappingRebuildType::None;
	bIgnoreAllPressedKeysUntilReleaseOnRebuild = true;
	bControlMappingsRebuiltThisTick = true;
}

template<typename T>
void InjectKey(T* InjectVia, FKey Key, const FInputActionValue& Value, float DeltaTime)
{
	// TODO: Overwrite PlayerInput->KeyStateMap directly to block device inputs whilst these are active?
	// TODO: Multi axis FKey support
	if (Key.IsAnalog())
	{
		InjectVia->InputKey(FInputKeyParams(Key, Value.Get<FVector>(), DeltaTime, 1, Key.IsGamepadKey()));
	}
	else
	{
		// TODO: IE_Repeat support. Ideally ticking at whatever rate the application platform is sending repeat key messages.
		InjectVia->InputKey(FInputKeyParams(Key, IE_Pressed, static_cast<double>(Value.Get<float>()), Key.IsGamepadKey()));
	}
}

void IEnhancedInputSubsystemInterface::TickForcedInput(float DeltaTime)
{
	UEnhancedPlayerInput* PlayerInput = GetPlayerInput();
	if (!PlayerInput)
	{
		return;
	}

	// Any continuous input injection needs to be added each frame until its stopped
	TMap<TObjectPtr<const UInputAction>, FInjectedInput>& ContinuouslyInjectedInputs = GetContinuouslyInjectedInputs();
	for (TPair<TObjectPtr<const UInputAction>, FInjectedInput>& ContinuousInjection : ContinuouslyInjectedInputs)
	{
		TObjectPtr<const UInputAction>& Action = ContinuousInjection.Key;
		if (const UInputAction* InputAction = Action.Get())
		{
			PlayerInput->InjectInputForAction(InputAction, ContinuousInjection.Value.RawValue, ContinuousInjection.Value.Modifiers, ContinuousInjection.Value.Triggers);
		}
	}

	// Forced action triggering
	for (TPair<TWeakObjectPtr<const UInputAction>, FInputActionValue>& ForcedActionPair : ForcedActions)
	{
		TWeakObjectPtr<const UInputAction>& Action = ForcedActionPair.Key;
		if (const UInputAction* InputAction = Action.Get())
		{
			PlayerInput->InjectInputForAction(InputAction, ForcedActionPair.Value);	// TODO: Support modifiers and triggers?
		}
	}

	// Forced key presses
	for (const TPair<FKey, FInputActionValue>& ForcedKeyPair : ForcedKeys)
	{
		// Prefer sending the key pressed event via a player controller if one is available.
		if (APlayerController* Controller = Cast<APlayerController>(PlayerInput->GetOuter()))
		{
			InjectKey(Controller, ForcedKeyPair.Key, ForcedKeyPair.Value, DeltaTime);
		}
		else
		{
			InjectKey(PlayerInput, ForcedKeyPair.Key, ForcedKeyPair.Value, DeltaTime);
		}
	}
}

void IEnhancedInputSubsystemInterface::HandleControlMappingRebuildDelegate()
{
	if (bControlMappingsRebuiltThisTick)
	{
		ControlMappingsRebuiltThisFrame();
		
		bControlMappingsRebuiltThisTick = false;
	}
}

void IEnhancedInputSubsystemInterface::ApplyForcedInput(const UInputAction* Action, FInputActionValue Value)
{
	check(Action);
	ForcedActions.Emplace(Action, Value);		// TODO: Support modifiers and triggers?
}

void IEnhancedInputSubsystemInterface::ApplyForcedInput(FKey Key, FInputActionValue Value)
{
	check(Key.IsValid());
	ForcedKeys.Emplace(Key, Value);
}

void IEnhancedInputSubsystemInterface::RemoveForcedInput(const UInputAction* Action)
{
	ForcedActions.Remove(Action);
}

void IEnhancedInputSubsystemInterface::RemoveForcedInput(FKey Key)
{
	check(Key.IsValid());
	ForcedKeys.Remove(Key);

	if (UEnhancedPlayerInput* PlayerInput = GetPlayerInput())
	{
		FInputKeyParams Params;
		Params.Key = Key;
		Params.Delta = FVector::ZeroVector;
		Params.Event = EInputEvent::IE_Released;
		
		// Prefer sending the key released event via a player controller if one is available.
		if (APlayerController* Controller = Cast<APlayerController>(PlayerInput->GetOuter()))
		{
			Controller->InputKey(Params);
		}
		else
		{
			PlayerInput->InputKey(Params);
		}
	}
}
