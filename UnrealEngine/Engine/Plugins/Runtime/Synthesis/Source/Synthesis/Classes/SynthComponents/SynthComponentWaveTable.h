// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/SynthComponent.h"
#include "DSP/SampleBufferReader.h"
#include "Sound/SampleBufferIO.h"
#include "SynthComponentWaveTable.generated.h"

UENUM(BlueprintType)
enum class ESamplePlayerSeekType : uint8
{
	FromBeginning,
	FromCurrentPosition,
	FromEnd,
	Count UMETA(Hidden)
};


// Called when a sample has finished loading into the sample player
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSampleLoaded);

// Called while a sample player is playing back. Indicates the playhead progress in percent and as absolute time value (within the file).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSamplePlaybackProgress, float, ProgressPercent, float, ProgressTimeSeconds);


UCLASS(ClassGroup = Synth, meta = (BlueprintSpawnableComponent))
class SYNTHESIS_API USynthSamplePlayer : public USynthComponent
{
	GENERATED_BODY()

	USynthSamplePlayer(const FObjectInitializer& ObjInitializer);
	~USynthSamplePlayer();

	// Initialize the synth component
	virtual bool Init(int32& SampleRate) override;

	// Called to generate more audio
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;

	//~ Begin ActorComponent Interface.
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	//~ End ActorComponent Interface

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Synth|Preset")
	TObjectPtr<USoundWave> SoundWave;

	UPROPERTY(BlueprintAssignable, Category = "Synth|Components|Audio")
	FOnSampleLoaded OnSampleLoaded;

	UPROPERTY(BlueprintAssignable, Category = "Synth|Components|Audio")
	FOnSamplePlaybackProgress OnSamplePlaybackProgress;

	// This will override the current sound wave if one is set, stop audio, and reload the new sound wave
	UFUNCTION(BlueprintCallable, Category = "Synth|Components|Audio")
	void SetSoundWave(USoundWave* InSoundWave);

	UFUNCTION(BlueprintCallable, Category = "Synth|Components|Audio")
	void SetPitch(float InPitch, float TimeSec);

	UFUNCTION(BlueprintCallable, Category = "Synth|Components|Audio")
	void SeekToTime(float TimeSec, ESamplePlayerSeekType SeekType, bool bWrap = true);

	UFUNCTION(BlueprintCallable, Category = "Synth|Components|Audio")
	void SetScrubMode(bool bScrubMode);

	UFUNCTION(BlueprintCallable, Category = "Synth|Components|Audio")
	void SetScrubTimeWidth(float InScrubTimeWidthSec);

	UFUNCTION(BlueprintCallable, Category = "Synth|Components|Audio")
	float GetSampleDuration() const;

	UFUNCTION(BlueprintCallable, Category = "Synth|Components|Audio")
	float GetCurrentPlaybackProgressTime() const;

	UFUNCTION(BlueprintCallable, Category = "Synth|Components|Audio")
	float GetCurrentPlaybackProgressPercent() const;

	UFUNCTION(BlueprintCallable, Category = "Synth|Components|Audio")
	bool IsLoaded() const;

protected:
	void LoadSoundWaveInternal();

	Audio::FSampleBufferReader SampleBufferReader;
	Audio::TSampleBuffer<int16> SampleBuffer;
	Audio::FSoundWavePCMLoader SoundWaveLoader;

	float SampleDurationSec;
	float SamplePlaybackProgressSec;

	bool bIsLoaded;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
