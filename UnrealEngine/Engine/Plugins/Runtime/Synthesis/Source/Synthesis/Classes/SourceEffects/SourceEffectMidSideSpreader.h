// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundEffectSource.h"
#include "UObject/ObjectMacros.h"
#include "DSP/Dsp.h"
#include "SourceEffectMidSideSpreader.generated.h"

// Stereo channel mode
UENUM(BlueprintType)
enum class EStereoChannelMode : uint8
{
		MidSide,
		LeftRight,
		count UMETA(Hidden)
};

// ========================================================================
// FSourceEffectMidSideSpreaderSettings
// This is the source effect's setting struct. 
// ========================================================================

USTRUCT(BlueprintType)
struct SYNTHESIS_API FSourceEffectMidSideSpreaderSettings
{
	GENERATED_USTRUCT_BODY()

	// Amount of Mid/Side Spread. 0.0 is no spread, 1.0 is full wide. 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float SpreadAmount;

	// Indicate the channel mode of the input signal
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset")
	EStereoChannelMode InputMode;

	// Indicate the channel mode of the output signal
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset")
	EStereoChannelMode OutputMode;

	// Indicate whether an equal power relationship between the mid and side channels should be maintained
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SourceEffect|Preset")
	bool bEqualPower;

	FSourceEffectMidSideSpreaderSettings()
		: SpreadAmount(0.5f)
		, InputMode(EStereoChannelMode::LeftRight)
		, OutputMode(EStereoChannelMode::LeftRight)
		, bEqualPower(false)
	{}
};

// ========================================================================
// FSourceEffectMidSideSpreader
// This is the instance of the source effect. Performs DSP calculations.
// ========================================================================

class SYNTHESIS_API FSourceEffectMidSideSpreader : public FSoundEffectSource
{
public:
	FSourceEffectMidSideSpreader();

	// Called on an audio effect at initialization on main thread before audio processing begins.
	virtual void Init(const FSoundEffectSourceInitData& InInitData) override;
	
	// Called when an audio effect preset is changed
	virtual void OnPresetChanged() override;

	// Process the input block of audio. Called on audio thread.
	virtual void ProcessAudio(const FSoundEffectSourceInputData& InData, float* OutAudioBufferData) override;

protected:

	float MidScale;
	float SideScale;

	int32 NumChannels;

	FSourceEffectMidSideSpreaderSettings SpreaderSettings;

};

// ========================================================================
// USourceEffectMidSideSpreaderPreset
// This code exposes your preset settings and effect class to the editor.
// And allows for a handle to setting/updating effect settings dynamically.
// ========================================================================

UCLASS(ClassGroup = AudioSourceEffect, meta = (BlueprintSpawnableComponent))
class SYNTHESIS_API USourceEffectMidSideSpreaderPreset : public USoundEffectSourcePreset
{
	GENERATED_BODY()

public:
	// Macro which declares and implements useful functions.
	EFFECT_PRESET_METHODS(SourceEffectMidSideSpreader)

	// Allows you to customize the color of the preset in the editor.
	virtual FColor GetPresetColor() const override { return FColor(126.0f, 180.0f, 255.0f); }

	// Change settings of your effect from blueprints. Will broadcast changes to active instances.
	UFUNCTION(BlueprintCallable, Category = "Audio|Effects")
	void SetSettings(const FSourceEffectMidSideSpreaderSettings& InSettings);
	
	// The copy of the settings struct. Can't be written to in BP, but can be read.
	// Note that the value read in BP is the serialized settings, will not reflect dynamic changes made in BP.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Effects")
	FSourceEffectMidSideSpreaderSettings Settings;
};
