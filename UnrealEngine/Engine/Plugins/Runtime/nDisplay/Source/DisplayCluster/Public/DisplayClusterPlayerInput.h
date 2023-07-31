// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/PlayerInput.h"
#include "DisplayClusterPlayerInput.generated.h"


/**
 * Object within PlayerController that processes player input.
 */
UCLASS(Within=PlayerController, config=Input, transient)
class DISPLAYCLUSTER_API UDisplayClusterPlayerInput : public UPlayerInput
{
	GENERATED_BODY()

public:
	UDisplayClusterPlayerInput();

public:
	/** Process the frame's input events given the current input component stack. */
	virtual void ProcessInputStack(const TArray<UInputComponent*>& InputComponentStack, const float DeltaTime, const bool bGamePaused) override;

private:
	bool SerializeKeyStateMap(TMap<FString, FString>& OutKeyStateMap);
	bool DeserializeKeyStateMap(const TMap<FString, FString>& InKeyStateMap);
};
