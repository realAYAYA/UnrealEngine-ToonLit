// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IAudioExtensionPlugin.h"
#include "AudioEffect.h"
#include "DSP/Dsp.h"
#include "Sound/SoundEffectSubmix.h"
#include "OVR_Audio.h"
#include "OculusAmbisonicsSettings.h"

class FOculusAmbisonicsFactory : public ISoundfieldFactory
{
public:
	FOculusAmbisonicsFactory();
	virtual ~FOculusAmbisonicsFactory();

	// Begin ISoundfieldFactory
	virtual const USoundfieldEncodingSettingsBase* GetDefaultEncodingSettings() override;
	virtual FName GetSoundfieldFormatName() override;
	virtual TUniquePtr<ISoundfieldEncoderStream> CreateEncoderStream(const FAudioPluginInitializationParams& InitInfo, const ISoundfieldEncodingSettingsProxy& InitialSettings) override;
	virtual TUniquePtr<ISoundfieldDecoderStream> CreateDecoderStream(const FAudioPluginInitializationParams& InitInfo, const ISoundfieldEncodingSettingsProxy& InitialSettings) override;
	virtual TUniquePtr<ISoundfieldTranscodeStream> CreateTranscoderStream(const FName SourceFormat, const ISoundfieldEncodingSettingsProxy& InitialSourceSettings, const FName DestinationFormat, const ISoundfieldEncodingSettingsProxy& InitialDestinationSettings, const FAudioPluginInitializationParams& InitInfo) override;
	virtual TUniquePtr<ISoundfieldMixerStream> CreateMixerStream(const ISoundfieldEncodingSettingsProxy& InitialSettings) override;
	virtual TUniquePtr<ISoundfieldAudioPacket> CreateEmptyPacket() override;
	virtual bool IsTranscodeRequiredBetweenSettings(const ISoundfieldEncodingSettingsProxy& SourceSettings, const ISoundfieldEncodingSettingsProxy& DestinationSettings) override;
	virtual bool CanTranscodeFromSoundfieldFormat(FName SourceFormat, const ISoundfieldEncodingSettingsProxy& SourceEncodingSettings) override;
	virtual bool CanTranscodeToSoundfieldFormat(FName DestinationFormat, const ISoundfieldEncodingSettingsProxy& DestinationEncodingSettings) override;
	virtual UClass* GetCustomEncodingSettingsClass() const override;
	//~ ISoundfieldFactory

private:
	static TMap<FAudioDevice*, ovrAudioContext> ContextMap;
};