// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TakeRecorderSource.h"
#include "TakeRecorderSources.h"
#include "UObject/SoftObjectPtr.h"
#include "Templates/SubclassOf.h"
#include "GameFramework/Actor.h"
#include "ISequenceAudioRecorder.h"
#include "TakeRecorderMicrophoneAudioSource.generated.h"

/** A recording source that records microphone audio */
UCLASS(Abstract, config=EditorSettings, DisplayName="Microphone Audio Recorder")
class UTakeRecorderMicrophoneAudioSourceSettings : public UTakeRecorderSource
{
public:
	GENERATED_BODY()

	UTakeRecorderMicrophoneAudioSourceSettings(const FObjectInitializer& ObjInit);

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	
	// UTakeRecorderSource Interface
	virtual FString GetSubsceneTrackName(ULevelSequence* InSequence) const override;
	virtual FString GetSubsceneAssetName(ULevelSequence* InSequence) const override;
	// ~UTakeRecorderSource Interface

	/** Name of the recorded audio track name */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Source")
	FText AudioTrackName;

	/** The name of the subdirectory audio will be placed in. Leave this empty to place into the same directory as the sequence base path */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Source")
	FString AudioSubDirectory;
};

/** A recording source that records microphone audio */
UCLASS(Category="Audio", config=EditorSettings, meta = (TakeRecorderDisplayName = "Microphone Audio"))
class UTakeRecorderMicrophoneAudioSource : public UTakeRecorderMicrophoneAudioSourceSettings
{
public:
	GENERATED_BODY()

	UTakeRecorderMicrophoneAudioSource(const FObjectInitializer& ObjInit);

	/** Gain in decibels to apply to recorded audio */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Source", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float AudioGain;

	/** Whether or not to split mic channels into separate audio tracks. If not true, a max of 2 input channels is supported. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Source")
	bool bSplitAudioChannelsIntoSeparateTracks;

	/** Replace existing recorded audio with any newly recorded audio */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Source")
	bool bReplaceRecordedAudio;

private:

	// UTakeRecorderSource
	virtual TArray<UTakeRecorderSource*> PreRecording(ULevelSequence* InSequence, FMovieSceneSequenceID InSequenceID, ULevelSequence* InMasterSequence, FManifestSerializer* InManifestSerializer) override;
	virtual void StartRecording(const FTimecode& InSectionStartTimecode, const FFrameNumber& InSectionFirstFrame, class ULevelSequence* InSequence) override;
	virtual void StopRecording(class ULevelSequence* InSequence) override;
	virtual TArray<UTakeRecorderSource*> PostRecording(ULevelSequence* InSequence, class ULevelSequence* InMasterSequence, const bool bCancelled) override;
	virtual FText GetDisplayTextImpl() const override;
	virtual void AddContentsToFolder(class UMovieSceneFolder* InFolder) override;
	virtual bool CanAddSource(UTakeRecorderSources* InSources) const override;
	// ~UTakeRecorderSource

private:
	TWeakObjectPtr<class UMovieSceneAudioTrack> CachedAudioTrack;

	TArray<TWeakObjectPtr<USoundWave>> RecordedSoundWaves;

	TUniquePtr<ISequenceAudioRecorder> AudioRecorder;
	
	//Created in PreRecord but used in StartRecording.
	FDirectoryPath AudioDirectory;
	FString AssetName;
};
