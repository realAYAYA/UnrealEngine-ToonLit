// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Sound/SoundEffectSource.h"
#include "DSP/Delay.h"
#include "SourceEffectSimpleDelay.generated.h"

USTRUCT(BlueprintType)
struct SYNTHESIS_API FSourceEffectSimpleDelaySettings
{
	GENERATED_USTRUCT_BODY()

	// Speed of sound in meters per second when using distance-based delay
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "0.01", UIMin = "0.01", UIMax = "10000.0", EditCondition = "bDelayBasedOnDistance"))
	float SpeedOfSound;

	// Delay amount in seconds
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "0.0", ClampMax = "2.0", UIMin = "0.0", UIMax = "2.0"))
	float DelayAmount;

	// Gain stage on dry (non-delayed signal)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float DryAmount;

	// Gain stage on wet (delayed) signal
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float WetAmount;

	// Amount to feedback into the delay line (because why not)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Feedback;

	// Whether or not to delay the audio based on the distance to the listener or use manual delay
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset")
	uint32 bDelayBasedOnDistance : 1;

	// Whether or not to allow the attenuation distance override value vs the distance to listener to be used for distance-based delay.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset")
	uint32 bUseDistanceOverride : 1;
	
	FSourceEffectSimpleDelaySettings()
		: SpeedOfSound(343.0f)
		, DelayAmount(0.0f)
		, DryAmount(0.0f)
		, WetAmount(1.0f)
		, Feedback(0.0f)
		, bDelayBasedOnDistance(true)
		, bUseDistanceOverride(true)
	{}
};

class SYNTHESIS_API FSourceEffectSimpleDelay : public FSoundEffectSource
{
public:
	// Called on an audio effect at initialization on main thread before audio processing begins.
	virtual void Init(const FSoundEffectSourceInitData& InitData) override;
	
	// Called when an audio effect preset is changed
	virtual void OnPresetChanged() override;

	// Process the input block of audio. Called on audio thread.
	virtual void ProcessAudio(const FSoundEffectSourceInputData& InData, float* OutAudioBufferData) override;

protected:
	// 2 Delay lines, one for each channel
	TArray<Audio::FDelay> Delays;
	TArray<float> FeedbackSamples;
	FSourceEffectSimpleDelaySettings SettingsCopy;
	bool bIsInit = true;
};

UCLASS(ClassGroup = AudioSourceEffect, meta = (BlueprintSpawnableComponent))
class SYNTHESIS_API USourceEffectSimpleDelayPreset : public USoundEffectSourcePreset
{
	GENERATED_BODY()

public:
	EFFECT_PRESET_METHODS(SourceEffectSimpleDelay)

	virtual FColor GetPresetColor() const override { return FColor(100.0f, 165.0f, 85.0f); }

	UFUNCTION(BlueprintCallable, Category = "Audio|Effects")
	void SetSettings(const FSourceEffectSimpleDelaySettings& InSettings);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Effects")
	FSourceEffectSimpleDelaySettings Settings;
};
