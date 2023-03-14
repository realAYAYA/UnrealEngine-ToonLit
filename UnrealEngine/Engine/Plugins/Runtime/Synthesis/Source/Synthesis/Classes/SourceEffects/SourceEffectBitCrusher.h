// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Set.h"
#include "DSP/BitCrusher.h"
#include "Sound/SoundEffectSource.h"
#include "Sound/SoundModulationDestination.h"

#include "SourceEffectBitCrusher.generated.h"


// Forward Declarations
class USoundModulatorBase;

USTRUCT(BlueprintType)
struct SYNTHESIS_API FSourceEffectBitCrusherBaseSettings
{
	GENERATED_USTRUCT_BODY()

	// The reduced frequency to use for the audio stream. 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "0.1", ClampMax = "96000.0", UIMin = "500.0", UIMax = "16000.0"))
	float SampleRate = 8000.0f;

	// The reduced bit depth to use for the audio stream
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "1.0", ClampMax = "24.0", UIMin = "1.0", UIMax = "16.0"))
	float BitDepth = 8.0f;
};

USTRUCT(BlueprintType)
struct SYNTHESIS_API FSourceEffectBitCrusherSettings
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(meta = (PropertyDeprecated))
	float CrushedSampleRate = 8000.0f;

	// The reduced frequency to use for the audio stream. 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (DisplayName = "Sample Rate", AudioParam = "SampleRate", AudioParamClass = "SoundModulationParameterFrequency", ClampMin = "0.1", ClampMax = "96000.0", UIMin = "500.0", UIMax = "16000.0"))
	FSoundModulationDestinationSettings SampleRateModulation;

	UPROPERTY(meta = (PropertyDeprecated))
	float CrushedBits = 8.0f;

	// The reduced bit depth to use for the audio stream
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (DisplayName = "Bit Depth", AudioParam = "BitDepth", AudioParamClass = "SoundModulationParameterScaled", ClampMin = "1.0", ClampMax = "24.0", UIMin = "1.0", UIMax = "16.0"))
	FSoundModulationDestinationSettings BitModulation;

	FSourceEffectBitCrusherSettings()
	{
		SampleRateModulation.Value = CrushedSampleRate;
		BitModulation.Value = CrushedBits;
	}
};

class SYNTHESIS_API FSourceEffectBitCrusher : public FSoundEffectSource
{
public:
	// Called on an audio effect at initialization on main thread before audio processing begins.
	virtual void Init(const FSoundEffectSourceInitData& InitData) override;
	
	// Called when an audio effect preset is changed
	virtual void OnPresetChanged() override;

	// Process the input block of audio. Called on audio thread.
	virtual void ProcessAudio(const FSoundEffectSourceInputData& InData, float* OutAudioBufferData) override;

	void SetBitModulator(const USoundModulatorBase* Modulator);
	void SetBitModulators(const TSet<USoundModulatorBase*>& InModulators);

	void SetSampleRateModulator(const USoundModulatorBase* Modulator);
	void SetSampleRateModulators(const TSet<USoundModulatorBase*>& InModulators);

protected:
	Audio::FBitCrusher BitCrusher;

	FSourceEffectBitCrusherSettings SettingsCopy;

	Audio::FModulationDestination SampleRateMod;
	Audio::FModulationDestination BitMod;
};

UCLASS(ClassGroup = AudioSourceEffect, meta = (BlueprintSpawnableComponent))
class SYNTHESIS_API USourceEffectBitCrusherPreset : public USoundEffectSourcePreset
{
	GENERATED_BODY()

public:
	EFFECT_PRESET_METHODS(SourceEffectBitCrusher)

	virtual void OnInit() override;

	virtual void Serialize(FArchive& Ar) override;

	virtual FColor GetPresetColor() const override { return FColor(196.0f, 185.0f, 121.0f); }

	UFUNCTION(BlueprintCallable, Category = "Audio|Effects|BitCrusher")
	void SetBits(float Bits);

	UFUNCTION(BlueprintCallable, Category = "Audio|Effects|BitCrusher")
	void SetBitModulator(const USoundModulatorBase* Modulator);

	UFUNCTION(BlueprintCallable, Category = "Audio|Effects|BitCrusher")
	void SetBitModulators(const TSet<USoundModulatorBase*>& InModulators);

	UFUNCTION(BlueprintCallable, Category = "Audio|Effects|BitCrusher")
	void SetSampleRate(float SampleRate);

	UFUNCTION(BlueprintCallable, Category = "Audio|Effects|BitCrusher")
	void SetSampleRateModulator(const USoundModulatorBase* Modulator);

	UFUNCTION(BlueprintCallable, Category = "Audio|Effects|BitCrusher")
	void SetSampleRateModulators(const TSet<USoundModulatorBase*>& InModulators);

	// Sets just base (i.e. carrier) setting values without modifying modulation source references
	UFUNCTION(BlueprintCallable, Category = "Audio|Effects|BitCrusher")
	void SetSettings(const FSourceEffectBitCrusherBaseSettings& Settings);

	UFUNCTION(BlueprintCallable, Category = "Audio|Effects|BitCrusher")
	void SetModulationSettings(const FSourceEffectBitCrusherSettings& ModulationSettings);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SourceEffect|Preset")
	FSourceEffectBitCrusherSettings Settings;
};
