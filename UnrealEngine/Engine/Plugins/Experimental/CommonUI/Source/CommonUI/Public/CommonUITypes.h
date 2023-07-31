// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataTable.h"
#include "Blueprint/UserWidget.h"
#include "Misc/EnumRange.h"
#include "Styling/SlateTypes.h"
#include "CommonInputBaseTypes.h"
#include "CommonUITypes.generated.h"

class UCommonInputSubsystem;

UENUM(BlueprintType)
enum class EInputActionState : uint8
{
	/** Enabled, will call all callbacks */
	Enabled,
	/** Disabled, will call all the disabled callback if specified otherwise do nothing */
	Disabled,
	
	/** The common input reflector will not visualize this but still calls all callbacks. NOTE: Use this sparingly */
	Hidden,
	
	/** Hidden and disabled behaves as if it were never added with no callbacks being called */
	HiddenAndDisabled,	
};

USTRUCT(BlueprintType)
struct COMMONUI_API FCommonInputTypeInfo
{
	GENERATED_USTRUCT_BODY()

	FCommonInputTypeInfo();
private:
	/** Key this action is bound to	*/
	UPROPERTY(EditAnywhere, Category = "CommonInput")
	FKey Key;
public:

	/** Get the input type key bound to this input type, with a potential override */
	FKey GetKey() const;

	/** Get the input type key bound to this input type, with a potential override */
	void SetKey(FKey InKey)
	{
		Key = InKey;
	};

	/** EInputActionState::Enabled means that the state isn't overriden and the games dynamic control will work */
	UPROPERTY(EditAnywhere, Category = "CommonInput")
	EInputActionState OverrrideState;

	/** Enables hold time if true */
	UPROPERTY(EditAnywhere, Category = "CommonInput")
	bool bActionRequiresHold;

	/** The hold time in seconds */
	UPROPERTY(EditAnywhere, Category = "CommonInput", meta = (EditCondition = "bActionRequiresHold"))
	float HoldTime;
	
	/** Override the brush specified by the Key Display Data  */
	UPROPERTY(EditAnywhere, Category = "CommonInput")
	FSlateBrush OverrideBrush;

	bool operator==(const FCommonInputTypeInfo& Other) const
	{
		return Key == Other.Key &&
			OverrrideState == Other.OverrrideState &&
			bActionRequiresHold == Other.bActionRequiresHold &&
			HoldTime == Other.HoldTime &&
			OverrideBrush == Other.OverrideBrush;
	}

	bool operator!=(const FCommonInputTypeInfo& Other) const
	{
		return !(*this == Other);
	}
};

USTRUCT(BlueprintType)
struct COMMONUI_API FCommonInputActionDataBase : public FTableRowBase
{
	GENERATED_BODY()

	FCommonInputActionDataBase();
	
	/** User facing name (used when NOT a hold action) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CommonInput")
	FText DisplayName;

	/** User facing name used when it IS a hold action */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CommonInput")
	FText HoldDisplayName;

	/** Priority in nav-bar */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CommonInput")
	int32 NavBarPriority = 0;

protected:
	/**
	* Key to bind to for each input method
	*/
	UPROPERTY(EditAnywhere, Category = "CommonInput")
	FCommonInputTypeInfo KeyboardInputTypeInfo;

	/**
	* Default input state for gamepads
	*/
	UPROPERTY(EditAnywhere, Category = "CommonInput")
	FCommonInputTypeInfo DefaultGamepadInputTypeInfo;

	/**
	* Override the input state for each input method
	*/
	UPROPERTY(EditAnywhere, Category = "CommonInput", Meta = (GetOptions = "CommonInput.CommonInputBaseControllerData.GetRegisteredGamepads"))
	TMap<FName, FCommonInputTypeInfo> GamepadInputOverrides;

	/**
	* Override the displayed brush for each input method
	*/
	UPROPERTY(EditAnywhere, Category = "CommonInput")
	FCommonInputTypeInfo TouchInputTypeInfo;

	friend class UCommonInputActionDataProcessor;

public:
	bool CanDisplayInReflector(ECommonInputType InputType, const FName& GamepadName) const;

	virtual const FCommonInputTypeInfo& GetCurrentInputTypeInfo(const UCommonInputSubsystem* CommonInputSubsystem) const;

	virtual const FCommonInputTypeInfo& GetInputTypeInfo(ECommonInputType InputType, const FName& GamepadName) const;

	virtual bool IsKeyBoundToInputActionData(const FKey& Key) const;

	bool IsKeyBoundToInputActionData(const FKey& Key, const UCommonInputSubsystem* CommonInputSubsystem) const;

	FSlateBrush GetCurrentInputActionIcon(const UCommonInputSubsystem* CommonInputSubsystem) const;

	virtual void OnPostDataImport(const UDataTable* InDataTable, const FName InRowName, TArray<FString>& OutCollectedImportProblems) override;

	virtual bool HasHoldBindings() const;

	const FCommonInputTypeInfo& GetDefaultGamepadInputTypeInfo() const;

	bool HasGamepadInputOverride(const FName& GamepadName) const;

	void AddGamepadInputOverride(const FName& GamepadName, const FCommonInputTypeInfo& InputInfo);

	bool operator==(const FCommonInputActionDataBase& Other) const
	{
		return DisplayName.EqualTo(Other.DisplayName) &&
			HoldDisplayName.EqualTo(Other.HoldDisplayName) &&
			KeyboardInputTypeInfo == Other.KeyboardInputTypeInfo &&
			DefaultGamepadInputTypeInfo == Other.DefaultGamepadInputTypeInfo &&
			GamepadInputOverrides.OrderIndependentCompareEqual(Other.GamepadInputOverrides) &&
			TouchInputTypeInfo == Other.TouchInputTypeInfo;
	}

	bool operator!=(const FCommonInputActionDataBase& Other) const
	{
		check(this);
		return *this == Other;
	}
};

class COMMONUI_API CommonUI
{
public:
	static void SetupStyles();
	static FScrollBoxStyle EmptyScrollBoxStyle;
	static const FCommonInputActionDataBase* GetInputActionData(const FDataTableRowHandle& InputActionRowHandle);
	static FSlateBrush GetIconForInputActions(const UCommonInputSubsystem* CommonInputSubsystem, const TArray<FDataTableRowHandle>& InputActions);
};

DECLARE_DYNAMIC_DELEGATE_OneParam(FOnItemClicked, UUserWidget*, Widget);
DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnItemSelected, UUserWidget*, Widget, bool, Selected);
