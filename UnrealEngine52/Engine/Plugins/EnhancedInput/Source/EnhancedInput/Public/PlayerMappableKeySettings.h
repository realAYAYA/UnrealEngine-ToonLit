// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

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
	virtual EDataValidationResult IsDataValid(TArray<FText>& ValidationErrors) override;
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
};
