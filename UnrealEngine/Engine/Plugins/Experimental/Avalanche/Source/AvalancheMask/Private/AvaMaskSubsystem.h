// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/EngineSubsystem.h"

#include "AvaMaskSubsystem.generated.h"

UCLASS()
class UAvaMaskSubsystem
	: public UEngineSubsystem
{
	GENERATED_BODY()

public:
#if WITH_EDITORONLY_DATA
	/** Stores the last specified (user or auto) channel name to use for subsequently added modifiers in Target mode. */
	void SetLastSpecifiedChannelName(const FName InName);
#endif
	
private:
#if WITH_EDITORONLY_DATA
	/** Stores the last specified (user or auto) channel name to use for subsequently added modifiers in Target mode. */
	UPROPERTY(Transient, Getter = "Auto", Setter)
	FName LastSpecifiedChannelName;
#endif
};
