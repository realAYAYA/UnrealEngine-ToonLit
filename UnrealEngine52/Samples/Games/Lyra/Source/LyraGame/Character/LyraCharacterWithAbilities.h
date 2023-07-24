// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Character/LyraCharacter.h"

#include "LyraCharacterWithAbilities.generated.h"

class UAbilitySystemComponent;
class ULyraAbilitySystemComponent;
class UObject;

// ALyraCharacter typically gets the ability system component from the possessing player state
// This represents a character with a self-contained ability system component.
UCLASS(Blueprintable)
class LYRAGAME_API ALyraCharacterWithAbilities : public ALyraCharacter
{
	GENERATED_BODY()

public:
	ALyraCharacterWithAbilities(const FObjectInitializer& ObjectInitializer);

	virtual void PostInitializeComponents() override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

private:

	// The ability system component sub-object used by player characters.
	UPROPERTY(VisibleAnywhere, Category = "Lyra|PlayerState")
	TObjectPtr<ULyraAbilitySystemComponent> AbilitySystemComponent;
};
