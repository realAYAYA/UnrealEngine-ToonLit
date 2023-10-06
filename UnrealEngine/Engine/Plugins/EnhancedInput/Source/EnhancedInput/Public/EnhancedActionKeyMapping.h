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

PRAGMA_DISABLE_DEPRECATION_WARNINGS

/**
 * A struct that represents player facing mapping options for an action key mapping.
 * Use this to set a unique FName for the mapping option to save it, as well as some FText options
 * for use in UI.
 */
USTRUCT(BlueprintType)
struct UE_DEPRECATED(5.3, "FPlayerMappableKeyOptions has been deprecated. Please use UPlayerMappableKeySettings instead.") FPlayerMappableKeyOptions
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
struct ENHANCEDINPUT_API FEnhancedActionKeyMapping
{
	friend class UInputMappingContext;
	friend class FEnhancedActionMappingCustomization;
	
	GENERATED_BODY()

	FEnhancedActionKeyMapping(const UInputAction* InAction = nullptr, const FKey InKey = EKeys::Invalid);
	
	/**
	* Returns the Player Mappable Key Settings owned by the Action Key Mapping or by the referenced Input Action, or nothing based of the Setting Behavior.
	*/
	UPlayerMappableKeySettings* GetPlayerMappableKeySettings() const;

	/**
	 * Returns the name of the mapping based on setting behavior used. If no name is found in the Mappable Key Settings it will return the name set in Player Mappable Options if bIsPlayerMappable is true.
	 */
	FName GetMappingName() const;

	/** The localized display name of this key mapping */
	const FText& GetDisplayName() const;

	/** The localized display category of this key mapping */
	const FText& GetDisplayCategory() const;

	/**
	* Returns true if this Action Key Mapping either holds a Player Mappable Key Settings or is set bIsPlayerMappable.
	*/
	bool IsPlayerMappable() const;

#if WITH_EDITOR
	EDataValidationResult IsDataValid(FDataValidationContext& Context) const;
#endif
	
	/** Identical comparison, including Triggers and Modifiers current inner values. */
	bool operator==(const FEnhancedActionKeyMapping& Other) const;

#if WITH_EDITORONLY_DATA

	/** Options for making this a player mappable keymapping */
	UE_DEPRECATED(5.3, "PlayerMappableOptions has been deprecated, please use PlayerMappableKeySettings instead.")
	UPROPERTY(EditAnywhere, Category = "Input|PlayerMappable", meta = (editCondition = "bIsPlayerMappable", DisplayAfter = "bIsPlayerMappable", DeprecatedProperty, DeprecationMessage="PlayerMappableOptions has been deprecated, please use the PlayerMappableKeySettings instead"))
	FPlayerMappableKeyOptions PlayerMappableOptions;

#endif	// WITH_EDITORONLY_DATA
	
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
	* 
	* Note: Modifiers defined in individual key mappings will be applied before those defined in the Input Action asset.
	* Modifiers will not override any that are defined on the Input Action asset, they will be combined together during evaluation.
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

#if WITH_EDITORONLY_DATA

	/** If true then this ActionKeyMapping will be exposed as a player mappable key */
	UE_DEPRECATED(5.3, "bIsPlayerMappable has been deprecated, please use SettingBehavior instead.")
	UPROPERTY(EditAnywhere, Category = "Input|PlayerMappable", meta=(DeprecatedProperty, DeprecationMessage="bIsPlayerMappable has been deprecated, please use the SettingBehavior instead"))
	uint8 bIsPlayerMappable : 1;

#endif	// WITH_EDITORONLY_DATA

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

public:

	template<typename T = UPlayerMappableKeySettings> 
	T* GetPlayerMappableKeySettings() const
	{
		return Cast<T>(GetPlayerMappableKeySettings());
	}

	/**
	 * If the template bIgnoreModifierAndTriggerValues is true, compare to Other ignoring
	 * different trigger states, like pending activation time, but only accept
	 * both as equal if the Trigger types are the same and in the same order.
	 */
	template<bool bIgnoreModifierAndTriggerValues = true>
	bool Equals(const FEnhancedActionKeyMapping& Other) const
	{
		if constexpr (bIgnoreModifierAndTriggerValues)
		{
			return (Action == Other.Action &&
					Key == Other.Key &&
					CompareByObjectTypes(Modifiers, Other.Modifiers) &&
					CompareByObjectTypes(Triggers, Other.Triggers));
		}
		else
		{
			return *this == Other;
		}
	}
	
	/**
	 * Compares if two TArray of UObjects contain the same number and types of
	 * objects, but doesn't compare their values.
	 *
	 * This is needed because TArray::operator== returns false when the objects'
	 * inner values differ. And for keeping old Trigger states, we need their
	 * comparison to ignore their current values.
	 */
	template<typename T>
	static bool CompareByObjectTypes(const TArray<TObjectPtr<T>>& A, const TArray<TObjectPtr<T>>& B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}

		for (int32 Idx = 0; Idx < A.Num(); ++Idx)
		{
			const T* ObjectA = A[Idx];
			const T* ObjectB = B[Idx];

			if ((ObjectA == nullptr) != (ObjectB == nullptr))
			{
				// One is nullptr, the other isn't
				return false;
			}
			if (!ObjectA)
			{
				// Both are nullptr. Consider that as same type.
				continue;
			}
			if (ObjectA->GetClass() != ObjectB->GetClass())
			{
				return false;
			}
		}

		return true;
	}

};

PRAGMA_ENABLE_DEPRECATION_WARNINGS
