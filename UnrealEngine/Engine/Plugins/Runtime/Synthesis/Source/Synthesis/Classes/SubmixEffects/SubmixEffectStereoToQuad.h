// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Sound/SoundEffectSubmix.h"
#include "SubmixEffectStereoToQuad.generated.h"


USTRUCT(BlueprintType)
struct SYNTHESIS_API FSubmixEffectStereoToQuadSettings
{
	GENERATED_USTRUCT_BODY()

	// Whether or not to flip the left and right input channels when sending to the rear channel.
	// This can be useful to have a stereo field when hearing audio to the left and right in surround output configuration.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StereoToQuad")
	bool bFlipChannels = false;

	// The gain (in decibels) of the rear channels
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StereoToQuad")
	float RearChannelGain = 0.0f;
};

class SYNTHESIS_API FSubmixEffectStereoToQuad : public FSoundEffectSubmix
{
public:
	FSubmixEffectStereoToQuad();
	~FSubmixEffectStereoToQuad();

	//~ Begin FSoundEffectSubmix
	virtual void Init(const FSoundEffectSubmixInitData& InData) override;
	virtual void OnProcessAudio(const FSoundEffectSubmixInputData& InData, FSoundEffectSubmixOutputData& OutData) override;
	//~ End FSoundEffectSubmix

	//~ Begin FSoundEffectBase
	virtual void OnPresetChanged() override;
	//~End FSoundEffectBase

private:

	FSubmixEffectStereoToQuadSettings CurrentSettings;
	float LinearGain = 1.0f;
	Audio::FAlignedFloatBuffer ScratchStereoBuffer;
};

// Submix effect which sends stereo audio to quad (left surround and right surround) if the channel count is greater than 2.
UCLASS()
class SYNTHESIS_API USubmixEffectStereoToQuadPreset : public USoundEffectSubmixPreset
{
	GENERATED_BODY()

public:
	EFFECT_PRESET_METHODS(SubmixEffectStereoToQuad)

	// Set all tap delay settings. This will replace any dynamically added or modified taps.
	UFUNCTION(BlueprintCallable, Category = "Audio|Effects|Delay")
	void SetSettings(const FSubmixEffectStereoToQuadSettings& InSettings);


public:
	virtual void OnInit() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SubmixEffectPreset, Meta = (ShowOnlyInnerProperties))
	FSubmixEffectStereoToQuadSettings Settings;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "AudioEffect.h"
#include "CoreMinimal.h"
#endif
