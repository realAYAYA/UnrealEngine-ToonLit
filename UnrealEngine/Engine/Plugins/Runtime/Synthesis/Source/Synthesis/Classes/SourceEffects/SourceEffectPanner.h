// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundEffectSource.h"
#include "SourceEffectPanner.generated.h"

USTRUCT(BlueprintType)
struct SYNTHESIS_API FSourceEffectPannerSettings
{
	GENERATED_USTRUCT_BODY()

	// The spread of the source. 1.0 means left only in left channel, right only in right; 0.0 means both mixed, -1.0 means right and left channels are inverted.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "-1.0", ClampMax = "1.0", UIMin = "-1.0", UIMax = "1.0"))
	float Spread;

	// The pan of the source. -1.0 means left, 0.0 means center, 1.0 means right.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "-1.0", ClampMax = "1.0", UIMin = "-1.0", UIMax = "1.0"))
	float Pan;

	FSourceEffectPannerSettings()
		: Spread(1.0f)
		, Pan(0.0f)
	{}
};

class SYNTHESIS_API FSourceEffectPanner : public FSoundEffectSource
{
public:
	// Called on an audio effect at initialization on main thread before audio processing begins.
	virtual void Init(const FSoundEffectSourceInitData& InitData) override;

	// Called when an audio effect preset is changed
	virtual void OnPresetChanged() override;

	// Process the input block of audio. Called on audio render thread.
	virtual void ProcessAudio(const FSoundEffectSourceInputData& InData, float* OutAudioBufferData) override;

	FSourceEffectPanner()
	{
		for (int32 i = 0; i < 2; ++i)
		{
			SpreadGains[i] = 0.0f;
			PanGains[i] = 0.0f;
		}
		NumChannels = 0;
	}

protected:

	// The pan value of the source effect
	float SpreadGains[2];
	float PanGains[2];
	int32 NumChannels;
};

UCLASS(ClassGroup = AudioSourceEffect, meta = (BlueprintSpawnableComponent))
class SYNTHESIS_API USourceEffectPannerPreset : public USoundEffectSourcePreset
{
	GENERATED_BODY()

public:
	EFFECT_PRESET_METHODS(SourceEffectPanner)

	virtual FColor GetPresetColor() const override { return FColor(127.0f, 155.0f, 101.0f); }

	UFUNCTION(BlueprintCallable, Category = "Audio|Effects")
	void SetSettings(const FSourceEffectPannerSettings& InSettings);
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Effects")
	FSourceEffectPannerSettings Settings;
};
