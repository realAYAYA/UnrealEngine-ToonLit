// Copyright Epic Games, Inc. All Rights Reserved.
 
#include "CommonUITypes.h"
#include "CommonUIPrivate.h"
#include "ICommonInputModule.h"
#include "CommonInputSettings.h"
#include "HAL/PlatformInput.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CommonUITypes)

FScrollBoxStyle CommonUI::EmptyScrollBoxStyle = FScrollBoxStyle();

FCommonInputTypeInfo::FCommonInputTypeInfo()
{
	OverrrideState = EInputActionState::Enabled;
	OverrideBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	bActionRequiresHold = false;
	HoldTime = 0.5f;
}

FKey FCommonInputTypeInfo::GetKey() const
{
	return FPlatformInput::RemapKey(Key);
}

FCommonInputActionDataBase::FCommonInputActionDataBase()
{
}

bool FCommonInputActionDataBase::CanDisplayInReflector(ECommonInputType InputType, const FName& GamepadName) const
{
	if (InputType == ECommonInputType::MouseAndKeyboard)
	{
		return true;
	}
	else if (InputType == ECommonInputType::Gamepad)
	{
		const FCommonInputTypeInfo& TypeInfo = GetInputTypeInfo(InputType, GamepadName);
		return TypeInfo.GetKey().IsValid();
	}
	else if (InputType == ECommonInputType::Touch)
	{
		return true;
	}

	return false;
}

const FCommonInputTypeInfo& FCommonInputActionDataBase::GetCurrentInputTypeInfo(const UCommonInputSubsystem* CommonInputSubsystem) const
{
	ECommonInputType CurrentInputType = ECommonInputType::MouseAndKeyboard;
	FName CurrentGamepadName = FCommonInputDefaults::GamepadGeneric;

	if (CommonInputSubsystem) // may not always have a valid input Subsystem, e.g. for the demo driver's SpectatorPlayerController when recording a replay
	{
		CurrentInputType = CommonInputSubsystem->GetCurrentInputType();
		CurrentGamepadName = CommonInputSubsystem->GetCurrentGamepadName();
	}

	return GetInputTypeInfo(CurrentInputType, CurrentGamepadName);
}

const FCommonInputTypeInfo& FCommonInputActionDataBase::GetInputTypeInfo(ECommonInputType InputType, const FName& GamepadName) const
{
	if (InputType == ECommonInputType::MouseAndKeyboard)
	{
		return KeyboardInputTypeInfo;
	}
	else if (InputType == ECommonInputType::Gamepad)
	{
		const FCommonInputTypeInfo* GamepadTypeInfo = &DefaultGamepadInputTypeInfo;
		if (const FCommonInputTypeInfo* OverrideTypeInfo = GamepadInputOverrides.Find(GamepadName))
		{
			GamepadTypeInfo = OverrideTypeInfo;
		}

		//ensureAlways(NewGamepadTypeInfo->GetKey().IsValid());
		UE_CLOG(!GamepadTypeInfo->GetKey().IsValid(), LogCommonUI, Verbose, TEXT("Invalid default common action key for action \"%s\""), *DisplayName.ToString());
		return *GamepadTypeInfo;
	}
	else if (InputType == ECommonInputType::Touch)
	{
		return TouchInputTypeInfo;
	}

	return KeyboardInputTypeInfo;
}

bool FCommonInputActionDataBase::IsKeyBoundToInputActionData(const FKey& Key) const
{
	if (Key == KeyboardInputTypeInfo.GetKey() || Key == TouchInputTypeInfo.GetKey())
	{
		return true;
	}

	for (const FName& GamepadName : UCommonInputBaseControllerData::GetRegisteredGamepads())
	{
		const FCommonInputTypeInfo& TypeInfo = GetInputTypeInfo(ECommonInputType::Gamepad, GamepadName);
		if (Key == TypeInfo.GetKey())
		{
			return true;
		}
	}

	return false;
}

bool FCommonInputActionDataBase::IsKeyBoundToInputActionData(const FKey& Key, const UCommonInputSubsystem* CommonInputSubsystem) const
{
	const FCommonInputTypeInfo&	InputTypeInfo = GetCurrentInputTypeInfo(CommonInputSubsystem);

	if (Key == InputTypeInfo.GetKey())
	{
		return true;
	}

	return false;
}

