// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneSection.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundAttenuation.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Channels/MovieSceneStringChannel.h"
#include "Channels/MovieSceneBoolChannel.h"
#include "Channels/MovieSceneIntegerChannel.h"
#include "Sections/MovieSceneActorReferenceSection.h"
#include "Channels/MovieSceneAudioTriggerChannel.h"
#include "MovieSceneAudioSection.generated.h"

class USoundBase;

/**
 * Audio section, for use in the audio track, or by attached audio objects
 */
UCLASS()
class MOVIESCENETRACKS_API UMovieSceneAudioSection
	: public UMovieSceneSection
{
	GENERATED_UCLASS_BODY()

public:

	/** Sets this section's sound */
	UFUNCTION(BlueprintCallable, Category="Sequencer|Section")
	void SetSound(class USoundBase* InSound);

	/** Gets the sound for this section */
	UFUNCTION(BlueprintPure, Category = "Sequencer|Section")
	class USoundBase* GetSound() const {return Sound;}

	/** Set the offset into the beginning of the audio clip */
	UFUNCTION(BlueprintCallable, Category = "Sequencer|Section")
	void SetStartOffset(FFrameNumber InStartOffset) {StartFrameOffset = InStartOffset;}

	/** Get the offset into the beginning of the audio clip */
	UFUNCTION(BlueprintPure, Category = "Sequencer|Section")
	FFrameNumber GetStartOffset() const {return StartFrameOffset;}

	/**
	 * Gets the sound volume curve
	 *
	 * @return The rich curve for this sound volume
	 */
	const FMovieSceneFloatChannel& GetSoundVolumeChannel() const { return SoundVolume; }

	/**
	 * Gets the sound pitch curve
	 *
	 * @return The rich curve for this sound pitch
	 */
	const FMovieSceneFloatChannel& GetPitchMultiplierChannel() const { return PitchMultiplier; }

	/**
	 * Return the sound volume
	 *
	 * @param InTime	The position in time within the movie scene
	 * @return The volume the sound will be played with.
	 */
	float GetSoundVolume(FFrameTime InTime) const
	{
		float OutValue = 0.f;
		SoundVolume.Evaluate(InTime, OutValue);
		return OutValue;
	}

	/**
	 * Return the pitch multiplier
	 *
	 * @param Position	The position in time within the movie scene
	 * @return The pitch multiplier the sound will be played with.
	 */
	float GetPitchMultiplier(FFrameTime InTime) const
	{
		float OutValue = 0.f;
		PitchMultiplier.Evaluate(InTime, OutValue);
		return OutValue;
	}

	/**
	 * @return Whether to allow looping if the section length is greater than the sound duration
	 */
	bool GetLooping() const
	{
		return bLooping;
	}

	/**
	 * @return Whether subtitles should be suppressed
	 */
	bool GetSuppressSubtitles() const
	{
		return bSuppressSubtitles;
	}

	/**
	 * @return Whether override settings on this section should be used
	 */
	bool GetOverrideAttenuation() const
	{
		return bOverrideAttenuation;
	}

	/**
	 * @return The attenuation settings
	 */
	USoundAttenuation* GetAttenuationSettings() const
	{
		return AttenuationSettings;
	}

	/*
	 * @return The attach actor data
	 */
	const FMovieSceneActorReferenceData& GetAttachActorData() const { return AttachActorData; }

	/*
	 * @return The attach component given the bound actor and the actor attach key with the component and socket names
	 */
	USceneComponent* GetAttachComponent(const AActor* InParentActor, const FMovieSceneActorReferenceKey& Key) const;

	/** ~UObject interface */
	virtual void PostLoad() override;

	/** Called when subtitles are sent to the SubtitleManager.  Set this delegate if you want to hijack the subtitles for other purposes */
	void SetOnQueueSubtitles(const FOnQueueSubtitles& InOnQueueSubtitles)
	{
		OnQueueSubtitles = InOnQueueSubtitles;
	}

	/** Called when subtitles are sent to the SubtitleManager.  Set this delegate if you want to hijack the subtitles for other purposes */
	const FOnQueueSubtitles& GetOnQueueSubtitles() const
	{
		return OnQueueSubtitles;
	}

	/** called when we finish playing audio, either because it played to completion or because a Stop() call turned it off early */
	void SetOnAudioFinished(const FOnAudioFinished& InOnAudioFinished)
	{
		OnAudioFinished = InOnAudioFinished;
	}

	/** called when we finish playing audio, either because it played to completion or because a Stop() call turned it off early */
	const FOnAudioFinished& GetOnAudioFinished() const
	{
		return OnAudioFinished;
	}
	
	void SetOnAudioPlaybackPercent(const FOnAudioPlaybackPercent& InOnAudioPlaybackPercent)
	{
		OnAudioPlaybackPercent = InOnAudioPlaybackPercent;
	}

	const FOnAudioPlaybackPercent& GetOnAudioPlaybackPercent() const
	{
		return OnAudioPlaybackPercent;
	}
	
	/** Overloads for each input type, const */
	void ForEachInput(TFunction<void(FName, const FMovieSceneBoolChannel&)> InFunction) const { ForEachInternal(InFunction, Inputs_Bool); }
	void ForEachInput(TFunction<void(FName, const FMovieSceneStringChannel&)> InFunction) const { ForEachInternal(InFunction, Inputs_String); }
	void ForEachInput(TFunction<void(FName, const FMovieSceneIntegerChannel&)> InFunction) const  { ForEachInternal(InFunction, Inputs_Int); }
	void ForEachInput(TFunction<void(FName, const FMovieSceneFloatChannel&)> InFunction) const { ForEachInternal(InFunction, Inputs_Float); }
	void ForEachInput(TFunction<void(FName, const FMovieSceneAudioTriggerChannel&)> InFunction) const { ForEachInternal(InFunction, Inputs_Trigger); }

public:

	//~ UMovieSceneSection interface
	virtual TOptional<TRange<FFrameNumber> > GetAutoSizeRange() const override;
	virtual void TrimSection(FQualifiedFrameTime TrimTime, bool bTrimLeft, bool bDeleteKeys) override;
	virtual UMovieSceneSection* SplitSection(FQualifiedFrameTime SplitTime, bool bDeleteKeys) override;
	virtual TOptional<FFrameTime> GetOffsetTime() const override;
	virtual void MigrateFrameTimes(FFrameRate SourceRate, FFrameRate DestinationRate) override;
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostEditImport() override;
	virtual EMovieSceneChannelProxyType CacheChannelProxy() override;

private:
	template<typename ChannelType, typename ForEachFunction>
	FORCEINLINE static void ForEachInternal(ForEachFunction InFuncton, const TMap<FName, ChannelType>& InMapToIterate) 
	{
		for (auto& Item : InMapToIterate)
		{
			InFuncton(Item.Key, Item.Value);
		}	
	}

	void SetupSoundInputParameters(const USoundBase* InSoundBase);

	/** The sound cue or wave that this section plays */
	UPROPERTY(EditAnywhere, Category="Audio")
	TObjectPtr<USoundBase> Sound;

	/** The offset into the beginning of the audio clip */
	UPROPERTY(EditAnywhere, Category="Audio")
	FFrameNumber StartFrameOffset;

	/** The offset into the beginning of the audio clip */
	UPROPERTY()
	float StartOffset_DEPRECATED;

	/** The absolute time that the sound starts playing at */
	UPROPERTY( )
	float AudioStartTime_DEPRECATED;
	
	/** The amount which this audio is time dilated by */
	UPROPERTY( )
	float AudioDilationFactor_DEPRECATED;

	/** The volume the sound will be played with. */
	UPROPERTY( )
	float AudioVolume_DEPRECATED;

	/** The volume the sound will be played with. */
	UPROPERTY( )
	FMovieSceneFloatChannel SoundVolume;

	/** The pitch multiplier the sound will be played with. */
	UPROPERTY( )
	FMovieSceneFloatChannel PitchMultiplier;

	/** Generic inputs for the sound  */
	UPROPERTY()
	TMap<FName, FMovieSceneFloatChannel> Inputs_Float;
	UPROPERTY()
	TMap<FName, FMovieSceneStringChannel> Inputs_String;
	UPROPERTY()
	TMap<FName, FMovieSceneBoolChannel> Inputs_Bool;
	UPROPERTY()
	TMap<FName, FMovieSceneIntegerChannel> Inputs_Int;
	UPROPERTY()
	TMap<FName, FMovieSceneAudioTriggerChannel> Inputs_Trigger;

	UPROPERTY()
	FMovieSceneActorReferenceData AttachActorData;

	/* Allow looping if the section length is greater than the sound duration */
	UPROPERTY(EditAnywhere, Category = "Audio")
	bool bLooping;

	UPROPERTY(EditAnywhere, Category = "Audio")
	bool bSuppressSubtitles;

	/** Should the attenuation settings on this section be used. */
	UPROPERTY( EditAnywhere, Category="Attenuation" )
	bool bOverrideAttenuation;

	/** The attenuation settings to use. */
	UPROPERTY( EditAnywhere, Category="Attenuation" )
	TObjectPtr<class USoundAttenuation> AttenuationSettings;

	/** Called when subtitles are sent to the SubtitleManager.  Set this delegate if you want to hijack the subtitles for other purposes */
	UPROPERTY()
	FOnQueueSubtitles OnQueueSubtitles;

	/** called when we finish playing audio, either because it played to completion or because a Stop() call turned it off early */
	UPROPERTY()
	FOnAudioFinished OnAudioFinished;

	UPROPERTY()
	FOnAudioPlaybackPercent OnAudioPlaybackPercent;
};
