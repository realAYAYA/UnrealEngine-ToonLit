// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HarmonixMetasound/Components/MusicClockComponent.h"
#include "HarmonixMidi/MidiSongPos.h"
#include "Components/ActorComponent.h"

#include "MusicTempometerComponent.generated.h"

class UMaterialParameterCollection;
class UMaterialParameterCollectionInstance;

/**
 * UMusicTempometerComponent provides playback properties of a UMusicClockComponent on its actor and optionally updates a UMaterialParameterCollection.
 */
UCLASS(ClassGroup = (MetaSoundMusic), PrioritizeCategories = "MusicClock", meta = (BlueprintSpawnableComponent, DisplayName = "Music Tempometer", ScriptName = MusicTempometerComponent))
class HARMONIXMETASOUND_API UMusicTempometerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMusicTempometerComponent();

	/**
	 * Seconds from the beginning of the entire music authoring.
	 * Includes all count-in and pickup bars (ie. won't be negative when 
	 * the music starts, and bar 1 beat 1 may not be at 0.0 seconds!
	 */
	UPROPERTY(EditAnywhere, Category = "MusicClock")
	FName SecondsIncludingCountInParameterName = "MusicSecondsIncludingCountIn";

	/**
	 * Bars from the beginning of the music. Includes all 
	 * count-in and pickup bars (ie. won't be negative when the music starts,
	 * and bar 1 beat 1 of the music may not be equal to elapsed bar 0.0!
	 * NOTE: This Bar, unlike the Bar in the MusicTimestamp structure, is
	 * floating point and will include a fractional portion.
	 */
	UPROPERTY(EditAnywhere, Category = "MusicClock")
	FName BarsIncludingCountInParameterName = "MusicBarsIncludingCountIn";

	/**
	 * Total Beats from the beginning of the music. Includes all 
	 * count-in and pickup bars/beats (ie. won't be negative when the music starts,
	 * and elapsed beat 0.0 may not equal a timestamp of bar 1 beat 1!
	 */
	UPROPERTY(EditAnywhere, Category = "MusicClock")
	FName BeatsIncludingCountInParameterName = "MusicBeatsIncludingCountIn";

	/**
	 * Seconds from Bar 1 Beat 1 of the music. If the music has a count-in
	 * or pickup bars this number may be negative when the music starts!
	 */
	UPROPERTY(EditAnywhere, Category = "MusicClock")
	FName SecondsFromBarOneParameterName = "MusicSecondsFromBarOne";

	/**
	 * This is the bar member of the MusicTimestamp structure which represents a classic
	 * "music time" where bar 1 beat 1 is the beginning of the music AFTER count-in and pickups.
	 * If the music has a count-in or pickup bars this number may be negative (or zero) until 
	 * bar 1 beat 1 is reached!
	 * NOTE: Unlike "BarsFromCountIn", Bar here will be an integer! This because it is the bar 
	 * from the MusicTimestamp structure which also provides a "beat in bar" member to denote 
	 * a "distance into the bar".
	 */
	UPROPERTY(EditAnywhere, Category = "MusicClock")
	FName TimestampBarParameterName = "MusicTimestampBar";

	/**
	 * This is the beat member of the MusicTimestamp structure which represents a classic
	 * "music time" where bar 1 beat 1 is the beginning of the music AFTER count-in and pickups.
	 * If the music has a count-in or pickup bars this number may be negative (or zero) until
	 * bar 1 beat 1 is reached! NOTE: beat in this context is 1 based! The first beat in a bar
	 * is beat 1!
	 * Note: This is a floating point number.
	 */
	UPROPERTY(EditAnywhere, Category = "MusicClock")
	FName TimestampBeatInBarParameterName = "MusicTimestampBeatInBar";

	/**
	 * Progress of the current bar [0, 1]. This is the same as the
	 * fractional part of BarsFromCountIn.
	 */
	UPROPERTY(EditAnywhere, Category = "MusicClock")
	FName BarProgressParameterName = "MusicBarProgress";

	/**
	 * Progress of the current beat [0, 1]. This is the same as the
	 * fractional part of BeatsFromCountIn;
	 */
	UPROPERTY(EditAnywhere, Category = "MusicClock")
	FName BeatProgressParameterName = "MusicBeatProgress";

	/**
	 * Current time signature numerator (beats per bar).
	 */
	UPROPERTY(EditAnywhere, Category = "MusicClock")
	FName TimeSignatureNumeratorParameterName = "MusicTimeSignatureNumerator";

	/**
	 * Current time signature denominator (scale from note duration to beat).
	 */
	UPROPERTY(EditAnywhere, Category = "MusicClock")
	FName TimeSignatureDenominatorParameterName = "MusicTimeSignatureDenominator";

	/**
	 * Current tempo (beats per minute).
	 */
	UPROPERTY(EditAnywhere, Category = "MusicClock")
	FName TempoParameterName = "MusicTempo";

	/**
	 * The FSongPos for which the game thread is currently issuing graphics rendering commands, according to calibration data.
	 */
	UFUNCTION(BlueprintPure, Category = "MusicClock")
	const FMidiSongPos& GetSongPos() const
	{
		UpdateCachedSongPosIfNeeded();
		return SongPos;
	}

	/**
	* Seconds from the beginning of the entire music authoring.
	* Includes all count-in and pickup bars (ie. won't be negative when
	* the music starts, and bar 1 beat 1 may not be at 0.0 seconds!
	*/
	UFUNCTION(BlueprintPure, Category = "MusicClock")
	float GetSecondsIncludingCountIn() const
	{
		UpdateCachedSongPosIfNeeded();
		return SongPos.SecondsIncludingCountIn;
	}

	/**
	* Bars from the beginning of the music. Includes all
	* count-in and pickup bars (ie. won't be negative when the music starts,
	* and bar 1 beat 1 of the music may not be equal to elapsed bar 0.0!
	*/
	UFUNCTION(BlueprintPure, Category = "MusicClock")
	float GetBarsIncludingCountIn() const
	{
		UpdateCachedSongPosIfNeeded();
		return SongPos.BarsIncludingCountIn;
	}

	/**
	 * Beats from the beginning of the music. Includes all
	 * count-in and pickup bars/beats (ie. won't be negative when the music starts,
	 * and elapsed beat 0.0 may not equal a timestamp of bar 1 beat 1!
	 */
	UFUNCTION(BlueprintPure, Category = "MusicClock")
	float GetBeatsIncludingCountIn() const
	{
		UpdateCachedSongPosIfNeeded();
		return SongPos.BeatsIncludingCountIn;
	}

	/**
	 * Seconds from Bar 1 Beat 1 of the music. If the music has a count-in
	 * or pickup bars this number may be negative when the music starts!
	 */
	UFUNCTION(BlueprintPure, Category = "MusicClock")
	float GetSecondsFromBarOne() const
	{
		UpdateCachedSongPosIfNeeded();
		return SongPos.SecondsFromBarOne;
	}

	/**
	 * Current bar & beat in the traditional format, where...
	 *     - bar 1 beat 1 is the beginning of the song.
	 *     - bars BEFORE bar 1 are count-in or "pickup" bars.
	 *     - beat is always 1 or greater.
	 */
	UFUNCTION(BlueprintPure, Category = "MusicClock")
	FMusicTimestamp GetTimestamp() const
	{
		UpdateCachedSongPosIfNeeded();
		return SongPos.Timestamp;
	}

	/**
	 * Progress of the current bar [0, 1).
	 */
	UFUNCTION(BlueprintPure, Category = "MusicClock")
	float GetBarProgress() const
	{
		UpdateCachedSongPosIfNeeded();
		return FMath::Fractional(SongPos.BarsIncludingCountIn);
	}

	/**
	 * Progress of the current beat [0, 1).
	 */
	UFUNCTION(BlueprintPure, Category = "MusicClock")
	float GetBeatProgress() const
	{
		UpdateCachedSongPosIfNeeded();
		return FMath::Fractional(SongPos.BeatsIncludingCountIn);
	}

	/**
	 * Current time signature numerator (beats per bar for a simple meter).
	 */
	UFUNCTION(BlueprintPure, Category = "MusicClock")
	float GetTimeSignatureNumerator() const
	{
		UpdateCachedSongPosIfNeeded();
		return SongPos.TimeSigNumerator;
	}

	/**
	 * Current time signature denominator (scale from note duration to beat fraction for a simple meter).
	 */
	UFUNCTION(BlueprintPure, Category = "MusicClock")
	float GetTimeSignatureDenominator() const
	{
		UpdateCachedSongPosIfNeeded();
		return SongPos.TimeSigDenominator;
	}

	/**
	 * Current tempo (beats per minute).
	 */
	UFUNCTION(BlueprintPure, Category = "MusicClock")
	float GetTempo() const
	{
		UpdateCachedSongPosIfNeeded();
		return SongPos.Tempo;
	}

	/**
	 * Set the MaterialParameterCollection whose parameters will be updated.
	 * If any of the named parameters are missing they will be ignored.
	 */
	UFUNCTION(BlueprintSetter, Category = "MusicClock")
	void SetMaterialParameterCollection(UMaterialParameterCollection* InMaterialParameterCollection)
	{
		FScopeLock lock(&SongPosUpdateMutex);
		MaterialParameterCollection = InMaterialParameterCollection;
		SetComponentTickEnabledAsync(MaterialParameterCollection != nullptr);
	}

	UFUNCTION(BlueprintPure, Category = "MusicClock")
	UMaterialParameterCollection* GetMaterialParameterCollection() const
	{
		FScopeLock lock(&SongPosUpdateMutex);
		return MaterialParameterCollection;
	}

	/**
	 * SetSongPosInterface allows setting any UObject implementing the ISongPosInterface as the attribute source.
	 */
	UFUNCTION(BlueprintCallable, Category = "MusicClock")
	void SetClock(UMusicClockComponent* InClockComponent)
	{
		FScopeLock lock(&SongPosUpdateMutex);
		MusicClock = MoveTemp(InClockComponent);
	}

	/**
	 * SetSongPosInterfaceFromActor sets the actor or the first of its owned components that implements ISongPosInterface as the attribute source.
	 * BeginPlay calls this on the owning actor when the source ISongPosInterface is not set.
	 */
	UFUNCTION(BlueprintCallable, Category = "MusicClock")
	void SetClockFromActor(AActor* Actor)
	{
		FScopeLock lock(&SongPosUpdateMutex);
		MusicClock = FindClock(Actor);
	}

	UFUNCTION(BlueprintGetter, Category = "MusicClock")
	const UMusicClockComponent* GetClock() const
	{
		FScopeLock lock(&SongPosUpdateMutex);
		return GetClockNoMutex();
	}

	//~ Begin UObject Interface.
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	//~ End UObject Interface.

