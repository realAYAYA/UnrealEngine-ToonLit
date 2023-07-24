// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/CheatManager.h"

#include "LyraBotCheats.generated.h"

class ULyraBotCreationComponent;
class UObject;
struct FFrame;

/** Cheats related to bots */
UCLASS(NotBlueprintable)
class ULyraBotCheats final : public UCheatManagerExtension
{
	GENERATED_BODY()

public:
	ULyraBotCheats();

	// Adds a bot player
	UFUNCTION(Exec, BlueprintAuthorityOnly)
	void AddPlayerBot();

	// Removes a random bot player
	UFUNCTION(Exec, BlueprintAuthorityOnly)
	void RemovePlayerBot();

private:
	ULyraBotCreationComponent* GetBotComponent() const;
};
