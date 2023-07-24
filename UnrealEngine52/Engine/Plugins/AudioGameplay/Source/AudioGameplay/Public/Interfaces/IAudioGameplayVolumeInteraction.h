// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"
#include "IAudioGameplayVolumeInteraction.generated.h"

/** Interface for interacting with the audio gameplay volume system */
UINTERFACE(BlueprintType, MinimalAPI)
class UAudioGameplayVolumeInteraction : public UInterface
{
	GENERATED_BODY()
};

class IAudioGameplayVolumeInteraction
{
	GENERATED_BODY()

public:

	/**
	 * Called when a listener 'enters' the associated proxy
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "AudioGameplayVolume")
	AUDIOGAMEPLAY_API void OnListenerEnter();
	virtual void OnListenerEnter_Implementation() {}

	/**
	 * Called when a listener 'exits' the associated proxy
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "AudioGameplayVolume")
	AUDIOGAMEPLAY_API void OnListenerExit();
	virtual void OnListenerExit_Implementation() {}
};
