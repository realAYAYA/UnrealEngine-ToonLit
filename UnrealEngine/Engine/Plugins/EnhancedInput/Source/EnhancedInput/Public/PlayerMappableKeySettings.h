// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameplayTagContainer.h"

#include "PlayerMappableKeySettings.generated.h"

struct FEnhancedActionKeyMapping;

/**
* Hold setting information of an Action Input or a Action Key Mapping for setting screen and save purposes.
* Experimental: Do not count on long term support for this structure.
*/
UCLASS(DefaultToInstanced, EditInlineNew, DisplayName="Player Mappable Key Settings (Experimental)")
class ENHANCEDINPUT_API UPlayerMappableKeySettings : public UObject
{
	GENERATED_BODY()

public:

	virtual FName MakeMappingName(const FEnhancedActionKeyMapping* OwningActionKeyMapping) const { return GetMappingName(); }
	virtual FName GetMappingName() const { return Name; }

#if WITH_EDITOR
	EDataValidationResult IsDataValid(class FDataValidationContext& Context) const;

	/**
	 * Get the known mapping names that are current in use. This is a helper function if you want to use a "GetOptions" metadata on a UPROPERTY.
	 * For example, the following will display a little drop down menu to select from all current mapping names:
	 *
	 *  UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(GetOptions="EnhancedInput.PlayerMappableKeySettings.GetKnownMappingNames"))
	 *  FName MappingName;
	 */
	UFUNCTION()
	static const TArray<FName>& GetKnownMappingNames();	
#endif // WITH_EDITOR

	/** Metadata that can used to store any other related items to this key mapping such as icons, ability assets, etc. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	TObjectPtr<UObject> Metadata = nullptr;

	/** A unique name for this player mapping to be saved with. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	FName Name;

	/** The localized display name of this key mapping. Use this when displaying the mappings to a user. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	FText DisplayName;

	/** The category that this player mapping is in */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	FText DisplayCategory = FText::GetEmpty();

	/** 
	* If this key mapping should only be added when a specific key profile is equipped, then set those here.
	* 
	* If this is empty, then the key mapping will not be filtered out based on the current profile.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings")
	FGameplayTagContainer SupportedKeyProfiles;
};
