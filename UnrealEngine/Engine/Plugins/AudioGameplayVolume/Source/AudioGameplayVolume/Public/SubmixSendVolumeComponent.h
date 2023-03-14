// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioGameplayVolumeMutator.h"
#include "SubmixSendVolumeComponent.generated.h"

/**
 *  FProxyMutator_SubmixSend - An audio thread representation of Submix Sends
 */
class FProxyMutator_SubmixSend : public FProxyVolumeMutator
{
public:

	FProxyMutator_SubmixSend();
	virtual ~FProxyMutator_SubmixSend() = default;

	TArray<FAudioVolumeSubmixSendSettings> SubmixSendSettings;

	virtual void Apply(FAudioProxyActiveSoundParams& Params) const override;

protected:

	constexpr static const TCHAR MutatorSubmixSendName[] = TEXT("SubmixSend");
};

/**
 *  USubmixSendVolumeComponent - Audio Gameplay Volume component for submix sends
 */
UCLASS(Blueprintable, Config = Game, ClassGroup = ("AudioGameplayVolume"), meta = (BlueprintSpawnableComponent, DisplayName = "Submix Send"))
class AUDIOGAMEPLAYVOLUME_API USubmixSendVolumeComponent : public UAudioGameplayVolumeMutator
{
	GENERATED_UCLASS_BODY()

public:

	virtual ~USubmixSendVolumeComponent() = default;

	UFUNCTION(BlueprintCallable, Category = "AudioGameplay")
	void SetSubmixSendSettings(const TArray<FAudioVolumeSubmixSendSettings>& NewSubmixSendSettings);

	const TArray<FAudioVolumeSubmixSendSettings>& GetSubmixSendSettings() const { return SubmixSendSettings; }

private:

	//~ Begin UAudioGameplayVolumeMutator interface
	virtual TSharedPtr<FProxyVolumeMutator> FactoryMutator() const override;
	virtual void CopyAudioDataToMutator(TSharedPtr<FProxyVolumeMutator>& Mutator) const override;
	//~ End UAudioGameplayVolumeMutator interface

	/** Submix send settings to use for this component. Allows audio to dynamically send to submixes based on source and listener locations (relative to parent volume.) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Submixes, meta = (AllowPrivateAccess = "true"))
	TArray<FAudioVolumeSubmixSendSettings> SubmixSendSettings;
};