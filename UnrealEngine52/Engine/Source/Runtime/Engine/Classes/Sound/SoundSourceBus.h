// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Sound/SoundWaveProcedural.h"
#include "Sound/SoundSourceBusSend.h"
#include "Sound/AudioBus.h"
#include "SoundSourceBus.generated.h"

class USoundSourceBus;
class USoundWaveProcedural;
class FAudioDevice;

// The number of channels to mix audio into the source bus
UENUM(BlueprintType)
enum class ESourceBusChannels : uint8
{
	Mono,
	Stereo,
};

// A source bus is a type of USoundBase and can be "played" like any sound.
UCLASS(hidecategories= (Compression, SoundWave, Streaming, Subtitles, Analysis, Format, Loading, Info, ImportSettings), ClassGroup = Sound, meta = (BlueprintSpawnableComponent))
class ENGINE_API USoundSourceBus : public USoundWave
{
	GENERATED_UCLASS_BODY()

public:

	/** How many channels to use for the source bus if the audio bus is not specified, otherwise it will use the audio bus object's channel count. */
	UPROPERTY(EditAnywhere, Category = BusProperties)
	ESourceBusChannels SourceBusChannels;

	/** The duration (in seconds) to use for the source bus. A duration of 0.0 indicates to play the source bus indefinitely. */
	UPROPERTY(EditAnywhere, Category = BusProperties, meta = (UIMin = 0.0, ClampMin = 0.0))
	float SourceBusDuration;

	/** Audio bus to use as audio for this source bus. This source bus will sonify the audio from the audio bus. */
	UPROPERTY(EditAnywhere, Category = BusProperties)
	TObjectPtr<UAudioBus> AudioBus;

	/** Stop the source bus when the volume goes to zero. */
	UPROPERTY(meta = (DeprecatedProperty))
	uint32 bAutoDeactivateWhenSilent:1;

	//~ Begin UObject Interface.
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual bool CanVisualizeAsset() const override
	{
		return false;
	}

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	//~ End UObject Interface.

	//~ Begin USoundBase Interface.
	virtual bool IsPlayable() const override;
	virtual float GetDuration() const override;
	//~ End USoundBase Interface.

protected:
	void Init();
};
