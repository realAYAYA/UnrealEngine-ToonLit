// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"

#include "EnhancedActionKeyMapping.generated.h"

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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|PlayerMappable")
	TObjectPtr<UObject> Metadata = nullptr;
	
	/** A unique name for this player binding to be saved with. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|PlayerMappable")
	FName Name;
	
	/** The localized display name of this key mapping. Use this when displaying the mappings to a user. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|PlayerMappable")
	FText DisplayName;

	/** The category that this player binding is in */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|PlayerMappable")
	FText DisplayCategory = FText::GetEmpty();
};

class UInputTrigger;
class UInputModifier;

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

	/** Options for making this a player mappable keymapping */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|PlayerMappable", meta = (editCondition = "bIsPlayerMappable", DisplayAfter = "bIsPlayerMappable"))
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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<const UInputAction> Action = nullptr;

	/** Key that affect the action. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	FKey Key;

	/** If true than this ActionKeyMapping will be exposed as a player mappable key */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input|PlayerMappable")
	uint8 bIsPlayerMappable : 1;

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
		, bIsPlayerMappable(false)
		, bShouldBeIgnored(false)
	{}

};
