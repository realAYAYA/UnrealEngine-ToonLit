// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/SceneComponent.h"
#include "MediaSampleQueue.h"
#include "Templates/SharedPointer.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"

#include "Kismet/BlueprintFunctionLibrary.h"

#include "DropTimecode.h"
#include "LinearTimecodeComponent.generated.h"

class FLinearTimecodeDecoder;
class FMediaPlayerFacade;
class IMediaPlayer;
class UMediaPlayer;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTimecodeChange, const FDropTimecode&, Timecode);

/**
 * Implements a Linear timecode decoder.
 */
UCLASS(ClassGroup = Media, editinlinenew, meta = (BlueprintSpawnableComponent))
class LINEARTIMECODE_API ULinearTimecodeComponent
	: public USceneComponent
{
	GENERATED_UCLASS_BODY()

public:
	/** The media player asset associated with this component. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Media")
	TObjectPtr<UMediaPlayer> MediaPlayer;

	UPROPERTY(BlueprintReadOnly, Category = "Media")
	FDropTimecode DropTimecode;

protected:
	static double FrameRateToFrameDelta(int32 InFrameRate, int32 InDrop);

public:
	/** Get the Frame Number
	 * @return Frame Number
	 */
	UFUNCTION(BlueprintCallable, Category = "Media")
	int32 GetDropFrameNumber() const;

	/** Convert a drop timecode into a frame number
	 * @param Timecode - Used to access the framerate, and drop flag
	 * @param FrameNumber - returned calculated frame number
	 */
	UFUNCTION(BlueprintCallable, Category = "Media")
	static void GetDropTimeCodeFrameNumber(const FDropTimecode& Timecode, int32& FrameNumber);

	/** Convert frame number into a drop timecode
	 * @param Timecode - used to access the framerate, and drop flag
	 * @param FrameNumber - Frame number to set in the returned timecode
	 * @param OutTimecode - Returned drop timecode
	 */
	UFUNCTION(BlueprintCallable, Category = "Media")
	static void SetDropTimecodeFrameNumber(const FDropTimecode& Timecode, int32 FrameNumber, FDropTimecode& OutTimecode);

	/** Called when the timecode changes */
	UPROPERTY(BlueprintAssignable, Category = "Media")
	FOnTimecodeChange OnTimecodeChange;

public:
	void UpdatePlayer();

public:
	//~ UActorComponent interface
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

public:
	//~ USceneComponent interface
	virtual void Activate(bool bReset = false) override;
	virtual void Deactivate() override;

protected:
	void ProcessAudio(TSharedPtr<FMediaAudioSampleQueue, ESPMode::ThreadSafe> SampleQueue);

protected:
	/** The player facade that's currently providing audio samples. */
	TWeakPtr<FMediaPlayerFacade, ESPMode::ThreadSafe> CurrentPlayerFacade;

	/** Audio sample queue. */
	TSharedPtr<FMediaAudioSampleQueue, ESPMode::ThreadSafe> SampleQueue;

	/** The Actual decoder */
	TSharedPtr<FLinearTimecodeDecoder, ESPMode::ThreadSafe> TimecodeDecoder;
};

/**
 * Extend type conversions to handle FDropTimecode structure
 */
UCLASS(meta = (BlueprintThreadSafe))
class LINEARTIMECODE_API UDropTimecodeToStringConversion : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	/** Convert a timecode structure into a string */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "ToString (DropTimecode)", CompactNodeTitle = "->", BlueprintAutocast), Category = "Utilities|String")
	static FString Conv_DropTimecodeToString(const FDropTimecode& InTimecode);
};

