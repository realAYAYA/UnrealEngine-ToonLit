// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InputAction.h"

#include "EnhancedActionKeyMapping.generated.h"

class UInputModifier;
class UInputTrigger;
class UPlayerMappableKeySettings;

enum class EDataValidationResult : uint8;

/**
* Defines which Player Mappable Key Setting to use for a Action Key Mapping.
*/
UENUM(BlueprintType)
enum class EPlayerMappableKeySettingBehaviors : uint8
{
	//Use the Settings specified in the Input Action.
	InheritSettingsFromAction,
	//Use the Settings specified in the Action Key Mapping overriding the ones specified in the Input action.
	OverrideSettings,
	//Don't use any Settings even if one is specified in the Input Action.
	IgnoreSettings
};

/**
 * A struct that represents player facing mapping options for an action key mapping.
 * Use this to set a unique FName for the mapping option to save it, as well as some FText options
 * for use in UI.
 */
USTRUCT(BlueprintType)
struct FPlayerMappableKeyOptions
{
	GENERATED_BODY()

public:
	
	FPlayerMappableKeyOptions(const UInputAction* InAction = nullptr)
	{
		if (InAction)
		{
			const FString& ActionName = InAction->GetName();
			Name = FName(*ActionName);
			DisplayName = FText::FromString(ActionName);
		}
		else
		{
			Name = NAME_None;
			DisplayName = FText::GetEmpty();
		}
	};

	/** Metadata that can used to store any other related items to this key mapping such as icons, ability assets, etc. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|PlayerMappable")
	TObjectPtr<UObject> Metadata = nullptr;
	
	/** A unique name for this player mapping to be saved with. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|PlayerMappable")
	FName Name;
	
	/** The localized display name of this key mapping. Use this when displaying the mappings to a user. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|PlayerMappable")
	FText DisplayName;

	/** The category that this player mapping is in */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|PlayerMappable")
	FText DisplayCategory = FText::GetEmpty();
};

/**
 * Defines a mapping between a key activation and the resulting enhanced action
 * An key could be a button press, joystick axis movement, etc.
 * An enhanced action could be MoveForward, Jump, Fire, etc.
 *
**/
USTRUCT(BlueprintType)
struct FEnhancedActionKeyMapping
{
	GENERATED_BODY()

	/**
	* Returns the Player Mappable Key Settings owned by the Action Key Mapping or by the referenced Input Action, or nothing based of the Setting Behavior.
	*/
	ENHANCEDINPUT_API UPlayerMappableKeySettings* GetPlayerMappableKeySettings() const;

	/**
	 * Returns the name of the mapping based on setting behavior used. If no name is found in the Mappable Key Settings it will return the name set in Player Mappable Options if bIsPlayerMappable is true.
	 */
	ENHANCEDINPUT_API FName GetMappingName() const;

	/**
	* Returns true if this Action Key Mapping either holds a Player Mappable Key Settings or is set bIsPlayerMappable.
	*/
	ENHANCEDINPUT_API bool IsPlayerMappable() const;

#if WITH_EDITOR
	EDataValidationResult IsDataValid(TArray<FText>& ValidationErrors);
#endif

	/** Options for making this a player mappable keymapping */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|PlayerMappable", meta = (editCondition = "bIsPlayerMappable", DisplayAfter = "bIsPlayerMappable"))
	FPlayerMappableKeyOptions PlayerMappableOptions;

	/**
	* Trigger qualifiers. If any trigger qualifiers exist the mapping will not trigger unless:
	* If there are any Explicit triggers in this list at least one of them must be met.
	* All Implicit triggers in this list must be met.
	*/
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "Input")
	TArray<TObjectPtr<UInputTrigger>> Triggers;

	/**
	* Modifiers applied to the raw key value.
	* These are applied sequentially in array order.
	*/
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "Input")
	TArray<TObjectPtr<UInputModifier>> Modifiers;
	
	/** Action to be affected by the key  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<const UInputAction> Action = nullptr;

	/** Key that affect the action. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	FKey Key;

	/**
	 * If true, then this Key Mapping should be ignored. This is set to true if the key is down
	 * during a rebuild of it's owning PlayerInput ControlMappings.
	 * 
	 * @see IEnhancedInputSubsystemInterface::RebuildControlMappings
	 */
	UPROPERTY(Transient)
	uint8 bShouldBeIgnored : 1;
	
	bool operator==(const FEnhancedActionKeyMapping& Other) const
	{
		return (Action == Other.Action &&
				Key == Other.Key &&
				Triggers == Other.Triggers &&
				Modifiers == Other.Modifiers);
	}

	FEnhancedActionKeyMapping(const UInputAction* InAction = nullptr, const FKey InKey = EKeys::Invalid)
		: PlayerMappableOptions(InAction)
		, Action(InAction)
		, Key(InKey)
		, bShouldBeIgnored(false)
		, bIsPlayerMappable(false)
	{}

	friend class FEnhancedActionMappingCustomization;

protected:

	/**
	* Defines which Player Mappable Key Setting to use for a Action Key Mapping.
	*/
	UPROPERTY(EditAnywhere, Category = "Input|Settings")
	EPlayerMappableKeySettingBehaviors SettingBehavior = EPlayerMappableKeySettingBehaviors::InheritSettingsFromAction;

	/**
	* Used to expose this mapping or to opt-out of settings completely.
	*/
	UPROPERTY(EditAnywhere, Instanced, Category = "Input|Settings", meta = (EditCondition = "SettingBehavior == EPlayerMappableKeySettingBehaviors::OverrideSettings", DisplayAfter = "SettingBehavior"))
	TObjectPtr<UPlayerMappableKeySettings> PlayerMappableKeySettings = nullptr;

	/** If true then this ActionKeyMapping will be exposed as a player mappable key */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|PlayerMappable")
	uint8 bIsPlayerMappable : 1;

public:

	template<typename T = UPlayerMappableKeySettings> 
	T* GetPlayerMappableKeySettings() const
	{
		return Cast<T>(GetPlayerMappableKeySettings());
	}

};
