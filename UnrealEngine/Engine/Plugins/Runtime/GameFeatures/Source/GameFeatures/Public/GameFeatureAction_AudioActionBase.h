// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioDeviceHandle.h"
#include "GameFeatureAction.h"
#include "GameFeaturesSubsystem.h"

#include "GameFeatureAction_AudioActionBase.generated.h"

/**
 * Base class for GameFeatureActions that affect the audio engine
 */
UCLASS(Abstract)
class GAMEFEATURES_API UGameFeatureAction_AudioActionBase : public UGameFeatureAction
{
	GENERATED_BODY()

public:
	//~ Begin of UGameFeatureAction interface
	virtual void OnGameFeatureActivating(FGameFeatureActivatingContext& Context) override;
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;
	//~ End of UGameFeatureAction interface

protected:
	/** Handle to delegate callbacks */
	FDelegateHandle DeviceCreatedHandle;
	FDelegateHandle DeviceDestroyedHandle;

	void OnDeviceCreated(Audio::FDeviceId InDeviceId);
	void OnDeviceDestroyed(Audio::FDeviceId InDeviceId);

	virtual void AddToDevice(const FAudioDeviceHandle& AudioDeviceHandle) {}
	virtual void RemoveFromDevice(const FAudioDeviceHandle& AudioDeviceHandle) {}
};