private:
	UMusicClockComponent* FindClock(AActor* Actor) const;
	void SetOwnerClock() const;

	UFUNCTION(BlueprintCallable, Category = "MusicClock")
	const UMusicClockComponent* GetClockNoMutex() const
	{
		if (!IsValid(MusicClock.Get()))
		{
			SetOwnerClock();
		}
		return MusicClock;
	}

	UMusicClockComponent* GetMutableClockNoMutex() const
	{
		if (!IsValid(MusicClock.Get()))
		{
			SetOwnerClock();
		}
		return MusicClock;
	}

	void UpdateCachedSongPos() const;
	void UpdateCachedSongPosIfNeeded() const
	{
		FScopeLock lock(&SongPosUpdateMutex);
		if (GFrameCounter != LastFrameCounter)
		{
			UpdateCachedSongPos();
		}
	}

	UPROPERTY(BlueprintGetter = GetSongPos, Transient, Category = "MusicClock")
	mutable FMidiSongPos SongPos;

	mutable FCriticalSection SongPosUpdateMutex;
	mutable uint64 LastFrameCounter;

	/**
	 * Music whose tempo to detect.
	 */
	UPROPERTY(BlueprintGetter = GetClock, BlueprintSetter = SetClock, Category = "MusicClock")
	mutable TObjectPtr<UMusicClockComponent> MusicClock;

	UPROPERTY(EditAnywhere, BlueprintGetter = GetMaterialParameterCollection, BlueprintSetter = SetMaterialParameterCollection, Category = "MusicClock")
	TObjectPtr<UMaterialParameterCollection> MaterialParameterCollection;

	UPROPERTY(Transient)
	mutable TObjectPtr<UMaterialParameterCollectionInstance> MaterialParameterCollectionInstance;
};
