// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IAudioExtensionPlugin.h"
#include "AudioMixerDevice.h"
#include "SoundFieldRendering.h"
#include "SoundFields.generated.h"

UCLASS(config = Engine, editinlinenew, BlueprintType)
class SOUNDFIELDS_API UAmbisonicsEncodingSettings : public USoundfieldEncodingSettingsBase
{
	GENERATED_BODY()

public:

protected:
	UPROPERTY(EditAnywhere, Category = EncodingSettings, meta=(ClampMin="1", ClampMax="5", UIMin="1", UIMax="5"))
	int32 AmbisonicsOrder;

	virtual TUniquePtr<ISoundfieldEncodingSettingsProxy> GetProxy() const override;

	friend class FAmbisonicsSoundfieldFormat;
};

// This is the default format for Ambisonics in the Unreal Audio Engine.
class FAmbisonicsSoundfieldFormat : public ISoundfieldFactory
{
public:
	FAmbisonicsSoundfieldFormat();

	// dtor
	virtual ~FAmbisonicsSoundfieldFormat();

	// Begin ISoundfieldFactory
	virtual FName GetSoundfieldFormatName() override;
	virtual TUniquePtr<ISoundfieldEncoderStream> CreateEncoderStream(const FAudioPluginInitializationParams& InitInfo, const ISoundfieldEncodingSettingsProxy& InitialSettings) override;
	virtual TUniquePtr<ISoundfieldDecoderStream> CreateDecoderStream(const FAudioPluginInitializationParams& InitInfo, const ISoundfieldEncodingSettingsProxy& InitialSettings) override;
	virtual TUniquePtr<ISoundfieldTranscodeStream> CreateTranscoderStream(const FName SourceFormat, const ISoundfieldEncodingSettingsProxy& InitialSourceSettings, const FName DestinationFormat, const ISoundfieldEncodingSettingsProxy& InitialDestinationSettings, const FAudioPluginInitializationParams& InitInfo) override;
	virtual TUniquePtr<ISoundfieldMixerStream> CreateMixerStream(const ISoundfieldEncodingSettingsProxy& InitialSettings) override;
	virtual TUniquePtr<ISoundfieldAudioPacket> CreateEmptyPacket() override;
	virtual bool IsTranscodeRequiredBetweenSettings(const ISoundfieldEncodingSettingsProxy& SourceSettings, const ISoundfieldEncodingSettingsProxy& DestinationSettings) override;
	virtual bool CanTranscodeFromSoundfieldFormat(FName InputFormat, const ISoundfieldEncodingSettingsProxy& InputEncodingSettings) override;
	virtual UClass* GetCustomEncodingSettingsClass() const override;
	virtual USoundfieldEncodingSettingsBase* GetDefaultEncodingSettings() override;
	virtual bool CanTranscodeToSoundfieldFormat(FName DestinationFormat, const ISoundfieldEncodingSettingsProxy& DestinationEncodingSettings) override;
	//~End ISoundfieldFactory
};