FSlateBrush FCommonInputActionDataBase::GetCurrentInputActionIcon(const UCommonInputSubsystem* CommonInputSubsystem) const
{
	const FCommonInputTypeInfo& CurrentInputTypeInfo = GetCurrentInputTypeInfo(CommonInputSubsystem);

	//if (CurrentInputTypeInfo.OverrideBrush.DrawAs != ESlateBrushDrawType::NoDrawType)
	//{
	//	return CurrentInputTypeInfo.OverrideBrush;
	//}

	FSlateBrush SlateBrush;
	if (UCommonInputPlatformSettings::Get()->TryGetInputBrush(SlateBrush, CurrentInputTypeInfo.GetKey(), CommonInputSubsystem->GetCurrentInputType(), CommonInputSubsystem->GetCurrentGamepadName()))
	{
		return SlateBrush;
	}

	return *FStyleDefaults::GetNoBrush();
}

void FCommonInputActionDataBase::OnPostDataImport(const UDataTable* InDataTable, const FName InRowName, TArray<FString>& OutCollectedImportProblems)
{
#if WITH_EDITOR
#endif // WITH_EDITOR
}

bool FCommonInputActionDataBase::HasHoldBindings() const
{
	if (DefaultGamepadInputTypeInfo.bActionRequiresHold)
	{
		return true;
	}

	for (const TPair<FName, FCommonInputTypeInfo>& GamepadInfo : GamepadInputOverrides)
	{
		if (GamepadInfo.Value.bActionRequiresHold)
		{
			return true;
		}
	}

	if (KeyboardInputTypeInfo.bActionRequiresHold)
	{
		return true;
	}

	if (TouchInputTypeInfo.bActionRequiresHold)
	{
		return true;
	}

	return false;
}

const FCommonInputTypeInfo& FCommonInputActionDataBase::GetDefaultGamepadInputTypeInfo() const
{
	return DefaultGamepadInputTypeInfo;
}

bool FCommonInputActionDataBase::HasGamepadInputOverride(const FName& GamepadName) const
{
	return GamepadInputOverrides.Contains(GamepadName);
}

void FCommonInputActionDataBase::AddGamepadInputOverride(const FName& GamepadName, const FCommonInputTypeInfo& InputInfo)
{
	GamepadInputOverrides.Add(GamepadName, InputInfo);
}

void CommonUI::SetupStyles()
{
	EmptyScrollBoxStyle.BottomShadowBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	EmptyScrollBoxStyle.TopShadowBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	EmptyScrollBoxStyle.LeftShadowBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	EmptyScrollBoxStyle.RightShadowBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
}

const FCommonInputActionDataBase* CommonUI::GetInputActionData(const FDataTableRowHandle& InputActionRowHandle)
{
	return InputActionRowHandle.GetRow<FCommonInputActionDataBase>(TEXT("CommonUIUtils::GetInputActionData couldn't find the row passed in, check data table if its missing."));
}

FSlateBrush CommonUI::GetIconForInputActions(const UCommonInputSubsystem* CommonInputSubsystem, const TArray<FDataTableRowHandle>& InputActions)
{
	TArray<FKey> Keys;
	for (const FDataTableRowHandle& InputAction : InputActions)
	{
		if (const FCommonInputActionDataBase* InputActionData = GetInputActionData(InputAction))
		{
			const FCommonInputTypeInfo& TypeInfo = InputActionData->GetCurrentInputTypeInfo(CommonInputSubsystem);
			Keys.Add(TypeInfo.GetKey());
		}
		else
		{
			return *FStyleDefaults::GetNoBrush();
		}
	}

	FSlateBrush SlateBrush;
	if (UCommonInputPlatformSettings::Get()->TryGetInputBrush(SlateBrush, Keys, CommonInputSubsystem->GetCurrentInputType(), CommonInputSubsystem->GetCurrentGamepadName()))
	{
		return SlateBrush;
	}

	return *FStyleDefaults::GetNoBrush();
}
