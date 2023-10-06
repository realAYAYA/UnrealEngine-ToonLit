// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"

#include "AudioGameplayRequirements.generated.h"

/**
 * UAudioRequirementPreset - A reusable GameplayTagQuery for audio
 */
UCLASS(BlueprintType)
class AUDIOGAMEPLAY_API UAudioRequirementPreset : public UDataAsset
{
	GENERATED_UCLASS_BODY()

public:

	~UAudioRequirementPreset() = default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay Tag Query")
	FGameplayTagQuery Query;

	/** Returns true if the query is not empty and matches the provided tags */
	bool Matches(const FGameplayTagContainer& Tags) const;
};

/**
 * AudioGameplayRequirements - A set of requirements for audio gameplay features.
 */
USTRUCT(BlueprintType)
struct AUDIOGAMEPLAY_API FAudioGameplayRequirements
{
	GENERATED_USTRUCT_BODY()

	/** An optional requirement preset tested against a collection of tags to determine a match.  Ignored if null. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Requirements")
	TObjectPtr<UAudioRequirementPreset> Preset = nullptr;

	/** An optional custom query tested against a collection of tags to determine a match.  Ignored if empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Requirements")
	FGameplayTagQuery Custom;

	/** Returns true if the preset and the custom query match the provided tags; ignores preset or custom query if they are empty. */
	bool Matches(const FGameplayTagContainer& Tags) const;
};